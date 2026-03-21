// 命令解析测试
#include "command_parser.hpp"
#include <gtest/gtest.h> // 引入GTest框架
#include <string>

using namespace CommandParser;

TEST(CommandParserTest, TestIsCommand)
{
    EXPECT_TRUE(is_command("/name wofwof")); //// 是命令 → 期望true
    EXPECT_TRUE(is_command("/online"));
    EXPECT_TRUE(is_command("/quit"));
    EXPECT_FALSE(is_command("Hello")); // 不是命令 → 期望false
    EXPECT_FALSE(is_command(""));      // 空消息
    EXPECT_FALSE(is_command(" "));     // 只有空格
}

TEST(CommandParserTest, TestParseNameCommand)
{
    // 正确格式
    ParsedCommand cmd1 = parse_message("/name wofwof");
    EXPECT_EQ(cmd1.type, CommandType::CHANGE_NAME);
    EXPECT_EQ(cmd1.argument, "wofwof");

    // 带空格
    ParsedCommand cmd2 = parse_message("/name  Dogpaint  ");
    EXPECT_EQ(cmd2.type, CommandType::CHANGE_NAME);
    EXPECT_EQ(cmd2.argument, "Dogpaint");

    // 带换行符
    ParsedCommand cmd3 = parse_message("/name laomiantiao\n");
    EXPECT_EQ(cmd3.type, CommandType::CHANGE_NAME);
    EXPECT_EQ(cmd3.argument, "laomiantiao");

    // 带回车换行
    ParsedCommand cmd4 = parse_message("/name blackstrong\r\n");
    EXPECT_EQ(cmd4.type, CommandType::CHANGE_NAME);
    EXPECT_EQ(cmd4.argument, "blackstrong");

    // 错误格式：没有参数
    ParsedCommand cmd5 = parse_message("/name");
    EXPECT_EQ(cmd5.type, CommandType::CHANGE_NAME);
    EXPECT_TRUE(cmd5.argument.empty());

    // 错误格式：只有空格
    ParsedCommand cmd6 = parse_message("/name   ");
    EXPECT_EQ(cmd6.type, CommandType::CHANGE_NAME);
    EXPECT_TRUE(cmd6.argument.empty());
}

TEST(CommandParserTest, TestParseOnlineCommand)
{
    ParsedCommand cmd = parse_message("/online");
    EXPECT_EQ(cmd.type, CommandType::ONLINE);
    EXPECT_TRUE(cmd.argument.empty());
}

TEST(CommandParserTest, TestParseQuitCommand)
{
    ParsedCommand cmd = parse_message("/quit");
    EXPECT_EQ(cmd.type, CommandType::QUIT);
    EXPECT_TRUE(cmd.argument.empty());
}

TEST(CommandParserTest, TestParseUnknownCommand)
{
    // 未知命令
    ParsedCommand cmd1 = parse_message("/unknown");
    EXPECT_EQ(cmd1.type, CommandType::UNKNOWN);

    // 普通消息
    ParsedCommand cmd2 = parse_message("Hello world");
    EXPECT_EQ(cmd2.type, CommandType::UNKNOWN);

    // 空消息
    ParsedCommand cmd3 = parse_message("");
    EXPECT_EQ(cmd3.type, CommandType::UNKNOWN);
}

TEST(CommandParserTest, TestExtractNickname)
{
    // 正确格式
    EXPECT_EQ(extract_nickname("/name wofwof"), "wofwof");
    EXPECT_EQ(extract_nickname("/name Dogpaint"), "Dogpaint");
    EXPECT_EQ(extract_nickname("/name  laomiantiao  "), "laomiantiao");

    // 错误格式
    EXPECT_EQ(extract_nickname("/name"), "");
    EXPECT_EQ(extract_nickname("/name "), "");
    EXPECT_EQ(extract_nickname("/name   "), "");
    EXPECT_EQ(extract_nickname("/name [wofwof]"), ""); // 包含方括号
    EXPECT_EQ(extract_nickname("wofwof"), "");         // 不是命令
    EXPECT_EQ(extract_nickname(""), "");               // 空字符串
}

TEST(CommandParserTest, TestCommandWithSpecialCharacters)
{
    // 包含方括号（应该失败）
    ParsedCommand cmd1 = parse_message("/name [wofwof]");
    EXPECT_EQ(cmd1.type, CommandType::CHANGE_NAME);
    EXPECT_EQ(cmd1.argument, "[wofwof]");

    // 测试extract_nickname会拒绝这个
    EXPECT_EQ(extract_nickname("/name [wofwof]"), "");

    // 包含换行符
    ParsedCommand cmd2 = parse_message("/name wofwof\nDogpaint");
    EXPECT_EQ(cmd2.type, CommandType::CHANGE_NAME);
    EXPECT_EQ(cmd2.argument, "wofwof\nDogpaint");
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}