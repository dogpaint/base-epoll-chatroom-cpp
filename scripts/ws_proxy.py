#!/usr/bin/env python3
import asyncio
import websockets

# 后端 TCP 服务地址
TCP_HOST = "127.0.0.1"
TCP_PORT = 8088 

async def proxy(websocket):
    # 连接后端 TCP 服务
    reader, writer = await asyncio.open_connection(TCP_HOST, TCP_PORT)
    
    # 双向转发数据：WebSocket ↔ TCP
    async def ws_to_tcp():
        async for msg in websocket:
            writer.write(msg.encode() + b"\n")  # 加换行符（和终端客户端一致）
            await writer.drain()

    async def tcp_to_ws():
        while True:
            data = await reader.read(1024)
            if not data:
                break
            await websocket.send(data.decode())

    # 启动双向转发
    await asyncio.gather(ws_to_tcp(), tcp_to_ws())

async def main():
    # 启动 WebSocket 服务（端口 8089，和后端端口区分）
    async with websockets.serve(proxy, "0.0.0.0", 8089):
        print("WebSocket 代理已启动：ws://localhost:8089")
        await asyncio.Future()  # 永久运行

if __name__ == "__main__":
    asyncio.run(main())