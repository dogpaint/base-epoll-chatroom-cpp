// 服务器单元测试
#include <gtest/gtest.h>

// 这是一个占位测试，确保编译通过
TEST(PlaceholderTest, BasicCheck)
{
    EXPECT_EQ(1, 1);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}