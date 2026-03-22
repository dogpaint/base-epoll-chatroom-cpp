# C++ 聊天室项目（Linux版）

基于 Linux epoll 实现的多客户端聊天室，支持昵称修改、在线用户查看、消息广播等核心功能，配套完整单元测试，可选 Web 前端 + WebSocket 代理扩展。

  

## 🛠️ 环境依赖

### 核心后端（纯C++聊天室，无前端/测试）

- 系统：Ubuntu 18.04+ / CentOS 7+ / 其他主流Linux发行版

- 编译器：g++ 7.5+（支持C++11及以上）

- 构建工具：CMake 3.10+

- 测试框架：Google Test（GTest）1.8+

  

### Web前端 + WebSocket代理（可选）

- Python：3.6+（Ubuntu 18.04默认3.6，20.04默认3.8，24.04默认3.12）

- Python库：websockets 10.0+（兼容Python3.6+）

- 浏览器：Chrome 60+ / Firefox 55+（支持WebSocket即可）

  

### 测试脚本（可选）

- Python：3.6+

- 可选依赖：pandas 1.0+、matplotlib 3.0+（性能报告可视化）

- 模糊测试：AFL++ 2.52b+（Ubuntu 18.04可通过apt安装）

  

## 📌 兼容性说明

1. **核心后端兼容Ubuntu 18.04**：

   - 默认g++7.5、CMake3.10可直接编译核心后端；

   - 安装GTest：`sudo apt install libgtest-dev && cd /usr/src/gtest && sudo cmake . && sudo make && sudo cp lib/*.a /usr/lib`。

2. **Python3.6+适配**：

   - Ubuntu 18.04：`pip3 install websockets==10.0`（指定兼容版本）；

   - Ubuntu 20.04+/24.04：`pip3 install websockets`（自动装最新版）。

3. **无Linux环境适配**：

   - 纯后端可在WSL1/WSL2（Windows）、Docker中运行；

   - 前端可在任意系统的现代浏览器中访问。

  

## 🚀 快速开始

### 1. 克隆项目

```bash

git clone https://github.com/dogpaint/base-epoll-chatroom-cpp.git

cd base-epoll-chatroom-cpp

```

  

### 2. 安装系统依赖（Ubuntu）

```bash

sudo apt update && sudo apt install -y g++ cmake

sudo apt install -y libgtest-dev

cd /usr/src/gtest && sudo cmake . && sudo make && sudo cp lib/libgtest* /usr/lib/

```

  

### 3. 编译后端代码

```bash

mkdir build && cd build

cmake ..

make

cd ..

```

  

### 4. 配置Python虚拟环境（WebSocket代理，可选）

```bash

# 安装虚拟环境依赖

sudo apt install python3.12-venv

  

# 创建并激活虚拟环境

python3 -m venv venv

source venv/bin/activate

  

# 安装依赖库

pip install websockets

```

  

### 5. 一键启动所有服务

```bash

# 赋予启动脚本执行权限

chmod +x scripts/start_all.sh

  

# 启动后端 + WebSocket 代理 + 前端 HTTP 服务

./scripts/start_all.sh

```

如果一键脚本运行失败，可分步手动启动（适合排查问题）：

  

#### 步骤 1：启动 C++ 后端（核心）

  

```bash

cd ~/charroom_epoll

./build/chat_server

# 成功标志：终端输出「服务器启动，监听 8088 端口」

# 保持该终端打开，关闭则后端停止

```

  

#### 步骤 2：启动 WebSocket 代理（新开终端）

  

```bash

cd ~/charroom_epoll

# 激活 Python 虚拟环境

source venv/bin/activate

# 启动代理

python scripts/ws_proxy.py

# 成功标志：终端输出「WebSocket 代理已启动：ws://0.0.0.0:8089」

# 保持该终端打开

```

  

#### 步骤 3：启动前端 HTTP 服务（新开终端）

  

```bash

cd ~/charroom_epoll

# 启动轻量 HTTP 服务器（端口 8889，可替换为未被占用的端口）

python3 -m http.server 8889

# 成功标志：终端输出「Serving HTTP on 0.0.0.0 port 8889」

```

  

#### 步骤 4：手动停止服务（可选）

  

```bash

# 方式 1：关闭对应终端（简单）

# 方式 2：终端执行命令杀死进程

sudo killall chat_server python3

```

### 6. 访问聊天室

打开浏览器访问：`http://<你的服务器IP>:8889/frontend/chat.html`

- 本地测试：`http://localhost:8889/frontend/chat.html`

- 局域网访问：`http://你的LinuxIP:8889/frontend/chat.html`

  

### 7. 基础使用

- `/name 昵称`：修改个人昵称

- 直接输入文字：发送群消息

- `/online`：查看当前在线用户

- 多开浏览器窗口：模拟多客户端聊天

  

## ⚙️ 常见问题

### Q1: 端口被占用（Address already in use）

```bash

# 更换HTTP服务端口（示例：8000）

python3 -m http.server 8000

# 访问地址：http://<IP>:8000/frontend/chat.html

```

  

### Q2: 浏览器显示“断开连接”

1. 检查服务状态：`ps -ef | grep chat_server`、`ps -ef | grep ws_proxy.py`

2. 关闭Linux防火墙：`sudo ufw disable`

3. 验证网络连通性：`ping <你的LinuxIP>`

  

### Q3: 局域网内其他人无法访问

1. 确认双方在同一局域网；

2. 服务已默认绑定 `0.0.0.0`（全网卡监听）；

3. 检查防火墙/路由器是否放行 8088/8089/8889 端口。

  

## 📂 项目结构

```

charroom_epoll/

├── src/           # C++ 后端核心代码

├── frontend/      # Web 前端（纯 HTML/JS，零配置）

├── tests/         # 测试脚本（单元/性能/压力/模糊）

├── scripts/       # 启动脚本/工具脚本

├── build/         # 编译产物（自动生成）

├── venv/          # Python 虚拟环境（自动生成，.gitignore 忽略）

└── README.md      # 使用说明

```

  

## 🧪 测试相关

### 测试文件目录

```

tests/

├── command_parser_test.cpp  # 命令解析测试

├── client_logic_test.cpp    # 客户端逻辑测试

├── server_logic_test.cpp    # 服务器逻辑测试

├── network_test.cpp         # 网络功能测试

└── integration_test.cpp     # 集成测试

```

  

### 总结

1. 核心后端基于Linux epoll实现，兼容Ubuntu 18.04+，需g++7.5+和CMake3.10+编译；

2. 可选WebSocket代理+Web前端，依赖Python3.6+和websockets库，浏览器访问指定地址即可使用；

3. 支持昵称修改、在线用户查看、消息广播等核心功能，配套完整测试脚本，端口占用/连接失败可按常见问题排查。