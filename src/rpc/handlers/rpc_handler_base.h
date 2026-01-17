/**
 * @file rpc_handler_base.h
 * @brief RPC处理器基础设施
 *
 * 提供RPC处理器的公共依赖和工具函数。
 * 所有RPC处理器模块都应包含此头文件。
 */

#ifndef FANZHOU_RPC_HANDLER_BASE_H
#define FANZHOU_RPC_HANDLER_BASE_H

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1String>

#include "core/core_context.h"
#include "rpc/json_rpc_dispatcher.h"
#include "rpc/rpc_error_codes.h"
#include "rpc/rpc_helpers.h"

namespace fanzhou {
namespace rpc {

/**
 * @brief RPC处理器公共常量
 *
 * 使用constexpr常量避免运行时字符串分配和线程安全问题。
 */
namespace RpcKeys {

// 通用JSON键常量
constexpr const char* keyOk() { return "ok"; }
constexpr const char* keyCh() { return "ch"; }
constexpr const char* keyChannel() { return "channel"; }
constexpr const char* keyStatusByte() { return "statusByte"; }
constexpr const char* keyCurrentA() { return "currentA"; }
constexpr const char* keyMode() { return "mode"; }
constexpr const char* keyPhaseLost() { return "phaseLost"; }
constexpr const char* keyNode() { return "node"; }
constexpr const char* keyOnline() { return "online"; }
constexpr const char* keyAgeMs() { return "ageMs"; }
constexpr const char* keyChannels() { return "channels"; }
constexpr const char* keyNodes() { return "nodes"; }
constexpr const char* keyJobId() { return "jobId"; }
constexpr const char* keyQueued() { return "queued"; }
constexpr const char* keySuccess() { return "success"; }
constexpr const char* keyGroupId() { return "groupId"; }
constexpr const char* keyName() { return "name"; }
constexpr const char* keyDevices() { return "devices"; }
constexpr const char* keyDeviceCount() { return "deviceCount"; }
constexpr const char* keyGroups() { return "groups"; }
constexpr const char* keyTotal() { return "total"; }
constexpr const char* keyAccepted() { return "accepted"; }
constexpr const char* keyMissing() { return "missing"; }
constexpr const char* keyJobIds() { return "jobIds"; }
constexpr const char* keyPending() { return "pending"; }
constexpr const char* keyActive() { return "active"; }
constexpr const char* keyLastJobId() { return "lastJobId"; }
constexpr const char* keyMessage() { return "message"; }
constexpr const char* keyFinishedMs() { return "finishedMs"; }
constexpr const char* keyId() { return "id"; }
constexpr const char* keyAction() { return "action"; }
constexpr const char* keyIntervalSec() { return "intervalSec"; }
constexpr const char* keyEnabled() { return "enabled"; }
constexpr const char* keyAutoStart() { return "autoStart"; }
constexpr const char* keyAttached() { return "attached"; }
constexpr const char* keyRunning() { return "running"; }
constexpr const char* keyStrategies() { return "strategies"; }

}  // namespace RpcKeys

/**
 * @brief RPC处理器公共常量
 */
namespace RpcConst {

/// 设备在线超时阈值（毫秒）：30秒内收到过响应认为在线
constexpr qint64 kOnlineTimeoutMs = 30000;

/// 最大通道ID（0-3表示4个通道）
constexpr int kMaxChannelId = 3;

/// 默认通道数量（GD427继电器默认4通道）
constexpr int kDefaultChannelCount = 4;

/// CAN TX队列拥堵阈值：超过此数量认为拥堵
constexpr int kTxQueueCongestionThreshold = 10;

}  // namespace RpcConst

/**
 * @brief RPC处理器公共工具函数
 */
namespace RpcUtils {

/**
 * @brief 格式化队列拥堵警告信息
 * @param queueSize 当前队列大小
 * @param context 上下文描述（如 "Queries may be delayed"）
 * @return 格式化的警告字符串
 */
inline QString formatQueueCongestionWarning(int queueSize, const QString &context)
{
    return QStringLiteral("CAN TX queue congested (%1 pending). %2 Check CAN bus connection.")
        .arg(queueSize)
        .arg(context);
}

/**
 * @brief 计算设备在线状态信息
 * @param lastSeenMs 设备最后响应时间戳（毫秒）
 * @param now 当前时间戳（毫秒）
 * @param outAgeMs 输出：设备响应时间间隔（毫秒），-1表示从未响应
 * @param outOnline 输出：设备是否在线
 */
inline void calcDeviceOnlineStatus(qint64 lastSeenMs, qint64 now, qint64 &outAgeMs, bool &outOnline)
{
    if (lastSeenMs > 0) {
        outAgeMs = now - lastSeenMs;
        outOnline = (outAgeMs <= RpcConst::kOnlineTimeoutMs);
    } else {
        outAgeMs = -1;  // 表示从未响应
        outOnline = false;
    }
}

/**
 * @brief 构建设备状态JSON对象
 * @param node 节点ID
 * @param ageMs 设备响应时间间隔（毫秒），-1表示从未响应
 * @param online 设备是否在线
 * @return 包含node、online、ageMs字段的JSON对象
 */
inline QJsonObject buildDeviceStatusObject(quint8 node, qint64 ageMs, bool online)
{
    QJsonObject obj;
    obj[QLatin1String(RpcKeys::keyNode())] = static_cast<int>(node);
    obj[QLatin1String(RpcKeys::keyOnline())] = online;
    // ageMs为-1时表示从未响应，返回null
    obj[QLatin1String(RpcKeys::keyAgeMs())] = (ageMs >= 0) ? static_cast<double>(ageMs) : QJsonValue();
    return obj;
}

}  // namespace RpcUtils

}  // namespace rpc
}  // namespace fanzhou

#endif  // FANZHOU_RPC_HANDLER_BASE_H
