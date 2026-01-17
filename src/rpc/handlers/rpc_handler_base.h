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

#include "core/core_context.h"
#include "rpc/json_rpc_dispatcher.h"
#include "rpc/rpc_error_codes.h"
#include "rpc/rpc_helpers.h"

namespace fanzhou {
namespace rpc {

/**
 * @brief RPC处理器公共常量
 */
namespace RpcKeys {

// 通用JSON键 - 避免重复的QStringLiteral分配
inline const QString &keyOk() { static const QString k = QStringLiteral("ok"); return k; }
inline const QString &keyCh() { static const QString k = QStringLiteral("ch"); return k; }
inline const QString &keyChannel() { static const QString k = QStringLiteral("channel"); return k; }
inline const QString &keyStatusByte() { static const QString k = QStringLiteral("statusByte"); return k; }
inline const QString &keyCurrentA() { static const QString k = QStringLiteral("currentA"); return k; }
inline const QString &keyMode() { static const QString k = QStringLiteral("mode"); return k; }
inline const QString &keyPhaseLost() { static const QString k = QStringLiteral("phaseLost"); return k; }
inline const QString &keyNode() { static const QString k = QStringLiteral("node"); return k; }
inline const QString &keyOnline() { static const QString k = QStringLiteral("online"); return k; }
inline const QString &keyAgeMs() { static const QString k = QStringLiteral("ageMs"); return k; }
inline const QString &keyChannels() { static const QString k = QStringLiteral("channels"); return k; }
inline const QString &keyNodes() { static const QString k = QStringLiteral("nodes"); return k; }
inline const QString &keyJobId() { static const QString k = QStringLiteral("jobId"); return k; }
inline const QString &keyQueued() { static const QString k = QStringLiteral("queued"); return k; }
inline const QString &keySuccess() { static const QString k = QStringLiteral("success"); return k; }
inline const QString &keyGroupId() { static const QString k = QStringLiteral("groupId"); return k; }
inline const QString &keyName() { static const QString k = QStringLiteral("name"); return k; }
inline const QString &keyDevices() { static const QString k = QStringLiteral("devices"); return k; }
inline const QString &keyDeviceCount() { static const QString k = QStringLiteral("deviceCount"); return k; }
inline const QString &keyGroups() { static const QString k = QStringLiteral("groups"); return k; }
inline const QString &keyTotal() { static const QString k = QStringLiteral("total"); return k; }
inline const QString &keyAccepted() { static const QString k = QStringLiteral("accepted"); return k; }
inline const QString &keyMissing() { static const QString k = QStringLiteral("missing"); return k; }
inline const QString &keyJobIds() { static const QString k = QStringLiteral("jobIds"); return k; }
inline const QString &keyPending() { static const QString k = QStringLiteral("pending"); return k; }
inline const QString &keyActive() { static const QString k = QStringLiteral("active"); return k; }
inline const QString &keyLastJobId() { static const QString k = QStringLiteral("lastJobId"); return k; }
inline const QString &keyMessage() { static const QString k = QStringLiteral("message"); return k; }
inline const QString &keyFinishedMs() { static const QString k = QStringLiteral("finishedMs"); return k; }
inline const QString &keyId() { static const QString k = QStringLiteral("id"); return k; }
inline const QString &keyAction() { static const QString k = QStringLiteral("action"); return k; }
inline const QString &keyIntervalSec() { static const QString k = QStringLiteral("intervalSec"); return k; }
inline const QString &keyEnabled() { static const QString k = QStringLiteral("enabled"); return k; }
inline const QString &keyAutoStart() { static const QString k = QStringLiteral("autoStart"); return k; }
inline const QString &keyAttached() { static const QString k = QStringLiteral("attached"); return k; }
inline const QString &keyRunning() { static const QString k = QStringLiteral("running"); return k; }
inline const QString &keyStrategies() { static const QString k = QStringLiteral("strategies"); return k; }

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
    obj[RpcKeys::keyNode()] = static_cast<int>(node);
    obj[RpcKeys::keyOnline()] = online;
    // ageMs为-1时表示从未响应，返回null
    obj[RpcKeys::keyAgeMs()] = (ageMs >= 0) ? static_cast<double>(ageMs) : QJsonValue();
    return obj;
}

}  // namespace RpcUtils

}  // namespace rpc
}  // namespace fanzhou

#endif  // FANZHOU_RPC_HANDLER_BASE_H
