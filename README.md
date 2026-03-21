# C++ 聊天室项目（Linux版）
一个基于Linux epoll实现的多客户端聊天室，支持昵称修改、在线用户查看、消息广播等核心功能，配套完整的单元测试。

## 📋 功能特性
- ✅ 多客户端并发连接（epoll IO多路复用）
- ✅ 昵称自定义修改（自动清理非法字符/多余空格）
- ✅ 在线用户列表查询
- ✅ 消息广播（发送者不回显自己的消息）
- ✅ 客户端优雅退出
- ✅ 完整单元测试（GTest覆盖命令解析、客户端显示逻辑）

## 🛠️ 环境依赖
- 系统：Ubuntu 18.04+（其他Linux发行版亦可）
- 编译器：g++ 7.5+（支持C++11及以上）
- 构建工具：CMake 3.10+
- 测试框架：Google Test（GTest）
  
##😀测试相关文件
-test相关:
tests/
├── command_parser_test.cpp    # 测试命令解析
├── client_logic_test.cpp      # 测试客户端逻辑
├── server_logic_test.cpp      # 测试服务器逻辑
├── network_test.cpp          # 测试网络相关
└── integration_test.cpp      # 集成测试

### 安装依赖（Ubuntu）
```bash
# 安装编译器和CMake
sudo apt update && sudo apt install -y g++ cmake

# 安装GTest（测试用）
sudo apt install -y libgtest-dev
cd /usr/src/gtest && sudo cmake . && sudo make && sudo cp lib/libgtest* /usr/lib/

