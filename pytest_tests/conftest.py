import pytest
import socket
import os

# 自动获取本机配置
def get_local_ip():
    """获取本机真实IP，优先返回局域网IP，失败则使用127.0.0.1"""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # 连接公网DNS，获取本机出口IP（最稳定方法）
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
    except Exception:
        ip = "127.0.0.1"
    finally:
        s.close()
    return ip

def get_chat_server_port():
    """优先从环境变量读取端口，无则使用默认8088"""
    return int(os.getenv("CHAT_SERVER_PORT", 8088))

# 全局配置（自动获取，无需手动修改）
HOST = get_local_ip()
PORT = get_chat_server_port()


@pytest.fixture(scope="session")
def server_addr():
    """供所有测试文件使用的服务器地址（HOST, PORT）"""
    return (HOST, PORT)

@pytest.fixture(scope="session", autouse=True)
def check_server_health():
    """测试会话开始前：自动检查服务器是否启动"""
    print(f"\n==================================================")
    print(f"  测试目标服务器：{HOST}:{PORT}")
    print(f"==================================================")

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(3)

    try:
        # connect_ex 不抛异常，返回 0 代表连接成功
        if s.connect_ex((HOST, PORT)) == 0:
            print("✅ 服务器连接正常 → 开始测试")
        else:
            pytest.exit(f"❌ 连接失败！请先启动 C++ 聊天室服务器！")

    except Exception as e:
        pytest.exit(f"❌ 服务器检查异常：{str(e)}")

    finally:
        s.close()