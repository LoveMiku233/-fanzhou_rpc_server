# 泛舟 RPC Server 文档

本文档目录记录 `fanzhou_rpc_server` 当前可维护资料。旧的阶段性总结和过期优化计划已清理，保留长期有用的架构、开发、API、云协议和冒烟测试说明。

## 文档索引

| 文档 | 状态 | 用途 |
|---|---|---|
| [ARCHITECTURE.zh.md](./ARCHITECTURE.zh.md) | 当前 | 系统架构、模块关系、主要数据流 |
| [DEVELOPMENT.zh.md](./DEVELOPMENT.zh.md) | 当前 | 构建、调试、开发约定、常见任务 |
| [API_REFERENCE.zh.md](./API_REFERENCE.zh.md) | 保留 | RPC/API 参考，内容较长，修改 RPC 时同步更新 |
| [FANZHOU_CLOUD_PROTOCOL.zh.md](./FANZHOU_CLOUD_PROTOCOL.zh.md) | 保留 | 泛舟云平台 MQTT/场景同步协议 |
| [CLOUD_UPLOAD_FEATURE.md](./CLOUD_UPLOAD_FEATURE.md) | 保留 | 云数据上传配置、UI 和 RPC 说明 |
| [RPC_SMOKE_TCP_V13.md](./RPC_SMOKE_TCP_V13.md) | 当前 | TCP 控制板 V1.3 冒烟测试脚本说明 |

## 工程概览

`fanzhou_rpc_server` 是温室控制系统核心服务，基于 Qt/C++ 实现。服务端提供 JSON-RPC 2.0 接口，管理 CAN/TCP 继电器设备、传感器、设备分组、自动策略、云端 MQTT 同步和本地配置持久化。

相关应用：

| 路径 | 说明 |
|---|---|
| `src/` | 服务端核心代码 |
| `qt_app/` | 1024x600 触屏 HMI 客户端 |
| `test_web/` | Web/Tauri 调试工具 |
| `scripts/` | RPC 冒烟测试脚本 |
| `config/` | 示例运行配置 |

## 当前运行入口

服务端入口：

```text
main.cpp
```

启动后主要动作：

1. 读取 `/var/lib/fanzhou_core/core.json`，失败时写入默认配置。
2. 初始化 `Logger`，输出到 `/var/log/fanzhou_core/core.log`。
3. 初始化 `CoreContext`，创建通讯、设备、MQTT、云同步、策略和传感器运行时对象。
4. 通过 `RpcRegistry` 注册 JSON-RPC 方法。
5. 启动 JSON-RPC 服务，默认端口 `12345`。
6. 启动设备协议 TCP Server，端口 `9000`，用于控制板作为 TCP Client 接入。

## 构建

服务端：

```bash
mkdir -p build
cd build
qmake ../fanzhou_rpc_server.pro
make -j$(nproc)
```

Qt HMI 客户端：

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

## 快速验证

基础 RPC：

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"rpc.ping","params":{}}' | nc localhost 12345
```

TCP 控制板 V1.3 冒烟测试：

```bash
./scripts/rpc_smoke_tcp_v13.sh
```

严格要求 TCP 控制板在线：

```bash
./scripts/rpc_smoke_tcp_v13.sh --strict-connection
```

## 文档维护规则

- RPC 方法新增、改名、参数变化时，同步更新 [API_REFERENCE.zh.md](./API_REFERENCE.zh.md)。
- 模块边界、启动链路、设备接入方式变化时，同步更新 [ARCHITECTURE.zh.md](./ARCHITECTURE.zh.md)。
- 构建、调试命令或开发流程变化时，同步更新 [DEVELOPMENT.zh.md](./DEVELOPMENT.zh.md)。
- 阶段性完成总结、临时优化计划不要长期放在 `docs/` 根目录；需要保留时放到 issue/PR 或归档目录。
