#include "platform.hpp"
#include <sstream>

namespace platform {

// ==================== 初始化/清理 ====================
bool network_startup() {
#if PLATFORM_WINDOWS
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        std::cerr << "[ERROR] WSAStartup failed: " << result << std::endl;
        return false;
    }
#endif
    return true;
}

void network_cleanup() {
#if PLATFORM_WINDOWS
    WSACleanup();
#endif
}

// ==================== Socket 操作 ====================
socket_t create_tcp_socket() {
    return socket(AF_INET, SOCK_STREAM, 0);
}

int close_socket(socket_t fd) {
    if (fd == INVALID_SOCK) return 0;
#if PLATFORM_WINDOWS
    return closesocket(fd);
#else
    return close(fd);
#endif
}

bool set_non_blocking(socket_t fd) {
#if PLATFORM_WINDOWS
    u_long mode = 1;
    if (ioctlsocket(fd, FIONBIO, &mode) != 0) {
        std::cerr << "[ERROR] ioctlsocket FIONBIO: " << get_last_socket_error() << std::endl;
        return false;
    }
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        std::cerr << "[ERROR] fcntl F_GETFL: " << strerror(errno) << std::endl;
        return false;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "[ERROR] fcntl F_SETFL: " << strerror(errno) << std::endl;
        return false;
    }
#endif
    return true;
}

bool set_reuse_addr(socket_t fd) {
    int opt = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
#if PLATFORM_WINDOWS
                      (const char*)&opt,
#else
                      &opt,
#endif
                      sizeof(opt)) == 0;
}

int get_last_socket_error() {
#if PLATFORM_WINDOWS
    return WSAGetLastError();
#else
    return errno;
#endif
}

std::string get_socket_error_string(int err) {
#if PLATFORM_WINDOWS
    char buf[256] = {0};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   buf, sizeof(buf), nullptr);
    return std::string(buf);
#else
    return std::string(strerror(err));
#endif
}

// ==================== 地址转换(线程安全) ====================
std::string sockaddr_to_string(const struct sockaddr_in& addr) {
    char buf[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET,
#if PLATFORM_WINDOWS
              (PVOID)&addr.sin_addr,
#else
              &addr.sin_addr,
#endif
              buf, sizeof(buf));
    return std::string(buf);
}

int sockaddr_to_port(const struct sockaddr_in& addr) {
    return ntohs(addr.sin_port);
}

// ==================== Poller ====================
Poller::Poller()
#if PLATFORM_LINUX
    : epoll_fd_(-1), wakeup_fd_(-1)
#elif PLATFORM_MACOS
    : kq_fd_(-1)
#endif
{
#if PLATFORM_MACOS
    wakeup_pipe_[0] = -1;
    wakeup_pipe_[1] = -1;
#endif
}

Poller::~Poller() {
#if PLATFORM_LINUX
    if (wakeup_fd_ != -1) close(wakeup_fd_);
    if (epoll_fd_ != -1) close(epoll_fd_);
#elif PLATFORM_MACOS
    if (wakeup_pipe_[0] != -1) close(wakeup_pipe_[0]);
    if (wakeup_pipe_[1] != -1) close(wakeup_pipe_[1]);
    if (kq_fd_ != -1) close(kq_fd_);
#elif PLATFORM_WINDOWS
    if (wakeup_sender_ != INVALID_SOCK) closesocket(wakeup_sender_);
    if (wakeup_receiver_ != INVALID_SOCK) closesocket(wakeup_receiver_);
#endif
}

bool Poller::init() {
#if PLATFORM_LINUX
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        std::cerr << "[ERROR] epoll_create1" << std::endl;
        return false;
    }
    wakeup_fd_ = eventfd(0, EFD_NONBLOCK);
    if (wakeup_fd_ == -1) {
        std::cerr << "[ERROR] eventfd" << std::endl;
        close(epoll_fd_);
        epoll_fd_ = -1;
        return false;
    }
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = wakeup_fd_;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wakeup_fd_, &ev) == -1) {
        std::cerr << "[ERROR] epoll_ctl wakeup" << std::endl;
        close(wakeup_fd_); wakeup_fd_ = -1;
        close(epoll_fd_); epoll_fd_ = -1;
        return false;
    }
#elif PLATFORM_MACOS
    kq_fd_ = kqueue();
    if (kq_fd_ == -1) {
        std::cerr << "[ERROR] kqueue" << std::endl;
        return false;
    }
    if (pipe(wakeup_pipe_) == -1) {
        std::cerr << "[ERROR] pipe" << std::endl;
        close(kq_fd_); kq_fd_ = -1;
        return false;
    }
    fcntl(wakeup_pipe_[0], F_SETFL, O_NONBLOCK);
    fcntl(wakeup_pipe_[1], F_SETFL, O_NONBLOCK);
    struct kevent kev;
    EV_SET(&kev, wakeup_pipe_[0], EVFILT_READ, EV_ADD, 0, 0, nullptr);
    if (kevent(kq_fd_, &kev, 1, nullptr, 0, nullptr) == -1) {
        std::cerr << "[ERROR] kevent wakeup" << std::endl;
        close(wakeup_pipe_[0]); wakeup_pipe_[0] = -1;
        close(wakeup_pipe_[1]); wakeup_pipe_[1] = -1;
        close(kq_fd_); kq_fd_ = -1;
        return false;
    }
#elif PLATFORM_WINDOWS
    SOCKET listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == INVALID_SOCKET) return false;
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = 0;
    if (bind(listener, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(listener);
        return false;
    }
    int addr_len = sizeof(addr);
    getsockname(listener, (sockaddr*)&addr, &addr_len);
    if (listen(listener, 1) == SOCKET_ERROR) {
        closesocket(listener);
        return false;
    }
    wakeup_receiver_ = socket(AF_INET, SOCK_STREAM, 0);
    if (wakeup_receiver_ == INVALID_SOCKET) {
        closesocket(listener);
        return false;
    }
    u_long mode = 1;
    ioctlsocket(wakeup_receiver_, FIONBIO, &mode);
    if (connect(wakeup_receiver_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            closesocket(wakeup_receiver_);
            closesocket(listener);
            return false;
        }
    }
    wakeup_sender_ = accept(listener, nullptr, nullptr);
    closesocket(listener);
    if (wakeup_sender_ == INVALID_SOCKET) {
        closesocket(wakeup_receiver_);
        return false;
    }
    ioctlsocket(wakeup_sender_, FIONBIO, &mode);
    pollfd pfd;
    pfd.fd = wakeup_receiver_;
    pfd.events = POLLIN;
    poll_fds_.push_back(pfd);
    is_listen_flags_.push_back(false);
#endif

    initialized_ = true;
    return true;
}

bool Poller::add_fd(socket_t fd, uint32_t events, bool is_listen) {
#if PLATFORM_LINUX
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
        std::cerr << "[ERROR] epoll_ctl ADD fd=" << fd << std::endl;
        return false;
    }
    listen_map_[fd] = is_listen;
#elif PLATFORM_MACOS
    struct kevent kev;
    EV_SET(&kev, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    if (kevent(kq_fd_, &kev, 1, nullptr, 0, nullptr) == -1) {
        std::cerr << "[ERROR] kevent ADD fd=" << fd << std::endl;
        return false;
    }
    listen_map_[fd] = is_listen;
#elif PLATFORM_WINDOWS
    pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    poll_fds_.push_back(pfd);
    is_listen_flags_.push_back(is_listen);
#endif
    return true;
}

bool Poller::mod_fd(socket_t fd, uint32_t events) {
#if PLATFORM_WINDOWS
    for (size_t i = 0; i < poll_fds_.size(); ++i) {
        if (poll_fds_[i].fd == fd) {
            poll_fds_[i].events = POLLIN;
            if (events & POLL_OUT) poll_fds_[i].events |= POLLOUT;
            return true;
        }
    }
#endif
    return true;
}

bool Poller::del_fd(socket_t fd) {
#if PLATFORM_LINUX
    listen_map_.erase(fd);
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        // ENOENT can happen if fd was already removed, ignore
        if (errno != ENOENT) {
            std::cerr << "[ERROR] epoll_ctl DEL fd=" << fd << ": " << strerror(errno) << std::endl;
            return false;
        }
    }
#elif PLATFORM_MACOS
    listen_map_.erase(fd);
    struct kevent kev;
    EV_SET(&kev, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    if (kevent(kq_fd_, &kev, 1, nullptr, 0, nullptr) == -1) {
        if (errno != ENOENT) {
            std::cerr << "[ERROR] kevent DEL fd=" << fd << ": " << strerror(errno) << std::endl;
        }
    }
#elif PLATFORM_WINDOWS
    for (size_t i = 0; i < poll_fds_.size(); ++i) {
        if (poll_fds_[i].fd == fd) {
            poll_fds_.erase(poll_fds_.begin() + i);
            is_listen_flags_.erase(is_listen_flags_.begin() + i);
            return true;
        }
    }
#endif
    return true;
}

int Poller::wait(PollEntry* entries, int max_events, int timeout_ms) {
#if PLATFORM_LINUX
    struct epoll_event evs[max_events > 0 ? max_events : 1024];
    int nfds = epoll_wait(epoll_fd_, evs, max_events, timeout_ms);
    if (nfds == -1) {
        if (errno == EINTR) return 0;
        return -1;
    }
    int count = 0;
    for (int i = 0; i < nfds && count < max_events; ++i) {
        if (evs[i].data.fd == wakeup_fd_) {
            uint64_t val;
            read(wakeup_fd_, &val, sizeof(val));
            continue;
        }
        entries[count].fd = evs[i].data.fd;
        entries[count].events = 0;
        entries[count].is_listen_fd = listen_map_[evs[i].data.fd];
        if (evs[i].events & EPOLLIN)  entries[count].events |= POLL_IN;
        if (evs[i].events & EPOLLERR) entries[count].events |= POLL_ERR;
        if (evs[i].events & EPOLLHUP) entries[count].events |= POLL_HUP;
        if (evs[i].events & EPOLLRDHUP) entries[count].events |= POLL_RDHUP;
        ++count;
    }
    return count;
#elif PLATFORM_MACOS
    struct kevent kevs[max_events > 0 ? max_events : 1024];
    struct timespec ts;
    struct timespec* pts = nullptr;
    if (timeout_ms >= 0) {
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000;
        pts = &ts;
    }
    int nfds = kevent(kq_fd_, nullptr, 0, kevs, max_events, pts);
    if (nfds == -1) {
        if (errno == EINTR) return 0;
        return -1;
    }
    int count = 0;
    for (int i = 0; i < nfds && count < max_events; ++i) {
        if ((int)kevs[i].ident == (unsigned int)wakeup_pipe_[0]) {
            char buf[64];
            while (read(wakeup_pipe_[0], buf, sizeof(buf)) > 0) {}
            continue;
        }
        entries[count].fd = kevs[i].ident;
        entries[count].events = 0;
        entries[count].is_listen_fd = listen_map_[static_cast<int>(kevs[i].ident)];
        if (kevs[i].filter == EVFILT_READ) entries[count].events |= POLL_IN;
        if (kevs[i].flags & EV_ERROR)       entries[count].events |= POLL_ERR;
        if (kevs[i].flags & EV_EOF)         entries[count].events |= POLL_HUP;
        ++count;
    }
    return count;
#elif PLATFORM_WINDOWS
    int result = WSAPoll(poll_fds_.data(), poll_fds_.size(), timeout_ms);
    if (result < 0) return -1;
    int count = 0;
    for (size_t i = 0; i < poll_fds_.size() && count < max_events; ++i) {
        if (poll_fds_[i].revents == 0) continue;
        if (poll_fds_[i].fd == wakeup_receiver_) {
            char buf[64];
            while (recv(wakeup_receiver_, buf, sizeof(buf), 0) > 0) {}
            poll_fds_[i].revents = 0;
            continue;
        }
        entries[count].fd = poll_fds_[i].fd;
        entries[count].events = 0;
        entries[count].is_listen_fd = is_listen_flags_[i];
        if (poll_fds_[i].revents & POLLIN)   entries[count].events |= POLL_IN;
        if (poll_fds_[i].revents & POLLERR)  entries[count].events |= POLL_ERR;
        if (poll_fds_[i].revents & POLLHUP)  entries[count].events |= POLL_HUP;
        poll_fds_[i].revents = 0;
        ++count;
    }
    return count;
#endif
}

void Poller::wakeup() {
#if PLATFORM_LINUX
    uint64_t val = 1;
    write(wakeup_fd_, &val, sizeof(val));
#elif PLATFORM_MACOS
    char c = 'x';
    write(wakeup_pipe_[1], &c, 1);
#elif PLATFORM_WINDOWS
    char c = 'x';
    send(wakeup_sender_, &c, 1, 0);
#endif
}

int Poller::native_handle() const {
#if PLATFORM_LINUX
    return epoll_fd_;
#elif PLATFORM_MACOS
    return kq_fd_;
#elif PLATFORM_WINDOWS
    return (int)wakeup_receiver_;
#endif
}

// ==================== 休眠 ====================
void sleep_ms(int milliseconds) {
#if PLATFORM_WINDOWS
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

// ==================== 时间戳 ====================
int64_t current_time_ms() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

} // namespace platform