#include <iostream>
#include <cstring>
#include <string>
#include <thread>
#include <atomic>
#include "../include/platform.hpp"
#include "../include/common.hpp"

std::atomic<bool> running{true};
std::string my_nickname = "Guest";

bool is_command(const std::string &input)
{
    return !input.empty() && input[0] == '/';
}

void receive_thread(platform::socket_t sockfd)
{
    char buffer[BUFFER_SIZE];

    while (running)
    {
        ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received > 0)
        {
            buffer[bytes_received] = '\0';
            std::string message(buffer, bytes_received);

            size_t rename_pos = message.find("renamed to");
            if (rename_pos != std::string::npos)
            {
                size_t start = rename_pos + 10;
                size_t end = message.find("\n", start);
                if (start < message.length() && end != std::string::npos)
                {
                    std::string new_name = message.substr(start, end - start);
                    size_t trim_start = new_name.find_first_not_of(" \t");
                    if (trim_start != std::string::npos)
                    {
                        my_nickname = new_name.substr(trim_start);
                    }
                }
            }

            std::cout << message;
        }
        else if (bytes_received == 0)
        {
            std::cout << "\n[Server] Connection closed" << std::endl;
            running = false;
            break;
        }
        else
        {
            int err = platform::get_last_socket_error();
#if PLATFORM_WINDOWS
            if (err != WSAEWOULDBLOCK) {
#else
            if (err != EAGAIN && err != EWOULDBLOCK) {
#endif
                std::cerr << "[ERROR] Receive error: "
                          << platform::get_socket_error_string(err) << std::endl;
                running = false;
                break;
            }
        }

        platform::sleep_ms(10);
    }
}

void heartbeat_thread(platform::socket_t sockfd)
{
    while (running)
    {
        platform::sleep_ms(HEARTBEAT_INTERVAL_SEC * 1000);
        if (!running) break;

        std::string ping = "PING\n";
        send(sockfd, ping.c_str(), ping.length(), 0);
    }
}

int main(int argc, char *argv[])
{
    std::string server_ip = "127.0.0.1";
    if (argc > 1)
    {
        server_ip = argv[1];
    }

    if (!platform::network_startup())
    {
        std::cerr << "[FATAL] Network startup failed" << std::endl;
        return 1;
    }

    std::cout << "=== Chatroom Client ===\n";
    std::cout << "Connecting to: " << server_ip << ":" << PORT << std::endl;

    platform::socket_t sockfd = platform::create_tcp_socket();
    if (sockfd == platform::INVALID_SOCK)
    {
        std::cerr << "[ERROR] Failed to create socket" << std::endl;
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(server_ip.c_str());

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "[ERROR] Connection failed: "
                  << platform::get_socket_error_string(platform::get_last_socket_error())
                  << std::endl;
        platform::close_socket(sockfd);
        return 1;
    }

    std::cout << "Connected!\n";
    std::cout << "Commands:\n";
    std::cout << "  /name <nickname>  - Change nickname\n";
    std::cout << "  /online           - View online users\n";
    std::cout << "  /quit             - Exit chatroom\n";
    std::cout << "-------------------------\n";

    platform::set_non_blocking(sockfd);

    std::thread receiver(receive_thread, sockfd);
    std::thread heartbeat(heartbeat_thread, sockfd);

    std::string input;
    while (running)
    {
        std::getline(std::cin, input);

        if (!running) break;

        if (!input.empty())
        {
            if (input == "/quit")
            {
                std::cout << "Leaving chatroom..." << std::endl;
                input += "\n";
                send(sockfd, input.c_str(), input.length(), 0);
                running = false;
                break;
            }

            if (!is_command(input))
            {
                std::cout << "[Me] " << input << std::endl;
            }

            input += "\n";
            if (send(sockfd, input.c_str(), input.length(), 0) < 0)
            {
                std::cerr << "[ERROR] Send failed" << std::endl;
                break;
            }
        }
    }

    running = false;
    if (receiver.joinable()) receiver.join();
    if (heartbeat.joinable()) heartbeat.join();
    platform::close_socket(sockfd);
    platform::network_cleanup();

    return 0;
}