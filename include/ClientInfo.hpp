#pragma once
#include "common.hpp"
#include <string>
#include <ctime>
#include <cstdint>

struct ClientInfo
{
    int fd;
    std::string ip_address;
    int port;
    std::string nickname;
    time_t connect_time;
    ClientState state;
    int64_t last_heartbeat_ms;
    int64_t last_activity_ms;
    int missed_heartbeats;

    ClientInfo(int client_fd, const std::string &ip, int client_port)
        : fd(client_fd), ip_address(ip), port(client_port),
          nickname("Guest" + std::to_string(client_fd)),
          connect_time(time(nullptr)),
          state(CONNECTED),
          last_heartbeat_ms(0),
          last_activity_ms(0),
          missed_heartbeats(0) {}

    std::string get_formatted_time() const
    {
        char buffer[80];
        struct tm *timeinfo = localtime(&connect_time);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        return std::string(buffer);
    }
};

using ClientPtr = std::shared_ptr<ClientInfo>;