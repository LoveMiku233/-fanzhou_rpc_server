# Fanzhou RPC Server 整体代码分析与优化方案（V1.3）

## 1. 文档目的

本文件用于沉淀当前 `fanzhou_rpc_server` 的整体技术现状，明确：

1. 当前系统架构与关键数据流
2. 已发现的主要风险点与性能瓶颈
3. 已完成优化项（V1.3 分支）
4. 后续优化方案与实施优先级（P0/P1/P2）
5. 验证指标与落地执行建议

---

## 2. 系统整体架构分析

### 2.1 核心模块分层

1. 入口与装配层
   - `main.cpp`
   - 职责：加载配置、初始化 `CoreContext`、启动 `JsonRpcServer`（12345）和 `DeviceTcpServer`（9000）、注册 RPC 方法

2. 传输与协议接入层
   - `src/rpc/json_rpc_server.*`：JSON-RPC TCP 服务端
   - `src/rpc/device_tcp_server.*`：F407 板端 TCP Client 接入
   - `src/rpc/json_rpc_dispatcher.*`：方法分发器

3. 业务编排层
   - `src/core/rpc_registry.cpp`：RPC 方法注册与业务入口
   - `src/core/core_context.*`：系统上下文、设备/分组/策略/队列/云协同

4. 设备与通信层
   - `src/device/can/relay_gd427.*`：继电器设备适配（已支持 CAN/TCP Client 双通道）
   - `src/comm/can/*`、`src/device/can/*`：CAN 收发与设备管理

5. 云与平台层
   - `src/cloud/*`：MQTT 多通道、云端上下行、配置同步

### 2.2 关键数据流

1. 控制链路（RPC -> 控制板）
   - `qt_app/web` -> `JsonRpcServer` -> `JsonRpcDispatcher` -> `rpc_registry` -> `CoreContext`/`DeviceTcpServer` -> F407

2. 板端上报链路（控制板 -> RPC）
   - F407(TCP Client) -> `DeviceTcpServer` -> `RelayGd427::onTcpBoardMessage` -> 状态缓存 -> `relay.statusAll/device.list`

3. 策略链路
   - `autoStrategyScheduler` 周期扫描 -> `evaluateConditions` -> `enqueueControl` -> 队列串行执行

---

## 3. 现状问题与风险分析

### 3.1 稳定性风险

1. 报文分片兼容性风险（历史）
   - 旧实现按换行分帧，遇到半包/多行 JSON 易报 `unterminated object`

2. 高并发连接公平性风险
   - 单连接若持续灌入大量请求，会抢占事件循环，影响其他连接响应

3. 策略与控制队列耦合风险
   - 队列积压时策略仍触发，会继续放大积压

### 3.2 性能瓶颈

1. 高频接口重复计算
   - `relay.statusAll`、`device.list` 被前端高频轮询，重复组装 JSON 成本高

2. 缺少方法级可观测性
   - 之前无法快速定位慢方法、异常方法和热点调用

3. 策略热路径开销
   - 生效时间重复解析、调试日志频繁构造、空动作策略仍参与扫描

### 3.3 可维护性问题

1. 优化动作分散在多模块，缺乏统一“性能基线文档”
2. 缺少面向运维的指标化校验方式（延迟/吞吐/错误率）

---

## 4. V1.3 已完成优化（已落地）

### 4.1 传输层优化

1. `DeviceTcpServer` 改为 JSON 对象边界组包
   - 支持半包/粘包/多行 JSON
   - 增加 `rx chunk` 与 `rx message` 日志用于协议排障

2. `JsonRpcServer` 改为流式 JSON 对象组包
   - 不再依赖单纯换行分帧
   - 兼容 TCP 流分片，降低误报 Parse error

3. 单轮处理上限
   - `JsonRpcServer` 每次 `readyRead` 最多处理 64 条请求
   - 防止单连接饿死其他连接

### 4.2 业务层优化

1. 策略调度背压保护
   - 控制队列长度超阈值（200）时暂停策略触发
   - 告警日志限频（10s）

2. 策略热路径降耗
   - 生效时间解析缓存
   - 高频 debug 日志开关化
   - 空动作策略直接跳过
   - 触发最小间隔常量化（10s）

### 4.3 RPC 可观测与缓存优化（第二轮）

1. 新增 RPC 方法统计
   - `rpc.stats`：返回方法调用统计（calls/errors/totalUs/avgUs/maxUs/lastUs）
   - `rpc.stats.reset`：清空统计

2. 高频接口微缓存
   - `relay.statusAll`：节点级 150ms 缓存，支持 `forceRefresh`
   - `device.list`：全量 300ms 缓存，支持 `forceRefresh`
   - 返回 `cached` / `cacheAgeMs`

---

## 5. 优化方案（下一阶段）

## 5.1 P0（高优先级，建议立即执行）

1. 统一缓存策略框架（当前为局部静态缓存）
   - 目标：把 `relay.statusAll/device.list` 的缓存抽为可配置策略（TTL、容量、命中统计）
   - 价值：降低重复代码，便于扩展到 `group.list/sys.dashboard`

2. 队列处理自适应节拍
   - 当前 `kQueueTickMs=500ms` 固定
   - 建议：根据队列积压动态调整 tick（例如 100ms~500ms）
   - 价值：高负载下降低排队延迟，低负载下保持温和节奏

3. 外部命令调用异步化
   - `sys.4g.* / sys.network.*` 存在 `waitForFinished`
   - 建议：迁移为异步任务 + jobId 查询机制，避免阻塞工作线程

## 5.2 P1（中优先级，建议近期执行）

1. 策略“事件触发+低频兜底”模型
   - 当前纯轮询扫描
   - 建议：传感器变化触发评估，辅以低频巡检（例如 5~10s）
   - 价值：降低空轮询 CPU 开销，提高触发实时性

2. 增加 `rpc.stats` 扩展维度
   - 新增：QPS、P95/P99（滑窗近似）、最近错误样本
   - 价值：可快速发现偶发慢调用

3. 连接级限流与黑名单策略
   - 按 IP/连接增加速率限制（请求数、错误数）
   - 价值：防止异常客户端导致整体退化

## 5.3 P2（持续演进）

1. 引入统一性能基准与压测脚本
   - 指标：`p50/p95/p99`、吞吐、错误率、队列积压、缓存命中率

2. 引入模块级熔断与降级
   - 例如云端异常时仅降级云功能，不影响本地控制主链路

3. 日志结构化
   - 关键链路转结构化日志（method、node、latency、errorCode）
   - 便于后续接入日志平台与告警系统

---

## 6. 建议的验收指标

1. 功能正确性
   - TCP Client 模式下 4 块 F407（dev 1~4）可稳定收发
   - `cfg.get/cfg.set`、`relay.set/status` 全部可回包

2. 性能指标（建议目标）
   - 常规控制 RPC：P95 < 50ms（不含板端执行时间）
   - `relay.statusAll` 高频轮询下 CPU 明显低于优化前
   - 队列积压时系统无雪崩，响应可持续

3. 稳定性
   - 24 小时运行无崩溃
   - 无持续性 JSON parse 告警风暴
   - 断线重连后路由恢复正常

---

## 7. 运维与调用建议

1. 观测建议
   - 定期调用 `rpc.stats` 观察热点方法
   - 出现异常时执行 `rpc.stats.reset` 后再采样 1~5 分钟做对比

2. 缓存使用建议
   - 页面自动刷新默认走缓存（提升总体吞吐）
   - 调试场景使用 `forceRefresh=true`

3. 策略建议
   - 大批量动作优先使用批量/优化接口（`controlMulti` 路径）
   - 避免低间隔重复触发同策略

---

## 8. 结论

当前版本已从“可用”进入“可观测、可稳态运行”的阶段，核心收益为：

1. 报文处理更鲁棒（半包/粘包/多行 JSON）
2. 服务端在高频请求下更公平，避免单连接抢占
3. 策略触发具备背压保护，避免积压放大
4. 有了方法级性能观测与高频查询缓存，可持续做数据驱动优化

下一阶段建议按 P0 -> P1 顺序推进，优先完成“异步化阻塞命令 + 队列自适应 + 缓存框架化”。

