#pragma once
#include <string>
#include <vector>

namespace CommandParser
{

    enum class CommandType
    {
        CHANGE_NAME, // /name
        ONLINE,      // /online
        QUIT,        // /quit
        UNKNOWN      // 未知命令
    };

    struct ParsedCommand
    {
        CommandType type;
        std::string argument; // such as:/name ww 中的 "ww"
        std::string raw_message;

        ParsedCommand() : type(CommandType::UNKNOWN) {}
    };

    /**
     * 解析聊天室命令
     * @param message 原始消息
     * @return 解析后的命令结构
     */
    ParsedCommand parse_message(const std::string &message);

    /**
     * 判断是否为命令
     * @param message 消息
     * @return 是否为命令
     */
    bool is_command(const std::string &message);

    /**
     * 提取昵称（从 /name 命令）
     * @param message 消息
     * @return 昵称，如果格式错误返回空字符串
     */
    std::string extract_nickname(const std::string &message);

} // namespace CommandParser