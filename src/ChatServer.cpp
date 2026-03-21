// ChatServer.cpp - 完整的实现
#include "ChatServer.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <algorithm>
#include "command_parser.hpp"
// ==================== 构造函数/析构函数 ====================
ChatServer::ChatServer() : server_fd_(-1), epoll_fd_(-1)
{
    std::cout << "[INFO] ChatServer 实例创建" << std::endl;
}

ChatServer::~ChatServer()
{
    stop();
    if (epoll_fd_ != -1)
        close(epoll_fd_);
    if (server_fd_ != -1)
        close(server_fd_);
    std::cout << "[INFO] ChatServer 资源已完成释放" << std::endl;
}

// ==================== 工具函数 ====================
void ChatServer::error_handling(const std::string &message)
{
    std::cerr << "[ERROR] " << message << ": " << strerror(errno) << std::endl;
}

void ChatServer::set_non_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        error_handling("fcntl F_GETFL");
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        error_handling("fcntl F_SETFL");
    }
}

// ==================== 初始化阶段 ====================
bool ChatServer::initialize()
{
    std::cout << "[INFO] 初始化服务器···" << std::endl;

    // 1. 创建监听socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ == -1)
    {
        error_handling("创建socket失败");
        return false;
    }

    // 2. 设置socket选项
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        error_handling("设置socket选项失败");
        close(server_fd_);
        return false;
    }

    // 3. 绑定地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP.c_str());
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd_, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        error_handling("绑定失败");
        close(server_fd_);
        return false;
    }

    // 4. 监听
    if (listen(server_fd_, 128) < 0)
    {
        error_handling("监听失败");
        close(server_fd_);
        return false;
    }

    // 5. 创建epoll实例
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1)
    {
        error_handling("创建epoll失败");
        close(server_fd_);
        return false;
    }

    // 6. 将监听socket加入epoll
    struct epoll_event ev;
    ev.events = EPOLLIN; // 水平触发
    ev.data.fd = server_fd_;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &ev) == -1)
    {
        error_handling("epoll_ctl添加监听socket失败");
        close(server_fd_);
        close(epoll_fd_);
        return false;
    }

    // 7. 设置非阻塞
    set_non_blocking(server_fd_);

    std::cout << "[SUCCESS] 服务器初始化完成，监听端口: " << PORT << std::endl;
    return true;
}

// ==================== 核心事件循环 ====================
void ChatServer::start()
{
    if (server_fd_ == -1 || epoll_fd_ == -1)
    {
        std::cerr << "[ERROR] 服务器未初始化" << std::endl;
        return;
    }

    is_running_ = true;
    std::cout << "[INFO] 服务器启动，等待连接..." << std::endl;

    while (is_running_)
    {
        // 等待事件发生
        int ready_count = epoll_wait(epoll_fd_, events_, MAX_EVENTS, -1);

        if (ready_count == -1)
        {
            if (errno == EINTR)
                continue; // 被信号中断
            error_handling("epoll_wait失败");
            break;
        }

        // 处理所有就绪的事件
        handle_events(ready_count);
    }

    std::cout << "[INFO] 服务器停止运行" << std::endl;
}

// ==================== 事件处理分发 ====================
void ChatServer::handle_events(int ready_count)
{
    for (int i = 0; i < ready_count; ++i)
    {
        int fd = events_[i].data.fd;
        uint32_t events = events_[i].events;

        // 处理错误事件
        if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
        {
            std::cout << "[WARN] 客户端异常，fd: " << fd << std::endl;
            handle_client_disconnect(fd);
            continue;
        }

        // 处理新连接
        if (fd == server_fd_)
        {
            handle_new_connection();
        }
        // 处理客户端数据
        else if (events & EPOLLIN)
        {
            handle_client_message(fd);
        }
    }
}

// ==================== 处理新连接 ====================
void ChatServer::handle_new_connection()
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = accept(server_fd_, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd == -1)
    {
        error_handling("接受连接失败");
        return;
    }

    // 获取客户端信息
    std::string client_ip = inet_ntoa(client_addr.sin_addr);
    int client_port = ntohs(client_addr.sin_port);

    // 创建客户端信息对象
    auto client = std::make_shared<ClientInfo>(client_fd, client_ip, client_port);

    // 设置非阻塞
    set_non_blocking(client_fd);

    // 添加到epoll监听（边缘触发）
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP; // 边缘触发 + 监听连接断开
    ev.data.fd = client_fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev) == -1)
    {
        error_handling("epoll_ctl添加客户端失败");
        close(client_fd);
        return;
    }

    // 添加到客户端映射
    clients_[client_fd] = client;

    // 发送欢迎消息
    std::string welcome_msg = "[系统] 欢迎 " + client->nickname +
                              " 加入聊天室! 当前在线: " +
                              std::to_string(clients_.size()) + " 人\n";
    send_to_client(client_fd, welcome_msg);

    // 广播新用户加入
    std::string broadcast_msg = "[系统] " + client->nickname +
                                " 进入了聊天室 (" + client_ip + ":" +
                                std::to_string(client_port) + ")\n";
    broadcast_message(broadcast_msg, client_fd);

    std::cout << "[CONNECT] 新客户端连接: " << client_ip << ":"
              << client_port << " (fd: " << client_fd << ")" << std::endl;
}

// ==================== 处理客户端消息 ====================
void ChatServer::handle_client_message(int client_fd)
{
    char buffer[BUFFER_SIZE];

    // 边缘触发模式下需要循环读取
    while (true)
    {
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

        if (bytes_read > 0)
        {
            buffer[bytes_read] = '\0'; // 确保字符串终止
            process_message(client_fd, buffer, bytes_read);
        }
        else if (bytes_read == 0)
        {
            // 客户端正常关闭连接
            std::cout << "[INFO] 客户端主动关闭连接, fd: " << client_fd << std::endl;
            handle_client_disconnect(client_fd);
            break;
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 数据读取完毕（非阻塞IO的特征）
                break;
            }
            else
            {
                // 读取错误
                error_handling("读取客户端数据失败");
                handle_client_disconnect(client_fd);
                break;
            }
        }
    }
}

// ==================== 处理消息内容 ====================
void ChatServer::process_message(int client_fd, const char *buffer, ssize_t len)
{
    auto it = clients_.find(client_fd);
    if (it == clients_.end())
    {
        std::cerr << "[WARN] 未知的客户端fd: " << client_fd << std::endl;
        return;
    }

    // 1. 构造完整消息字符串（去除末尾的空字符/乱码）
    std::string message(buffer, len);
    // 清理消息末尾的换行/回车/空格（统一预处理）
    message.erase(std::remove(message.begin(), message.end(), '\n'), message.end());
    message.erase(std::remove(message.begin(), message.end(), '\r'), message.end());

    // 2. 复用 command_parser 模块解析命令（统一逻辑，避免手写出错）
    CommandParser::ParsedCommand cmd = CommandParser::parse_message(message);

    // 3. 根据解析后的命令类型处理
    if (cmd.type == CommandParser::CommandType::CHANGE_NAME)
    {
        // 修改昵称命令（使用解析后的干净参数）
        std::string new_name = cmd.argument;
        if (!new_name.empty())
        {
            std::string old_name = it->second->nickname;
            it->second->nickname = new_name;

            std::string sys_msg = "[系统] " + old_name + " 改名为 " + new_name + "\n";
            broadcast_message(sys_msg);
        }
        else
        {
            // 提示用户昵称不能为空
            send_to_client(client_fd, "[系统] 昵称不能为空，请重新输入！\n");
        }
    }
    else if (cmd.type == CommandParser::CommandType::ONLINE)
    {
        // 查看在线用户
        std::string online_list = "[在线用户列表]\n";
        for (const auto &client : clients_)
        {
            online_list += "  " + client.second->nickname + " (" +
                           client.second->ip_address + ")\n";
        }
        send_to_client(client_fd, online_list);
    }
    else if (cmd.type == CommandParser::CommandType::QUIT)
    {
        // 客户端退出
        std::string leave_msg = "[系统] " + it->second->nickname + " 离开了聊天室\n";
        broadcast_message(leave_msg, client_fd);
        handle_client_disconnect(client_fd);
    }
    else
    {
        // 普通聊天消息（保留原始消息的换行，保证显示正常）
        std::string formatted_msg = "[" + it->second->nickname + "] " + message + "\n";

        // 输出到服务器控制台（便于调试）
        std::cout << "[CHAT] " << formatted_msg;

        // 广播给其他所有人
        broadcast_message(formatted_msg, client_fd);
    }
}

// ==================== 处理客户端断开 ====================
void ChatServer::handle_client_disconnect(int client_fd)
{
    auto it = clients_.find(client_fd);
    if (it != clients_.end())
    {
        std::string leave_msg = "[系统] " + it->second->nickname + " 离开了聊天室\n";

        // 从epoll中移除
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);

        // 关闭socket
        close(client_fd);

        // 从映射中移除
        clients_.erase(it);

        // 广播离开消息
        broadcast_message(leave_msg);

        std::cout << "[DISCONNECT] 客户端断开, fd: " << client_fd
                  << "，剩余在线: " << clients_.size() << " 人" << std::endl;
    }
}

// ==================== 消息发送函数 ====================
void ChatServer::send_to_client(int client_fd, const std::string &message)
{
    ssize_t bytes_sent = write(client_fd, message.c_str(), message.length());
    if (bytes_sent < 0)
    {
        error_handling("发送消息失败");
        handle_client_disconnect(client_fd);
    }
}

void ChatServer::broadcast_message(const std::string &message, int exclude_fd)
{
    for (const auto &pair : clients_)
    {
        if (pair.first != exclude_fd)
        {
            send_to_client(pair.first, message);
        }
    }
}

// ==================== 服务器控制 ====================
void ChatServer::stop()
{
    if (!is_running_)
    {
        return; // 已经停止
    }

    is_running_ = false;
    std::cout << "[INFO] 正在停止服务器..." << std::endl;

    // 向wakeup_fd写入，唤醒epoll_wait
    if (wakeup_fd_ != -1)
    {
        uint64_t val = 1;
        write(wakeup_fd_, &val, sizeof(val));
    }

    // 关闭所有客户端连接
    for (const auto &pair : clients_)
    {
        close(pair.first);
    }
    clients_.clear();
}

std::string ChatServer::get_server_info() const
{
    std::string info = "=== 服务器状态 ===\n";
    info += "监听端口: " + std::to_string(PORT) + "\n";
    info += "在线用户: " + std::to_string(clients_.size()) + "\n";
    info += "运行状态: " + std::string(is_running_ ? "运行中" : "已停止") + "\n";
    return info;
}