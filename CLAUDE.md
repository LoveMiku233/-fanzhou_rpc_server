# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**FanZhou RPC Server** is a greenhouse control system core service built with Qt framework, designed to run on the Allwinner A133 ARM platform. It provides JSON-RPC 2.0 compatible interfaces for controlling CAN bus relay devices and collecting sensor data via serial/Modbus interfaces, with MQTT cloud integration capabilities.

**Key components**:
- CAN bus communication via SocketCAN
- Serial/Modbus sensor support (Modbus RTU, custom protocols, raw UART)
- JSON-RPC 2.0 TCP server (port 12345 default) with token authentication
- MQTT cloud integration with multiple channel support
- Qt desktop GUI application (`qt_app/`) with auto-connect and cloud status monitoring
- Web-based debug interface (`test_web/`)
- Scene management for FanZhou cloud platform
- Cloud data upload with configurable modes (change-based or interval-based)

## Build System

### Prerequisites
- Qt 5.12+ (Core, Network, MQTT, SerialPort, SerialBus modules)
- GCC 7+ / Clang 6+ with C++17 support
- Linux SocketCAN development libraries
- CAN utilities (candump, cansend) for debugging

### Build Commands

**Core Server**:
```bash
mkdir build && cd build
qmake ../fanzhou_rpc_server.pro
make -j$(nproc)
sudo make install
```

**Cross-compile for A133 Platform**:
```bash
export PATH=/opt/a133-toolchain/bin:$PATH
export CROSS_COMPILE=aarch64-linux-gnu-
/opt/qt-a133/bin/qmake ../fanzhou_rpc_server.pro
make -j$(nproc)
```

**Qt Desktop Application**:
```bash
cd qt_app
mkdir build && cd build
qmake ../qt_app.pro
make -j$(nproc)
```

### Troubleshooting Build Issues
- If you see `undefined reference to vtable` errors, re-run `qmake` to regenerate MOC files: `qmake && make clean && make`

## Code Architecture

### Layered Structure
```
┌─────────────────────────────────────────────────────────┐
│  RPC Layer     - JSON-RPC 2.0 server/dispatcher        │
│  Core Layer    - CoreContext, RpcRegistry               │
│  Device Layer  - Device adapters (relays, sensors)      │
│  Comm Layer    - CanComm, SerialComm (low-level)        │
│  Cloud Layer   - MQTT client, FanZhou cloud protocol    │
└─────────────────────────────────────────────────────────┘
```

### Full System Architecture
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
│   (TCP服务器+认证)  │   (方法分发器)          │   (辅助函数)         │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      核心业务层 (src/core)                        │
├─────────────────────────────────────────────────────────────────┤
│   CoreContext      │   RpcRegistry        │   CoreConfig         │
│   (系统上下文)       │   (RPC方法注册)       │   (配置管理)          │
└─────────────────────────────────────────────────────────────────┘
           ┌──────────────────┼──────────────────┐
           ▼                  ▼                  ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│   设备层          │  │   策略层         │  │   云平台层       │
│   (src/device)   │  │   (strategies)  │  │   (src/cloud)    │
├─────────────────┤  ├─────────────────┤  ├─────────────────┤
│ RelayGd427      │  │ TimerStrategy   │  │ MqttClient       │
│ CanDeviceManager│  │ SensorStrategy  │  │ MqttChannelMgr   │
│ ISensor接口      │  │ SceneManagement │  │ FanZhouCloud     │
│ SerialSensor    │  │                 │  │ Uploader         │
│ ModbusSensor    │  │                 │  │                  │
└─────────────────┘  └─────────────────┘  └─────────────────┘
           │
           ▼
┌─────────────────────────────────────────────────────────────────┐
│                      通信层 (src/comm)                           │
├─────────────────────────────────────────────────────────────────┤
│   CanComm (CAN总线)    │   SerialComm (串口)   │   CommAdapter    │
│   (Linux SocketCAN)    │   (RS485/UART)       │   (抽象接口)       │
└─────────────────────────────────────────────────────────────────┘
```

### Key Files

| File | Purpose |
|------|---------|
| `main.cpp` | Application entry point |
| `fanzhou_rpc_server.pro` | Qt project build configuration |
| `src/core/core_context.h/cpp` | Central hub managing all components |
| `src/core/rpc_registry.cpp` | All RPC method definitions and registration |
| `src/core/core_config.h/cpp` | Configuration structure and loading/saving |
| `src/comm/can/can_comm.h/cpp` | CAN bus communication via SocketCAN |
| `src/device/can/relay_gd427.h/cpp` | GD427 CAN relay device driver |
| `src/rpc/json_rpc_server.h/cpp` | TCP server for JSON-RPC with authentication |
| `src/cloud/mqtt/mqtt_client.h/cpp` | MQTT client implementation |
| `src/cloud/fanzhoucloud/*` | FanZhou cloud protocol implementation |
| `config/config_example.json` | Example configuration file |
| `qt_app/src/mainwindow.cpp` | Qt application main window with auto-connect and cloud status |

### Important Design Decisions

1. **Group by Channel Binding**: Strategies only control channels explicitly bound to a group via `addChannelToGroup`. Channel key = `nodeId * 256 + channel`.

2. **Configuration Persistence**: Changes must be saved with `config.save` RPC call to persist across restarts.

3. **Device Online Detection**: Device considered online if CAN response received within last 30 seconds.

4. **Control Queue**: Control commands are enqueued and processed asynchronously to avoid CAN bus congestion. Queue tick: 500ms.

5. **CAN Bus Auto-Recovery**: System implements automatic recovery for CAN TX buffer full conditions with exponential backoff (10ms → 320ms). After 10 retries at max backoff, frames are dropped to allow recovery.

6. **RPC Authentication**:
   - Local connections (127.0.0.1, ::1, localhost) do not require token authentication
   - Remote connections can be whitelisted in configuration
   - Public methods (rpc.ping, auth.login, etc.) don't require authentication
   - Token-based authentication available for secure remote access

7. **Cloud Data Upload**:
   - Two modes: change-based upload (recommended) and interval-based upload
   - Configurable data properties: channel status, phase loss, current value, online status
   - Change detection thresholds prevent excessive uploads
   - Multiple MQTT channels supported for redundancy

8. **Serial Sensor Framework**:
   - Unified framework supporting Modbus RTU, custom frame protocols, and raw UART
   - Protocol selection via configuration (Modbus/Custom/Raw)
   - Extensible for new sensor types

## Code Conventions

### Naming
```cpp
class ClassName;              // PascalCase for classes
void methodName();            // camelCase for methods
RpcClient *rpcClient_;        // camelCase_ with underscore suffix for members
const QString kKeyOk;         // kPrefix + PascalCase for constants
enum class DeviceTypeId;      // PascalCase for enums
```

### Qt Strings
Use `QStringLiteral` for string literals to avoid runtime construction:
```cpp
return QStringLiteral("missing parameter");  // Preferred
// NOT: return QString("missing parameter");
```

### Adding New RPC Methods

1. Register method in `src/core/rpc_registry.cpp`:
```cpp
dispatcher_->registerMethod(QStringLiteral("module.methodName"),
    [this](const QJsonObject &params) {
        // Parse params with RpcHelpers
        // Call CoreContext business logic
        // Return result or error
    });
```

2. Call registration from `RpcRegistry::registerAll()`

3. Update `docs/API_REFERENCE.zh.md`

### Adding New Sensor Types

1. Implement `ISensor` interface or inherit from `SerialSensor`/`ModbusSensor`
2. Add sensor type to `src/types/device_type.h`
3. Add configuration handling in `CoreConfig`
4. Register RPC methods in `RpcRegistry` if needed

## CAN Bus Debugging

```bash
candump can0                    # Monitor CAN traffic
cansend can0 101#0100000000000000  # Send test frame
ip -details -statistics link show can0  # Check interface status
```

## RPC Debugging

```bash
# Send RPC command via netcat
echo '{"jsonrpc":"2.0","id":1,"method":"rpc.ping","params":{}}' | nc localhost 12345

# Control relay
echo '{"jsonrpc":"2.0","id":2,"method":"relay.control","params":{"node":1,"ch":0,"action":"fwd"}}' | nc localhost 12345
```

## Configuration

- Default config path: `/var/lib/fanzhou_core/core.json`
- Example config: `config/config_example.json`
- Log path: `/var/log/fanzhou_core/core.log` (configurable)

Key config sections: `main`, `log`, `can`, `devices`, `groups`, `strategies`, `cloud`, `cloudUpload`, `mqtt`.

### Cloud Upload Configuration
```json
{
  "cloudUpload": {
    "enabled": false,
    "uploadMode": "change",
    "intervalSec": 60,
    "uploadChannelStatus": true,
    "uploadPhaseLoss": true,
    "uploadCurrent": true,
    "uploadOnlineStatus": true,
    "currentThreshold": 0.1,
    "statusChangeOnly": true,
    "minUploadIntervalSec": 5
  }
}
```

### Authentication Configuration
```json
{
  "main": {
    "auth": {
      "enabled": true,
      "secret": "your-secret-key",
      "tokenExpireSec": 3600,
      "allowedTokens": ["token1", "token2"],
      "whitelist": ["127.0.0.1", "192.168.1.100"],
      "publicMethods": ["rpc.ping", "rpc.list", "auth.login"]
    }
  }
}
```

## Qt Application Features

The Qt desktop application (`qt_app/`) provides:

1. **Auto-connection**: Automatically connects to RPC server on startup (configurable)
2. **Cloud Status Monitor**: Real-time display of MQTT connection status in status bar
3. **Data Upload Configuration**: UI for configuring cloud upload settings (mode, properties, thresholds)
4. **Device Management**: Relay control, group management, strategy configuration
5. **Log Viewing**: Real-time log display with filtering

Recent refactoring (2026-03): The Qt app has been restructured with simplified widget architecture, removing the `views/` directory in favor of direct widget implementations in `src/`.

## Documentation

- `docs/README.zh.md` - User and admin guide
- `docs/ARCHITECTURE.zh.md` - System architecture details
- `docs/DEVELOPMENT.zh.md` - Developer guide with code conventions
- `docs/API_REFERENCE.zh.md` - Complete RPC API documentation
- `docs/FANZHOU_CLOUD_PROTOCOL.zh.md` - Cloud platform protocol spec
- `docs/CLOUD_UPLOAD_FEATURE.md` - Cloud data upload feature documentation
- `docs/IMPLEMENTATION_SUMMARY.md` - Implementation summary for recent features
