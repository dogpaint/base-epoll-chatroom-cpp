#  指定CMake最低版本（兼容性保证）
cmake_minimum_required(VERSION 3.10)
# ^ 如果你的CMake版本低于3.10会报错

#  定义项目基本信息
project(ChatroomEpoll CXX)
# 项目名: ChatroomEpoll


# 设置C++标准
set(CMAKE_CXX_STANDARD 17)       # 使用C++17标准
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # 强制要求C++17
# 可选: set(CMAKE_CXX_EXTENSIONS OFF)  # 禁用编译器扩展

# 添加可执行文件（服务器）
add_executable(chat_server       # 生成的可执行文件名
    src/main.cpp                 # 源文件列表
    src/ChatServer.cpp           # 可以有多个文件
    src/command_parser.cpp
)
# 编译后会生成: chat_server (Linux) 或 chat_server.exe (Windows)

# 设置头文件包含路径
target_include_directories(chat_server PRIVATE include)
# ^ 告诉编译器：在include目录中查找头文件
# PRIVATE: 只对chat_server目标有效

# 链接库（如果需要）
# target_link_libraries(chat_server pthread mysqlclient)

# 添加客户端子目录
add_subdirectory(client)
# ^ 进入client目录，执行那里的CMakeLists.txt

#测试
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
   enable_testing()
   add_subdirectory(tests)
endif()
