#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <thread>
#include <atomic>
#include <string>

const int PORT = 8088; // 确保和服务器端口一致
const int BUFFER_SIZE = 4096;
std::atomic<bool> running{true};
std::atomic<bool> waiting_for_input{true};
std::string my_nickname = "Guest";

// 设置非阻塞
void set_non_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 判断是否为命令
bool is_command(const std::string &input)
{
    return !input.empty() && input[0] == '/';
}

// 处理本地回显
void handle_local_echo(const std::string &input)
{
    if (input.empty())
        return;

    // 不显示命令（除了/quit）
    if (is_command(input) && input != "/quit")
    {
        return;
    }

    // 显示自己发送的消息
    std::cout << "[我] " << input << std::endl;
}

// 接收消息线程
void receive_thread(int sockfd)
{
    char buffer[BUFFER_SIZE];

    while (running)
    {
        ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received > 0)
        {
            buffer[bytes_received] = '\0';

            // 处理系统消息（如改名通知）
            std::string message(buffer, bytes_received);

            // 如果收到改名通知，更新本地昵称
            if (message.find("改名为") != std::string::npos)
            {
                // 格式："[系统] 旧昵称 改名为 新昵称\n"
                size_t start = message.find("改名为") + 9; // 中文字符长度
                size_t end = message.find("\n", start);
                if (start < message.length() && end != std::string::npos)
                {
                    my_nickname = message.substr(start, end - start);
                }
            }

            std::cout << message; // 显示收到的消息
        }
        else if (bytes_received == 0)
        {
            std::cout << "\n[服务器] 连接已断开" << std::endl;
            running = false;
            waiting_for_input = false;
            break;
        }
        else if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            perror("接收错误");
            running = false;
            waiting_for_input = false;
            break;
        }

        // 短暂休眠
        usleep(10000); // 10ms
    }
}

int main(int argc, char *argv[])
{
    std::string server_ip = "127.0.0.1";
    if (argc > 1)
    {
        server_ip = argv[1];
    }

    std::cout << "=== 聊天室客户端 ===\n";
    std::cout << "连接服务器: " << server_ip << ":" << PORT << std::endl;

    // 创建socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket创建失败");
        return 1;
    }

    // 连接服务器
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(server_ip.c_str());

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("连接服务器失败");
        close(sockfd);
        return 1;
    }

    std::cout << "连接成功!\n";
    std::cout << "特殊命令:\n";
    std::cout << "  /name 昵称     - 修改昵称 (例: /name Alice)\n";
    std::cout << "  /online        - 查看在线用户\n";
    std::cout << "  /quit          - 退出聊天室\n";
    std::cout << "-------------------------\n";

    // 设置非阻塞（用于接收）
    set_non_blocking(sockfd);

    // 启动接收线程
    std::thread receiver(receive_thread, sockfd);

    // 主线程处理用户输入
    std::string input;
    while (running)
    {
        std::getline(std::cin, input);

        if (!running)
            break;

        if (!input.empty())
        {
            if (input == "/quit")
            {
                // 本地回显退出消息
                std::cout << "退出聊天室..." << std::endl;
                input += "\n";
                send(sockfd, input.c_str(), input.length(), 0);
                running = false;
                break;
            }

            // 处理本地回显（不显示命令）
            if (!is_command(input))
            {
                std::cout << "[我] " << input << std::endl;
            }

            // 添加换行符作为消息结束符
            input += "\n";

            if (send(sockfd, input.c_str(), input.length(), 0) < 0)
            {
                perror("发送失败");
                break;
            }
        }
    }

    // 清理
    running = false;
    waiting_for_input = false;
    receiver.join();
    close(sockfd);

    return 0;
}