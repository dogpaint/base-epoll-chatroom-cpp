#pragma once
#include "ClientInfo.hpp"
#include "common.hpp"
#include "platform.hpp"
#include <unordered_map>
#include <functional>
#include <atomic>
#include <vector>

class ChatServer
{
public:
    ChatServer();
    ~ChatServer();

    bool initialize();
    void start();
    void stop();

    int get_client_count() const { return static_cast<int>(clients_.size()); }
    std::string get_server_info() const;
    std::string get_health_report() const;

    int64_t get_total_messages() const { return total_messages_.load(); }
    int64_t get_heartbeat_failures() const { return heartbeat_failures_.load(); }

private:
    void handle_events(int ready_count);
    void handle_new_connection();
    void handle_client_message(platform::socket_t client_fd);
    void handle_client_disconnect(platform::socket_t client_fd);

    void broadcast_message(const std::string &message, platform::socket_t exclude_fd = platform::INVALID_SOCK);
    void send_to_client(platform::socket_t client_fd, const std::string &message);
    void process_message(platform::socket_t client_fd, const char *buffer, ssize_t len);
    void process_single_message(platform::socket_t client_fd,
        std::unordered_map<platform::socket_t, ClientPtr>::iterator it,
        const std::string &message);

    void heartbeat_check();
    void health_monitor_loop();

    platform::socket_t server_fd_;
    platform::Poller poller_;
    platform::PollEntry events_[MAX_EVENTS];

    std::unordered_map<platform::socket_t, ClientPtr> clients_;
    std::atomic<bool> is_running_{false};

    std::atomic<int64_t> total_messages_{0};
    std::atomic<int64_t> total_connections_{0};
    std::atomic<int64_t> heartbeat_failures_{0};
    std::atomic<int64_t> health_check_count_{0};

    static void error_handling(const std::string &message);
};