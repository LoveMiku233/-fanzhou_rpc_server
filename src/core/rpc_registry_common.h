/**
 * @file rpc_registry_common.h
 * @brief RpcRegistry 模块间共享常量与小工具
 */

#ifndef FANZHOU_RPC_REGISTRY_COMMON_H
#define FANZHOU_RPC_REGISTRY_COMMON_H

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

namespace fanzhou {
namespace core {

// 通用阈值
constexpr qint64 kOnlineTimeoutMs = 30000;
constexpr int kMaxChannelId = 3;
constexpr int kDefaultChannelCount = 4;
constexpr int kTxQueueCongestionThreshold = 10;
constexpr qint64 kRelayStatusAllCacheMs = 150;
constexpr qint64 kDeviceListCacheMs = 300;

inline QString formatQueueCongestionWarning(int queueSize, const QString &context)
{
    return QStringLiteral("CAN TX queue congested (%1 pending). %2 Check CAN bus connection.")
        .arg(queueSize)
        .arg(context);
}

inline void calcDeviceOnlineStatus(qint64 lastSeenMs, qint64 now, qint64 &outAgeMs,
                                   bool &outOnline)
{
    if (lastSeenMs > 0) {
        outAgeMs = now - lastSeenMs;
        outOnline = (outAgeMs <= kOnlineTimeoutMs);
    } else {
        outAgeMs = -1;
        outOnline = false;
    }
}

inline QJsonObject buildDeviceStatusObject(quint8 node, qint64 ageMs, bool online)
{
    QJsonObject obj;
    obj[QStringLiteral("node")] = static_cast<int>(node);
    obj[QStringLiteral("online")] = online;
    obj[QStringLiteral("ageMs")] = (ageMs >= 0) ? static_cast<double>(ageMs) : QJsonValue();
    return obj;
}

}  // namespace core
}  // namespace fanzhou

#endif  // FANZHOU_RPC_REGISTRY_COMMON_H
