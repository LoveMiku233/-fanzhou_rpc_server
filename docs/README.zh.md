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
│   ├── README.zh.md            # 中文文档
│   └── API_REFERENCE.zh.md     # API参考手册
├── test_web/                   # Web调试工具
│   └── index.html              # 调试界面
├── qt_app/                     # Qt桌面应用程序
│   ├── qt_app.pro              # Qt项目文件
│   ├── resources/              # 资源文件
│   └── src/                    # 源代码
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
        ├── device_types.h      # 设备/通信/接口/协议类型定义
        ├── base/
        │   ├── device_adapter.h/cpp   # 设备适配器基类
        │   └── i_sensor.h             # 传感器接口
        ├── can/                # CAN设备
        │   ├── i_can_device.h         # CAN设备接口
        │   ├── can_device_manager.h/cpp
        │   ├── relay_protocol.h       # 继电器协议
        │   └── relay_gd427.h/cpp      # GD427继电器
        ├── serial/             # 串口传感器（统一模块）
        │   ├── serial_protocol.h      # 串口协议定义（Modbus/Custom/Raw）
        │   ├── serial_sensor.h/cpp    # 串口传感器基类（支持协议切换）
        │   └── serial_temp_sensor.h/cpp # 串口温度传感器
        ├── modbus/             # Modbus传感器（兼容模块）
        │   ├── modbus_sensor.h/cpp    # Modbus传感器基类
        │   └── modbus_temp_sensor.h/cpp # 温度传感器
        └── uart/               # UART传感器（兼容模块）
            └── uart_sensor.h/cpp      # UART传感器基类
```

### 串口传感器协议选择

新增的 `serial/` 目录提供统一的串口传感器框架，支持以下协议：

| 协议类型 | 说明 |
|----------|------|
| `Modbus` | 标准Modbus RTU协议，使用从机地址和寄存器读取数据 |
| `Custom` | 自定义帧格式协议，支持帧头/帧尾/固定长度配置 |
| `Raw`    | 原始数据流，无帧格式，适用于特殊传感器 |

配置示例：

```json
{
  "name": "temp-sensor01",
  "type": 22,
  "commType": 1,
  "nodeId": 10,
  "bus": "/dev/ttyS1",
  "params": {
    "protocol": "Modbus",
    "modbusAddr": 1,
    "baudRate": 9600,
    "registerAddr": 0,
    "registerCount": 1,
    "scale": 0.1
  }
}
```

使用自定义协议时：

```json
{
  "name": "custom-sensor01",
  "type": 81,
  "commType": 1,
  "nodeId": 20,
  "bus": "/dev/ttyS2",
  "params": {
    "protocol": "Custom",
    "baudRate": 9600,
    "frameHeader": "AA55",
    "frameLength": 8
  }
}
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

### CAN总线诊断

| 方法 | 参数 | 说明 |
|------|------|------|
| `can.status` | 无 | 获取CAN总线状态（接口、波特率、是否打开、发送队列大小） |
| `can.send` | `{id, dataHex, extended?}` | 发送原始CAN帧 |

**说明**：当CAN控制不生效时，首先调用 `can.status` 检查CAN总线状态。如果 `opened` 为 `false`，则CAN接口未正确打开，请参考故障排除章节。

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

项目提供了Web调试工具，位于 `test_web/dist/` 目录。可以使用任何HTTP服务器打开：

```bash
# 使用Python启动简单HTTP服务器
cd test_web/dist
python3 -m http.server 8080

# 在浏览器中访问
# http://localhost:8080
```

#### 连接原理

由于浏览器安全限制，无法直接通过TCP连接到RPC服务器。需要使用websocat作为WebSocket到TCP的代理：

```
浏览器 → WebSocket(localhost:12346) → websocat代理 → TCP(目标设备:12345)
```

**数据流向说明：**
- **浏览器**：通过WebSocket协议连接到本地代理（localhost:12346）
- **websocat代理**：运行在本机，负责协议转换（WebSocket ↔ TCP）
- **RPC服务器**：运行在目标设备上，监听TCP端口（默认12345）

#### 使用Tauri桌面应用

如果使用编译好的Tauri桌面应用，可以自动管理websocat代理：

1. 在启动页填写目标设备的RPC服务器地址（如 `192.168.0.104`）和端口（默认 `12345`）
2. 进入主界面后，点击"启动代理"按钮自动启动websocat
3. 点击"连接"按钮通过WebSocket连接

#### 手动启动代理（非Tauri环境）

如果在普通浏览器中使用，需要手动启动websocat代理：

```bash
# 安装websocat
# 下载地址: https://github.com/vi/websocat/releases

# 在本机启动代理，连接到远程RPC服务器
# 将 192.168.0.104 替换为目标设备的实际IP地址
websocat --text ws-l:0.0.0.0:12346 tcp:192.168.0.104:12345

# 然后在浏览器的连接设置中：
# - RPC服务器地址: 192.168.0.104
# - RPC端口: 12345
# - WS端口: 12346
# 点击"连接"按钮
```

#### 常见问题

**Q: 连接时显示"WebSocket连接错误"？**

A: 请检查：
1. websocat代理是否已启动
2. 本地WS端口是否正确（默认12346）
3. 目标RPC服务器是否可达（可用 `ping` 测试）
4. 目标设备的防火墙是否允许12345端口连接

**Q: 为什么需要websocat代理？**

A: 浏览器出于安全考虑，只允许使用WebSocket协议进行实时通信，不能直接使用TCP。websocat作为中间层，将WebSocket协议转换为TCP协议，从而实现浏览器与RPC服务器的通信。

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

### CAN TX buffer full（发送缓冲区满）

当日志中出现类似以下信息时：

```
[DEBUG] [CAN] TX buffer full, backing off 10ms
[DEBUG] [CAN] TX buffer full, backing off 20ms
[DEBUG] [CAN] TX buffer full, backing off 40ms
...
[DEBUG] [CAN] TX buffer full, backing off 320ms
```

**问题原因**：

这表明CAN帧无法成功发送出去。服务器启动时会自动查询所有继电器设备的状态，如果CAN总线无法正常工作，发送队列会持续积压，系统进入指数退避模式。

**自动恢复机制**：

系统实现了自动恢复机制，防止TX缓冲区持续满载导致系统卡死：

- 系统使用指数退避策略（10ms → 20ms → 40ms → 80ms → 160ms → 320ms）
- 当连续10次达到最大退避时间（320ms）后，系统会自动丢弃当前帧并重置退避状态
- 丢弃帧时会输出警告日志：`TX持续失败，丢弃帧: id=0x..., dlc=..., 已重试10次`
- 这允许系统在CAN总线恢复后自动恢复正常工作

**常见原因**：

1. **CAN总线未连接设备**：CAN协议要求至少有一个接收设备发送ACK信号，否则发送方会认为发送失败
2. **波特率不匹配**：发送设备和接收设备的波特率必须一致（默认125000bps）
3. **缺少终端电阻**：CAN总线两端各需要一个120Ω的终端电阻
4. **接线问题**：CAN_H和CAN_L接线错误或接触不良
5. **CAN接口未正确配置**：接口未启动或配置错误

**诊断步骤**：

1. 检查CAN接口详细状态：
   ```bash
   ip -details link show can0
   ```
   关注 `state` 字段，正常应该是 `UP`，以及 `can_state`（应该是 `ERROR-ACTIVE`）。

2. 查看CAN统计信息：
   ```bash
   ip -s link show can0
   ```
   如果 `TX errors` 持续增加，说明发送有问题。

3. 检查内核CAN错误计数：
   ```bash
   cat /sys/class/net/can0/device/net/can0/statistics/*
   ```

4. 确认CAN接口配置：
   ```bash
   canconfig can0
   ```

**解决方案**：

1. **确保CAN总线上至少有一个其他设备**（如继电器模块）已正确连接并通电

2. **重新配置CAN接口**：
   ```bash
   ip link set can0 down
   canconfig can0 bitrate 125000 ctrlmode triple-sampling on
   ip link set can0 up
   ```

3. **检查硬件连接**：
   - 确认CAN_H、CAN_L正确连接
   - 确认终端电阻（120Ω）已安装在总线两端
   - 使用万用表测量CAN_H和CAN_L之间的电阻，应约为60Ω（两个120Ω并联）

4. **如果只是测试环境**，可以使用虚拟CAN：
   ```bash
   modprobe vcan
   ip link add dev vcan0 type vcan
   ip link set vcan0 up
   ```
   然后修改配置文件中的 `can.ifname` 为 `vcan0`。

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
