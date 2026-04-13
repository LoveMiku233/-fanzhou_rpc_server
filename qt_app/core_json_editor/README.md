# Core JSON Editor (Qt)

用于可视化编辑 `core.json`，覆盖以下核心区块：
- `main`（`deviceId`、`rpcPort`）
- `can`
- `devices`
- `groups`（支持通道绑定）

程序会保留未编辑区块（如 `log/cloudUpload/mqttChannels/strategies` 等）原样写回。

## Build

```bash
cd /home/yukino/fanzhou_rpc_server/qt_app/core_json_editor
qmake core_json_editor.pro
make -j$(nproc)
```

## Run

```bash
cd /home/yukino/fanzhou_rpc_server/qt_app/core_json_editor
./core_json_editor
```

## Features

- 新建 / 打开 / 保存 / 另存为
- `devices`、`groups` 表格增删
- `devices.type` 下拉选择（常用设备类型）
- `devices.commType` 下拉选择（Serial/CAN/Modbus/UART/TCP Client）
- `groups.enabled` 下拉选择（true/false）
- 分组通道绑定：`bindings(csv node:ch)`，例如：`1:0,1:1,4:2`
- 内置配置校验（Tools -> Validate）
- 实时 JSON 预览（只读）
- 关闭窗口时未保存保护

## Validation

- `devices.nodeId` 必须 `1..255` 且不重复
- `groups.groupId` 必须 `>0` 且不重复
- `groups.devices` 必须引用存在的设备 `nodeId`
- `groups.bindings` 必须为 `node:channel`，其中 `channel` 为 `0..3`
- 保存时会自动把 `bindings` 转换为 `groups[].channels` 整数编码数组（`node*256+channel`）
