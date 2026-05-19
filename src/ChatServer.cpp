#include "ChatServer.hpp"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <thread>
#include <sstream>
#include <cmath>
#include "command_parser.hpp"

ChatServer::ChatServer() : server_fd_(platform::INVALID_SOCK)
{
    std::cout << "[INFO] ChatServer instance created" << std::endl;
}

ChatServer::~ChatServer()
{
    stop();
    if (server_fd_ != platform::INVALID_SOCK)
        platform::close_socket(server_fd_);
    std::cout << "[INFO] ChatServer resources released" << std::endl;
}

void ChatServer::error_handling(const std::string &message)
{
    int err = platform::get_last_socket_error();
    std::cerr << "[ERROR] " << message << ": "
              << platform::get_socket_error_string(err) << " (code=" << err << ")" << std::endl;
}

bool ChatServer::initialize()
{
    std::cout << "[INFO] Initializing server..." << std::endl;

    if (!platform::network_startup()) {
        std::cerr << "[ERROR] Network startup failed" << std::endl;
        return false;
    }

    server_fd_ = platform::create_tcp_socket();
    if (server_fd_ == platform::INVALID_SOCK)
    {
        error_handling("create socket");
        return false;
    }

    if (!platform::set_reuse_addr(server_fd_))
    {
        error_handling("set socket option");
        platform::close_socket(server_fd_);
        return false;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP.c_str());
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd_, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        error_handling("bind");
        platform::close_socket(server_fd_);
        return false;
    }

    if (listen(server_fd_, SOMAXCONN) < 0)
    {
        error_handling("listen");
        platform::close_socket(server_fd_);
        return false;
    }

    if (!poller_.init())
    {
        std::cerr << "[ERROR] poller init failed" << std::endl;
        platform::close_socket(server_fd_);
        return false;
    }

    if (!poller_.add_fd(server_fd_, platform::POLL_IN, true))
    {
        std::cerr << "[ERROR] add listen fd to poller failed" << std::endl;
        platform::close_socket(server_fd_);
        return false;
    }

    platform::set_non_blocking(server_fd_);

    std::cout << "[SUCCESS] Server initialized, listening on port: " << PORT << std::endl;
    return true;
}

void ChatServer::start()
{
    if (server_fd_ == platform::INVALID_SOCK)
    {
        std::cerr << "[ERROR] Server not initialized" << std::endl;
        return;
    }

    is_running_ = true;
    std::cout << "[INFO] Server started, waiting for connections..." << std::endl;

    std::thread health_thread(&ChatServer::health_monitor_loop, this);

    while (is_running_)
    {
        int ready_count = poller_.wait(events_, MAX_EVENTS, HEARTBEAT_INTERVAL_SEC * 1000);

        if (!is_running_) break;

        if (ready_count == -1)
        {
            error_handling("poller wait");
            break;
        }

        heartbeat_check();

        if (ready_count > 0)
        {
            handle_events(ready_count);
        }
    }

    if (health_thread.joinable())
        health_thread.join();

    std::cout << "[INFO] Server stopped" << std::endl;
}

void ChatServer::handle_events(int ready_count)
{
    for (int i = 0; i < ready_count; ++i)
    {
        platform::socket_t fd = events_[i].fd;
        uint32_t ev = events_[i].events;

        if (ev & (platform::POLL_ERR | platform::POLL_HUP | platform::POLL_RDHUP))
        {
            std::cout << "[WARN] Client error, fd: " << fd << std::endl;
            handle_client_disconnect(fd);
            continue;
        }

        if (events_[i].is_listen_fd)
        {
            handle_new_connection();
        }
        else if (ev & platform::POLL_IN)
        {
            if (clients_.find(fd) != clients_.end()) {
                handle_client_message(fd);
            } else {
                poller_.del_fd(fd);
            }
        }
    }
}

void ChatServer::handle_new_connection()
{
    while (true)
    {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        platform::socket_t client_fd = accept(server_fd_, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd == platform::INVALID_SOCK)
        {
            int err = platform::get_last_socket_error();
#if PLATFORM_WINDOWS
            if (err == WSAEWOULDBLOCK) break;
#else
            if (err == EAGAIN || err == EWOULDBLOCK) break;
#endif
            error_handling("accept");
            break;
        }

        if (static_cast<int>(clients_.size()) >= MAX_CLIENTS)
        {
            std::string reject_msg = "[System] Server is full, please try again later\n";
            send(client_fd, reject_msg.c_str(), reject_msg.size(), 0);
            platform::close_socket(client_fd);
            std::cout << "[WARN] Connection rejected: server full" << std::endl;
            continue;
        }

        std::string client_ip = platform::sockaddr_to_string(client_addr);
        int client_port = platform::sockaddr_to_port(client_addr);

        auto client = std::make_shared<ClientInfo>(client_fd, client_ip, client_port);
        int64_t now_ms = platform::current_time_ms();
        client->last_activity_ms = now_ms;
        client->last_heartbeat_ms = now_ms;

        platform::set_non_blocking(client_fd);

        if (!poller_.add_fd(client_fd, platform::POLL_IN))
        {
            error_handling("add client to poller");
            platform::close_socket(client_fd);
            continue;
        }

        clients_[client_fd] = client;
        total_connections_++;

        std::string welcome_msg = "[System] Welcome " + client->nickname +
                                  " to the chatroom! Online: " +
                                  std::to_string(clients_.size()) + " users\n";
        send_to_client(client_fd, welcome_msg);

        std::string broadcast_msg = "[System] " + client->nickname +
                                    " joined the chatroom (" + client_ip + ":" +
                                    std::to_string(client_port) + ")\n";
        broadcast_message(broadcast_msg, client_fd);

        std::cout << "[CONNECT] New client: " << client_ip << ":"
                  << client_port << " (fd: " << client_fd << ")" << std::endl;
    }
}

void ChatServer::handle_client_message(platform::socket_t client_fd)
{
    char buffer[BUFFER_SIZE];

    while (true)
    {
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_read > 0)
        {
            buffer[bytes_read] = '\0';

            auto it = clients_.find(client_fd);
            if (it != clients_.end()) {
                it->second->last_activity_ms = platform::current_time_ms();
            }

            process_message(client_fd, buffer, bytes_read);
        }
        else if (bytes_read == 0)
        {
            std::cout << "[INFO] Client closed connection, fd: " << client_fd << std::endl;
            handle_client_disconnect(client_fd);
            break;
        }
        else
        {
            int err = platform::get_last_socket_error();
#if PLATFORM_WINDOWS
            if (err == WSAEWOULDBLOCK) break;
            if (err == WSAENOTCONN) {
                std::cout << "[INFO] Client not connected, fd: " << client_fd << std::endl;
                handle_client_disconnect(client_fd);
                break;
            }
#else
            if (err == EAGAIN || err == EWOULDBLOCK) break;
            if (err == ENOTCONN) {
                std::cout << "[INFO] Client not connected, fd: " << client_fd << std::endl;
                handle_client_disconnect(client_fd);
                break;
            }
#endif
            error_handling("read client data");
            handle_client_disconnect(client_fd);
            break;
        }
    }
}

void ChatServer::process_message(platform::socket_t client_fd, const char *buffer, ssize_t len)
{
    std::string raw(buffer, len);

    std::istringstream stream(raw);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());

        auto it = clients_.find(client_fd);
        if (it == clients_.end()) return;
        process_single_message(client_fd, it, line);
    }
}

void ChatServer::process_single_message(platform::socket_t client_fd,
    std::unordered_map<platform::socket_t, ClientPtr>::iterator it,
    const std::string &message)
{
    if (message == "PING")
    {
        it->second->last_heartbeat_ms = platform::current_time_ms();
        it->second->missed_heartbeats = 0;
        send_to_client(client_fd, "PONG\n");
        return;
    }

    CommandParser::ParsedCommand cmd = CommandParser::parse_message(message);

    if (cmd.type == CommandParser::CommandType::CHANGE_NAME)
    {
        std::string new_name = cmd.argument;
        if (!new_name.empty())
        {
            std::string old_name = it->second->nickname;
            it->second->nickname = new_name;

            std::string sys_msg = "[System] " + old_name + " renamed to " + new_name + "\n";
            broadcast_message(sys_msg);
        }
        else
        {
            send_to_client(client_fd, "[System] Nickname cannot be empty!\n");
        }
    }
    else if (cmd.type == CommandParser::CommandType::ONLINE)
    {
        std::string online_list = "[Online Users]\n";
        online_list += "Online: " + std::to_string(clients_.size()) + " users\n";
        online_list += "------------------------\n";
        for (const auto &c : clients_)
        {
            online_list += "  " + c.second->nickname + " (" +
                           c.second->ip_address + ")\n";
        }
        send_to_client(client_fd, online_list);
    }
    else if (cmd.type == CommandParser::CommandType::QUIT)
    {
        std::string leave_msg = "[System] " + it->second->nickname + " left the chatroom\n";
        broadcast_message(leave_msg, client_fd);
        handle_client_disconnect(client_fd);
    }
    else
    {
        std::string formatted_msg = "[" + it->second->nickname + "] " + message + "\n";
        std::cout << "[CHAT] " << formatted_msg;
        broadcast_message(formatted_msg, client_fd);
    }

    total_messages_++;
}

void ChatServer::handle_client_disconnect(platform::socket_t client_fd)
{
    auto it = clients_.find(client_fd);
    if (it != clients_.end())
    {
        std::string leave_msg = "[System] " + it->second->nickname + " left the chatroom\n";

        poller_.del_fd(client_fd);
        platform::close_socket(client_fd);
        clients_.erase(it);

        broadcast_message(leave_msg);

        std::cout << "[DISCONNECT] Client disconnected, fd: " << client_fd
                  << ", remaining online: " << clients_.size() << std::endl;
    }
    else
    {
        poller_.del_fd(client_fd);
        platform::close_socket(client_fd);
    }
}

void ChatServer::send_to_client(platform::socket_t client_fd, const std::string &message)
{
    ssize_t bytes_sent = send(client_fd, message.c_str(), message.length(), 0);
    if (bytes_sent < 0)
    {
        int err = platform::get_last_socket_error();
#if PLATFORM_WINDOWS
        if (err != WSAEWOULDBLOCK)
#else
        if (err != EAGAIN && err != EWOULDBLOCK)
#endif
        {
            error_handling("send message");
            handle_client_disconnect(client_fd);
        }
    }
}

void ChatServer::broadcast_message(const std::string &message, platform::socket_t exclude_fd)
{
    std::vector<platform::socket_t> fds;
    fds.reserve(clients_.size());
    for (const auto &pair : clients_) {
        if (pair.first != exclude_fd)
            fds.push_back(pair.first);
    }
    for (auto fd : fds) {
        if (clients_.find(fd) != clients_.end())
            send_to_client(fd, message);
    }
}

// ==================== 心跳检测 ====================
void ChatServer::heartbeat_check()
{
    int64_t now_ms = platform::current_time_ms();
    int64_t timeout_ms = static_cast<int64_t>(HEARTBEAT_TIMEOUT_SEC) * 1000;
    std::vector<platform::socket_t> to_disconnect;

    for (auto &pair : clients_)
    {
        int64_t elapsed = now_ms - pair.second->last_activity_ms;

        if (elapsed > timeout_ms)
        {
            pair.second->missed_heartbeats++;
            heartbeat_failures_++;
            std::cout << "[HEARTBEAT] Client " << pair.second->nickname
                      << " (fd=" << pair.first << ") heartbeat timeout: "
                      << elapsed << "ms, missed: " << pair.second->missed_heartbeats
                      << std::endl;
            to_disconnect.push_back(pair.first);
        }
    }

    for (auto fd : to_disconnect)
    {
        std::string timeout_msg = "[System] " + clients_[fd]->nickname +
                                  " timed out (heartbeat)\n";
        broadcast_message(timeout_msg, fd);
        handle_client_disconnect(fd);
    }
}

// ==================== 健康监测循环 ====================
void ChatServer::health_monitor_loop()
{
    while (is_running_)
    {
        platform::sleep_ms(HEARTBEAT_INTERVAL_SEC * 1000);

        if (!is_running_) break;

        health_check_count_++;
        int client_count = static_cast<int>(clients_.size());
        int64_t messages = total_messages_.load();
        int64_t failures = heartbeat_failures_.load();
        int64_t checks = health_check_count_.load();

        std::ostringstream health_log;
        health_log << "[HEALTH #" << checks << "] "
                   << "clients=" << client_count << "/" << MAX_CLIENTS
                   << " msgs=" << messages
                   << " hb_fail=" << failures
                   << " mem_clients=" << (client_count > 0 ? "OK" : "IDLE");
        std::cout << health_log.str() << std::endl;

        if (failures > 0 && checks % 12 == 0)
        {
            std::cerr << "[ALERT] Heartbeat failures detected: "
                      << failures << " total" << std::endl;
        }
    }
}

void ChatServer::stop()
{
    if (!is_running_) return;

    is_running_ = false;
    std::cout << "[INFO] Stopping server..." << std::endl;

    poller_.wakeup();

    for (const auto &pair : clients_)
    {
        platform::close_socket(pair.first);
    }
    clients_.clear();
}

std::string ChatServer::get_server_info() const
{
    std::string info = "=== Server Status ===\n";
    info += "Listening port: " + std::to_string(PORT) + "\n";
    info += "Online users: " + std::to_string(clients_.size()) + "\n";
    info += "Max clients: " + std::to_string(MAX_CLIENTS) + "\n";
    info += "Status: " + std::string(is_running_ ? "Running" : "Stopped") + "\n";
    info += "Total messages: " + std::to_string(total_messages_.load()) + "\n";
    info += "Total connections: " + std::to_string(total_connections_.load()) + "\n";
    info += "Heartbeat failures: " + std::to_string(heartbeat_failures_.load()) + "\n";
    info += "Health checks: " + std::to_string(health_check_count_.load()) + "\n";
    return info;
}

std::string ChatServer::get_health_report() const
{
    std::ostringstream report;
    report << "{\n";
    report << "  \"status\": \"" << (is_running_ ? "running" : "stopped") << "\",\n";
    report << "  \"clients\": " << clients_.size() << ",\n";
    report << "  \"max_clients\": " << MAX_CLIENTS << ",\n";
    report << "  \"total_messages\": " << total_messages_.load() << ",\n";
    report << "  \"total_connections\": " << total_connections_.load() << ",\n";
    report << "  \"heartbeat_failures\": " << heartbeat_failures_.load() << ",\n";
    report << "  \"health_checks\": " << health_check_count_.load() << "\n";
    report << "}";
    return report.str();
}