#!/bin/bash
# 一键启动所有服务
# 检查 chat_server 是否存在
if [ ! -f "./build/chat_server" ]; then
    echo "❌ 错误：未找到 chat_server 可执行文件，正在自动编译..."
    # 自动编译后端
    rm -rf build && mkdir build && cd build && cmake .. && make && cd ..
    # 再次检查
    if [ ! -f "./build/chat_server" ]; then
        echo "❌ 编译失败！请手动检查 CMake 配置"
        exit 1
    fi
fi
# 启动C++后端（后台运行）
./build/chat_server > server.log 2>&1 &
echo "✅ 后端服务器已启动（日志：server.log）"

# 激活虚拟环境并启动WebSocket代理（后台运行）
source venv/bin/activate
python scripts/ws_proxy.py > proxy.log 2>&1 &
echo "✅ WebSocket代理已启动（日志：proxy.log）"

# 启动HTTP服务器（前台运行，方便停止）
python3 -m http.server 8889
echo "✅ HTTP服务器已启动"