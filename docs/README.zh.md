# 泛舟RPC服务器

## 项目简介

泛舟RPC服务器是一个基于Qt的温室控制系统核心服务，运行在全志A133平台上。它提供JSON-RPC 2.0接口用于控制和监控CAN总线继电器设备。

## 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                    泛舟RPC服务器                              │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │  RPC层      │  │  核心层     │  │  设备层              │  │
│  │             │  │             │  │                     │  │
│  │ JSON-RPC    │◄─│ CoreContext │◄─│ CanDeviceManager    │  │
│  │ Server      │  │             │  │                     │  │
│  │             │  │ RpcRegistry │  │ RelayGd427          │  │
│  │ Dispatcher  │  │             │  │                     │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐│
│  │                     通信层                               ││
│  │  ┌─────────────┐              ┌─────────────────────┐   ││
│  │  │  CanComm    │              │  SerialComm         │   ││
│  │  │  (CAN总线)  │              │  (串口/RS485)       │   ││
│  │  └─────────────┘              └─────────────────────┘   ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

## 目标平台

- **处理器**: 全志A133 (ARM Cortex-A53 四核)
- **操作系统**: Linux (Buildroot/Yocto)
- **Qt版本**: Qt 5.12+
- **CAN接口**: SocketCAN (can0)

## 目录结构

```
fanzhou_rpc_server/
├── main.cpp                    # 程序入口
├── fanzhou_rpc_server.pro      # Qt项目文件
├── config/                     # 配置文件
│   └── config_example.json     # 配置示例
├── docs/                       # 文档
│   └── README.zh.md            # 中文文档
├── test_web/                   # Web调试工具
│   └── index.html              # 调试界面
└── src/
    ├── utils/                  # 工具类
    │   ├── logger.h/cpp        # 日志系统
    │   └── utils.h/cpp         # 通用工具
    ├── config/                 # 配置管理
    │   └── system_settings.h/cpp
    ├── core/                   # 核心模块
    │   ├── core_config.h/cpp   # 配置管理
    │   ├── core_context.h/cpp  # 核心上下文
    │   └── rpc_registry.h/cpp  # RPC注册
    ├── rpc/                    # RPC模块
    │   ├── json_rpc_dispatcher.h/cpp  # 请求分发
    │   ├── json_rpc_server.h/cpp      # TCP服务器
    │   ├── json_rpc_client.h/cpp      # TCP客户端
    │   ├── rpc_helpers.h/cpp          # 辅助函数
    │   └── rpc_error_codes.h          # 错误码
    ├── comm/                   # 通信层
    │   ├── base/
    │   │   └── comm_adapter.h/cpp     # 通信适配器基类
    │   ├── can_comm.h/cpp             # CAN通信
    │   └── serial_comm.h/cpp          # 串口通信
    └── device/                 # 设备层
        ├── device_types.h      # 设备类型定义
        ├── base/
        │   └── device_adapter.h/cpp   # 设备适配器基类
        └── can/
            ├── i_can_device.h         # CAN设备接口
            ├── can_device_manager.h/cpp
            ├── relay_protocol.h       # 继电器协议
            └── relay_gd427.h/cpp      # GD427继电器
```

## 编译

### 依赖项

- Qt 5.12+ (Core, Network)
- Linux SocketCAN支持
- GCC 编译器

### 编译步骤

```bash
# 1. 创建构建目录
mkdir build && cd build

# 2. 运行qmake
qmake ../fanzhou_rpc_server.pro

# 3. 编译
make -j$(nproc)

# 4. 安装
sudo make install
```

### 交叉编译 (A133平台)

```bash
# 设置交叉编译工具链
export PATH=/opt/a133-toolchain/bin:$PATH
export CROSS_COMPILE=aarch64-linux-gnu-

# 使用Qt交叉编译版本
/opt/qt-a133/bin/qmake ../fanzhou_rpc_server.pro
make -j$(nproc)
```

## 配置文件

配置文件位于 `/var/lib/fanzhou_core/core.json`：

```json
{
  "main": {
    "rpcPort": 12345
  },
  "log": {
    "logToConsole": true,
    "logToFile": true,
    "logFilePath": "/var/log/fanzhou_core/core.log",
    "logLevel": 0
  },
  "can": {
    "ifname": "can0",
    "bitrate": 125000,
    "tripleSampling": true,
    "canFd": false
  },
  "devices": [
    {
      "name": "relay01",
      "type": 1,
      "commType": 2,
      "nodeId": 1,
      "bus": "can0",
      "params": {
        "channels": 4,
        "enabled": true
      }
    }
  ],
  "groups": [
    {
      "groupId": 1,
      "name": "main-group",
      "enabled": true,
      "devices": [1, 2]
    }
  ],
  "strategies": [
    {
      "id": 1,
      "name": "default-stop",
      "groupId": 1,
      "channel": 0,
      "action": "stop",
      "intervalSec": 120,
      "enabled": true,
      "autoStart": false
    }
  ]
}
```

### 配置项说明

| 配置项 | 类型 | 说明 |
|--------|------|------|
| `main.rpcPort` | int | RPC服务器监听端口 |
| `log.logLevel` | int | 日志级别 (0=Debug, 1=Info, 2=Warning, 3=Error, 4=Critical) |
| `can.ifname` | string | CAN接口名 |
| `can.bitrate` | int | CAN波特率 |
| `devices[].nodeId` | int | CAN节点ID (1-255) |
| `groups[].groupId` | int | 设备组ID |
| `strategies[].action` | string | 控制动作 (stop/fwd/rev) |

## A133平台部署

### 1. CAN接口配置

```bash
# 配置CAN接口
ip link set can0 down
canconfig can0 bitrate 125000 ctrlmode triple-sampling on
ip link set can0 up

# 验证CAN接口
canconfig can0
candump can0
```

### 2. 系统服务配置

创建 systemd 服务文件 `/etc/systemd/system/fanzhou-rpc.service`:

```ini
[Unit]
Description=FanZhou RPC Server
After=network.target

[Service]
Type=simple
ExecStart=/opt/fanzhou_rpc_server/bin/fanzhou_rpc_server
Restart=always
RestartSec=5
User=root

[Install]
WantedBy=multi-user.target
```

启动服务:

```bash
sudo systemctl daemon-reload
sudo systemctl enable fanzhou-rpc
sudo systemctl start fanzhou-rpc
sudo systemctl status fanzhou-rpc
```

### 3. 日志查看

```bash
# 查看日志文件
tail -f /var/log/fanzhou_core/core.log

# 查看系统日志
journalctl -u fanzhou-rpc -f
```

## RPC接口

### 基础方法

| 方法 | 参数 | 说明 |
|------|------|------|
| `rpc.ping` | 无 | 测试连接 |
| `rpc.list` | 无 | 列出所有RPC方法 |
| `echo` | 任意 | 回显参数 |

### 继电器控制

| 方法 | 参数 | 说明 |
|------|------|------|
| `relay.control` | `{node, ch, action}` | 控制继电器 |
| `relay.query` | `{node, ch}` | 查询状态 |
| `relay.status` | `{node, ch}` | 获取通道状态 |
| `relay.statusAll` | `{node}` | 获取所有通道状态 |
| `relay.nodes` | 无 | 获取设备列表 |

### 分组管理

| 方法 | 参数 | 说明 |
|------|------|------|
| `group.list` | 无 | 列出所有分组 |
| `group.create` | `{groupId, name}` | 创建分组 |
| `group.delete` | `{groupId}` | 删除分组 |
| `group.addDevice` | `{groupId, node}` | 添加设备（所有通道） |
| `group.removeDevice` | `{groupId, node}` | 移除设备 |
| `group.addChannel` | `{groupId, node, channel}` | 添加指定通道到分组 |
| `group.removeChannel` | `{groupId, node, channel}` | 从分组移除通道 |
| `group.getChannels` | `{groupId}` | 获取分组的通道列表 |
| `group.control` | `{groupId, ch, action}` | 分组控制 |

### 设备管理

| 方法 | 参数 | 说明 |
|------|------|------|
| `device.types` | 无 | 获取支持的设备类型列表 |
| `device.list` | 无 | 获取已注册设备列表 |
| `device.get` | `{nodeId}` | 获取设备详细信息 |
| `device.add` | `{nodeId, type, name, ...}` | 动态添加设备 |
| `device.remove` | `{nodeId}` | 动态移除设备 |

### 屏幕配置

| 方法 | 参数 | 说明 |
|------|------|------|
| `screen.get` | 无 | 获取屏幕配置 |
| `screen.set` | `{brightness, contrast, ...}` | 设置屏幕参数 |

### 控制队列

| 方法 | 参数 | 说明 |
|------|------|------|
| `control.queue.status` | 无 | 获取队列状态 |
| `control.queue.result` | `{jobId}` | 获取任务结果 |

### 自动策略

| 方法 | 参数 | 说明 |
|------|------|------|
| `auto.strategy.list` | 无 | 列出所有策略 |
| `auto.strategy.enable` | `{id, enabled}` | 启用/禁用策略 |
| `auto.strategy.trigger` | `{id}` | 手动触发策略 |

## 使用示例

### 命令行测试

```bash
# 测试连接
echo '{"jsonrpc":"2.0","id":1,"method":"rpc.ping","params":{}}' | nc localhost 12345

# 获取设备列表
echo '{"jsonrpc":"2.0","id":2,"method":"relay.nodes","params":{}}' | nc localhost 12345

# 控制继电器
echo '{"jsonrpc":"2.0","id":3,"method":"relay.control","params":{"node":1,"ch":0,"action":"fwd"}}' | nc localhost 12345

# 查询状态
echo '{"jsonrpc":"2.0","id":4,"method":"relay.statusAll","params":{"node":1}}' | nc localhost 12345
```

### Web调试工具

项目提供了Web调试工具，位于 `test_web/index.html`。可以使用任何HTTP服务器打开：

```bash
# 使用Python启动简单HTTP服务器
cd test_web
python3 -m http.server 8080

# 在浏览器中访问
# http://localhost:8080
```

## 错误码

| 错误码 | 说明 |
|--------|------|
| -32700 | JSON解析错误 |
| -32600 | 无效请求 |
| -32601 | 方法不存在 |
| -32602 | 无效参数 |
| -60010 | 缺少必需参数 |
| -60011 | 参数类型错误 |
| -60012 | 参数值无效 |
| -60013 | 无效操作状态 |
| -60120 | CAN未打开 |
| -60122 | CAN写入失败 |

## 故障排除

### CAN通信失败

1. 检查CAN接口状态：
   ```bash
   ip link show can0
   ```

2. 检查CAN配置：
   ```bash
   canconfig can0
   ```

3. 使用candump监控CAN总线：
   ```bash
   candump can0
   ```

### 服务无法启动

1. 检查配置文件语法：
   ```bash
   cat /var/lib/fanzhou_core/core.json | python3 -m json.tool
   ```

2. 检查日志输出：
   ```bash
   journalctl -u fanzhou-rpc -n 50
   ```

3. 确保必要目录存在：
   ```bash
   mkdir -p /var/lib/fanzhou_core
   mkdir -p /var/log/fanzhou_core
   ```

## 许可证

本项目采用 MIT 许可证。

## 联系方式

如有问题，请提交 Issue 或联系开发团队。
