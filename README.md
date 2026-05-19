# Base Epoll Chatroom (C++) / 基于 Epoll 的跨平台 C++ 聊天室

A high-performance chatroom server built with C++17, featuring **cross-platform** I/O (Linux epoll / macOS kqueue / Windows WSAPoll), **heartbeat monitoring**, **comprehensive stress testing**, and **CI/CD** with performance gates.
一套基于 C++17 的高性能聊天室服务器，支持**跨平台** I/O（Linux epoll / macOS kqueue / Windows WSAPoll）、**心跳监测**、**全面压力测试**以及带性能阈值的 **CI/CD** 流水线。

---

## 项目特性 / Key Features

| Feature 特性                                             | Description 说明                                                   |
| -------------------------------------------------------- | ------------------------------------------------------------------ |
| Cross-platform I/O / 跨平台 I/O                         | Linux `epoll` / macOS `kqueue` / Windows `WSAPoll` auto-detected  |
| Heartbeat engine / 心跳引擎                              | Server-side health loop + client PING/PONG, timeout disconnect     |
| Stress testing / 压力测试                                | 5 scenarios: 20–100 concurrent users, generated JSON report        |
| CI/CD / 持续集成                                         | GitHub Actions: Linux × macOS × Windows, perf threshold validation  |
| Command parser / 命令解析                                | `/name`, `/online`, `/quit` with nickname support                  |
| Web frontend (optional) / 可选 Web 前端                   | `ws_proxy.py` WebSocket bridge → browser chat UI                   |

---

## 架构概览 / Architecture

```
                          ┌─────────────┐
                          │  Browser UI  │
                          └──────┬──────┘
                          ws://  │
                     ┌───────────▼───────────┐
                     │   ws_proxy.py (Python)│
                     └───────────┬───────────┘
                          tcp:// │
┌────────────────────────────────▼─────────────────────────────────┐
│                      ChatServer  (C++17)                          │
│  ┌──────────────┐  ┌──────────────┐  ┌────────────────────────┐  │
│  │ Poller       │  │ Heartbeat    │  │ Client Map             │  │
│  │ epoll/kqueue │  │ Engine       │  │ (fd → ClientInfo)      │  │
│  │ /WSAPoll     │  │ 5s interval  │  │                        │  │
│  └──────────────┘  └──────────────┘  └────────────────────────┘  │
│  ┌──────────────┐  ┌──────────────────────────────────────────┐  │
│  │ CommandParser│  │ Broadcast / Unicast Message Engine       │  │
│  └──────────────┘  └──────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────────────────┘
```

---

## 快速开始 / Quick Start

### 依赖 / Prerequisites

| Component         | Requirement 要求                             |
| ----------------- | -------------------------------------------- |
| C++ Compiler / 编译器 | g++ 7.5+ / Clang 10+ / MSVC 2019+            |
| CMake             | 3.10+                                        |
| Google Test       | 1.8+ (for unit tests / 单元测试需要)           |
| Python            | 3.8+ (optional, for WebSocket proxy / 可选)   |

### 克隆 & 编译 / Clone & Build

```bash
git clone https://github.com/dogpaint/base-epoll-chatroom-cpp.git
cd base-epoll-chatroom-cpp
```

**Linux (Ubuntu)**
```bash
sudo apt install -y g++ cmake libgtest-dev
mkdir build && cd build
cmake -DBUILD_TESTS=ON ..
make -j$(nproc)
```

**macOS**
```bash
brew install cmake googletest
mkdir build && cd build
cmake -DBUILD_TESTS=ON ..
make -j$(sysctl -n hw.logicalcpu)
```

**Windows (PowerShell)**
```powershell
# Install Google Test
git clone https://github.com/google/googletest.git --depth 1
cd googletest && mkdir bld && cd bld
cmake .. && cmake --build . --config Release
cmake --install . --prefix C:/gtest

# Build project
mkdir build && cd build
cmake -DBUILD_TESTS=ON -DCMAKE_PREFIX_PATH="C:/gtest" ..
cmake --build . --config Release
```

### 运行服务器 / Start Server

```bash
./build/chat_server
```

### 运行客户端 / Start Client

```bash
./build/chat_client          # connect to 127.0.0.1
./build/chat_client 10.0.0.1 # connect to specific IP / 指定IP
```

### 聊天命令 / Chat Commands

| Command 命令        | Description 说明                |
| ------------------- | ------------------------------ |
| `/name <nickname>`  | Change your nickname / 修改昵称 |
| `/online`           | View online users / 在线用户    |
| `/quit`             | Exit chatroom / 退出聊天室       |

---

## 运行测试 / Run Tests

```bash
cd build

# Unit tests only / 仅单元测试
ctest --output-on-failure -E "StressTest"

# Stress tests (with server auto-started) / 压力测试（自动启动服务端）
ctest --output-on-failure -R "StressTest" --timeout 300

# All tests / 全部测试
ctest --output-on-failure --timeout 300
```

### 压力测试场景 / Stress Test Scenarios

| Test 测试                                          | Clients / 用户 | Duration / 时长 | Notes 说明               |
| -------------------------------------------------- | ------------ | ------------ | ------------------------ |
| `LightLoad_20Clients_10Seconds`                     | 20           | 10 s         | Light load / 轻负载       |
| `MediumLoad_50Clients_15Seconds`                    | 50           | 15 s         | Medium load / 中负载      |
| `HeavyLoad_100Clients_20Seconds`                    | 100          | 20 s         | Heavy load / 重负载       |
| `HeartbeatReliability`                              | 30           | 20 s         | PING/PONG reliability    |
| `ConnectionBurst_SimultaneousClients`               | 100          | instant      | Burst connect / 突发连接  |

### 性能报告 / Performance Report

每次压力测试后自动生成 `build/tests/stress_test_report.json`：

```json
{
  "results": {
    "throughput_msg_per_sec": 315.4,
    "avg_latency_ms": 5.8,
    "p95_latency_ms": 7.0,
    "p99_latency_ms": 11.3,
    "error_rate_percent": 0.0,
    "heartbeat_ok": 90,
    "connect_errors": 0
  }
}
```

验证报告 / Validate report：

```bash
python3 scripts/validate_performance.py build/tests/stress_test_report.json
```

### 性能阈值 / Performance Thresholds

| Metric 指标                        | Threshold 阈值 | Description 说明                |
| ---------------------------------- | ------------ | ------------------------------- |
| Throughput 吞吐量                   | ≥ 50 msg/s   | Minimum messages per second     |
| Avg connect time 平均连接时间        | ≤ 5000 ms    | Connection setup latency        |
| Error rate 错误率                   | ≤ 5%         | Total errors / total messages   |
| P95 latency P95延迟                 | ≤ 500 ms     | 95th percentile round-trip      |
| P99 latency P99延迟                 | ≤ 1000 ms    | 99th percentile round-trip      |

---

## 心跳监测机制 / Heartbeat Mechanism

- **Server side / 服务端**: `health_monitor_loop` runs every 5 seconds, logs health status. `heartbeat_check` disconnects clients idle > 15 seconds.
- **Client side / 客户端**: sends `PING` every 5 seconds, expects `PONG` response.
- **Alert / 告警**: abnormal heartbeat failures logged as `[ALERT]` after sustained failures.

---

## 项目结构 / Project Structure

```
base-epoll-chatroom-cpp/
├── include/                    # Header files / 头文件
│   ├── platform.hpp           # Cross-platform abstraction / 跨平台抽象
│   ├── ChatServer.hpp         # Server class / 服务端类
│   ├── ClientInfo.hpp         # Client metadata / 客户端元信息
│   ├── common.hpp             # Global constants / 全局常量
│   └── command_parser.hpp     # Command parser / 命令解析器
├── src/                        # Server source / 服务端源码
│   ├── main.cpp               # Entry point + signal handling / 入口+信号处理
│   ├── ChatServer.cpp         # Core server logic / 核心服务器逻辑
│   ├── platform.cpp           # Platform implementation / 平台实现
│   └── command_parser.cpp     # Command parsing logic / 命令解析逻辑
├── client/
│   └── client.cpp             # Terminal client (heartbeat) / 终端客户端（心跳）
├── tests/
│   ├── stress_test.cpp        # ★ Stress test suite / 压力测试
│   ├── command_parser_test.cpp
│   ├── chat_server_test.cpp
│   ├── integration_test.cpp
│   └── client_display_test.cpp
├── scripts/
│   ├── validate_performance.py # ★ Performance validation / 性能验证
│   └── ws_proxy.py             # WebSocket proxy / 代理
├── frontend/
│   └── chat.html               # Browser chat UI / 浏览器聊天界面
├── .github/workflows/
│   └── cpp-ci.yml              # ★ CI/CD pipeline / 持续集成
├── CMakeLists.txt
└── README.md
```

---

## CI/CD 流水线 / CI/CD Pipeline

The [GitHub Actions workflow](.github/workflows/cpp-ci.yml) runs on every push and PR:

| Job                          | Platform  | Description                                        |
| ---------------------------- | --------- | -------------------------------------------------- |
| `linux-gcc`                  | Ubuntu    | Build + unit tests + stress tests + perf validate  |
| `macos`                      | macOS     | Build + unit tests + artifact upload               |
| `windows`                    | Windows   | Build + unit tests + artifact upload               |
| `perf-gate`                  | Ubuntu    | Downloads report artifact, validates thresholds    |

---

## Web 前端（可选）/ Optional Web UI

(保持原有 ws_proxy.py 启动方式，如果你需要也可以保留)

```bash
python3 -m venv venv && source venv/bin/activate
pip install websockets
python3 scripts/ws_proxy.py &
python3 -m http.server 8889 &
# Open / 打开: http://localhost:8889/frontend/chat.html
```

---

## 许可证 / License

MIT License