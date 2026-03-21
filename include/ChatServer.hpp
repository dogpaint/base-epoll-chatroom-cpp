#pragma once
#include "ClientInfo.hpp"
#include "common.hpp"
#include <sys/epoll.h>
#include <sys/eventfd.h> // 线程阻塞在 epoll_wait，添加唤醒机制
#include <unordered_map>
#include <functional>
#include <atomic>

class ChatServer
{
public:
    ChatServer();
    ~ChatServer();

    bool initialize();
    void start();
    void stop();

    int get_client_count() const { return clients_.size(); }
    std::string get_server_info() const;

private:
    // 核心方法
    void handle_events(int ready_count);
    void handle_new_connection();
    void handle_client_message(int client_fd);
    void handle_client_disconnect(int client_fd);

    // 消息处理
    void broadcast_message(const std::string &message, int exclude_fd = -1);
    void send_to_client(int client_fd, const std::string &message);
    void process_message(int client_fd, const char *buffer, ssize_t len);

    // 成员变量
    int server_fd_;
    int epoll_fd_;
    int wakeup_fd_; // 新增：用于唤醒的eventfd
    struct epoll_event events_[MAX_EVENTS];

    std::unordered_map<int, ClientPtr> clients_;
    std::atomic<bool> is_running_{false};

    // 工具函数
    static void set_non_blocking(int fd);
    static void error_handling(const std::string &message);
};