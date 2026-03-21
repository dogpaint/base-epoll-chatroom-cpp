// tests/client_display_test.cpp
#include <gtest/gtest.h>
#include <string>
#include <sstream>

// 模拟客户端显示逻辑
class MockClient
{
public:
    std::stringstream received_messages;

    void simulate_send(const std::string &message, bool should_echo = true)
    {
        if (should_echo)
        {
            // 客户端本地回显
            received_messages << "[我] " << message << "\n";
        }

        // 发送到服务器（模拟）
        // ...
    }

    void simulate_receive(const std::string &message)
    {
        received_messages << message;
    }

    std::string get_display() const
    {
        return received_messages.str();
    }
};

TEST(ClientDisplayTest, TestMessageEcho)
{
    MockClient client;

    // 模拟发送消息
    client.simulate_send("你好");

    // 模拟收到服务器广播
    client.simulate_receive("[Alice] 你好\n");

    std::string display = client.get_display();

    // 应该看到两条消息
    EXPECT_NE(display.find("[我] 你好"), std::string::npos);
    EXPECT_NE(display.find("[Alice] 你好"), std::string::npos);

    std::cout << "客户端显示内容:\n"
              << display << std::endl;
}

TEST(ClientDisplayTest, TestCommandNoEcho)
{
    MockClient client;

    // 命令不应该本地回显
    client.simulate_send("/name Alice", false);
    client.simulate_receive("[系统] Guest1 改名为 Alice\n");

    std::string display = client.get_display();

    EXPECT_EQ(display.find("[我] /name Alice"), std::string::npos);
    EXPECT_NE(display.find("[系统] Guest1 改名为 Alice"), std::string::npos);
}