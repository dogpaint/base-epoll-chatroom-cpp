#pragma once // 预处理指令 保证同一个文件在同一个源文件中只包含一次
#include <string>
#include <memory>

const int MAX_EVENTS = 1024;  // epoll最大事件数
const int BUFFER_SIZE = 4096; // 读写缓冲区
const int PORT = 8088;        // 服务器端口
const std::string SERVER_IP = "0.0.0.0";

enum ClientState
{
    CONNECTED,   // 连接
    DISCONNECTED // 无连接
};

enum MessageType
{
    MSG_TEXT = 1,   // 文本
    MSG_SYSTEM = 2, // 系统消息
    MSG_ERROR = 3   // 错误消息
};
