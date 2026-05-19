#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <cstring>
#include <csignal>
#include "platform.hpp"
#include "common.hpp"
#include "ChatServer.hpp"

static const int STRESS_PORT = PORT;
static std::atomic<bool> server_ready{false};
static std::atomic<int64_t> total_messages_sent{0};
static std::atomic<int64_t> total_messages_received{0};
static std::atomic<int64_t> total_connect_errors{0};
static std::atomic<int64_t> total_send_errors{0};
static std::atomic<int64_t> total_heartbeat_ok{0};
static std::atomic<int64_t> total_heartbeat_fail{0};

struct StressClientResult {
    int client_id;
    int64_t connect_time_ms;
    int64_t messages_sent;
    int64_t messages_received;
    double  avg_latency_ms;
    int     errors;
    bool    heartbeat_working;
};

struct StressTestReport {
    int total_clients;
    int duration_sec;
    int64_t total_messages_sent;
    int64_t total_messages_received;
    int64_t total_connect_errors;
    int64_t total_send_errors;
    int64_t total_heartbeat_ok;
    int64_t total_heartbeat_fail;
    double  avg_connect_time_ms;
    double  max_connect_time_ms;
    double  min_connect_time_ms;
    double  throughput_msg_per_sec;
    double  avg_latency_ms;
    double  p50_latency_ms;
    double  p95_latency_ms;
    double  p99_latency_ms;
    double  error_rate;
    bool    passed;
    std::string failures;
    std::vector<StressClientResult> client_results;
};

class StressTestEnvironment : public ::testing::Environment {
public:
    ~StressTestEnvironment() override = default;

    void SetUp() override {
        platform::network_startup();
    }

    void TearDown() override {
        platform::network_cleanup();
    }
};

static void run_stress_client(int client_id,
                              int duration_sec,
                              int msg_interval_ms,
                              StressClientResult& result,
                              std::vector<double>& latencies)
{
    result.client_id = client_id;
    result.messages_sent = 0;
    result.messages_received = 0;
    result.avg_latency_ms = 0;
    result.errors = 0;
    result.heartbeat_working = false;

    platform::socket_t sock = platform::create_tcp_socket();
    if (sock == platform::INVALID_SOCK) {
        result.errors++;
        total_connect_errors++;
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(STRESS_PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    auto t1 = std::chrono::steady_clock::now();
    int conn_ret = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    auto t2 = std::chrono::steady_clock::now();
    result.connect_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

    if (conn_ret < 0) {
        result.errors++;
        total_connect_errors++;
        platform::close_socket(sock);
        return;
    }

    platform::set_non_blocking(sock);

    auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(duration_sec);
    auto next_msg_time = std::chrono::steady_clock::now();
    auto next_ping_time = std::chrono::steady_clock::now() + std::chrono::seconds(HEARTBEAT_INTERVAL_SEC);

    std::string send_name = "/name StressUser" + std::to_string(client_id) + "\n";
    send(sock, send_name.c_str(), send_name.length(), 0);
    result.messages_sent++;
    total_messages_sent++;

    char recv_buf[BUFFER_SIZE];

    while (std::chrono::steady_clock::now() < end_time)
    {
        auto now = std::chrono::steady_clock::now();

        if (now >= next_ping_time) {
            auto ping_t1 = std::chrono::steady_clock::now();
            std::string ping = "PING\n";
            send(sock, ping.c_str(), ping.length(), 0);
            result.messages_sent++;
            total_messages_sent++;

            platform::sleep_ms(200);

            ssize_t n = recv(sock, recv_buf, sizeof(recv_buf) - 1, 0);
            if (n > 0) {
                recv_buf[n] = '\0';
                std::string resp(recv_buf, n);
                if (resp.find("PONG") != std::string::npos) {
                    result.heartbeat_working = true;
                    total_heartbeat_ok++;
                    auto ping_t2 = std::chrono::steady_clock::now();
                    double lat = std::chrono::duration_cast<std::chrono::microseconds>(ping_t2 - ping_t1).count() / 1000.0;
                    latencies.push_back(lat);
                } else {
                    total_heartbeat_fail++;
                }
            } else {
                total_heartbeat_fail++;
            }
            next_ping_time = now + std::chrono::seconds(HEARTBEAT_INTERVAL_SEC);
        }

        if (now >= next_msg_time) {
            auto msg_t1 = std::chrono::steady_clock::now();
            std::string msg = "Stress message #" + std::to_string(result.messages_sent) +
                              " from client " + std::to_string(client_id) + "\n";
            int send_ret = send(sock, msg.c_str(), msg.length(), 0);
            if (send_ret < 0) {
                total_send_errors++;
                result.errors++;
            } else {
                result.messages_sent++;
                total_messages_sent++;
            }

            platform::sleep_ms(5);

            ssize_t n = recv(sock, recv_buf, sizeof(recv_buf) - 1, 0);
            if (n > 0) {
                result.messages_received++;
                total_messages_received++;
                auto msg_t2 = std::chrono::steady_clock::now();
                double lat = std::chrono::duration_cast<std::chrono::microseconds>(msg_t2 - msg_t1).count() / 1000.0;
                latencies.push_back(lat);
            }

            next_msg_time = now + std::chrono::milliseconds(msg_interval_ms);
        }

        platform::sleep_ms(1);
    }

    std::string quit_msg = "/quit\n";
    send(sock, quit_msg.c_str(), quit_msg.length(), 0);
    platform::sleep_ms(50);
    platform::close_socket(sock);
}

static StressTestReport run_stress_test(int num_clients, int duration_sec, int msg_interval_ms) {
    StressTestReport report;
    report.total_clients = num_clients;
    report.duration_sec = duration_sec;

    total_messages_sent = 0;
    total_messages_received = 0;
    total_connect_errors = 0;
    total_send_errors = 0;
    total_heartbeat_ok = 0;
    total_heartbeat_fail = 0;

    report.client_results.resize(num_clients);
    std::vector<std::thread> threads;
    std::vector<std::vector<double>> all_latencies(num_clients);

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_clients; ++i) {
        threads.emplace_back(run_stress_client, i, duration_sec, msg_interval_ms,
                             std::ref(report.client_results[i]), std::ref(all_latencies[i]));
        platform::sleep_ms(5);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::steady_clock::now();
    double actual_duration_sec = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0;

    report.total_messages_sent = total_messages_sent.load();
    report.total_messages_received = total_messages_received.load();
    report.total_connect_errors = total_connect_errors.load();
    report.total_send_errors = total_send_errors.load();
    report.total_heartbeat_ok = total_heartbeat_ok.load();
    report.total_heartbeat_fail = total_heartbeat_fail.load();

    std::vector<int64_t> connect_times;
    for (const auto& r : report.client_results) {
        if (r.connect_time_ms > 0) {
            connect_times.push_back(r.connect_time_ms);
        }
    }

    if (!connect_times.empty()) {
        report.avg_connect_time_ms = std::accumulate(connect_times.begin(), connect_times.end(), 0.0) / connect_times.size();
        report.max_connect_time_ms = *std::max_element(connect_times.begin(), connect_times.end());
        report.min_connect_time_ms = *std::min_element(connect_times.begin(), connect_times.end());
    }

    std::vector<double> all_lat;
    for (const auto& lats : all_latencies) {
        all_lat.insert(all_lat.end(), lats.begin(), lats.end());
    }

    if (!all_lat.empty()) {
        std::sort(all_lat.begin(), all_lat.end());
        report.avg_latency_ms = std::accumulate(all_lat.begin(), all_lat.end(), 0.0) / all_lat.size();
        report.p50_latency_ms = all_lat[all_lat.size() * 50 / 100];
        report.p95_latency_ms = all_lat[all_lat.size() * 95 / 100];
        report.p99_latency_ms = all_lat[all_lat.size() * 99 / 100];
    }

    report.throughput_msg_per_sec = actual_duration_sec > 0 ?
        report.total_messages_received / actual_duration_sec : 0;

    int total_ops = report.total_messages_sent + report.total_messages_received;
    report.error_rate = total_ops > 0 ?
        (report.total_connect_errors + report.total_send_errors) * 100.0 / total_ops : 0;

    report.passed = true;
    std::ostringstream failures;

    if (report.throughput_msg_per_sec < STRESS_TEST_MIN_THROUGHPUT_PASS) {
        report.passed = false;
        failures << "Throughput " << report.throughput_msg_per_sec
                 << " msg/sec < threshold " << STRESS_TEST_MIN_THROUGHPUT_PASS << "; ";
    }

    if (report.avg_connect_time_ms > STRESS_TEST_MAX_CONNECT_TIME_MS) {
        report.passed = false;
        failures << "Avg connect time " << report.avg_connect_time_ms
                 << "ms > threshold " << STRESS_TEST_MAX_CONNECT_TIME_MS << "ms; ";
    }

    if (report.error_rate > 5.0) {
        report.passed = false;
        failures << "Error rate " << report.error_rate << "% > 5%; ";
    }

    if (report.total_heartbeat_fail > report.total_heartbeat_ok * 0.1) {
        report.passed = false;
        failures << "Heartbeat failure rate too high; ";
    }

    report.failures = failures.str();

    // Generate JSON report
    std::ostringstream json;
    json << "{\n";
    json << "  \"test_config\": {\n";
    json << "    \"total_clients\": " << num_clients << ",\n";
    json << "    \"duration_sec\": " << duration_sec << ",\n";
    json << "    \"msg_interval_ms\": " << msg_interval_ms << "\n";
    json << "  },\n";
    json << "  \"results\": {\n";
    json << "    \"passed\": " << (report.passed ? "true" : "false") << ",\n";
    json << "    \"total_messages_sent\": " << report.total_messages_sent << ",\n";
    json << "    \"total_messages_received\": " << report.total_messages_received << ",\n";
    json << "    \"throughput_msg_per_sec\": " << report.throughput_msg_per_sec << ",\n";
    json << "    \"avg_connect_time_ms\": " << report.avg_connect_time_ms << ",\n";
    json << "    \"max_connect_time_ms\": " << report.max_connect_time_ms << ",\n";
    json << "    \"min_connect_time_ms\": " << report.min_connect_time_ms << ",\n";
    json << "    \"avg_latency_ms\": " << report.avg_latency_ms << ",\n";
    json << "    \"p50_latency_ms\": " << report.p50_latency_ms << ",\n";
    json << "    \"p95_latency_ms\": " << report.p95_latency_ms << ",\n";
    json << "    \"p99_latency_ms\": " << report.p99_latency_ms << ",\n";
    json << "    \"error_rate_percent\": " << report.error_rate << ",\n";
    json << "    \"heartbeat_ok\": " << report.total_heartbeat_ok << ",\n";
    json << "    \"heartbeat_fail\": " << report.total_heartbeat_fail << ",\n";
    json << "    \"connect_errors\": " << report.total_connect_errors << ",\n";
    json << "    \"send_errors\": " << report.total_send_errors << "\n";
    json << "  },\n";
    json << "  \"thresholds\": {\n";
    json << "    \"min_throughput_msg_per_sec\": " << STRESS_TEST_MIN_THROUGHPUT_PASS << ",\n";
    json << "    \"max_connect_time_ms\": " << STRESS_TEST_MAX_CONNECT_TIME_MS << ",\n";
    json << "    \"max_error_rate_percent\": 5.0\n";
    json << "  }\n";
    json << "}\n";

    std::ofstream report_file("stress_test_report.json");
    if (report_file.is_open()) {
        report_file << json.str();
        report_file.close();
        std::cout << "\n[STRESS] Report written to stress_test_report.json" << std::endl;
    }

    std::cout << "\n========== STRESS TEST REPORT ==========" << std::endl;
    std::cout << json.str();
    std::cout << "=========================================" << std::endl;

    return report;
}

// ==================== GTest Cases ====================

TEST(StressTest, LightLoad_20Clients_10Seconds) {
    auto report = run_stress_test(20, 10, STRESS_TEST_DEFAULT_MSG_INTERVAL_MS);
    double min_throughput = 50.0; // 20 clients @ 200ms = ~100 max, threshold at 50%
    EXPECT_TRUE(report.passed || report.throughput_msg_per_sec >= min_throughput)
        << "Failures: " << report.failures
        << " throughput=" << report.throughput_msg_per_sec;
    EXPECT_LT(report.error_rate, 5.0) << "Error rate too high";
    std::cout << "[STRESS] Light load: " << report.total_clients << " clients, "
              << report.total_messages_received << " msgs received, "
              << report.throughput_msg_per_sec << " msg/sec" << std::endl;
}

TEST(StressTest, MediumLoad_50Clients_15Seconds) {
    auto report = run_stress_test(50, 15, 200);
    double min_th = 100.0; // 50 clients @ 200ms interval = 250 m/s max, threshold at 40%
    EXPECT_TRUE(report.passed || report.throughput_msg_per_sec >= min_th)
        << "Failures: " << report.failures;
    EXPECT_LT(report.avg_connect_time_ms, STRESS_TEST_MAX_CONNECT_TIME_MS);
    EXPECT_GT(report.throughput_msg_per_sec, min_th);
    EXPECT_LT(report.error_rate, 5.0);
    std::cout << "[STRESS] Medium load: " << report.total_clients << " clients, "
              << report.total_messages_received << " msgs received, "
              << report.throughput_msg_per_sec << " msg/sec" << std::endl;
}

TEST(StressTest, HeavyLoad_100Clients_20Seconds) {
    auto report = run_stress_test(100, 20, 300);
    double min_th = 150.0; // 100 clients @ 300ms interval = 333 m/s max, threshold at 45%
    EXPECT_TRUE(report.passed || report.throughput_msg_per_sec >= min_th)
        << "Failures: " << report.failures;
    EXPECT_LT(report.error_rate, 5.0);
    EXPECT_LT(report.avg_connect_time_ms, STRESS_TEST_MAX_CONNECT_TIME_MS);
    std::cout << "[STRESS] Heavy load: " << report.total_clients << " clients, "
              << report.total_messages_received << " msgs received, "
              << report.throughput_msg_per_sec << " msg/sec" << std::endl;
}

TEST(StressTest, HeartbeatReliability) {
    auto report = run_stress_test(30, 20, 500);
    EXPECT_GT(report.total_heartbeat_ok, 0) << "No heartbeats recorded";
    double hb_ratio = report.total_heartbeat_ok > 0 ?
        (double)report.total_heartbeat_fail / report.total_heartbeat_ok : 1.0;
    EXPECT_LT(hb_ratio, 0.1) << "Heartbeat failure rate: " << (hb_ratio * 100) << "%";
    std::cout << "[STRESS] Heartbeat: " << report.total_heartbeat_ok << " OK, "
              << report.total_heartbeat_fail << " FAIL" << std::endl;
}

TEST(StressTest, ConnectionBurst_SimultaneousClients) {
    int num_clients = 100;
    auto start = std::chrono::steady_clock::now();

    std::vector<platform::socket_t> sockets;
    sockets.reserve(num_clients);

    for (int i = 0; i < num_clients; ++i) {
        platform::socket_t sock = platform::create_tcp_socket();
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(STRESS_PORT);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            sockets.push_back(sock);
        } else {
            platform::close_socket(sock);
        }
    }

    auto end = std::chrono::steady_clock::now();
    double burst_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "[STRESS] Connection burst: " << sockets.size() << "/" << num_clients
              << " connected in " << burst_time_ms << "ms" << std::endl;

    EXPECT_GE(static_cast<int>(sockets.size()), num_clients * 90 / 100)
        << "Less than 90% of burst connections succeeded";
    EXPECT_LT(burst_time_ms, STRESS_TEST_MAX_CONNECT_TIME_MS * num_clients / 10)
        << "Burst connection time too high";

    for (auto sock : sockets) {
        platform::close_socket(sock);
    }
}

static bool is_list_only_mode(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--gtest_list_tests" || arg.find("--gtest_list_tests") == 0) {
            return true;
        }
    }
    return false;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new StressTestEnvironment);

    signal(SIGPIPE, SIG_IGN);

    if (is_list_only_mode(argc, argv)) {
        return RUN_ALL_TESTS();
    }

    ChatServer* server = nullptr;
    std::thread* server_thread = nullptr;

    {
        struct sockaddr_in test_addr;
        memset(&test_addr, 0, sizeof(test_addr));
        test_addr.sin_family = AF_INET;
        test_addr.sin_port = htons(STRESS_PORT);
        test_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        platform::socket_t test_fd = platform::create_tcp_socket();
        if (test_fd != platform::INVALID_SOCK) {
            if (connect(test_fd, (struct sockaddr*)&test_addr, sizeof(test_addr)) == 0) {
                server_ready = true;
                std::cout << "[STRESS] Server already running on port " << STRESS_PORT << std::endl;
            }
            platform::close_socket(test_fd);
        }
    }

    if (!server_ready) {
        server = new ChatServer();

        if (server->initialize()) {
            server_thread = new std::thread([server]() {
                std::cout << "[STRESS] Test server starting on port "
                          << STRESS_PORT << "..." << std::endl;
                server_ready = true;
                server->start();
            });

            auto wait_start = std::chrono::steady_clock::now();
            while (!server_ready && std::chrono::steady_clock::now() - wait_start < std::chrono::seconds(10)) {
                platform::sleep_ms(100);
            }

            if (!server_ready) {
                std::cerr << "[STRESS] Server failed to start" << std::endl;
                server->stop();
                server_thread->join();
                delete server_thread;
                delete server;
                return 1;
            }
        } else {
            std::cerr << "[STRESS] Could not start test server" << std::endl;
            delete server;
            return 1;
        }
    }

    platform::sleep_ms(300);

    int result = RUN_ALL_TESTS();

    if (server && server_thread) {
        server->stop();
        server_thread->join();
        delete server_thread;
        delete server;
    }

    return result;
}