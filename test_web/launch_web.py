#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
泛舟RPC服务器调试工具 - 自动启动脚本

功能说明:
1. 自动启动websocat代理，连接到指定的RPC服务器
2. 启动HTTP服务器托管Web调试界面
3. 自动打开浏览器访问调试界面

使用方法:
    python3 launch_web.py                    # 使用默认配置
    python3 launch_web.py --host 192.168.0.104  # 指定RPC服务器地址
    python3 launch_web.py --rpc-port 12345 --ws-port 12346 --http-port 8080

依赖:
    - websocat: 需要预先安装，下载地址 https://github.com/vi/websocat/releases
    - Python 3.6+
"""

import argparse
import http.server
import os
import platform
import signal
import socketserver
import subprocess
import sys
import threading
import time
import webbrowser
from pathlib import Path


class WebLauncher:
    """
    Web调试工具启动器类

    负责管理websocat代理进程和HTTP服务器
    """

    def __init__(self, rpc_host, rpc_port, ws_port, http_port, web_dir):
        """
        初始化启动器

        Args:
            rpc_host: RPC服务器地址
            rpc_port: RPC服务器TCP端口
            ws_port: WebSocket代理监听端口
            http_port: HTTP服务器端口
            web_dir: Web文件目录
        """
        self.rpc_host = rpc_host
        self.rpc_port = rpc_port
        self.ws_port = ws_port
        self.http_port = http_port
        self.web_dir = web_dir
        self.websocat_process = None
        self.http_server = None
        self.running = False

    def find_websocat(self):
        """
        查找websocat可执行文件

        Returns:
            websocat路径，如果未找到则返回None
        """
        # 常见的安装位置
        paths_to_check = [
            "websocat",  # 系统PATH中
            "/usr/local/bin/websocat",
            "/usr/bin/websocat",
            os.path.expanduser("~/.local/bin/websocat"),
            os.path.expanduser("~/bin/websocat"),
        ]

        # Windows平台添加.exe后缀
        if platform.system() == "Windows":
            paths_to_check = [p + ".exe" for p in paths_to_check]

        for path in paths_to_check:
            if os.path.isfile(path) and os.access(path, os.X_OK):
                return path
            # 检查系统PATH
            if path == "websocat" or path == "websocat.exe":
                result = subprocess.run(
                    ["which" if platform.system() != "Windows" else "where", path],
                    capture_output=True,
                    text=True,
                )
                if result.returncode == 0:
                    return result.stdout.strip().split("\n")[0]

        return None

    def start_websocat(self):
        """
        启动websocat代理

        Returns:
            是否成功启动
        """
        websocat_path = self.find_websocat()

        if not websocat_path:
            print("\n⚠️  未找到websocat，请先安装websocat")
            print("   下载地址: https://github.com/vi/websocat/releases")
            print("\n   安装示例:")
            print("   Linux: sudo mv websocat.x86_64-unknown-linux-musl /usr/local/bin/websocat && sudo chmod +x /usr/local/bin/websocat")
            print("   macOS: brew install websocat")
            return False

        # 构建websocat命令
        # websocat --text ws-l:0.0.0.0:12346 tcp:192.168.0.104:12345
        cmd = [
            websocat_path,
            "--text",
            f"ws-l:0.0.0.0:{self.ws_port}",
            f"tcp:{self.rpc_host}:{self.rpc_port}",
        ]

        print(f"\n🔌 启动WebSocket代理...")
        print(f"   命令: {' '.join(cmd)}")
        print(f"   代理: ws://localhost:{self.ws_port} -> tcp://{self.rpc_host}:{self.rpc_port}")

        try:
            # 启动websocat进程
            self.websocat_process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            # 等待一小段时间检查是否启动成功
            time.sleep(0.5)
            if self.websocat_process.poll() is not None:
                # 进程已退出，读取错误信息
                stderr = self.websocat_process.stderr.read().decode("utf-8")
                print(f"\n❌ websocat启动失败: {stderr}")
                return False

            print(f"   ✅ WebSocket代理已启动 (PID: {self.websocat_process.pid})")
            return True

        except FileNotFoundError:
            print(f"\n❌ 无法执行websocat: {websocat_path}")
            return False
        except Exception as e:
            print(f"\n❌ 启动websocat时发生错误: {e}")
            return False

    def start_http_server(self):
        """
        启动HTTP服务器

        Returns:
            是否成功启动
        """
        os.chdir(self.web_dir)

        handler = http.server.SimpleHTTPRequestHandler

        try:
            self.http_server = socketserver.TCPServer(("", self.http_port), handler)
            print(f"\n🌐 启动HTTP服务器...")
            print(f"   目录: {self.web_dir}")
            print(f"   地址: http://localhost:{self.http_port}")

            # 在后台线程中运行HTTP服务器
            server_thread = threading.Thread(target=self.http_server.serve_forever)
            server_thread.daemon = True
            server_thread.start()

            print(f"   ✅ HTTP服务器已启动")
            return True

        except OSError as e:
            if e.errno == 98 or e.errno == 48:  # Address already in use
                print(f"\n❌ 端口 {self.http_port} 已被占用，请使用其他端口")
            else:
                print(f"\n❌ 启动HTTP服务器失败: {e}")
            return False

    def open_browser(self):
        """
        打开浏览器访问调试界面
        """
        url = f"http://localhost:{self.http_port}?host=localhost&port={self.ws_port}&autoconnect=true"
        print(f"\n🚀 正在打开浏览器...")
        print(f"   URL: {url}")

        # 等待一小段时间让服务器启动
        time.sleep(0.5)

        try:
            webbrowser.open(url)
            print("   ✅ 浏览器已打开")
        except Exception as e:
            print(f"   ⚠️ 无法自动打开浏览器: {e}")
            print(f"   请手动访问: {url}")

    def stop(self):
        """
        停止所有服务
        """
        print("\n🛑 正在停止服务...")

        if self.websocat_process:
            try:
                self.websocat_process.terminate()
                self.websocat_process.wait(timeout=5)
                print("   ✅ WebSocket代理已停止")
            except subprocess.TimeoutExpired:
                self.websocat_process.kill()
                print("   ⚠️ WebSocket代理被强制终止")

        if self.http_server:
            self.http_server.shutdown()
            print("   ✅ HTTP服务器已停止")

        self.running = False

    def run(self):
        """
        运行启动器
        """
        print("=" * 60)
        print("   泛舟RPC服务器调试工具 - 自动启动脚本")
        print("=" * 60)

        # 检查web目录是否存在
        if not os.path.isdir(self.web_dir):
            print(f"\n❌ Web目录不存在: {self.web_dir}")
            return False

        # 启动websocat代理
        if not self.start_websocat():
            return False

        # 启动HTTP服务器
        if not self.start_http_server():
            self.stop()
            return False

        # 打开浏览器
        self.open_browser()

        self.running = True

        print("\n" + "=" * 60)
        print("   服务已启动，按 Ctrl+C 停止")
        print("=" * 60)

        # 等待用户中断
        try:
            while self.running:
                # 检查websocat进程是否仍在运行
                if self.websocat_process.poll() is not None:
                    print("\n⚠️ WebSocket代理已退出")
                    break
                time.sleep(1)
        except KeyboardInterrupt:
            pass

        self.stop()
        return True


def main():
    """
    主函数 - 解析命令行参数并启动服务
    """
    # 获取脚本所在目录
    script_dir = Path(__file__).parent.absolute()

    parser = argparse.ArgumentParser(
        description="泛舟RPC服务器调试工具 - 自动启动脚本",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
使用示例:
  %(prog)s                           # 使用默认配置 (连接到localhost:12345)
  %(prog)s --host 192.168.0.104      # 连接到指定的RPC服务器
  %(prog)s --host 192.168.0.104 --rpc-port 12345 --ws-port 12346 --http-port 8080

说明:
  此脚本会启动两个服务:
  1. websocat WebSocket代理 - 将WebSocket连接转发到RPC服务器的TCP端口
  2. HTTP服务器 - 托管Web调试界面

  然后自动打开浏览器访问调试界面。
""",
    )

    parser.add_argument(
        "--host",
        default="localhost",
        help="RPC服务器地址 (默认: localhost)",
    )

    parser.add_argument(
        "--rpc-port",
        type=int,
        default=12345,
        help="RPC服务器TCP端口 (默认: 12345)",
    )

    parser.add_argument(
        "--ws-port",
        type=int,
        default=12346,
        help="WebSocket代理监听端口 (默认: 12346)",
    )

    parser.add_argument(
        "--http-port",
        type=int,
        default=8080,
        help="HTTP服务器端口 (默认: 8080)",
    )

    parser.add_argument(
        "--web-dir",
        default=str(script_dir),
        help=f"Web文件目录 (默认: {script_dir})",
    )

    parser.add_argument(
        "--no-browser",
        action="store_true",
        help="不自动打开浏览器",
    )

    args = parser.parse_args()

    # 创建启动器实例
    launcher = WebLauncher(
        rpc_host=args.host,
        rpc_port=args.rpc_port,
        ws_port=args.ws_port,
        http_port=args.http_port,
        web_dir=args.web_dir,
    )

    # 设置信号处理
    def signal_handler(sig, frame):
        launcher.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    # 运行启动器
    success = launcher.run()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
