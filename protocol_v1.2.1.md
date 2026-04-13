# 控制板通信协议文档（V1.2.1）

本文档基于当前工程实现（`Core` + `User`）整理，适用于固件 **V1.2.1**。

## 1. 总览

当前支持三类控制接口：

1. 串口命令行（日志口，文本协议）
2. JSON TCP（设备主动连接网关，客户端/服务器模式中的“设备侧客户端”）
3. Modbus TCP（设备监听 `502`，标准 MBAP + PDU）

另外支持 Modbus RTU（UART1/UART2）与 CAN 设置帧，但本文重点放在你当前在用的串口、JSON TCP、Modbus TCP。

## 2. 串口命令协议（CLI）

### 2.1 基本规则

1. 一行一条命令，以 `\r` 或 `\n` 结束。
2. 支持前缀 `/`，例如 `/status`、`/cfg save`。
3. 支持 `CMD ` 前缀，例如 `CMD HELP`。
4. 命令不区分大小写。

### 2.2 常用命令

1. `/help`  
打印帮助。

2. `/ping`  
回复：`[CMD][OK] PONG`

3. `/status`  
打印系统总状态（uptime、IP、relay、电流、故障、队列、监控状态等）。

4. `/current`  
打印 4 路电流与故障位（`phase/over/no`）。

5. `/relay <ch> <state>`  
`ch=0..3`，`state=0|1|2`。

6. `/stopall`  
关闭全部继电器。

### 2.3 配置命令（/cfg）

1. `/cfg`  
查看当前配置。

2. `/cfg addr <0-15>`  
设置设备地址（注意：运行时有效，建议随后保存）。

3. `/cfg baud <125K|250K|500K|1M>`  
设置 CAN 波特率。

4. `/cfg sample <ms>`  
电流采样周期，最小 `50ms`。

5. `/cfg phase <mA>`  
缺相判据阈值（mA）。

6. `/cfg zero <mA>`  
零电流阈值（mA）。

7. `/cfg over <mA>`  
过流阈值（mA）。

8. `/cfg nocur <cycles>`  
“无电流保护”触发所需连续周期数。

9. `/cfg grace <cycles>`  
启动宽限周期数。

10. `/cfg save`  
保存到配置存储（当前实现优先内部 Flash 双槽）。

11. `/cfg reset`  
恢复默认并保存。

### 2.4 EEPROM 诊断命令

1. `/eeprom`
2. `/eeprom scan`
3. `/eeprom addr 0x50~0x57`
4. `/eeprom diag`
5. `/eeprom write_test [addr]`

### 2.5 TCP 网关命令（JSON TCP 目标）

1. `/tcp`  
查看自动连接状态与目标地址。

2. `/tcp set <ip> <port>`  
设置 JSON TCP 网关地址与端口，并自动使能。

3. `/tcp on` / `/tcp off`  
打开/关闭自动连接。

4. `/tcp json` 或 `/tcp json show`  
查看 JSON TCP 配置（enable、target）。

5. `/tcp json on` / `/tcp json off`  
仅开关 JSON TCP 自动连接。

6. `/tcp json set <ip> <port> [on|off]`  
设置 JSON TCP 目标并可选指定使能状态。

### 2.6 SD 日志状态

1. `/sdlog`  
查询 SD 日志开关、当前文件、分片槽位、行数等。

## 3. JSON TCP 协议

## 3.1 连接模型

1. 设备作为 TCP Client 主动连接目标网关（由 `/tcp set` 配置）。
2. 默认端口 `9000`（可改）。
3. 连接成功后先发送 `hello` 事件。
4. 周期发送 `heartbeat` 事件（当前 30s）。
5. 支持命令 JSON 单行收发；建议每条命令以 `\n` 结尾。

## 3.2 上行事件（设备 -> 网关）

1. hello
```json
{"event":"hello","dev":15,"ip":"192.168.137.245"}
```

2. heartbeat
```json
{"event":"heartbeat","dev":15,"tick":31020}
```

## 3.3 下行命令（网关 -> 设备）

1. `ping`
```json
{"id":1,"cmd":"ping"}
```
响应：
```json
{"id":1,"ok":true,"msg":"pong"}
```

2. `status`
```json
{"id":2,"cmd":"status"}
```
响应（示例）：
```json
{"id":2,"ok":true,"data":{"dev":15,"ip":"192.168.137.245","link":1,"up":1,"sample_ms":100,"relay":[0,0,0,0],"current_mA":[0,0,0,0]}}
```

3. `relay.set`
```json
{"id":3,"cmd":"relay.set","args":{"ch":0,"state":1}}
```
响应：
```json
{"id":3,"ok":true}
```

4. `relay.stopall`
```json
{"id":4,"cmd":"relay.stopall"}
```
响应：
```json
{"id":4,"ok":true}
```

5. `cfg.save`
```json
{"id":5,"cmd":"cfg.save"}
```
响应：
```json
{"id":5,"ok":true}
```

## 3.4 JSON 错误响应

1. 非法 JSON：
```json
{"ok":false,"code":"BAD_JSON"}
```

2. 命令字段非法：
```json
{"id":1,"ok":false,"code":"BAD_CMD","msg":"invalid cmd"}
```

3. 参数错误：
```json
{"id":3,"ok":false,"code":"PARAM"}
```

4. 未知命令：
```json
{"id":9,"ok":false,"code":"UNKNOWN"}
```

5. 执行被拒绝（业务层拒绝）：
```json
{"id":3,"ok":false,"code":"REJECTED"}
```

## 4. Modbus TCP 协议

## 4.1 基本规则

1. 端口：`502`
2. 支持功能码：`0x03`、`0x06`、`0x10`
3. Unit ID：需等于设备地址（`1..15`）或广播 `0`
4. 使用标准 MBAP：
   - Transaction ID(2)
   - Protocol ID(2, 固定 0)
   - Length(2)
   - Unit ID(1)
   - PDU(...)

## 4.2 保持寄存器映射

1. `0x0000` `MB_REG_DEVICE_ADDR`：设备地址
2. `0x0001` `MB_REG_COMM_MODE`：通信模式
3. `0x0002` `MB_REG_NETWORK_MODE`：网络模式
4. `0x0010~0x0013`：4 路继电器状态
5. `0x0020~0x0023`：4 路故障位
   - bit0: 缺相
   - bit1: 过流
   - bit2: 无电流
6. `0x0030~0x003B`：12 路电流（单位 `0.01A`）
7. `0x0100~0x0103`：写继电器命令（`0/1/2`）
8. `0x0110`：缺相阈值（`0.01A`）
9. `0x0111`：零电流阈值（`0.001A`）
10. `0x0112`：过流阈值（`0.01A`）
11. `0x0113`：采样周期（ms）

## 4.3 Modbus TCP 十六进制示例

1. 读设备地址（UID=0x0F，读 `0x0000` 长度1）
请求：
```text
00 01 00 00 00 06 0F 03 00 00 00 01
```
响应（示例，地址=15）：
```text
00 01 00 00 00 05 0F 03 02 00 0F
```

2. 写继电器 CH0=1（`0x0100`）
请求：
```text
00 02 00 00 00 06 0F 06 01 00 00 01
```
响应：
```text
00 02 00 00 00 06 0F 06 01 00 00 01
```

3. 再读继电器状态 `0x0010`
请求：
```text
00 03 00 00 00 06 0F 03 00 10 00 01
```
响应（示例）：
```text
00 03 00 00 00 05 0F 03 02 00 01
```

## 4.4 Modbus 异常码

1. `0x01` Illegal Function
2. `0x02` Illegal Data Address
3. `0x03` Illegal Data Value
4. `0x04` Slave Device Failure

## 5. 联调建议（避免“无响应/超时”）

1. Modbus TCP 必须发原始二进制帧，不是 ASCII 文本。
2. Unit ID 要与当前设备地址一致（例如 `15` 就是 `0x0F`）。
3. JSON TCP 建议每条 JSON 以换行结尾；虽然固件有兼容路径，但标准化更稳。
4. JSON TCP 是设备主动连网关，网关端口要先监听。
5. 若出现 JSON 5s/30s 断开，先看网关是否主动关闭、是否收发心跳。
6. 若 `/cfg save` 失败，先检查存储介质和写保护（EEPROM 的 WP、电源/I2C；或内部 Flash 擦写）。

## 6. 版本信息

1. 文档版本：`V1.2.1`
2. 对应工程：`D:/GD32/STM32CMAKE`
3. 主要源文件：
   - `User/common/log.c`
   - `User/tasks/task_tcp_client.c`
   - `User/modules/modbus_proto.h`
   - `User/modules/modbus_proto.c`
   - `User/common/settings.h`
   - `User/common/settings.c`
