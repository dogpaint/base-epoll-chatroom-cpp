#include "ChatServer.hpp"
#include "platform.hpp"
#include <iostream>
#include <thread>
#include <csignal>

ChatServer *g_server = nullptr;

#if PLATFORM_WINDOWS
BOOL WINAPI console_ctrl_handler(DWORD ctrl_type)
{
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT)
    {
        std::cout << "\n[INFO] Received stop signal, shutting down..." << std::endl;
        if (g_server) g_server->stop();
        return TRUE;
    }
    return FALSE;
}
#else
#include <csignal>
void signal_handler(int signal)
{
    if (signal == SIGINT && g_server)
    {
        std::cout << "\n[INFO] Received stop signal (SIGINT), shutting down..." << std::endl;
        g_server->stop();
    }
}
#endif

void console_thread(ChatServer &server)
{
    std::string command;
    while (true)
    {
        std::cout << "\nServer command (status/stop/help/health): ";
        std::getline(std::cin, command);

        if (command == "status")
        {
            std::cout << server.get_server_info();
        }
        else if (command == "health")
        {
            std::cout << server.get_health_report() << std::endl;
        }
        else if (command == "stop")
        {
            server.stop();
            break;
        }
        else if (command == "help")
        {
            std::cout << "Available commands:\n";
            std::cout << "  status  - Show server status\n";
            std::cout << "  health  - Show health report (JSON)\n";
            std::cout << "  stop    - Stop server\n";
            std::cout << "  help    - Show this help\n";
        }
        else if (!command.empty())
        {
            std::cout << "Unknown command: " << command << std::endl;
        }
    }
}

int main()
{
    std::cout << "=== Cross-Platform Chatroom Server ===\n" << std::endl;

    signal(SIGPIPE, SIG_IGN);

    if (!platform::network_startup())
    {
        std::cerr << "[FATAL] Network startup failed" << std::endl;
        return 1;
    }

#if PLATFORM_WINDOWS
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#else
    signal(SIGINT, signal_handler);
#endif

    ChatServer server;
    g_server = &server;

    if (!server.initialize())
    {
        std::cerr << "[FATAL] Server initialization failed!" << std::endl;
        return 1;
    }

    std::thread console(console_thread, std::ref(server));

    server.start();

    console.join();

    platform::network_cleanup();

    std::cout << "\n[INFO] Server shut down safely" << std::endl;
    return 0;
}