/**
 * @file group_handlers.h
 * @brief 设备分组相关RPC处理器
 *
 * 提供设备分组的管理和控制RPC方法。
 */

#ifndef FANZHOU_RPC_GROUP_HANDLERS_H
#define FANZHOU_RPC_GROUP_HANDLERS_H

namespace fanzhou {

namespace core {
class CoreContext;
}

namespace rpc {

class JsonRpcDispatcher;

/**
 * @brief 注册设备分组相关RPC方法
 *
 * 注册的方法包括：
 * - group.list: 获取分组列表
 * - group.get: 获取分组详情
 * - group.create: 创建分组
 * - group.delete: 删除分组
 * - group.addDevice: 添加设备到分组
 * - group.removeDevice: 从分组移除设备
 * - group.addChannel: 添加通道到分组
 * - group.removeChannel: 从分组移除通道
 * - group.control: 分组控制
 * - group.controlOptimized: 分组控制优化版
 * - control.queue: 获取队列状态
 * - control.job: 获取任务结果
 *
 * @param context 核心上下文
 * @param dispatcher RPC分发器
 */
void registerGroupHandlers(core::CoreContext *context, JsonRpcDispatcher *dispatcher);

}  // namespace rpc
}  // namespace fanzhou

#endif  // FANZHOU_RPC_GROUP_HANDLERS_H
