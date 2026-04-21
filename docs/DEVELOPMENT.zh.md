# 开发指南

本文档记录当前工程的构建、调试和开发约定。

## 环境要求

服务端：

- Linux
- Qt 5.12+，模块：`core network mqtt serialport serialbus`
- C++14 编译器
- SocketCAN 工具：`ip`, `candump`, `cansend`

Qt HMI：

- Qt 5.12+
- 模块：`core gui network widgets`
- 目标分辨率：`1024x600`

## 构建命令

服务端：

```bash
mkdir -p build
cd build
qmake ../fanzhou_rpc_server.pro
make -j$(nproc)
```

Qt HMI：

```bash
cd qt_app
mkdir -p build
cd build
qmake ../qt_app.pro
make -j$(nproc)
```

Tauri/Web 调试工具：

```bash
cd test_web
cargo tauri dev
```

## 本地运行

服务端默认读取：

```text
/var/lib/fanzhou_core/core.json
```

首次运行配置不存在时，程序会写入默认配置。默认 JSON-RPC 端口为 `12345`，设备 TCP Server 端口为 `9000`。

基础连通性验证：

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"rpc.ping","params":{}}' | nc localhost 12345
```

列出 RPC：

```bash
echo '{"jsonrpc":"2.0","id":2,"method":"rpc.list","params":{}}' | nc localhost 12345
```

RPC 调用统计：

```bash
echo '{"jsonrpc":"2.0","id":3,"method":"rpc.stats","params":{}}' | nc localhost 12345
```

## CAN 调试

虚拟 CAN：

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
```

真实 CAN：

```bash
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 125000
sudo ip link set can0 up
```

查看状态：

```bash
ip -details link show can0
candump can0
```

RPC 侧检查：

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"can.status","params":{}}' | nc localhost 12345
```

## TCP 控制板调试

服务端启动后监听 `9000`，控制板作为 TCP Client 接入。相关 RPC 方法为 `device.tcp.*`。

冒烟测试：

```bash
./scripts/rpc_smoke_tcp_v13.sh
```

要求控制板必须在线：

```bash
./scripts/rpc_smoke_tcp_v13.sh --strict-connection
```

常用环境变量：

```bash
RPC_HOST=127.0.0.1 \
RPC_PORT=12345 \
DEV_LIST=1,2,3,4 \
RPC_TIMEOUT_SEC=3 \
./scripts/rpc_smoke_tcp_v13.sh
```

## 代码组织

| 目录 | 说明 |
|---|---|
| `src/core` | 核心上下文、配置、RPC 注册 |
| `src/rpc` | JSON-RPC 服务、分发器、TCP 控制板 RPC |
| `src/comm` | CAN、串口通讯适配 |
| `src/device` | 继电器、CAN 设备、串口/Modbus/UART 传感器 |
| `src/cloud` | MQTT 多通道、泛舟云协议、上传和配置同步 |
| `src/types` | 云、通讯、设备、策略、系统类型 |
| `src/utils` | 日志、系统设置、系统监控、USB 监控 |
| `qt_app/src` | Qt HMI 页面和 RPC 客户端 |

## 开发约定

命名：

- 类名：`PascalCase`
- 方法名：`camelCase`
- 成员变量：`camelCase_`
- 常量：`kPascalCase`
- 枚举：`PascalCase`

Qt 字符串：

```cpp
QStringLiteral("text")
```

RPC 方法命名：

```text
模块.动作
```

示例：

```text
rpc.ping
sys.dashboard
relay.control
group.controlOptimized
device.tcp.status
cloud.upload.get
```

## 添加 RPC 方法

1. 在对应 `src/core/rpc_registry_*.cpp` 文件中注册方法。
2. 参数解析使用 `RpcHelpers`。
3. 业务逻辑放在 `CoreContext` 或专门模块中，避免把复杂逻辑塞进注册函数。
4. 更新 [API_REFERENCE.zh.md](./API_REFERENCE.zh.md)。
5. 增加 smoke 脚本覆盖或记录手工验证命令。

典型结构：

```cpp
dispatcher_->registerMethod(QStringLiteral("module.method"),
    [this](const QJsonObject &params) -> QJsonValue {
        qint32 value = 0;
        if (!rpc::RpcHelpers::getI32(params, "value", value)) {
            return rpc::RpcHelpers::err(
                rpc::RpcError::MissingParameter,
                QStringLiteral("missing value"));
        }

        QString error;
        if (!context_->doSomething(value, &error)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, error);
        }

        return QJsonObject{
            {QStringLiteral("ok"), true}
        };
    });
```

## 添加设备或传感器

1. 在 `src/device/device_types.h` 或 `src/types/device_type.h` 定义类型。
2. 在 `src/device/...` 增加驱动类。
3. 在 `CoreContext::init()` 或设备动态添加路径中创建实例。
4. 如需 RPC 管理，更新 `rpc_registry_device.cpp` 或对应注册文件。
5. 更新配置读写逻辑：`src/core/core_config_*`。

## Qt HMI 开发注意事项

- 目标屏幕为 `1024x600`，优先保证现场可读性。
- 页面入口在 `qt_app/src/mainwindow.cpp`。
- RPC 调用封装在 `qt_app/src/rpc_client.*`。
- 新页面应通过 `logMessage` 信号接入主窗口日志和 Toast。
- 危险动作必须保留确认，例如全停、急停、批量控制。

## 文档同步要求

- 架构变化：更新 [ARCHITECTURE.zh.md](./ARCHITECTURE.zh.md)。
- RPC 变化：更新 [API_REFERENCE.zh.md](./API_REFERENCE.zh.md)。
- 云协议变化：更新 [FANZHOU_CLOUD_PROTOCOL.zh.md](./FANZHOU_CLOUD_PROTOCOL.zh.md)。
- 冒烟测试变化：更新 [RPC_SMOKE_TCP_V13.md](./RPC_SMOKE_TCP_V13.md)。

## 提交前检查

```bash
qmake ../fanzhou_rpc_server.pro
make -j$(nproc)
```

如修改 Qt HMI：

```bash
cd qt_app/build
qmake ../qt_app.pro
make -j$(nproc)
```

基础 smoke：

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"rpc.ping","params":{}}' | nc localhost 12345
./scripts/rpc_smoke_tcp_v13.sh
```
