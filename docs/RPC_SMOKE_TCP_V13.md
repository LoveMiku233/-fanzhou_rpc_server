# RPC Smoke Test (TCP V1.3)

适用范围：
- RPC 服务监听端口（默认 `12345`）
- F407 TCP JSON 协议 V1.3 相关 RPC 接口
- 覆盖成功/失败两类用例（含参数校验路径）

脚本路径：
- `scripts/rpc_smoke_tcp_v13.sh`

## 1. 快速执行

```bash
cd /home/yukino/fanzhou_rpc_server
./scripts/rpc_smoke_tcp_v13.sh
```

默认行为：
- 先跑基础健康检查和参数校验用例（不依赖板子在线）
- 再检查 `device.tcp.status`
- 若无 TCP 控制板连接，则跳过 live 命令测试（`SKIP`）

## 2. 严格连接模式

若你希望“未连接板子”也判失败：

```bash
./scripts/rpc_smoke_tcp_v13.sh --strict-connection
```

## 3. 常用环境变量

```bash
RPC_HOST=127.0.0.1 \
RPC_PORT=12345 \
TARGET_DEV=1 \
DEV_LIST=1,2,3,4 \
RPC_TIMEOUT_SEC=3 \
./scripts/rpc_smoke_tcp_v13.sh
```

如启用认证，可附带：

```bash
AUTH_TOKEN=your_token ./scripts/rpc_smoke_tcp_v13.sh
```

## 4. 当前覆盖点

- `rpc.ping`
- `rpc.list`（包含 `device.tcp.ping`）
- `device.tcp.relay.set` 非法参数（`ch` 越界）
- `device.tcp.cfg.set` 非法参数（`comm_mode` 越界）
- `relay.controlBatch` strict 模式非法项
- `device.tcp.status`（连接状态）
- `device.tcp.ping`（按 `DEV_LIST` 逐设备执行）
- `device.tcp.status.get`（按 `DEV_LIST` 逐设备执行）
- `device.tcp.cfg.get`（按 `DEV_LIST` 逐设备执行）

默认 `DEV_LIST=1,2,3,4`，适配你当前 4 块 TCP 控制板场景。

## 5. 结果判定

- 输出包含：`[PASS] [FAIL] [SKIP]`
- 末尾总结：`Summary: PASS=x FAIL=y SKIP=z`
- 退出码：
  - `0`：无失败
  - `1`：有失败
