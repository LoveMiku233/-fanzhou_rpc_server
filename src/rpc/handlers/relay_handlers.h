/**
 * @file relay_handlers.h
 * @brief 继电器控制相关RPC处理器
 *
 * 提供继电器设备的控制、查询、状态获取等RPC方法。
 */

#ifndef FANZHOU_RPC_RELAY_HANDLERS_H
#define FANZHOU_RPC_RELAY_HANDLERS_H

namespace fanzhou {

namespace core {
class CoreContext;
}

namespace rpc {

class JsonRpcDispatcher;

/**
 * @brief 注册继电器相关RPC方法
 *
 * 注册的方法包括：
 * - relay.control: 控制继电器
 * - relay.query: 查询继电器状态
 * - relay.status: 获取继电器状态详情
 * - relay.statusAll: 获取继电器所有通道状态
 * - relay.nodes: 获取所有继电器节点列表
 * - relay.queryAll: 批量查询所有设备状态
 * - relay.emergencyStop: 急停所有设备
 * - relay.emergencyStopOptimized: 急停优化版
 * - relay.controlBatch: 批量控制
 * - relay.controlMulti: 多通道控制
 * - relay.queryAllChannels: 查询所有通道
 * - relay.autoStatus: 获取自动状态上报数据
 * - relay.setOvercurrent: 设置过流标志
 * - sensor.read: 读取传感器
 * - sensor.list: 传感器列表
 *
 * @param context 核心上下文
 * @param dispatcher RPC分发器
 */
void registerRelayHandlers(core::CoreContext *context, JsonRpcDispatcher *dispatcher);

}  // namespace rpc
}  // namespace fanzhou

#endif  // FANZHOU_RPC_RELAY_HANDLERS_H
