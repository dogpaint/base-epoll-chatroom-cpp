#pragma once
#include "common.hpp"
#include<string>
#include<ctime>

struct ClientInfo
{
    int fd;  //文件描述符
    std::string ip_address;
    int port;
    std::string nickname;
    time_t connect_time;
    ClientState state; // 当前状态

    ClientInfo(int client_fd, const std::string &ip, int client_port)
        : fd(client_fd), ip_address(ip), port(client_port),
        nickname("Guest"+std::to_string(client_fd)),connect_time(time(nullptr)),state(CONNECTED){}
    // 获取格式化时间
    std::string get_formatted_time()const{
        char buffer[80];
        struct tm *timeinfo = localtime(&connect_time);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%s", timeinfo);
        return std::string(buffer);
    }
};
using ClientPtr = std::shared_ptr<ClientInfo>;//进行一个空间管理