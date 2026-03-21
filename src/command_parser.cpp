#include "command_parser.hpp"
#include <algorithm>
#include <iostream>

namespace CommandParser
{

    ParsedCommand parse_message(const std::string &message)
    {
        ParsedCommand result;
        result.raw_message = message;

        if (message.empty())
        {
            result.type = CommandType::UNKNOWN;
            return result;
        }

        // 检查是否为命令
        if (message[0] != '/')
        {
            result.type = CommandType::UNKNOWN;
            return result;
        }

        // 解析命令类型
        if (message.substr(0, 6) == "/name ")
        {
            result.type = CommandType::CHANGE_NAME;
            result.argument = message.substr(6);

            // 清理参数：去除前后空格和换行
            auto start = result.argument.find_first_not_of(" \t\r\n");
            auto end = result.argument.find_last_not_of(" \t\r\n");

            if (start != std::string::npos && end != std::string::npos)
            {
                result.argument = result.argument.substr(start, end - start + 1);
            }
            else
            {
                result.argument.clear();
            }
        }
        else if (message == "/online")
        {
            result.type = CommandType::ONLINE;
        }
        else if (message == "/quit")
        {
            result.type = CommandType::QUIT;
        }
        else
        {
            result.type = CommandType::UNKNOWN;
        }

        return result;
    }

    bool is_command(const std::string &message)
    {
        if (message.empty())
            return false;
        return message[0] == '/';
    }

    std::string extract_nickname(const std::string &message)
    {
        if (!is_command(message) || message.substr(0, 6) != "/name ")
        {
            return "";
        }

        std::string nickname = message.substr(6);

        // 清理昵称
        auto start = nickname.find_first_not_of(" \t\r\n");
        auto end = nickname.find_last_not_of(" \t\r\n");

        if (start != std::string::npos && end != std::string::npos)
        {
            nickname = nickname.substr(start, end - start + 1);

            // 验证昵称：不能为空，不能包含特殊字符
            if (nickname.empty())
            {
                return "";
            }

            // 检查是否包含非法字符
            for (char c : nickname)
            {
                if (c == '[' || c == ']' || c == '\n' || c == '\r')
                {
                    return "";
                }
            }

            return nickname;
        }

        return "";
    }

}