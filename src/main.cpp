#include "ChatServer.hpp"
#include <iostream>
#include <csignal>
#include <thread>

ChatServer *g_server = nullptr;

// 信号处理函数（处理Ctrl+C）
void signal_handler(int signal)
{
    if (signal == SIGINT && g_server)
    {
        std::cout << "\n[INFO] 接收到停止信号，正在关闭服务器..." << std::endl;
        g_server->stop();
    }
}

// 控制台命令处理线程
void console_thread(ChatServer &server)
{
    std::string command;
    while (true)
    {
        std::cout << "\n服务器命令 (status/stop/help): ";
        std::getline(std::cin, command);

        if (command == "status")
        {
            std::cout << server.get_server_info();
        }
        else if (command == "stop")
        {
            server.stop();
            break;
        }
        else if (command == "help")
        {
            std::cout << "可用命令:\n";
            std::cout << "  status  - 显示服务器状态\n";
            std::cout << "  stop    - 停止服务器\n";
            std::cout << "  help    - 显示帮助\n";
        }
        else if (!command.empty())
        {
            std::cout << "未知命令: " << command << std::endl;
        }
    }
}

int main()
{
    std::cout << "=== 基于epoll的聊天室服务器 ===\n"
              << std::endl;

    // 注册信号处理
    signal(SIGINT, signal_handler);

    ChatServer server;
    g_server = &server;

    // 初始化服务器
    if (!server.initialize())
    {
        std::cerr << "服务器初始化失败!" << std::endl;
        return 1;
    }

    // 启动控制台线程
    std::thread console(console_thread, std::ref(server));

    // 启动服务器主循环
    server.start();

    // 等待控制台线程结束
    console.join();

    std::cout << "\n[INFO] 服务器已安全关闭" << std::endl;
    return 0;
}