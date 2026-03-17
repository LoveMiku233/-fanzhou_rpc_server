# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**FanZhou RPC Server** is a greenhouse control system core service built with Qt framework, designed to run on the Allwinner A133 ARM platform. It provides JSON-RPC 2.0 compatible interfaces for controlling CAN bus relay devices and collecting sensor data via serial/Modbus interfaces, with MQTT cloud integration capabilities.

**Key components**:
- CAN bus communication via SocketCAN
- Serial/Modbus sensor support
- JSON-RPC 2.0 TCP server (port 12345 default)
- MQTT cloud integration
- Qt desktop GUI application (`qt_app/`)
- Web-based debug interface (`test_web/`)

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
└─────────────────────────────────────────────────────────┘
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
| `src/rpc/json_rpc_server.h/cpp` | TCP server for JSON-RPC |
| `config/config_example.json` | Example configuration file |

### Important Design Decisions

1. **Group by Channel Binding**: Strategies only control channels explicitly bound to a group via `addChannelToGroup`. Channel key = `nodeId * 256 + channel`.

2. **Configuration Persistence**: Changes must be saved with `config.save` RPC call to persist across restarts.

3. **Device Online Detection**: Device considered online if CAN response received within last 30 seconds.

4. **Control Queue**: Control commands are enqueued and processed asynchronously to avoid CAN bus congestion. Queue tick: 500ms.

5. **CAN Bus Auto-Recovery**: System implements automatic recovery for CAN TX buffer full conditions with exponential backoff (10ms → 320ms). After 10 retries at max backoff, frames are dropped to allow recovery.

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

### CAN Bus Debugging

```bash
candump can0                    # Monitor CAN traffic
cansend can0 101#0100000000000000  # Send test frame
ip -details -statistics link show can0  # Check interface status
```

### RPC Debugging

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

Key config sections: `main`, `log`, `can`, `devices`, `groups`, `strategies`.

## Documentation

- `docs/README.zh.md` - User and admin guide
- `docs/DEVELOPMENT.zh.md` - Developer guide with code conventions
- `docs/API_REFERENCE.zh.md` - Complete RPC API documentation
- `docs/FANZHOU_CLOUD_PROTOCOL.zh.md` - Cloud platform protocol spec
