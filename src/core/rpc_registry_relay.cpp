/**
 * @file rpc_registry_relay.cpp
 * @brief 继电器与传感器相关RPC方法注册
 */

#include "rpc_registry.h"
#include "core_context.h"
#include "rpc_registry_common.h"
#include "rpc_registry_keys.h"

#include "comm/can/can_comm.h"
#include "device/can/relay_gd427.h"
#include "device/can/relay_protocol.h"
#include "device/device_types.h"
#include "rpc/json_rpc_dispatcher.h"
#include "rpc/rpc_error_codes.h"
#include "rpc/rpc_helpers.h"
#include "utils/logger.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QVariant>
#include <climits>

namespace fanzhou {
namespace core {

namespace {
const QString &kKeyOk = rpc_keys::Ok();
const QString &kKeyCh = rpc_keys::Ch();
const QString &kKeyChannel = rpc_keys::Channel();
const QString &kKeyStatusByte = rpc_keys::StatusByte();
const QString &kKeyCurrentA = rpc_keys::CurrentA();
const QString &kKeyMode = rpc_keys::Mode();
const QString &kKeyPhaseLost = rpc_keys::PhaseLost();
const QString &kKeyNode = rpc_keys::Node();
const QString &kKeyOnline = rpc_keys::Online();
const QString &kKeyAgeMs = rpc_keys::AgeMs();
const QString &kKeyChannels = rpc_keys::Channels();
const QString &kKeyNodes = rpc_keys::Nodes();
const QString &kKeyJobId = rpc_keys::JobId();
const QString &kKeyQueued = rpc_keys::Queued();
const QString &kKeySuccess = rpc_keys::Success();
const QString &kKeyName = rpc_keys::Name();
const QString &kKeyTotal = rpc_keys::Total();
const QString &kKeyAccepted = rpc_keys::Accepted();
const QString &kKeyMissing = rpc_keys::Missing();
const QString &kKeyJobIds = rpc_keys::JobIds();
const QString &kKeyAction = rpc_keys::Action();
const QString &kKeyEnabled = rpc_keys::Enabled();

bool parseBatchControlCommand(const QJsonObject &cmd, CoreContext *context,
                              BatchControlItem &item, QString &error)
{
    quint8 node = 0;
    quint8 channel = 0;
    QString actionStr;

    if (!rpc::RpcHelpers::getU8InRange(cmd, "node", node, 1, 255)) {
        error = QStringLiteral("missing/invalid node (1..255)");
        return false;
    }
    if (!rpc::RpcHelpers::getU8InRange(cmd, "ch", channel, 0, kMaxChannelId)) {
        error = QStringLiteral("missing/invalid ch (0..%1)").arg(kMaxChannelId);
        return false;
    }
    if (!rpc::RpcHelpers::getString(cmd, "action", actionStr)) {
        error = QStringLiteral("missing action");
        return false;
    }

    bool okAction = false;
    const auto action = context->parseAction(actionStr, &okAction);
    if (!okAction) {
        error = QStringLiteral("invalid action (stop/fwd/rev)");
        return false;
    }

    item.node = node;
    item.channel = channel;
    item.action = action;
    return true;
}

}  // namespace

void RpcRegistry::registerRelay()
{
    dispatcher_->registerMethod(QStringLiteral("relay.control"),
                                 [this](const QJsonObject &params) {
        quint8 node = 0, channel = 0;
        QString actionStr;

        if (!rpc::RpcHelpers::getU8InRange(params, "node", node, 1, 255))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid node"));
        if (!rpc::RpcHelpers::getU8InRange(params, "ch", channel, 0, 3))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("missing/invalid ch(0..3)"));
        if (!rpc::RpcHelpers::getString(params, "action", actionStr))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing action"));

        bool okAction = false;
        const auto action = context_->parseAction(actionStr, &okAction);
        if (!okAction)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("invalid action (stop/fwd/rev)"));

        const auto result = context_->enqueueControl(node, channel, action, QStringLiteral("rpc:relay.control"));
        if (!result.accepted) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, result.error);
        }

        QJsonObject obj{
            {kKeyOk, true},
            {kKeyJobId, QString::number(result.jobId)},
            {kKeyQueued, !result.executedImmediately}
        };
        if (result.executedImmediately) {
            obj[kKeySuccess] = result.success;
        }

        // Add CAN TX queue status for diagnostics - helps client detect congestion
        if (context_->canBus) {
            const int txQueueSize = context_->canBus->txQueueSize();
            obj[QStringLiteral("txQueueSize")] = txQueueSize;
            if (txQueueSize > kTxQueueCongestionThreshold) {
                obj[QStringLiteral("warning")] = formatQueueCongestionWarning(txQueueSize, QString());
            }
        }

        return obj;
    });

    dispatcher_->registerMethod(QStringLiteral("relay.query"),
                                 [this](const QJsonObject &params) {
        quint8 node = 0, channel = 0;

        if (!rpc::RpcHelpers::getU8InRange(params, "node", node, 1, 255))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid node"));
        if (!rpc::RpcHelpers::getU8InRange(params, "ch", channel, 0, 3))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("missing/invalid ch(0..3)"));

        auto *dev = context_->relays.value(node, nullptr);
        if (!dev)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("unknown node"));

        return QJsonObject{{kKeyOk, dev->query(channel)}};
    });

    dispatcher_->registerMethod(QStringLiteral("relay.status"),
                                 [this](const QJsonObject &params) {
        quint8 node = 0, channel = 0;

        if (!rpc::RpcHelpers::getU8InRange(params, "node", node, 1, 255))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid node"));
        if (!rpc::RpcHelpers::getU8InRange(params, "ch", channel, 0, 3))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("missing/invalid ch(0..3)"));

        auto *dev = context_->relays.value(node, nullptr);
        if (!dev)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("unknown node"));

        const auto status = dev->lastStatus(channel);
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const qint64 lastSeen = dev->lastSeenMs();
        qint64 ageMs = 0;
        bool online = false;
        calcDeviceOnlineStatus(lastSeen, now, ageMs, online);

        QJsonObject result{
            {kKeyOk, true},
            {kKeyChannel, static_cast<int>(status.channel)},
            {kKeyStatusByte, static_cast<int>(status.statusByte)},
            {kKeyCurrentA, static_cast<double>(status.currentA)},
            {kKeyMode, static_cast<int>(device::RelayProtocol::modeBits(status.statusByte))},
            {kKeyPhaseLost, device::RelayProtocol::phaseLost(status.statusByte)},
            {kKeyOnline, online},
            {kKeyAgeMs, (ageMs >= 0) ? static_cast<double>(ageMs) : QJsonValue()}
        };

        // Add diagnostic info when device is offline (consistent with relay.statusAll)
        if (!online) {
            if (lastSeen == 0) {
                result[QStringLiteral("diagnostic")] = QStringLiteral(
                    "Device never responded. Status values are defaults.");
            } else {
                result[QStringLiteral("diagnostic")] = QStringLiteral(
                    "Device offline (last seen %1ms ago). Status may be stale.").arg(ageMs);
            }
        }

        return result;
    });

    dispatcher_->registerMethod(QStringLiteral("relay.statusAll"),
                                 [this](const QJsonObject &params) {
        quint8 node = 0;
        if (!rpc::RpcHelpers::getU8InRange(params, "node", node, 1, 255))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid node"));

        auto *dev = context_->relays.value(node, nullptr);
        if (!dev)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("unknown node"));

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const bool forceRefresh = params.value(QStringLiteral("forceRefresh")).toBool(false);
        static QHash<int, QPair<qint64, QJsonObject>> cache;
        if (!forceRefresh) {
            const auto it = cache.constFind(static_cast<int>(node));
            if (it != cache.constEnd()) {
                const qint64 age = now - it.value().first;
                if (age >= 0 && age <= kRelayStatusAllCacheMs) {
                    QJsonObject cached = it.value().second;
                    cached[QStringLiteral("cached")] = true;
                    cached[QStringLiteral("cacheAgeMs")] = static_cast<double>(age);
                    return cached;
                }
            }
        }

        QJsonObject channels;
        double totalCurrent = 0.0;
        for (quint8 ch = 0; ch < 4; ++ch) {
            const auto status = dev->lastStatus(ch);
            QJsonObject obj;
            obj[kKeyCh] = static_cast<int>(ch);
            obj[kKeyChannel] = static_cast<int>(status.channel);
            obj[kKeyStatusByte] = static_cast<int>(status.statusByte);
            obj[kKeyCurrentA] = static_cast<double>(status.currentA);
            obj[kKeyMode] = static_cast<int>(device::RelayProtocol::modeBits(status.statusByte));
            obj[kKeyPhaseLost] = device::RelayProtocol::phaseLost(status.statusByte);
            obj[QStringLiteral("current")] = static_cast<double>(status.currentA) * 1000.0;
            channels[QString::number(ch)] = obj;
            totalCurrent += static_cast<double>(status.currentA) * 1000.0;
        }

        const qint64 lastSeen = dev->lastSeenMs();
        qint64 ageMs = 0;
        bool online = false;
        calcDeviceOnlineStatus(lastSeen, now, ageMs, online);

        QJsonObject result{
            {kKeyOk, true},
            {kKeyNode, static_cast<int>(node)},
            {kKeyOnline, online},
            {kKeyAgeMs, (ageMs >= 0) ? static_cast<double>(ageMs) : QJsonValue()},
            {kKeyChannels, channels},
            {QStringLiteral("totalCurrent"), totalCurrent},
            {QStringLiteral("cached"), false}
        };

        // Add diagnostic info when device is offline or never responded
        if (!online) {
            QString diagnostic;
            if (lastSeen == 0) {
                diagnostic = QStringLiteral(
                    "Device never responded. Status values are defaults. "
                    "Check: 1) CAN bus connection 2) Device power 3) Node ID 4) Bitrate");
            } else {
                diagnostic = QStringLiteral(
                    "Device offline (last seen %1ms ago). Status may be stale. "
                    "Check CAN bus connection.").arg(ageMs);
            }
            result[QStringLiteral("diagnostic")] = diagnostic;

            // Also add CAN TX queue size to help diagnose transmission issues
            if (context_->canBus) {
                result[QStringLiteral("txQueueSize")] = context_->canBus->txQueueSize();
            }
        }

        cache[static_cast<int>(node)] = qMakePair(now, result);
        return result;
    });

    // 获取所有继电器节点列表，包含在线状态（按节点ID排序）
    dispatcher_->registerMethod(QStringLiteral("relay.nodes"),
                                 [this](const QJsonObject &) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        
        // 获取所有节点ID并排序
        QList<quint8> nodeIds = context_->relays.keys();
        std::sort(nodeIds.begin(), nodeIds.end());
        
        QJsonArray arr;
        for (quint8 node : nodeIds) {
            auto *dev = context_->relays.value(node, nullptr);
            const qint64 lastSeen = dev ? dev->lastSeenMs() : 0;
            
            qint64 ageMs = 0;
            bool online = false;
            calcDeviceOnlineStatus(lastSeen, now, ageMs, online);
            
            arr.append(buildDeviceStatusObject(node, ageMs, online));
        }
        return QJsonObject{{kKeyOk, true}, {kKeyNodes, arr}};
    });

    // 批量查询所有设备状态
    dispatcher_->registerMethod(QStringLiteral("relay.queryAll"),
                                 [this](const QJsonObject &) {
        int queriedCount = 0;
        for (auto it = context_->relays.begin(); it != context_->relays.end(); ++it) {
            auto *dev = it.value();
            if (dev) {
                // 查询每个设备的所有通道
                for (quint8 ch = 0; ch < 4; ++ch) {
                    dev->query(ch);
                }
                queriedCount++;
            }
        }

        QJsonObject result{
            {kKeyOk, true},
            {QStringLiteral("queriedDevices"), queriedCount}
        };

        // Add CAN TX queue size for diagnostics
        if (context_->canBus) {
            const int txQueueSize = context_->canBus->txQueueSize();
            result[QStringLiteral("txQueueSize")] = txQueueSize;
            if (txQueueSize > kTxQueueCongestionThreshold) {
                result[QStringLiteral("warning")] = formatQueueCongestionWarning(
                    txQueueSize, QStringLiteral("Queries may be delayed."));
            }
        }

        return result;
    });

    // 急停 - 立即停止所有设备的所有通道
    dispatcher_->registerMethod(QStringLiteral("relay.emergencyStop"),
                                 [this](const QJsonObject &) {
        int stoppedCount = 0;
        int failedCount = 0;
        QJsonArray failedNodes;

        // 遍历所有继电器设备
        for (auto it = context_->relays.begin(); it != context_->relays.end(); ++it) {
            quint8 node = it.key();
            auto *dev = it.value();
            if (!dev) continue;

            // 停止每个设备的所有通道
            for (quint8 ch = 0; ch < kDefaultChannelCount; ++ch) {
                const auto result = context_->enqueueControl(
                    node, ch,
                    device::RelayProtocol::Action::Stop,
                    QStringLiteral("rpc:relay.emergencyStop"),
                    false  // 不强制排队，立即执行
                );
                if (result.accepted) {
                    stoppedCount++;
                } else {
                    failedCount++;
                    failedNodes.append(static_cast<int>(node));
                }
            }
        }

        QJsonObject result{
            {kKeyOk, true},
            {QStringLiteral("stoppedChannels"), stoppedCount},
            {QStringLiteral("failedChannels"), failedCount},
            {rpc_keys::DeviceCount(), context_->relays.size()}
        };

        if (failedCount > 0) {
            result[QStringLiteral("failedNodes")] = failedNodes;
        }

        // 添加CAN TX队列大小用于诊断
        if (context_->canBus) {
            const int txQueueSize = context_->canBus->txQueueSize();
            result[QStringLiteral("txQueueSize")] = txQueueSize;
        }

        return result;
    });

    // 急停优化版 - 使用controlMulti合并多通道控制，减少CAN帧数
    dispatcher_->registerMethod(QStringLiteral("relay.emergencyStopOptimized"),
                                 [this](const QJsonObject &) {
        int stoppedDevices = 0;
        int stoppedChannels = 0;
        int failedCount = 0;
        int originalFrames = 0;
        int optimizedFrames = 0;

        // 所有通道全部停止
        device::RelayProtocol::Action stopActions[4] = {
            device::RelayProtocol::Action::Stop,
            device::RelayProtocol::Action::Stop,
            device::RelayProtocol::Action::Stop,
            device::RelayProtocol::Action::Stop
        };

        // 遍历所有继电器设备
        for (auto it = context_->relays.begin(); it != context_->relays.end(); ++it) {
            auto *dev = it.value();
            if (!dev) continue;

            originalFrames += kDefaultChannelCount;  // 优化前需要4帧

            // 使用controlMulti一次性停止所有通道
            const bool ok = dev->controlMulti(stopActions);
            optimizedFrames++;  // 优化后只需1帧

            if (ok) {
                stoppedDevices++;
                stoppedChannels += kDefaultChannelCount;
            } else {
                failedCount++;
            }
        }

        QJsonObject result{
            {kKeyOk, true},
            {QStringLiteral("stoppedDevices"), stoppedDevices},
            {QStringLiteral("stoppedChannels"), stoppedChannels},
            {QStringLiteral("failedDevices"), failedCount},
            {rpc_keys::DeviceCount(), context_->relays.size()},
            {QStringLiteral("originalFrames"), originalFrames},
            {QStringLiteral("optimizedFrames"), optimizedFrames},
            {QStringLiteral("framesSaved"), originalFrames - optimizedFrames}
        };

        if (context_->canBus) {
            result[QStringLiteral("txQueueSize")] = context_->canBus->txQueueSize();
        }

        return result;
    });

    // 批量控制 - 一次调用控制多个节点/通道，自动合并优化
    dispatcher_->registerMethod(QStringLiteral("relay.controlBatch"),
                                 [this](const QJsonObject &params) {
        // 参数格式：{"commands": [{"node": 1, "ch": 0, "action": "fwd"}, ...]}
        if (!params.contains(QStringLiteral("commands")) ||
            !params[QStringLiteral("commands")].isArray()) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                QStringLiteral("missing commands array"));
        }
        bool strict = false;
        if (params.contains(QStringLiteral("strict")) &&
            !rpc::RpcHelpers::getBool(params, "strict", strict, false)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue,
                                        QStringLiteral("invalid strict (bool)"));
        }

        const QJsonArray commands = params[QStringLiteral("commands")].toArray();
        QList<BatchControlItem> items;
        QJsonArray rejected;
        QString firstRejectReason;
        int firstRejectIndex = -1;
        int index = 0;

        for (const auto &cmdVal : commands) {
            BatchControlItem item;
            QString error;
            if (!cmdVal.isObject()) {
                error = QStringLiteral("command is not object");
            } else if (parseBatchControlCommand(cmdVal.toObject(), context_, item, error)) {
                items.append(item);
                ++index;
                continue;
            }

            if (firstRejectIndex < 0) {
                firstRejectIndex = index;
                firstRejectReason = error;
            }
            rejected.append(QJsonObject{
                {QStringLiteral("index"), index},
                {QStringLiteral("error"), error}
            });
            ++index;
        }

        if (strict && !rejected.isEmpty()) {
            return rpc::RpcHelpers::err(
                rpc::RpcError::BadParameterValue,
                QStringLiteral("invalid command at index %1: %2")
                    .arg(firstRejectIndex)
                    .arg(firstRejectReason));
        }

        if (items.isEmpty()) {
            if (firstRejectIndex >= 0) {
                return rpc::RpcHelpers::err(
                    rpc::RpcError::BadParameterValue,
                    QStringLiteral("no valid commands; first invalid index %1: %2")
                        .arg(firstRejectIndex)
                        .arg(firstRejectReason));
            }
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue,
                                        QStringLiteral("no valid commands"));
        }

        // 执行批量控制
        const auto result = context_->batchControl(items, QStringLiteral("rpc:relay.controlBatch"));

        QJsonArray jobIds;
        for (quint64 id : result.jobIds) {
            jobIds.append(QString::number(id));
        }

        QJsonObject response{
            {kKeyOk, result.ok},
            {kKeyTotal, result.total},
            {kKeyAccepted, result.accepted},
            {QStringLiteral("failed"), result.failed},
            {QStringLiteral("originalFrames"), result.originalFrames},
            {QStringLiteral("optimizedFrames"), result.optimizedFrames},
            {QStringLiteral("framesSaved"), result.originalFrames - result.optimizedFrames},
            {kKeyJobIds, jobIds},
            {QStringLiteral("requested"), commands.size()},
            {QStringLiteral("parsed"), items.size()},
            {QStringLiteral("rejected"), rejected.size()}
        };
        if (!rejected.isEmpty()) {
            response[QStringLiteral("rejectedCommands")] = rejected;
        }

        if (context_->canBus) {
            response[QStringLiteral("txQueueSize")] = context_->canBus->txQueueSize();
        }

        return response;
    });

    // 分组控制优化版 - 自动合并同一节点的多通道控制
    dispatcher_->registerMethod(QStringLiteral("group.controlOptimized"),
                                 [this](const QJsonObject &params) {
        qint32 groupId = 0;
        qint32 channel = -1;  // 默认-1表示使用分组绑定的通道
        QString actionStr;

        if (!rpc::RpcHelpers::getI32InRange(params, "groupId", groupId, 1, INT_MAX))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing groupId"));
        rpc::RpcHelpers::getI32(params, "ch", channel);
        if (channel < -1 || channel > kMaxChannelId)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, 
                QStringLiteral("invalid ch (-1 for bound channels, or 0-%1)").arg(kMaxChannelId));
        if (!rpc::RpcHelpers::getString(params, "action", actionStr))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing action"));

        bool okAction = false;
        const auto action = context_->parseAction(actionStr, &okAction);
        if (!okAction)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("invalid action (stop/fwd/rev)"));

        if (!context_->deviceGroups.contains(groupId))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("group not found"));

        // 使用优化版分组控制
        const auto stats = context_->queueGroupControlOptimized(groupId, channel, action, 
            QStringLiteral("rpc:group.controlOptimized"));

        QJsonArray jobs;
        for (quint64 id : stats.jobIds) {
            jobs.append(QString::number(id));
        }

        QJsonObject result{
            {kKeyOk, true},
            {kKeyTotal, stats.total},
            {kKeyAccepted, stats.accepted},
            {kKeyMissing, stats.missing},
            {kKeyJobIds, jobs},
            {QStringLiteral("originalFrames"), stats.originalFrameCount},
            {QStringLiteral("optimizedFrames"), stats.optimizedFrameCount},
            {QStringLiteral("framesSaved"), stats.originalFrameCount - stats.optimizedFrameCount}
        };

        if (context_->canBus) {
            result[QStringLiteral("txQueueSize")] = context_->canBus->txQueueSize();
        }

        return result;
    });

    // ========================= 新协议v1.2 RPC方法 =========================

    // 多通道控制 - 同时控制所有4个通道
    dispatcher_->registerMethod(QStringLiteral("relay.controlMulti"),
                                 [this](const QJsonObject &params) {
        quint8 node = 0;

        if (!rpc::RpcHelpers::getU8InRange(params, "node", node, 1, 255))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid node"));

        auto *dev = context_->relays.value(node, nullptr);
        if (!dev)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("unknown node"));

        // 获取每个通道的动作（可选，默认为当前状态）
        device::RelayProtocol::Action actions[4] = {
            device::RelayProtocol::Action::Stop,
            device::RelayProtocol::Action::Stop,
            device::RelayProtocol::Action::Stop,
            device::RelayProtocol::Action::Stop
        };

        // 支持三种参数格式：
        // 1. actions: ["stop", "fwd", "rev", "stop"] - 控制所有4个通道
        // 2. action0, action1, action2, action3 - 逐个指定通道动作
        // 3. ch + action - 单通道控制（同一动作应用到指定通道，其他保持stop）
        if (params.contains(QStringLiteral("actions")) && params[QStringLiteral("actions")].isArray()) {
            // 格式1: actions数组
            const QJsonArray arr = params[QStringLiteral("actions")].toArray();
            for (int i = 0; i < 4 && i < arr.size(); ++i) {
                bool okAction = false;
                const auto action = context_->parseAction(arr[i].toString(), &okAction);
                if (okAction) {
                    actions[i] = action;
                }
            }
        } else if (params.contains(kKeyCh) && params.contains(kKeyAction)) {
            // 格式3: ch + action（单通道控制）
            quint8 channel = 0;
            QString actionStr;
            if (!rpc::RpcHelpers::getU8(params, "ch", channel) || channel > kMaxChannelId)
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("invalid ch(0..3)"));
            if (!rpc::RpcHelpers::getString(params, "action", actionStr))
                return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing action"));

            bool okAction = false;
            const auto action = context_->parseAction(actionStr, &okAction);
            if (!okAction)
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("invalid action (stop/fwd/rev)"));

            // 只设置指定通道的动作，其他通道保持默认stop
            actions[channel] = action;
        } else {
            // 格式2: action0, action1, action2, action3
            for (int i = 0; i < 4; ++i) {
                QString actionStr;
                if (rpc::RpcHelpers::getString(params, QStringLiteral("action%1").arg(i), actionStr)) {
                    bool okAction = false;
                    const auto action = context_->parseAction(actionStr, &okAction);
                    if (okAction) {
                        actions[i] = action;
                    }
                }
            }
        }

        const bool ok = dev->controlMulti(actions);

        QJsonObject result{
            {kKeyOk, ok}
        };

        if (context_->canBus) {
            result[QStringLiteral("txQueueSize")] = context_->canBus->txQueueSize();
        }

        return result;
    });

    // 查询所有通道状态 - 使用新协议0x16x
    dispatcher_->registerMethod(QStringLiteral("relay.queryAllChannels"),
                                 [this](const QJsonObject &params) {
        quint8 node = 0;

        if (!rpc::RpcHelpers::getU8InRange(params, "node", node, 1, 255))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid node"));

        auto *dev = context_->relays.value(node, nullptr);
        if (!dev)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("unknown node"));

        const bool ok = dev->queryAll();

        QJsonObject result{
            {kKeyOk, ok}
        };

        if (context_->canBus) {
            result[QStringLiteral("txQueueSize")] = context_->canBus->txQueueSize();
        }

        return result;
    });

    // 获取自动状态上报数据
    dispatcher_->registerMethod(QStringLiteral("relay.autoStatus"),
                                 [this](const QJsonObject &params) {
        quint8 node = 0;

        if (!rpc::RpcHelpers::getU8InRange(params, "node", node, 1, 255))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid node"));

        auto *dev = context_->relays.value(node, nullptr);
        if (!dev)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("unknown node"));

        const auto report = dev->lastAutoStatus();
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const qint64 lastSeen = dev->lastSeenMs();
        qint64 ageMs = 0;
        bool online = false;
        calcDeviceOnlineStatus(lastSeen, now, ageMs, online);

        QJsonArray channels;
        for (int i = 0; i < 4; ++i) {
            QJsonObject ch;
            ch[kKeyCh] = i;
            ch[QStringLiteral("status")] = static_cast<int>(report.status[i]);
            ch[kKeyPhaseLost] = report.phaseLost[i];
            ch[QStringLiteral("overcurrent")] = report.overcurrent[i];
            ch[kKeyCurrentA] = static_cast<double>(report.currentA[i]);
            channels.append(ch);
        }

        return QJsonObject{
            {kKeyOk, true},
            {kKeyNode, static_cast<int>(node)},
            {kKeyOnline, online},
            {kKeyAgeMs, (ageMs >= 0) ? static_cast<double>(ageMs) : QJsonValue()},
            {kKeyChannels, channels}
        };
    });

    // 设置过流标志
    dispatcher_->registerMethod(QStringLiteral("relay.setOvercurrent"),
                                 [this](const QJsonObject &params) {
        quint8 node = 0;
        qint32 channel = 0;
        qint32 flag = 0;

        if (!rpc::RpcHelpers::getU8InRange(params, "node", node, 1, 255))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid node"));

        // channel: 0-3 表示单个通道，-1 或 255 表示所有通道
        if (!rpc::RpcHelpers::getI32(params, "ch", channel))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing ch"));

        // 验证channel参数：必须是0-3或-1/255表示所有通道
        if (channel != -1 && channel != 255 && (channel < 0 || channel > kMaxChannelId))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue,
                QStringLiteral("invalid ch (0-3 for single channel, -1 or 255 for all channels)"));

        if (!rpc::RpcHelpers::getI32InRange(params, "flag", flag, 0, 255))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing flag"));

        auto *dev = context_->relays.value(node, nullptr);
        if (!dev)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("unknown node"));

        // 转换channel参数
        quint8 channelParam = (channel == -1 || channel == 255) ? 0xFF : static_cast<quint8>(channel);

        const bool ok = dev->setOvercurrentFlag(channelParam, static_cast<quint8>(flag));

        return QJsonObject{
            {kKeyOk, ok},
            {kKeyChannel, channel},
            {QStringLiteral("flag"), flag}
        };
    });

    // 设置通信模式（MB_REG_COMM_MODE）和网络模式（MB_REG_NETWORK_MODE）
    dispatcher_->registerMethod(QStringLiteral("relay.setCommMode"),
                                 [this](const QJsonObject &params) {
        quint8 node = 0;
        qint32 commMode = -1;
        qint32 networkMode = 0;  // 默认 DHCP

        if (!rpc::RpcHelpers::getU8InRange(params, "node", node, 1, 255))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid node"));

        if (!rpc::RpcHelpers::getI32InRange(params, "commMode", commMode, 0, 3))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing commMode"));

        if (params.contains(QStringLiteral("networkMode")) &&
            !rpc::RpcHelpers::getI32InRange(params, "networkMode", networkMode, 0, 1)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid networkMode"));
        }

        auto *dev = context_->relays.value(node, nullptr);
        if (!dev) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("unknown node"));
        }

        const bool ok = dev->setCommMode(
            static_cast<device::RelayProtocol::CommMode>(commMode),
            static_cast<device::RelayProtocol::NetworkMode>(networkMode));

        return QJsonObject{
            {kKeyOk, ok},
            {kKeyNode, static_cast<int>(node)},
            {QStringLiteral("commMode"), commMode},
            {QStringLiteral("networkMode"), networkMode}
        };
    });

    // 传感器读取 - 读取指定传感器的当前数值
    dispatcher_->registerMethod(QStringLiteral("sensor.read"),
                                 [this](const QJsonObject &params) {
        quint8 nodeId = 0;

        if (!rpc::RpcHelpers::getU8InRange(params, "nodeId", nodeId, 1, 255))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid nodeId"));

        // 查找设备配置
        auto config = context_->getDeviceConfig(nodeId);
        if (config.nodeId < 0) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("sensor not found"));
        }

        // 检查是否为传感器类型
        if (!device::isSensorType(config.deviceType)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("device is not a sensor"));
        }

        // 构建传感器信息
        QJsonObject result{
            {kKeyOk, true},
            {QStringLiteral("nodeId"), static_cast<int>(nodeId)},
            {kKeyName, config.name},
            {QStringLiteral("type"), static_cast<int>(config.deviceType)},
            {QStringLiteral("typeName"), QString::fromLatin1(device::deviceTypeToString(config.deviceType))},
            {QStringLiteral("commType"), static_cast<int>(config.commType)},
            {QStringLiteral("commTypeName"), QString::fromLatin1(device::commTypeToString(config.commType))},
            {QStringLiteral("bus"), config.bus}
        };

        // 如果有额外参数，也返回
        if (!config.params.isEmpty()) {
            result[QStringLiteral("params")] = config.params;
        }

        // TODO: 实际的传感器数据读取需要根据设备类型调用相应的驱动
        // 这里返回配置信息，实际数据需要通过设备驱动获取
        result[QStringLiteral("note")] = QStringLiteral("Sensor data reading requires device driver implementation");

        return result;
    });

    // 传感器列表 - 列出所有已配置的传感器（包括物理设备传感器和MQTT虚拟传感器）
    dispatcher_->registerMethod(QStringLiteral("sensor.list"),
                                 [this](const QJsonObject &params) {
        QString sourceFilter;
        rpc::RpcHelpers::getString(params, "source", sourceFilter);  // 可选过滤：local/mqtt/all

        QString commTypeFilter;
        rpc::RpcHelpers::getString(params, "commType", commTypeFilter);  // 可选过滤：serial/can（仅对local有效）

        QJsonArray sensors;

        // 1. 添加 sensorConfigs 中的配置传感器（包括MQTT虚拟传感器和本地传感器）
        for (auto it = context_->sensorConfigs.constBegin();
             it != context_->sensorConfigs.constEnd(); ++it) {
            const QString &sensorId = it.key();
            const auto &cfg = it.value();

            // 来源过滤
            if (!sourceFilter.isEmpty() && sourceFilter.toLower() != QStringLiteral("all")) {
                if (sourceFilter.toLower() == QStringLiteral("mqtt") &&
                    cfg.source != core::SensorSource::Mqtt) {
                    continue;
                }
                if (sourceFilter.toLower() == QStringLiteral("local") &&
                    cfg.source != core::SensorSource::Local) {
                    continue;
                }
            }

            QJsonObject sensorObj;
            sensorObj[QStringLiteral("sensorId")] = sensorId;
            sensorObj[kKeyName] = cfg.name;
            sensorObj[QStringLiteral("source")] = (cfg.source == core::SensorSource::Mqtt) 
                ? QStringLiteral("mqtt") : QStringLiteral("local");
            sensorObj[QStringLiteral("sourceType")] = static_cast<int>(cfg.source);
            sensorObj[QStringLiteral("unit")] = cfg.unit;
            sensorObj[kKeyEnabled] = cfg.enabled;

            if (cfg.source == core::SensorSource::Mqtt) {
                sensorObj[QStringLiteral("mqttChannelId")] = cfg.mqttChannelId;
                sensorObj[QStringLiteral("topic")] = cfg.topic;
                sensorObj[QStringLiteral("jsonPath")] = cfg.jsonPath;
            } else {
                sensorObj[QStringLiteral("nodeId")] = cfg.nodeId;
                sensorObj[kKeyChannel] = cfg.channel;
            }

            // 添加当前值（如果有）
            if (context_->sensorValues.contains(sensorId)) {
                const QVariant &val = context_->sensorValues.value(sensorId);
                sensorObj[QStringLiteral("value")] = QJsonValue::fromVariant(val);
                sensorObj[QStringLiteral("hasValue")] = true;

                if (context_->sensorUpdateTime.contains(sensorId)) {
                    const QDateTime &updateTime = context_->sensorUpdateTime.value(sensorId);
                    sensorObj[QStringLiteral("updateTime")] = updateTime.toString(Qt::ISODate);
                }
            } else {
                sensorObj[QStringLiteral("hasValue")] = false;
            }

            sensors.append(sensorObj);
        }

        // 2. 添加 listDevices() 中的物理传感器设备（如果没有过滤为mqtt）
        if (sourceFilter.isEmpty() || sourceFilter.toLower() == QStringLiteral("all") ||
            sourceFilter.toLower() == QStringLiteral("local")) {
            const auto devices = context_->listDevices();

            for (const auto &dev : devices) {
                // 只返回传感器类型的设备
                if (!device::isSensorType(dev.deviceType)) {
                    continue;
                }

                // 如果指定了通信类型过滤
                if (!commTypeFilter.isEmpty()) {
                    if (commTypeFilter.toLower() == QStringLiteral("serial") &&
                        dev.commType != device::CommTypeId::Serial) {
                        continue;
                    }
                    if (commTypeFilter.toLower() == QStringLiteral("can") &&
                        dev.commType != device::CommTypeId::Can) {
                        continue;
                    }
                }

                QJsonObject sensorObj;
                sensorObj[QStringLiteral("sensorId")] = QStringLiteral("device_%1").arg(dev.nodeId);
                sensorObj[QStringLiteral("nodeId")] = dev.nodeId;
                sensorObj[kKeyName] = dev.name;
                sensorObj[QStringLiteral("source")] = QStringLiteral("local");
                sensorObj[QStringLiteral("sourceType")] = static_cast<int>(core::SensorSource::Local);
                sensorObj[QStringLiteral("type")] = static_cast<int>(dev.deviceType);
                sensorObj[QStringLiteral("typeName")] = QString::fromLatin1(device::deviceTypeToString(dev.deviceType));
                sensorObj[QStringLiteral("commType")] = static_cast<int>(dev.commType);
                sensorObj[QStringLiteral("commTypeName")] = QString::fromLatin1(device::commTypeToString(dev.commType));
                sensorObj[QStringLiteral("bus")] = dev.bus;
                sensorObj[QStringLiteral("hasValue")] = false;

                if (!dev.params.isEmpty()) {
                    sensorObj[QStringLiteral("params")] = dev.params;
                }

                sensors.append(sensorObj);
            }
        }

        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("sensors"), sensors},
            {kKeyTotal, sensors.size()}
        };
    });

    // 获取传感器当前值 - sensor.value
    dispatcher_->registerMethod(QStringLiteral("sensor.value"),
                                 [this](const QJsonObject &params) {
        QString sensorId;
        if (!rpc::RpcHelpers::getString(params, "sensorId", sensorId))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing sensorId"));

        // 检查传感器是否配置
        if (!context_->sensorConfigs.contains(sensorId)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, 
                QStringLiteral("sensor not found: %1").arg(sensorId));
        }

        const auto &cfg = context_->sensorConfigs.value(sensorId);
        
        QJsonObject result{
            {kKeyOk, true},
            {QStringLiteral("sensorId"), sensorId},
            {kKeyName, cfg.name},
            {QStringLiteral("source"), static_cast<int>(cfg.source)},
            {QStringLiteral("unit"), cfg.unit}
        };

        // 检查是否有值
        if (context_->sensorValues.contains(sensorId)) {
            const QVariant &val = context_->sensorValues.value(sensorId);
            result[QStringLiteral("value")] = QJsonValue::fromVariant(val);
            result[QStringLiteral("hasValue")] = true;
            
            // 添加更新时间
            if (context_->sensorUpdateTime.contains(sensorId)) {
                const QDateTime &updateTime = context_->sensorUpdateTime.value(sensorId);
                result[QStringLiteral("updateTime")] = updateTime.toString(Qt::ISODate);
                result[QStringLiteral("updateMs")] = updateTime.toMSecsSinceEpoch();
            }
        } else {
            result[QStringLiteral("hasValue")] = false;
            result[QStringLiteral("note")] = QStringLiteral("No value received yet");
        }

        return result;
    });

    // 获取所有传感器值 - sensor.values
    dispatcher_->registerMethod(QStringLiteral("sensor.values"),
                                 [this](const QJsonObject &) {
        QJsonArray sensorsArr;
        
        for (auto it = context_->sensorConfigs.constBegin(); 
             it != context_->sensorConfigs.constEnd(); ++it) {
            const QString &sensorId = it.key();
            const auto &cfg = it.value();
            
            QJsonObject sensorObj{
                {QStringLiteral("sensorId"), sensorId},
                {kKeyName, cfg.name},
                {QStringLiteral("source"), static_cast<int>(cfg.source)},
                {QStringLiteral("unit"), cfg.unit},
                {kKeyEnabled, cfg.enabled}
            };
            
            if (context_->sensorValues.contains(sensorId)) {
                const QVariant &val = context_->sensorValues.value(sensorId);
                sensorObj[QStringLiteral("value")] = QJsonValue::fromVariant(val);
                sensorObj[QStringLiteral("hasValue")] = true;
                
                if (context_->sensorUpdateTime.contains(sensorId)) {
                    const QDateTime &updateTime = context_->sensorUpdateTime.value(sensorId);
                    sensorObj[QStringLiteral("updateTime")] = updateTime.toString(Qt::ISODate);
                    sensorObj[QStringLiteral("updateMs")] = updateTime.toMSecsSinceEpoch();
                }
            } else {
                sensorObj[QStringLiteral("hasValue")] = false;
            }
            
            sensorsArr.append(sensorObj);
        }

        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("sensors"), sensorsArr},
            {kKeyTotal, sensorsArr.size()}
        };
    });

    // 添加传感器配置 - sensor.add
    dispatcher_->registerMethod(QStringLiteral("sensor.add"),
                                 [this](const QJsonObject &params) {
        core::SensorNodeConfig cfg;
        
        // 必需参数
        if (!rpc::RpcHelpers::getString(params, "sensorId", cfg.sensorId) || cfg.sensorId.isEmpty())
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid sensorId"));
        
        // 检查是否已存在
        if (context_->sensorConfigs.contains(cfg.sensorId)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, 
                QStringLiteral("sensor already exists: %1").arg(cfg.sensorId));
        }

        // 可选参数
        QString name;
        if (rpc::RpcHelpers::getString(params, "name", name)) {
            cfg.name = name;
        } else {
            cfg.name = cfg.sensorId;
        }

        // 传感器来源
        qint32 sourceInt = 0;
        if (rpc::RpcHelpers::getI32(params, "source", sourceInt)) {
            cfg.source = static_cast<core::SensorSource>(sourceInt);
        }

        // 单位和缩放
        QString unit;
        if (rpc::RpcHelpers::getString(params, "unit", unit))
            cfg.unit = unit;
        
        if (params.contains(QStringLiteral("scale")))
            cfg.scale = params[QStringLiteral("scale")].toDouble(1.0);
        if (params.contains(QStringLiteral("offset")))
            cfg.offset = params[QStringLiteral("offset")].toDouble(0.0);

        // 本地传感器参数
        qint32 nodeId = -1, channel = -1;
        if (rpc::RpcHelpers::getI32(params, "nodeId", nodeId))
            cfg.nodeId = nodeId;
        if (rpc::RpcHelpers::getI32(params, "channel", channel))
            cfg.channel = channel;

        // MQTT传感器参数
        qint32 mqttChannelId = -1;
        if (rpc::RpcHelpers::getI32(params, "mqttChannelId", mqttChannelId))
            cfg.mqttChannelId = mqttChannelId;
        QString topic, jsonPath;
        if (rpc::RpcHelpers::getString(params, "topic", topic))
            cfg.topic = topic;
        if (rpc::RpcHelpers::getString(params, "jsonPath", jsonPath))
            cfg.jsonPath = jsonPath;

        cfg.enabled = params.value(kKeyEnabled).toBool(true);

        // 添加到配置
        context_->sensorConfigs.insert(cfg.sensorId, cfg);
        context_->coreConfig.sensors.append(cfg);

        LOG_INFO("RpcRegistry", 
                 QStringLiteral("Sensor added: %1 (source=%2)")
                     .arg(cfg.sensorId)
                     .arg(static_cast<int>(cfg.source)));

        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("sensorId"), cfg.sensorId}
        };
    });

    // 更新传感器配置 - sensor.update
    dispatcher_->registerMethod(QStringLiteral("sensor.update"),
                                 [this](const QJsonObject &params) {
        QString sensorId;
        if (!rpc::RpcHelpers::getString(params, "sensorId", sensorId))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing sensorId"));

        if (!context_->sensorConfigs.contains(sensorId)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, 
                QStringLiteral("sensor not found: %1").arg(sensorId));
        }

        auto &cfg = context_->sensorConfigs[sensorId];

        // 更新可选参数
        QString name;
        if (rpc::RpcHelpers::getString(params, "name", name))
            cfg.name = name;

        QString unit;
        if (rpc::RpcHelpers::getString(params, "unit", unit))
            cfg.unit = unit;
        
        if (params.contains(QStringLiteral("scale")))
            cfg.scale = params[QStringLiteral("scale")].toDouble(cfg.scale);
        if (params.contains(QStringLiteral("offset")))
            cfg.offset = params[QStringLiteral("offset")].toDouble(cfg.offset);
        if (params.contains(kKeyEnabled))
            cfg.enabled = params[kKeyEnabled].toBool(cfg.enabled);

        // MQTT相关
        QString jsonPath;
        if (rpc::RpcHelpers::getString(params, "jsonPath", jsonPath))
            cfg.jsonPath = jsonPath;
        qint32 mqttChannelId;
        if (rpc::RpcHelpers::getI32(params, "mqttChannelId", mqttChannelId))
            cfg.mqttChannelId = mqttChannelId;

        // 同步到coreConfig
        for (auto &s : context_->coreConfig.sensors) {
            if (s.sensorId == sensorId) {
                s = cfg;
                break;
            }
        }

        LOG_INFO("RpcRegistry", 
                 QStringLiteral("Sensor updated: %1").arg(sensorId));

        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("sensorId"), sensorId}
        };
    });

    // 删除传感器配置 - sensor.delete
    dispatcher_->registerMethod(QStringLiteral("sensor.delete"),
                                 [this](const QJsonObject &params) {
        QString sensorId;
        if (!rpc::RpcHelpers::getString(params, "sensorId", sensorId))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing sensorId"));

        if (!context_->sensorConfigs.contains(sensorId)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, 
                QStringLiteral("sensor not found: %1").arg(sensorId));
        }

        // 从运行时配置移除
        context_->sensorConfigs.remove(sensorId);
        context_->sensorValues.remove(sensorId);
        context_->sensorUpdateTime.remove(sensorId);

        // 从coreConfig移除
        for (int i = 0; i < context_->coreConfig.sensors.size(); ++i) {
            if (context_->coreConfig.sensors[i].sensorId == sensorId) {
                context_->coreConfig.sensors.removeAt(i);
                break;
            }
        }

        LOG_INFO("RpcRegistry", 
                 QStringLiteral("Sensor deleted: %1").arg(sensorId));

        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("sensorId"), sensorId}
        };
    });

    // 获取传感器配置详情 - sensor.config
    dispatcher_->registerMethod(QStringLiteral("sensor.config"),
                                 [this](const QJsonObject &params) {
        QString sensorId;
        if (!rpc::RpcHelpers::getString(params, "sensorId", sensorId))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing sensorId"));

        if (!context_->sensorConfigs.contains(sensorId)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, 
                QStringLiteral("sensor not found: %1").arg(sensorId));
        }

        const auto &cfg = context_->sensorConfigs.value(sensorId);
        
        QJsonObject result{
            {kKeyOk, true},
            {QStringLiteral("sensorId"), cfg.sensorId},
            {kKeyName, cfg.name},
            {QStringLiteral("source"), static_cast<int>(cfg.source)},
            {QStringLiteral("valueType"), static_cast<int>(cfg.valueType)},
            {QStringLiteral("nodeId"), cfg.nodeId},
            {kKeyChannel, cfg.channel},
            {QStringLiteral("mqttChannelId"), cfg.mqttChannelId},
            {QStringLiteral("topic"), cfg.topic},
            {QStringLiteral("jsonPath"), cfg.jsonPath},
            {QStringLiteral("unit"), cfg.unit},
            {QStringLiteral("scale"), cfg.scale},
            {QStringLiteral("offset"), cfg.offset},
            {kKeyEnabled, cfg.enabled}
        };

        return result;
    });

    // 手动设置传感器值（用于测试或外部数据注入）- sensor.setValue
    dispatcher_->registerMethod(QStringLiteral("sensor.setValue"),
                                 [this](const QJsonObject &params) {
        QString sensorId;
        if (!rpc::RpcHelpers::getString(params, "sensorId", sensorId))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing sensorId"));

        if (!params.contains(QStringLiteral("value"))) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing value"));
        }

        const QJsonValue val = params.value(QStringLiteral("value"));
        
        // 如果传感器不存在，自动创建配置
        if (!context_->sensorConfigs.contains(sensorId)) {
            core::SensorNodeConfig cfg;
            cfg.sensorId = sensorId;
            cfg.name = sensorId;
            cfg.source = core::SensorSource::Local;
            cfg.enabled = true;
            context_->sensorConfigs.insert(sensorId, cfg);
            
            LOG_INFO("RpcRegistry", 
                     QStringLiteral("Sensor auto-created for setValue: %1").arg(sensorId));
        }

        context_->sensorValues[sensorId] = val.toVariant();
        context_->sensorUpdateTime[sensorId] = QDateTime::currentDateTime();

        LOG_DEBUG("RpcRegistry", 
                  QStringLiteral("Sensor value set: %1 = %2")
                      .arg(sensorId)
                      .arg(val.toVariant().toString()));

        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("sensorId"), sensorId},
            {QStringLiteral("value"), val}
        };
    });
}


}  // namespace core
}  // namespace fanzhou
