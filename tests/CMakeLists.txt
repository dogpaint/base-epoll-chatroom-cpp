# 测试配置
cmake_minimum_required(VERSION 3.10)
project(ChatroomTests CXX)

# 查找gtest
find_package(GTest REQUIRED)
include(GoogleTest)

# 包含主项目头文件
include_directories(${CMAKE_SOURCE_DIR}/include)

# 1. 命令解析测试
add_executable(command_parser_test 
    command_parser_test.cpp  # 测试代码
    ${CMAKE_SOURCE_DIR}/src/command_parser.cpp  # 函数实现代码
)
target_link_libraries(command_parser_test GTest::gtest GTest::gtest_main pthread)

# 2. 服务器单元测试
add_executable(chat_server_test chat_server_test.cpp 
    ${CMAKE_SOURCE_DIR}/src/ChatServer.cpp
    ${CMAKE_SOURCE_DIR}/src/command_parser.cpp)
target_include_directories(chat_server_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(chat_server_test GTest::gtest GTest::gtest_main pthread)

# 3. 集成测试
add_executable(integration_test integration_test.cpp)
target_link_libraries(integration_test GTest::gtest GTest::gtest_main pthread)

# ========== 客户端显示测试 ==========
add_executable(client_display_test 
    client_display_test.cpp  # 仅你的模拟测试代码，无其他依赖
)
# 仅链接GTest库
target_link_libraries(client_display_test GTest::gtest GTest::gtest_main)

# ========== 注册所有测试 ==========
gtest_discover_tests(command_parser_test)
gtest_discover_tests(chat_server_test)
gtest_discover_tests(integration_test)
gtest_discover_tests(client_display_test)  # 注册客户端显示测试