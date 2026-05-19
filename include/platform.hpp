#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS 1
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
#elif defined(__APPLE__) || defined(__MACH__)
    #define PLATFORM_MACOS 1
    #define PLATFORM_POSIX 1
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <sys/select.h>
    #include <sys/event.h>
    #include <sys/time.h>
    #include <errno.h>
#elif defined(__linux__)
    #define PLATFORM_LINUX 1
    #define PLATFORM_POSIX 1
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <sys/epoll.h>
    #include <sys/eventfd.h>
    #include <errno.h>
#else
    #error "Unsupported platform"
#endif

#include <string>
#include <cstring>
#include <vector>
#include <functional>
#include <atomic>
#include <chrono>
#include <iostream>
#include <unordered_map>

namespace platform {

// ==================== Socket 类型 ====================
#if PLATFORM_WINDOWS
    using socket_t = SOCKET;
    const socket_t INVALID_SOCK = INVALID_SOCKET;
#else
    using socket_t = int;
    const socket_t INVALID_SOCK = -1;
#endif

// ==================== 初始化/清理 ====================
bool network_startup();
void network_cleanup();

// ==================== Socket 操作 ====================
socket_t create_tcp_socket();
int      close_socket(socket_t fd);
bool     set_non_blocking(socket_t fd);
bool     set_reuse_addr(socket_t fd);
int      get_last_socket_error();
std::string get_socket_error_string(int err);

// ==================== 地址转换 (线程安全) ====================
std::string sockaddr_to_string(const struct sockaddr_in& addr);
int         sockaddr_to_port(const struct sockaddr_in& addr);

// ==================== Poller 抽象 ====================
enum PollEvent : uint32_t {
    POLL_IN    = 0x01,
    POLL_OUT   = 0x02,
    POLL_ERR   = 0x04,
    POLL_HUP   = 0x08,
    POLL_RDHUP = 0x10
};

struct PollEntry {
    socket_t  fd;
    uint32_t  events;
    bool      is_listen_fd;
};

class Poller {
public:
    Poller();
    ~Poller();

    bool init();
    bool add_fd(socket_t fd, uint32_t events, bool is_listen = false);
    bool mod_fd(socket_t fd, uint32_t events);
    bool del_fd(socket_t fd);
    int  wait(PollEntry* entries, int max_events, int timeout_ms);
    void wakeup();

    int  native_handle() const;

private:
#if PLATFORM_LINUX
    int epoll_fd_;
    int wakeup_fd_;
    std::unordered_map<int, bool> listen_map_;
#elif PLATFORM_MACOS
    int kq_fd_;
    int wakeup_pipe_[2];
    std::unordered_map<int, bool> listen_map_;
#elif PLATFORM_WINDOWS
    socket_t wakeup_sender_;
    socket_t wakeup_receiver_;
    std::vector<pollfd> poll_fds_;
    std::vector<bool>   is_listen_flags_;
#endif

    std::atomic<bool> initialized_{false};
};

// ==================== 休眠 ====================
void sleep_ms(int milliseconds);

// ==================== 时间戳 ====================
int64_t current_time_ms();

} // namespace platform