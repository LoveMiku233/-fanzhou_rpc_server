# 泛舟RPC服务器 API参考手册

本文档提供泛舟RPC服务器所有模块、文件和函数的详细说明。

---

## 目录

1. [项目概述](#项目概述)
2. [目录结构](#目录结构)
3. [核心模块 (src/core)](#核心模块-srccore)
4. [RPC模块 (src/rpc)](#rpc模块-srcrpc)
5. [通信模块 (src/comm)](#通信模块-srccomm)
6. [设备模块 (src/device)](#设备模块-srcdevice)
7. [工具模块 (src/utils)](#工具模块-srcutils)
8. [配置模块 (src/config)](#配置模块-srcconfig)
9. [RPC接口详细说明](#rpc接口详细说明)
10. [错误码参考](#错误码参考)

---

## 项目概述

泛舟RPC服务器是一个基于Qt的温室控制系统核心服务，提供JSON-RPC 2.0接口，用于控制和监控CAN总线继电器设备。

**目标平台**：全志A133 (ARM Cortex-A53 四核)

**主要功能**：
- CAN总线通信与设备管理
- 继电器控制（正转/反转/停止）
- 设备分组管理
- 自动控制策略（定时/传感器触发）
- JSON-RPC 2.0远程调用接口

---

## 目录结构

```
fanzhou_rpc_server/
├── main.cpp                    # 程序主入口
├── fanzhou_rpc_server.pro      # Qt项目配置文件
├── config/                     # 配置文件目录
│   └── config_example.json     # 配置示例
├── docs/                       # 文档目录
│   ├── README.zh.md            # 中文说明文档
│   └── API_REFERENCE.zh.md     # API参考手册（本文档）
├── test_web/                   # Web调试工具
│   └── index.html              # 调试界面
└── src/
    ├── core/                   # 核心业务模块
    ├── rpc/                    # RPC通信模块
    ├── comm/                   # 底层通信模块
    ├── device/                 # 设备驱动模块
    ├── utils/                  # 工具函数模块
    └── config/                 # 系统配置模块
```

---

## 核心模块 (src/core)

核心模块是系统的业务逻辑中心，负责协调各子系统。

### core_context.h / core_context.cpp

**文件说明**：核心系统上下文，管理控制系统的核心组件。

#### 类：`CoreContext`

系统核心上下文类，继承自 `QObject`。

| 成员变量 | 类型 | 说明 |
|----------|------|------|
| `systemSettings` | `SystemSettings*` | 系统设置控制器 |
| `canBus` | `CanComm*` | CAN总线通信器 |
| `canManager` | `CanDeviceManager*` | CAN设备管理器 |
| `relays` | `QHash<quint8, RelayGd427*>` | 继电器设备表 |
| `deviceGroups` | `QHash<int, QList<quint8>>` | 设备分组表 |
| `groupNames` | `QHash<int, QString>` | 分组名称表 |
| `rpcPort` | `quint16` | RPC服务端口 |
| `canInterface` | `QString` | CAN接口名 |
| `canBitrate` | `int` | CAN波特率 |

**主要方法**：

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `init()` | 无 | `bool` | 使用默认配置初始化 |
| `init(config)` | `CoreConfig` | `bool` | 使用指定配置初始化 |
| `createGroup()` | `groupId, name, *error` | `bool` | 创建设备分组 |
| `deleteGroup()` | `groupId, *error` | `bool` | 删除设备分组 |
| `addDeviceToGroup()` | `groupId, node, *error` | `bool` | 添加设备到分组 |
| `removeDeviceFromGroup()` | `groupId, node, *error` | `bool` | 从分组移除设备 |
| `addChannelToGroup()` | `groupId, node, channel, *error` | `bool` | 添加指定通道到分组 |
| `removeChannelFromGroup()` | `groupId, node, channel, *error` | `bool` | 从分组移除通道 |
| `getGroupChannels()` | `groupId` | `QList<int>` | 获取分组的通道列表 |
| `enqueueControl()` | `node, channel, action, source` | `EnqueueResult` | 入队控制命令 |
| `queueGroupControl()` | `groupId, channel, action, source` | `GroupControlStats` | 分组批量控制 |
| `parseAction()` | `str, *ok` | `Action` | 解析动作字符串 |

**信号**：无

---

### core_config.h / core_config.cpp

**文件说明**：核心配置管理，定义系统配置结构和加载/保存功能。

#### 结构体列表

| 结构体 | 说明 |
|--------|------|
| `DeviceConfig` | 设备配置 |
| `RelayNodeConfig` | 继电器节点配置 |
| `CanConfig` | CAN总线配置 |
| `LogConfig` | 日志配置 |
| `MainConfig` | 主配置 |
| `DeviceGroupConfig` | 设备组配置 |
| `ScreenConfig` | 屏幕参数配置 |
| `AutoStrategyConfig` | 自动控制策略配置（定时） |
| `SensorStrategyConfig` | 传感器触发策略配置 |
| `RelayStrategyConfig` | 定时继电器策略配置 |
| `SensorRelayStrategyConfig` | 传感器触发继电器策略配置 |

#### 类：`CoreConfig`

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `loadFromFile()` | `path, *error` | `bool` | 从JSON文件加载配置 |
| `saveToFile()` | `path, *error` | `bool` | 保存配置到JSON文件 |
| `makeDefault()` | 无 | `CoreConfig` | 创建默认配置对象 |

---

### rpc_registry.h / rpc_registry.cpp

**文件说明**：RPC方法注册器，将所有RPC方法注册到分发器。

#### 类：`RpcRegistry`

继承自 `QObject`。

| 方法 | 说明 |
|------|------|
| `registerAll()` | 注册所有RPC方法 |
| `registerBase()` | 注册基础方法（ping, list, echo） |
| `registerSystem()` | 注册系统方法（sys.info, sys.can.*） |
| `registerCan()` | 注册CAN方法（can.send） |
| `registerRelay()` | 注册继电器方法（relay.*） |
| `registerGroup()` | 注册分组方法（group.*） |
| `registerAuto()` | 注册自动策略方法（auto.*） |
| `registerDevice()` | 注册设备管理方法（device.*） |
| `registerScreen()` | 注册屏幕配置方法（screen.*） |

---

## RPC模块 (src/rpc)

RPC模块提供JSON-RPC 2.0协议的完整实现。

### json_rpc_dispatcher.h / json_rpc_dispatcher.cpp

**文件说明**：JSON-RPC方法分发器，管理方法处理器并分发请求。

#### 类：`JsonRpcDispatcher`

继承自 `QObject`。

| 类型定义 | 说明 |
|----------|------|
| `Handler` | `std::function<QJsonValue(const QJsonObject&)>` 方法处理器类型 |

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `registerMethod()` | `method, handler` | `void` | 注册方法处理器 |
| `methods()` | 无 | `QStringList` | 获取已注册方法列表 |
| `handle()` | `request` | `QJsonObject` | 处理JSON-RPC请求 |

---

### json_rpc_server.h / json_rpc_server.cpp

**文件说明**：JSON-RPC TCP服务器，接受TCP连接并处理请求。

#### 类：`JsonRpcServer`

继承自 `QTcpServer`。

| 方法 | 说明 |
|------|------|
| 构造函数 | 接收 `JsonRpcDispatcher*` 作为分发器 |

**信号**：继承自 `QTcpServer`

**协议格式**：使用换行符分隔的JSON格式（每行一个完整的JSON-RPC请求）

---

### json_rpc_client.h / json_rpc_client.cpp

**文件说明**：JSON-RPC TCP客户端，提供同步和异步RPC调用功能。

#### 类：`JsonRpcClient`

继承自 `QObject`。

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `setEndpoint()` | `host, port` | `void` | 设置服务器端点 |
| `connectToServer()` | `timeoutMs` | `bool` | 连接到服务器 |
| `disconnectFromServer()` | 无 | `void` | 断开连接 |
| `isConnected()` | 无 | `bool` | 检查连接状态 |
| `call()` | `method, params, timeoutMs` | `QJsonValue` | 同步调用（阻塞） |
| `callAsync()` | `method, params` | `int` | 异步调用 |
| `callAsync()` | `method, params, callback, timeoutMs` | `int` | 带回调的异步调用 |

**信号**：

| 信号 | 参数 | 说明 |
|------|------|------|
| `connected()` | 无 | 连接成功 |
| `disconnected()` | 无 | 断开连接 |
| `transportError()` | `error` | 传输错误 |
| `callFinished()` | `id, result, error` | 调用完成 |

---

### rpc_helpers.h / rpc_helpers.cpp

**文件说明**：RPC辅助函数，提供解析JSON-RPC参数和构建响应的工具函数。

#### 命名空间：`RpcHelpers`

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `getU8()` | `params, key, &out` | `bool` | 提取无符号8位整数（支持字符串） |
| `getI32()` | `params, key, &out` | `bool` | 提取32位整数（支持字符串） |
| `getBool()` | `params, key, &out, defaultValue` | `bool` | 提取布尔值 |
| `getString()` | `params, key, &out` | `bool` | 提取字符串 |
| `getHexBytes()` | `params, key, &out` | `bool` | 提取十六进制字节数组 |
| `ok()` | `value` | `QJsonObject` | 创建成功响应 |
| `err()` | `code, message` | `QJsonObject` | 创建错误响应 |

**注意**：`getI32()` 和 `getU8()` 支持数字类型和字符串类型的值，字符串会尝试转换为整数。这使得客户端可以发送 `"channel": "0"` 或 `"channel": 0` 两种格式。

---

### rpc_error_codes.h

**文件说明**：JSON-RPC错误码定义。

#### 命名空间：`RpcError`

| 错误码 | 值 | 说明 |
|--------|-----|------|
| `ParseError` | -32700 | JSON解析错误 |
| `InvalidRequest` | -32600 | 无效请求 |
| `MethodNotFound` | -32601 | 方法不存在 |
| `InvalidParams` | -32602 | 无效参数 |
| `InternalError` | -32603 | 内部错误 |
| `MissingParameter` | -60010 | 缺少必需参数 |
| `BadParameterType` | -60011 | 参数类型错误 |
| `BadParameterValue` | -60012 | 参数值无效 |
| `InvalidState` | -60013 | 无效操作状态 |
| `CanNotOpened` | -60120 | CAN未打开 |
| `CanWriteFailed` | -60122 | CAN写入失败 |

---

## 通信模块 (src/comm)

通信模块提供底层通信协议的抽象和实现。

### base/comm_adapter.h / comm_adapter.cpp

**文件说明**：通信适配器抽象接口。

#### 类：`CommAdapter`

抽象基类，继承自 `QObject`。

| 虚方法 | 返回值 | 说明 |
|--------|--------|------|
| `open()` | `bool` | 打开通信通道 |
| `close()` | `void` | 关闭通信通道 |
| `writeBytes()` | `qint64` | 写入字节数据 |

**信号**：

| 信号 | 说明 |
|------|------|
| `bytesReceived()` | 接收到数据 |
| `errorOccurred()` | 发生错误 |
| `opened()` | 通道打开 |
| `closed()` | 通道关闭 |

---

### can_comm.h / can_comm.cpp

**文件说明**：CAN总线通信适配器，使用Linux SocketCAN实现。

#### 结构体：`CanConfig`

| 成员 | 类型 | 说明 |
|------|------|------|
| `interface` | `QString` | CAN接口名（默认：can0） |
| `canFd` | `bool` | 是否启用CAN FD模式 |

#### 类：`CanComm`

继承自 `CommAdapter`。

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `open()` | 无 | `bool` | 打开CAN接口 |
| `close()` | 无 | `void` | 关闭CAN接口 |
| `sendFrame()` | `canId, payload, extended, rtr` | `bool` | 发送CAN帧 |

**信号**：

| 信号 | 参数 | 说明 |
|------|------|------|
| `canFrameReceived()` | `canId, payload, extended, rtr` | 接收到CAN帧 |

---

### serial_comm.h / serial_comm.cpp

**文件说明**：串口通信适配器，支持RS485和标准串口。

---

## 设备模块 (src/device)

设备模块提供各类设备的驱动和管理。

### device_types.h

**文件说明**：设备类型定义。

#### 枚举：`DeviceTypeId`

| 值 | 说明 |
|----|------|
| `RelayGd427 = 1` | GD427 CAN继电器 |
| `SensorModbusGeneric = 21` | 通用Modbus传感器 |
| `SensorModbusTemp = 22` | 温度传感器 |
| `SensorModbusHumidity = 23` | 湿度传感器 |
| `SensorModbusSoil = 24` | 土壤传感器 |
| `SensorModbusCO2 = 25` | CO2传感器 |
| `SensorModbusLight = 26` | 光照传感器 |
| `SensorModbusPH = 27` | pH传感器 |
| `SensorModbusEC = 28` | EC传感器 |
| `SensorCanGeneric = 51` | 通用CAN传感器 |

#### 枚举：`CommTypeId`

| 值 | 说明 |
|----|------|
| `Serial = 1` | 串口通信 |
| `Can = 2` | CAN总线通信 |

#### 工具函数

| 函数 | 说明 |
|------|------|
| `deviceTypeToString()` | 设备类型转字符串 |
| `commTypeToString()` | 通信类型转字符串 |
| `isSensorType()` | 判断是否为传感器 |
| `isModbusSensorType()` | 判断是否为Modbus传感器 |
| `allDeviceTypes()` | 获取所有设备类型信息 |

---

### base/device_adapter.h / device_adapter.cpp

**文件说明**：设备适配器抽象接口。

#### 类：`DeviceAdapter`

抽象基类，继承自 `QObject`。

| 虚方法 | 返回值 | 说明 |
|--------|--------|------|
| `init()` | `bool` | 初始化设备 |
| `poll()` | `void` | 轮询更新状态 |
| `name()` | `QString` | 获取设备名称 |

---

### can/relay_protocol.h

**文件说明**：CAN继电器协议定义。

#### 命名空间：`RelayProtocol`

**常量**：

| 常量 | 值 | 说明 |
|------|-----|------|
| `kCtrlBaseId` | 0x100 | 控制命令基地址 |
| `kStatusBaseId` | 0x200 | 状态响应基地址 |

**枚举**：

| 枚举 | 值 | 说明 |
|------|-----|------|
| `CmdType::ControlRelay` | 0x01 | 控制继电器 |
| `CmdType::QueryStatus` | 0x02 | 查询状态 |
| `Action::Stop` | 0x00 | 停止 |
| `Action::Forward` | 0x01 | 正转 |
| `Action::Reverse` | 0x02 | 反转 |

**结构体**：

| 结构体 | 说明 |
|--------|------|
| `CtrlCmd` | 控制命令 |
| `Status` | 状态响应 |

**函数**：

| 函数 | 说明 |
|------|------|
| `modeBits()` | 从状态字节提取模式位 |
| `phaseLost()` | 检查缺相标志 |
| `leFloat()` | 解码小端浮点数 |
| `putLeFloat()` | 编码小端浮点数 |
| `encodeCtrl()` | 编码控制命令 |
| `decodeStatus()` | 解码状态响应 |

---

### can/relay_gd427.h / relay_gd427.cpp

**文件说明**：GD427 CAN继电器设备控制器。

#### 类：`RelayGd427`

继承自 `DeviceAdapter` 和 `ICanDevice`。

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `nodeId()` | 无 | `quint8` | 获取节点ID |
| `control()` | `channel, action` | `bool` | 控制继电器通道 |
| `query()` | `channel` | `bool` | 查询通道状态 |
| `lastStatus()` | `channel` | `Status` | 获取最后状态 |
| `lastSeenMs()` | 无 | `qint64` | 获取最后响应时间 |

**信号**：

| 信号 | 参数 | 说明 |
|------|------|------|
| `statusUpdated()` | `channel, status` | 状态更新 |

---

### can/can_device_manager.h / can_device_manager.cpp

**文件说明**：CAN设备管理器，管理多个CAN设备并分发帧。

#### 类：`CanDeviceManager`

继承自 `QObject`。

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `addDevice()` | `device` | `void` | 添加设备 |
| `removeDevice()` | `device` | `void` | 移除设备 |
| `pollAll()` | 无 | `void` | 轮询所有设备 |

---

### can/i_can_device.h

**文件说明**：CAN设备接口定义。

#### 接口：`ICanDevice`

| 虚方法 | 返回值 | 说明 |
|--------|--------|------|
| `canDeviceName()` | `QString` | 获取设备名称 |
| `canAccept()` | `bool` | 检查是否接受该帧 |
| `canOnFrame()` | `void` | 处理接收到的帧 |

---

## 工具模块 (src/utils)

工具模块提供通用工具函数和日志系统。

### logger.h / logger.cpp

**文件说明**：线程安全日志系统。

#### 枚举：`LogLevel`

| 值 | 说明 |
|----|------|
| `Debug = 0` | 调试信息 |
| `Info = 1` | 一般信息 |
| `Warning = 2` | 警告消息 |
| `Error = 3` | 错误消息 |
| `Critical = 4` | 严重错误 |

#### 类：`Logger`

单例模式。

| 方法 | 说明 |
|------|------|
| `instance()` | 获取单例实例 |
| `init()` | 初始化日志系统 |
| `setMinLevel()` | 设置最小日志级别 |
| `setConsoleEnabled()` | 设置控制台输出开关 |
| `log()` | 记录日志 |
| `debug/info/warning/error/critical()` | 各级别便捷方法 |
| `flush()` | 刷新缓冲区 |
| `close()` | 关闭日志文件 |

**日志宏**：

| 宏 | 说明 |
|----|------|
| `LOG_DEBUG(source, msg)` | 调试日志 |
| `LOG_INFO(source, msg)` | 信息日志 |
| `LOG_WARNING(source, msg)` | 警告日志 |
| `LOG_ERROR(source, msg)` | 错误日志 |
| `LOG_CRITICAL(source, msg)` | 严重错误日志 |

---

### utils.h / utils.cpp

**文件说明**：通用工具函数。

| 函数 | 说明 |
|------|------|
| `sysErrorString()` | 获取带前缀的系统错误字符串 |

---

## 配置模块 (src/config)

配置模块提供系统设置和命令执行功能。

### system_settings.h / system_settings.cpp

**文件说明**：系统设置控制器。

#### 类：`SystemSettings`

继承自 `QObject`。

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `runCommand()` | `program, args, timeoutMs` | `QString` | 执行系统命令，返回输出 |
| `runCommandWithStatus()` | `program, args, timeoutMs` | `bool` | 执行系统命令，返回成功状态 |
| `canDown()` | `interface` | `bool` | 关闭CAN接口 |
| `canUp()` | `interface` | `bool` | 启动CAN接口 |
| `setCanBitrate()` | `interface, bitrate, tripleSampling` | `bool` | 设置CAN波特率 |
| `sendCanFrame()` | `interface, canId, data, extended` | `bool` | 发送CAN帧 |
| `startCanDump()` | `interface, extraArgs` | `bool` | 启动candump捕获 |
| `stopCanDump()` | 无 | `void` | 停止candump |
| `getSystemTime()` | 无 | `QString` | 获取系统时间 |
| `setSystemTime()` | `datetime` | `bool` | 设置系统时间 |
| `saveHardwareClock()` | 无 | `bool` | 保存到硬件时钟 |
| `readHardwareClock()` | 无 | `QString` | 读取硬件时钟 |
| `getNetworkInfo()` | `interface` | `QString` | 获取网络信息 |
| `pingTest()` | `host, count, timeoutSec` | `bool` | 测试网络连通性 |
| `setStaticIp()` | `interface, address, netmask, gateway` | `bool` | 设置静态IP |

**信号**：

| 信号 | 参数 | 说明 |
|------|------|------|
| `commandOutput()` | `line` | 命令输出 |
| `errorOccurred()` | `error` | 发生错误 |
| `candumpLine()` | `line` | candump输出行 |

---

## RPC接口详细说明

### 基础方法

| 方法名 | 参数 | 返回值 | 说明 |
|--------|------|--------|------|
| `rpc.ping` | 无 | `{ok: true}` | 测试连接 |
| `rpc.list` | 无 | `["method1", "method2", ...]` | 列出所有方法 |
| `echo` | 任意对象 | 原样返回 | 回显测试 |

### 系统方法

| 方法名 | 参数 | 返回值 | 说明 |
|--------|------|--------|------|
| `sys.info` | 无 | 系统信息对象（包含canOpened、canTxQueueSize） | 获取服务器信息 |
| `sys.can.setBitrate` | `{ifname, bitrate, tripleSampling?}` | `{ok}` | 设置CAN波特率 |
| `sys.can.dump.start` | `{ifname}` | `{ok}` | 启动CAN抓包 |
| `sys.can.dump.stop` | 无 | `{ok}` | 停止CAN抓包 |

### RTC时间方法

| 方法名 | 参数 | 返回值 | 说明 |
|--------|------|--------|------|
| `sys.time.get` | 无 | `{ok, datetime, timestamp}` | 获取系统时间 |
| `sys.time.set` | `{datetime}` | `{ok, datetime}` | 设置系统时间，格式：YYYY-MM-DD HH:mm:ss |
| `sys.time.saveHwclock` | 无 | `{ok}` | 保存系统时间到硬件时钟 (hwclock -w) |
| `sys.time.readHwclock` | 无 | `{ok, hwclock}` | 从硬件时钟读取时间 (hwclock -r) |

**使用示例**：

```bash
# 1. 查看当前时间
{"jsonrpc":"2.0","id":1,"method":"sys.time.get","params":{}}

# 2. 设置时间为2023年11月27日17时23分03秒
{"jsonrpc":"2.0","id":2,"method":"sys.time.set","params":{"datetime":"2023-11-27 17:23:03"}}

# 3. 保存时间到硬件时钟
{"jsonrpc":"2.0","id":3,"method":"sys.time.saveHwclock","params":{}}

# 4. 从硬件时钟读取时间
{"jsonrpc":"2.0","id":4,"method":"sys.time.readHwclock","params":{}}
```

### 网络配置方法

| 方法名 | 参数 | 返回值 | 说明 |
|--------|------|--------|------|
| `sys.network.info` | `{interface?}` | `{ok, info}` | 获取网络接口信息 |
| `sys.network.ping` | `{host, count?, timeout?}` | `{ok, host, reachable}` | 测试网络连通性 |
| `sys.network.setStaticIp` | `{interface, address, netmask?, gateway?}` | `{ok, interface, address}` | 设置静态IP地址 |

**参数说明**：
- `interface`: 网络接口名（如 eth0）
- `address`: IP地址（如 192.168.2.22）
- `netmask`: 子网掩码（如 255.255.255.0）
- `gateway`: 网关地址（如 192.168.2.1）
- `host`: 测试连通性的主机地址或域名
- `count`: ping次数（默认4次）
- `timeout`: 超时秒数（默认10秒）

**使用示例**：

```bash
# 1. 查看所有网络接口信息
{"jsonrpc":"2.0","id":1,"method":"sys.network.info","params":{}}

# 2. 查看eth0接口信息
{"jsonrpc":"2.0","id":2,"method":"sys.network.info","params":{"interface":"eth0"}}

# 3. 测试网络连通性
{"jsonrpc":"2.0","id":3,"method":"sys.network.ping","params":{"host":"www.baidu.com"}}

# 4. 设置静态IP
{"jsonrpc":"2.0","id":4,"method":"sys.network.setStaticIp","params":{
  "interface":"eth0",
  "address":"192.168.2.22",
  "netmask":"255.255.255.0",
  "gateway":"192.168.2.1"
}}
```

### CAN方法

| 方法名 | 参数 | 返回值 | 说明 |
|--------|------|--------|------|
| `can.status` | 无 | `{ok, interface, bitrate, opened, txQueueSize, diagnostic?}` | 获取CAN总线状态 |
| `can.send` | `{id, dataHex, extended?}` | `{ok}` | 发送原始CAN帧 |

**注意**：当继电器控制不生效时，首先调用 `can.status` 检查CAN总线状态。如果 `opened` 为 `false`，表示CAN接口未正确打开，`diagnostic` 字段会提供诊断建议。

### 继电器方法

| 方法名 | 参数 | 返回值 | 说明 |
|--------|------|--------|------|
| `relay.control` | `{node, ch, action}` | `{ok, jobId, queued, txQueueSize, warning?}` | 控制继电器 |
| `relay.query` | `{node, ch}` | `{ok}` | 查询状态 |
| `relay.status` | `{node, ch}` | 通道状态对象（包含online、ageMs、diagnostic?） | 获取通道详情 |
| `relay.statusAll` | `{node}` | 所有通道状态（包含online、ageMs、diagnostic?、txQueueSize?） | 获取节点所有通道 |
| `relay.nodes` | 无 | 节点列表 | 获取所有节点 |
| `relay.queryAll` | 无 | `{ok, queriedDevices, txQueueSize, warning?}` | 批量查询所有设备 |
| `relay.emergencyStop` | 无 | `{ok, stoppedChannels, failedChannels, deviceCount}` | **急停** - 立即停止所有设备的所有通道 |

**参数说明**：
- `node`: 节点ID (1-255)，支持数字或字符串
- `ch`: 通道号 (0-3)，支持数字或字符串
- `action`: 动作 ("stop" / "fwd" / "rev")

**诊断字段**：
- `txQueueSize`: CAN发送队列中待发送的帧数，数值过大表示CAN总线拥堵
- `warning`: 当队列拥堵时返回警告信息
- `diagnostic`: 当设备离线或从未响应时返回诊断信息，帮助排查问题
- `online`: 设备是否在线（30秒内有响应）
- `ageMs`: 设备上次响应距今的毫秒数，-1表示从未响应

**急停方法说明**：

`relay.emergencyStop` 方法用于紧急情况下立即停止所有设备。该方法会遍历所有已注册的继电器设备，向每个设备的所有通道（0-3）发送停止命令。

使用示例：
```bash
# 执行急停
{"jsonrpc":"2.0","id":1,"method":"relay.emergencyStop","params":{}}

# 响应示例
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "ok": true,
    "stoppedChannels": 12,
    "failedChannels": 0,
    "deviceCount": 3,
    "txQueueSize": 12
  }
}
```

### 传感器方法

| 方法名 | 参数 | 返回值 | 说明 |
|--------|------|--------|------|
| `sensor.list` | `{commType?}` | `{ok, sensors, total}` | 获取传感器列表，可按通信类型过滤 |
| `sensor.read` | `{nodeId, sensorType?}` | 传感器信息对象 | 读取指定传感器数据 |

**参数说明**：
- `nodeId`: 传感器节点ID (1-255)
- `commType`: 可选过滤参数，值为 "serial" 或 "can"
- `sensorType`: 可选传感器类型过滤

**使用示例**：
```bash
# 获取所有传感器列表
{"jsonrpc":"2.0","id":1,"method":"sensor.list","params":{}}

# 只获取串口传感器
{"jsonrpc":"2.0","id":2,"method":"sensor.list","params":{"commType":"serial"}}

# 只获取CAN传感器
{"jsonrpc":"2.0","id":3,"method":"sensor.list","params":{"commType":"can"}}

# 读取指定传感器
{"jsonrpc":"2.0","id":4,"method":"sensor.read","params":{"nodeId":10}}
```

### 分组方法

| 方法名 | 参数 | 返回值 | 说明 |
|--------|------|--------|------|
| `group.list` | 无 | 分组列表 | 列出所有分组 |
| `group.get` | `{groupId}` | 分组详情 | 获取分组信息 |
| `group.create` | `{groupId, name}` | `{ok, groupId}` | 创建分组 |
| `group.delete` | `{groupId}` | `{ok}` | 删除分组 |
| `group.addDevice` | `{groupId, node}` | `{ok}` | 添加设备 |
| `group.removeDevice` | `{groupId, node}` | `{ok}` | 移除设备 |
| `group.addChannel` | `{groupId, node, channel}` | `{ok}` | 添加通道 |
| `group.removeChannel` | `{groupId, node, channel}` | `{ok}` | 移除通道 |
| `group.getChannels` | `{groupId}` | 通道列表 | 获取分组通道 |
| `group.control` | `{groupId, ch, action}` | 控制结果 | 分组批量控制 |

### 设备方法

| 方法名 | 参数 | 返回值 | 说明 |
|--------|------|--------|------|
| `device.types` | 无 | 设备类型列表 | 获取支持的设备类型 |
| `device.list` | 无 | 设备列表 | 获取已注册设备 |
| `device.get` | `{nodeId}` | 设备详情 | 获取设备信息 |
| `device.add` | `{nodeId, type, name, ...}` | `{ok, nodeId}` | 添加设备 |
| `device.remove` | `{nodeId}` | `{ok}` | 移除设备 |

### 屏幕方法

| 方法名 | 参数 | 返回值 | 说明 |
|--------|------|--------|------|
| `screen.get` | 无 | 屏幕配置对象 | 获取屏幕配置 |
| `screen.set` | `{brightness?, contrast?, ...}` | `{ok}` | 设置屏幕参数 |

### 控制队列方法

| 方法名 | 参数 | 返回值 | 说明 |
|--------|------|--------|------|
| `control.queue.status` | 无 | 队列状态 | 获取队列状态 |
| `control.queue.result` | `{jobId}` | 任务结果 | 获取任务执行结果 |

### 自动策略方法

#### 定时策略（分组）

| 方法名 | 参数 | 返回值 | 说明 |
|--------|------|--------|------|
| `auto.strategy.list` | 无 | 策略列表 | 列出所有策略 |
| `auto.strategy.create` | 策略配置 | `{ok, id}` | 创建策略 |
| `auto.strategy.delete` | `{id}` | `{ok}` | 删除策略 |
| `auto.strategy.enable` | `{id, enabled}` | `{ok}` | 启用/禁用策略 |
| `auto.strategy.trigger` | `{id}` | `{ok}` | 手动触发策略 |

#### 传感器策略（分组）

| 方法名 | 参数 | 返回值 | 说明 |
|--------|------|--------|------|
| `auto.sensor.list` | 无 | 策略列表 | 列出传感器策略 |
| `auto.sensor.create` | 策略配置 | `{ok, id}` | 创建策略 |
| `auto.sensor.delete` | `{id}` | `{ok}` | 删除策略 |
| `auto.sensor.enable` | `{id, enabled}` | `{ok}` | 启用/禁用策略 |

#### 继电器策略（单个继电器）

| 方法名 | 参数 | 返回值 | 说明 |
|--------|------|--------|------|
| `auto.relay.list` | 无 | 策略列表 | 列出继电器策略 |
| `auto.relay.create` | `{id, name, nodeId, channel, action, intervalSec, ...}` | `{ok, id}` | 创建策略 |
| `auto.relay.delete` | `{id}` | `{ok}` | 删除策略 |
| `auto.relay.enable` | `{id, enabled}` | `{ok}` | 启用/禁用策略 |

#### 传感器触发继电器策略

| 方法名 | 参数 | 返回值 | 说明 |
|--------|------|--------|------|
| `auto.sensorRelay.list` | 无 | 策略列表 | 列出策略 |
| `auto.sensorRelay.create` | 策略配置 | `{ok, id}` | 创建策略 |
| `auto.sensorRelay.delete` | `{id}` | `{ok}` | 删除策略 |
| `auto.sensorRelay.enable` | `{id, enabled}` | `{ok}` | 启用/禁用策略 |

### 配置管理方法

这组方法解决了"Web页面修改无法保存"的问题。

> **问题原因**：之前通过RPC调用创建的设备、分组、策略等配置只保存在内存中，服务重启后会丢失。
>
> **解决方案**：调用 `config.save` 方法将配置持久化到配置文件。

| 方法名 | 参数 | 返回值 | 说明 |
|--------|------|--------|------|
| `config.get` | 无 | 完整配置对象 | 获取当前运行时配置 |
| `config.save` | `{path?}` | `{ok, message}` | **保存配置到文件** |
| `config.reload` | `{path?}` | `{ok, message}` | 从文件重新加载配置 |

**参数说明**：
- `path`: 可选，指定配置文件路径。默认使用服务启动时的配置文件路径

**使用示例**：

```bash
# 1. 创建分组
{"jsonrpc":"2.0","id":1,"method":"group.create","params":{"groupId":1,"name":"测试分组"}}

# 2. 添加设备到分组
{"jsonrpc":"2.0","id":2,"method":"group.addDevice","params":{"groupId":1,"node":1}}

# 3. 保存配置（重要！不调用此方法修改会在重启后丢失）
{"jsonrpc":"2.0","id":3,"method":"config.save","params":{}}

# 4. 查看当前配置
{"jsonrpc":"2.0","id":4,"method":"config.get","params":{}}

# 5. 重新加载配置（会覆盖未保存的修改）
{"jsonrpc":"2.0","id":5,"method":"config.reload","params":{}}
```

---

## 错误码参考

### JSON-RPC 2.0 标准错误

| 错误码 | 说明 |
|--------|------|
| -32700 | JSON解析错误 |
| -32600 | 无效请求 |
| -32601 | 方法不存在 |
| -32602 | 无效参数 |
| -32603 | 内部错误 |

### 应用定义错误

| 错误码 | 说明 |
|--------|------|
| -60000 | 功能未实现 |
| -60001 | 服务器忙 |
| -60002 | 操作超时 |
| -60003 | 权限拒绝 |

### 参数错误

| 错误码 | 说明 |
|--------|------|
| -60010 | 缺少必需参数 |
| -60011 | 参数类型错误 |
| -60012 | 参数值无效 |
| -60013 | 无效操作状态 |

### 串口错误

| 错误码 | 说明 |
|--------|------|
| -60100 | 串口未打开 |
| -60101 | 打开串口失败 |
| -60102 | 串口写入失败 |
| -60103 | 串口读取失败 |

### CAN错误

| 错误码 | 说明 |
|--------|------|
| -60120 | CAN未打开 |
| -60121 | 打开CAN失败 |
| -60122 | CAN写入失败 |
| -60123 | CAN读取失败 |
| -60124 | CAN载荷超过8字节 |
| -60125 | 无效的CAN ID |

---

## 使用示例

### 控制继电器

```json
// 请求
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "relay.control",
  "params": {
    "node": 1,
    "ch": 0,
    "action": "fwd"
  }
}

// 响应
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "ok": true,
    "jobId": "123",
    "queued": false,
    "success": true
  }
}
```

### 创建继电器定时策略

```json
// 请求
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "auto.relay.create",
  "params": {
    "id": 1,
    "name": "定时灌溉",
    "nodeId": 1,
    "channel": 0,
    "action": "fwd",
    "intervalSec": 3600,
    "enabled": true,
    "autoStart": true
  }
}

// 响应
{
  "jsonrpc": "2.0",
  "id": 2,
  "result": {
    "ok": true,
    "id": 1
  }
}
```

**注意**：`channel` 参数支持数字类型（`"channel": 0`）和字符串类型（`"channel": "0"`）两种格式。

---

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0.0 | 2024-01 | 初始版本 |
| 1.0.1 | 2026-01 | 修复：channel参数支持字符串类型 |
| 1.0.2 | 2026-01 | 新增：can.status方法，sys.info增加canOpened/canTxQueueSize字段，改进CAN诊断 |
| 1.0.3 | 2026-01 | 改进：relay.control/status/statusAll/queryAll增加txQueueSize和diagnostic字段，帮助诊断CAN总线拥堵和设备离线问题 |
| 1.0.4 | 2026-01 | 新增：RTC时间管理（sys.time.*）和网络配置（sys.network.*）RPC方法 |
| 1.0.5 | 2026-01 | 新增：配置管理RPC方法（config.get/save/reload），解决"Web页面修改无法保存"问题；改进CAN诊断信息 |
| 1.0.6 | 2026-01 | 新增：急停功能（relay.emergencyStop）和传感器接口（sensor.list/read），支持串口和CAN通讯方式 |

---

*本文档由泛舟RPC服务器开发团队维护。*
