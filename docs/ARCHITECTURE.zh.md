# 泛舟RPC服务器 系统架构文档

## 文档信息
- **版本**: 1.0.0
- **更新日期**: 2026-01
- **适用对象**: 开发人员、系统接手者

---

## 1. 系统概述

泛舟RPC服务器是一个基于Qt的温室智能控制系统核心服务，提供JSON-RPC 2.0接口，用于控制和监控CAN总线继电器设备。

### 1.1 目标平台
- **硬件**: 全志A133 (ARM Cortex-A53 四核)
- **操作系统**: Linux (Buildroot/Yocto)
- **开发框架**: Qt 5.x/6.x

### 1.2 主要功能
- CAN总线通信与设备管理
- 继电器控制（正转/反转/停止）
- 设备分组管理（按通道绑定）
- 自动控制策略（定时/传感器触发）
- JSON-RPC 2.0远程调用接口
- Web调试界面
- Qt桌面管理工具

---

## 2. 系统架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                         客户端层                                  │
├─────────────────────┬───────────────────┬───────────────────────┤
│   Web调试界面        │   Qt桌面应用       │   其他RPC客户端        │
│   (test_web)        │   (qt_app)        │   (第三方集成)          │
└─────────────────────┴───────────────────┴───────────────────────┘
                              │
                              ▼ JSON-RPC 2.0 (TCP/WebSocket)
┌─────────────────────────────────────────────────────────────────┐
│                       RPC服务层 (src/rpc)                        │
├─────────────────────────────────────────────────────────────────┤
│   JsonRpcServer    │   JsonRpcDispatcher   │   RpcHelpers        │
│   (TCP服务器)       │   (方法分发器)          │   (辅助函数)         │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      核心业务层 (src/core)                        │
├─────────────────────────────────────────────────────────────────┤
│   CoreContext      │   RpcRegistry        │   CoreConfig         │
│   (系统上下文)       │   (RPC方法注册)       │   (配置管理)          │
└─────────────────────────────────────────────────────────────────┘
                              │
           ┌──────────────────┼──────────────────┐
           ▼                  ▼                  ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│   设备层          │  │   策略层         │  │   配置层         │
│   (src/device)   │  │   (strategies)  │  │   (src/config)  │
├─────────────────┤  ├─────────────────┤  ├─────────────────┤
│ RelayGd427      │  │ TimerStrategy   │  │ SystemSettings  │
│ CanDeviceManager│  │ SensorStrategy  │  │ ScreenConfig    │
│ ISensor接口      │  │ RelayStrategy   │  │ NetworkConfig   │
└─────────────────┘  └─────────────────┘  └─────────────────┘
           │
           ▼
┌─────────────────────────────────────────────────────────────────┐
│                      通信层 (src/comm)                           │
├─────────────────────────────────────────────────────────────────┤
│   CanComm (CAN总线)    │   SerialComm (串口)   │   CommAdapter    │
│   (Linux SocketCAN)    │   (RS485/UART)       │   (抽象接口)       │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      硬件层                                      │
├─────────────────────────────────────────────────────────────────┤
│   CAN总线设备    │   Modbus传感器    │   GPIO    │   UART传感器    │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. 目录结构

```
fanzhou_rpc_server/
├── main.cpp                    # 程序主入口
├── fanzhou_rpc_server.pro      # Qt项目配置文件
│
├── config/                     # 配置文件目录
│   └── config_example.json     # 配置示例
│
├── docs/                       # 文档目录
│   ├── README.zh.md            # 中文说明文档
│   ├── API_REFERENCE.zh.md     # API参考手册
│   ├── ARCHITECTURE.zh.md      # 系统架构文档（本文档）
│   └── DEVELOPMENT.zh.md       # 开发指南
│
├── test_web/                   # Web调试工具
│   ├── index.html              # 调试界面HTML
│   ├── js/app.js               # JavaScript功能
│   └── css/style.css           # 样式文件
│
├── qt_app/                     # Qt桌面管理应用
│   ├── qt_app.pro              # Qt项目文件
│   └── src/
│       ├── main.cpp            # 程序入口
│       ├── mainwindow.*        # 主窗口
│       ├── rpc_client.*        # RPC客户端
│       ├── device_widget.*     # 设备管理（卡片式）
│       ├── group_widget.*      # 分组管理
│       ├── strategy_widget.*   # 策略管理
│       ├── settings_widget.*   # 系统设置
│       └── ...                 # 其他组件
│
└── src/                        # 服务器源代码
    ├── core/                   # 核心业务模块
    │   ├── core_context.*      # 系统上下文
    │   ├── core_config.*       # 配置管理
    │   └── rpc_registry.*      # RPC方法注册
    │
    ├── rpc/                    # RPC通信模块
    │   ├── json_rpc_server.*   # TCP服务器
    │   ├── json_rpc_dispatcher.* # 方法分发器
    │   ├── json_rpc_client.*   # 客户端（用于测试）
    │   ├── rpc_helpers.*       # 辅助函数
    │   └── rpc_error_codes.h   # 错误码定义
    │
    ├── comm/                   # 底层通信模块
    │   ├── base/
    │   │   └── comm_adapter.*  # 通信适配器基类
    │   ├── can_comm.*          # CAN总线通信
    │   └── serial_comm.*       # 串口通信
    │
    ├── device/                 # 设备驱动模块
    │   ├── device_types.h      # 设备类型定义
    │   ├── base/
    │   │   ├── device_adapter.* # 设备适配器基类
    │   │   └── i_sensor.h      # 传感器接口
    │   └── can/
    │       ├── relay_protocol.h # CAN继电器协议
    │       ├── relay_gd427.*   # GD427继电器驱动
    │       ├── can_device_manager.* # CAN设备管理器
    │       └── i_can_device.h  # CAN设备接口
    │
    ├── utils/                  # 工具模块
    │   ├── logger.*            # 日志系统
    │   └── utils.*             # 通用工具函数
    │
    └── config/                 # 配置模块
        └── system_settings.*   # 系统设置控制器
```

---

## 4. 核心模块说明

### 4.1 核心上下文 (CoreContext)

`CoreContext` 是系统的核心协调器，管理所有子系统的生命周期和交互。

**主要职责**：
- 初始化和管理CAN总线通信
- 管理继电器设备实例
- 管理设备分组（按通道绑定）
- 处理控制命令队列
- 管理自动化策略

**关键数据结构**：
```cpp
class CoreContext {
    SystemSettings *systemSettings;     // 系统设置控制器
    CanComm *canBus;                    // CAN总线通信器
    CanDeviceManager *canManager;       // CAN设备管理器
    QHash<quint8, RelayGd427*> relays;  // 继电器设备表
    QHash<int, QList<quint8>> deviceGroups;  // 设备分组表
    QHash<int, QString> groupNames;     // 分组名称表
    QHash<int, QList<int>> groupChannels;  // 分组通道列表（按通道绑定）
    // ... 策略管理器等
};
```

### 4.2 RPC注册器 (RpcRegistry)

`RpcRegistry` 负责注册所有RPC方法到分发器。

**方法分类**：
- `registerBase()` - 基础方法（ping, list, echo）
- `registerSystem()` - 系统方法（时间、网络、云平台）
- `registerCan()` - CAN方法（状态、发送）
- `registerRelay()` - 继电器方法（控制、查询、急停）
- `registerGroup()` - 分组方法（创建、删除、通道绑定）
- `registerAuto()` - 自动策略方法
- `registerDevice()` - 设备管理方法
- `registerScreen()` - 屏幕配置方法
- `registerConfig()` - 配置保存/加载方法

### 4.3 分组与通道绑定

**重要设计原则**：
- 分组通过绑定特定设备的特定通道来工作
- 策略执行时只控制分组中已绑定的通道
- 不再在策略创建时指定控制通道

**通道绑定API**：
```
group.addChannel    - 添加通道到分组
group.removeChannel - 从分组移除通道
group.getChannels   - 获取分组的通道列表
```

---

## 5. 数据流

### 5.1 控制命令流程

```
客户端 → RPC请求 → JsonRpcDispatcher → 方法处理器
                                            ↓
           响应 ← JsonRpcServer ← 控制结果 ← CoreContext
                                            ↓
                                      控制队列处理
                                            ↓
                                      RelayGd427.control()
                                            ↓
                                      CanComm.sendFrame()
                                            ↓
                                      CAN总线 → 设备
```

### 5.2 策略执行流程

```
策略定时器触发 / 传感器条件满足
            ↓
      策略管理器
            ↓
    获取分组绑定的通道列表
            ↓
    对每个通道执行控制命令
            ↓
      CoreContext.enqueueControl()
            ↓
      发送CAN帧到设备
```

---

## 6. 配置管理

### 6.1 配置文件格式

配置文件使用JSON格式，包含以下主要节：

```json
{
  "main": {
    "rpcPort": 12345
  },
  "can": {
    "interface": "can0",
    "bitrate": 125000
  },
  "devices": [
    {
      "nodeId": 1,
      "name": "继电器-1",
      "type": 1
    }
  ],
  "groups": [
    {
      "groupId": 1,
      "name": "通风组",
      "channels": [
        {"nodeId": 1, "channel": 0},
        {"nodeId": 1, "channel": 1}
      ]
    }
  ],
  "strategies": [
    {
      "id": 1,
      "name": "定时通风",
      "groupId": 1,
      "action": "fwd",
      "intervalSec": 3600
    }
  ]
}
```

### 6.2 配置持久化

通过 `config.save` RPC方法将运行时配置保存到文件：

```
config.save   - 保存当前配置到文件
config.reload - 从文件重新加载配置
config.get    - 获取当前运行时配置
```

---

## 7. 扩展指南

### 7.1 添加新设备类型

1. 在 `device_types.h` 中添加设备类型枚举
2. 创建设备驱动类（继承 `DeviceAdapter`）
3. 在 `CoreContext` 中添加设备管理逻辑
4. 在 `RpcRegistry` 中注册相关RPC方法

### 7.2 添加新传感器

1. 实现 `ISensor` 接口
2. 添加传感器类型到 `device_types.h`
3. 在 `CoreContext` 中添加传感器管理
4. 可选：创建传感器触发策略

### 7.3 添加新RPC方法

1. 在 `RpcRegistry` 中添加方法注册
2. 使用 `RpcHelpers` 解析参数
3. 调用 `CoreContext` 中的业务逻辑
4. 更新API文档

---

## 8. 部署说明

### 8.1 编译

```bash
# 配置Qt环境
source /opt/qt5/bin/qt5-env.sh

# 编译服务器
qmake fanzhou_rpc_server.pro
make -j4

# 编译Qt应用
cd qt_app
qmake qt_app.pro
make -j4
```

### 8.2 运行

```bash
# 启动CAN接口
ip link set can0 type can bitrate 125000
ip link set can0 up

# 启动服务器
./fanzhou_rpc_server -c /etc/fanzhou/config.json

# 启动Qt应用
./qt_app
```

### 8.3 服务配置

```bash
# systemd服务文件: /etc/systemd/system/fanzhou-rpc.service
[Unit]
Description=Fanzhou RPC Server
After=network.target can.target

[Service]
Type=simple
ExecStart=/usr/bin/fanzhou_rpc_server -c /etc/fanzhou/config.json
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

---

## 9. 故障排查

### 9.1 CAN总线问题

**症状**: 设备无响应、控制无效

**检查步骤**:
1. 检查CAN接口状态: `ip link show can0`
2. 检查CAN发送队列: 调用 `can.status` RPC
3. 检查波特率设置
4. 检查终端电阻（120Ω）
5. 使用 `candump can0` 监控总线数据

### 9.2 配置保存问题

**症状**: 修改后重启丢失

**解决方案**:
1. 确保调用 `config.save` RPC方法
2. 检查配置文件写入权限
3. 查看服务器日志

### 9.3 策略不执行

**症状**: 策略已创建但不触发

**检查步骤**:
1. 确认策略已启用（enabled=true）
2. 确认分组存在且有绑定通道
3. 检查定时器是否正常运行
4. 查看策略运行状态

---

## 10. 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0.0 | 2026-01 | 初始版本 |

---

*本文档由泛舟RPC服务器开发团队维护。*
