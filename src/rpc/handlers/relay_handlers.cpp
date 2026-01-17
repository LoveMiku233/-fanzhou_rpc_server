/**
 * @file relay_handlers.cpp
 * @brief 继电器控制相关RPC处理器实现
 */

#include "relay_handlers.h"
#include "rpc_handler_base.h"

#include "comm/can_comm.h"
#include "device/can/relay_gd427.h"
#include "device/device_types.h"

#include <algorithm>

namespace fanzhou {
namespace rpc {

using namespace RpcKeys;
using namespace RpcConst;
using namespace RpcUtils;

void registerRelayHandlers(core::CoreContext *context, JsonRpcDispatcher *dispatcher)
{
    // ===================== 基本控制方法 =====================

    dispatcher->registerMethod(QStringLiteral("relay.control"),
                                 [context](const QJsonObject &params) {
        quint8 node = 0, channel = 0;
        QString actionStr;

        if (!RpcHelpers::getU8(params, "node", node))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing/invalid node"));
        if (!RpcHelpers::getU8(params, "ch", channel) || channel > 3)
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("missing/invalid ch(0..3)"));
        if (!RpcHelpers::getString(params, "action", actionStr))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing action"));

        bool okAction = false;
        const auto action = context->parseAction(actionStr, &okAction);
        if (!okAction)
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("invalid action (stop/fwd/rev)"));

        const auto result = context->enqueueControl(node, channel, action, QStringLiteral("rpc:relay.control"));
        if (!result.accepted) {
            return RpcHelpers::err(RpcError::BadParameterValue, result.error);
        }

        QJsonObject obj{
            {QStringLiteral("ok"), true},
            {QStringLiteral("jobId"), QString::number(result.jobId)},
            {QStringLiteral("queued"), !result.executedImmediately}
        };
        if (result.executedImmediately) {
            obj[QStringLiteral("success")] = result.success;
        }

        // Add CAN TX queue status for diagnostics
        if (context->canBus) {
            const int txQueueSize = context->canBus->txQueueSize();
            obj[QStringLiteral("txQueueSize")] = txQueueSize;
            if (txQueueSize > kTxQueueCongestionThreshold) {
                obj[QStringLiteral("warning")] = formatQueueCongestionWarning(txQueueSize, QString());
            }
        }

        return obj;
    });

    dispatcher->registerMethod(QStringLiteral("relay.query"),
                                 [context](const QJsonObject &params) {
        quint8 node = 0, channel = 0;

        if (!RpcHelpers::getU8(params, "node", node))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing/invalid node"));
        if (!RpcHelpers::getU8(params, "ch", channel) || channel > 3)
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("missing/invalid ch(0..3)"));

        auto *dev = context->relays.value(node, nullptr);
        if (!dev)
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("unknown node"));

        return QJsonObject{{QStringLiteral("ok"), dev->query(channel)}};
    });

    // ===================== 状态查询方法 =====================

    dispatcher->registerMethod(QStringLiteral("relay.status"),
                                 [context](const QJsonObject &params) {
        quint8 node = 0, channel = 0;

        if (!RpcHelpers::getU8(params, "node", node))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing/invalid node"));
        if (!RpcHelpers::getU8(params, "ch", channel) || channel > 3)
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("missing/invalid ch(0..3)"));

        auto *dev = context->relays.value(node, nullptr);
        if (!dev)
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("unknown node"));

        const auto status = dev->lastStatus(channel);
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const qint64 lastSeen = dev->lastSeenMs();
        qint64 ageMs = 0;
        bool online = false;
        calcDeviceOnlineStatus(lastSeen, now, ageMs, online);

        QJsonObject result{
            {QStringLiteral("ok"), true},
            {QStringLiteral("channel"), static_cast<int>(status.channel)},
            {QStringLiteral("statusByte"), static_cast<int>(status.statusByte)},
            {QStringLiteral("currentA"), static_cast<double>(status.currentA)},
            {QStringLiteral("mode"), static_cast<int>(device::RelayProtocol::modeBits(status.statusByte))},
            {QStringLiteral("phaseLost"), device::RelayProtocol::phaseLost(status.statusByte)},
            {keyOnline(), online},
            {keyAgeMs(), (ageMs >= 0) ? static_cast<double>(ageMs) : QJsonValue()}
        };

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

    dispatcher->registerMethod(QStringLiteral("relay.statusAll"),
                                 [context](const QJsonObject &params) {
        quint8 node = 0;
        if (!RpcHelpers::getU8(params, "node", node))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing/invalid node"));

        auto *dev = context->relays.value(node, nullptr);
        if (!dev)
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("unknown node"));

        QJsonArray channels;
        for (quint8 ch = 0; ch < 4; ++ch) {
            const auto status = dev->lastStatus(ch);
            QJsonObject obj;
            obj[keyCh()] = static_cast<int>(ch);
            obj[keyChannel()] = static_cast<int>(status.channel);
            obj[keyStatusByte()] = static_cast<int>(status.statusByte);
            obj[keyCurrentA()] = static_cast<double>(status.currentA);
            obj[keyMode()] = static_cast<int>(device::RelayProtocol::modeBits(status.statusByte));
            obj[keyPhaseLost()] = device::RelayProtocol::phaseLost(status.statusByte);
            channels.append(obj);
        }

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const qint64 lastSeen = dev->lastSeenMs();
        qint64 ageMs = 0;
        bool online = false;
        calcDeviceOnlineStatus(lastSeen, now, ageMs, online);

        QJsonObject result{
            {keyOk(), true},
            {keyNode(), static_cast<int>(node)},
            {keyOnline(), online},
            {keyAgeMs(), (ageMs >= 0) ? static_cast<double>(ageMs) : QJsonValue()},
            {keyChannels(), channels}
        };

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

            if (context->canBus) {
                result[QStringLiteral("txQueueSize")] = context->canBus->txQueueSize();
            }
        }

        return result;
    });

    // 获取所有继电器节点列表
    dispatcher->registerMethod(QStringLiteral("relay.nodes"),
                                 [context](const QJsonObject &) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();

        QList<quint8> nodeIds = context->relays.keys();
        std::sort(nodeIds.begin(), nodeIds.end());

        QJsonArray arr;
        for (quint8 node : nodeIds) {
            auto *dev = context->relays.value(node, nullptr);
            const qint64 lastSeen = dev ? dev->lastSeenMs() : 0;

            qint64 ageMs = 0;
            bool online = false;
            calcDeviceOnlineStatus(lastSeen, now, ageMs, online);

            arr.append(buildDeviceStatusObject(node, ageMs, online));
        }
        return QJsonObject{{keyOk(), true}, {keyNodes(), arr}};
    });

    // ===================== 批量操作方法 =====================

    dispatcher->registerMethod(QStringLiteral("relay.queryAll"),
                                 [context](const QJsonObject &) {
        int queriedCount = 0;
        for (auto it = context->relays.begin(); it != context->relays.end(); ++it) {
            auto *dev = it.value();
            if (dev) {
                for (quint8 ch = 0; ch < 4; ++ch) {
                    dev->query(ch);
                }
                queriedCount++;
            }
        }

        QJsonObject result{
            {keyOk(), true},
            {QStringLiteral("queriedDevices"), queriedCount}
        };

        if (context->canBus) {
            const int txQueueSize = context->canBus->txQueueSize();
            result[QStringLiteral("txQueueSize")] = txQueueSize;
            if (txQueueSize > kTxQueueCongestionThreshold) {
                result[QStringLiteral("warning")] = formatQueueCongestionWarning(
                    txQueueSize, QStringLiteral("Queries may be delayed."));
            }
        }

        return result;
    });

    // 急停
    dispatcher->registerMethod(QStringLiteral("relay.emergencyStop"),
                                 [context](const QJsonObject &) {
        int stoppedCount = 0;
        int failedCount = 0;
        QJsonArray failedNodes;

        for (auto it = context->relays.begin(); it != context->relays.end(); ++it) {
            quint8 node = it.key();
            auto *dev = it.value();
            if (!dev) continue;

            for (quint8 ch = 0; ch < kDefaultChannelCount; ++ch) {
                const auto result = context->enqueueControl(
                    node, ch,
                    device::RelayProtocol::Action::Stop,
                    QStringLiteral("rpc:relay.emergencyStop"),
                    false
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
            {keyOk(), true},
            {QStringLiteral("stoppedChannels"), stoppedCount},
            {QStringLiteral("failedChannels"), failedCount},
            {QStringLiteral("deviceCount"), context->relays.size()}
        };

        if (failedCount > 0) {
            result[QStringLiteral("failedNodes")] = failedNodes;
        }

        if (context->canBus) {
            result[QStringLiteral("txQueueSize")] = context->canBus->txQueueSize();
        }

        return result;
    });

    // 急停优化版
    dispatcher->registerMethod(QStringLiteral("relay.emergencyStopOptimized"),
                                 [context](const QJsonObject &) {
        int stoppedDevices = 0;
        int stoppedChannels = 0;
        int failedCount = 0;
        int originalFrames = 0;
        int optimizedFrames = 0;

        device::RelayProtocol::Action stopActions[4] = {
            device::RelayProtocol::Action::Stop,
            device::RelayProtocol::Action::Stop,
            device::RelayProtocol::Action::Stop,
            device::RelayProtocol::Action::Stop
        };

        for (auto it = context->relays.begin(); it != context->relays.end(); ++it) {
            auto *dev = it.value();
            if (!dev) continue;

            originalFrames += kDefaultChannelCount;

            const bool ok = dev->controlMulti(stopActions);
            optimizedFrames++;

            if (ok) {
                stoppedDevices++;
                stoppedChannels += kDefaultChannelCount;
            } else {
                failedCount++;
            }
        }

        QJsonObject result{
            {keyOk(), true},
            {QStringLiteral("stoppedDevices"), stoppedDevices},
            {QStringLiteral("stoppedChannels"), stoppedChannels},
            {QStringLiteral("failedDevices"), failedCount},
            {QStringLiteral("deviceCount"), context->relays.size()},
            {QStringLiteral("originalFrames"), originalFrames},
            {QStringLiteral("optimizedFrames"), optimizedFrames},
            {QStringLiteral("framesSaved"), originalFrames - optimizedFrames}
        };

        if (context->canBus) {
            result[QStringLiteral("txQueueSize")] = context->canBus->txQueueSize();
        }

        return result;
    });

    // 批量控制
    dispatcher->registerMethod(QStringLiteral("relay.controlBatch"),
                                 [context](const QJsonObject &params) {
        if (!params.contains(QStringLiteral("commands")) ||
            !params[QStringLiteral("commands")].isArray()) {
            return RpcHelpers::err(RpcError::MissingParameter,
                QStringLiteral("missing commands array"));
        }

        const QJsonArray commands = params[QStringLiteral("commands")].toArray();
        QList<core::BatchControlItem> items;

        for (const auto &cmdVal : commands) {
            if (!cmdVal.isObject()) continue;
            const QJsonObject cmd = cmdVal.toObject();

            core::BatchControlItem item;

            if (!cmd.contains(QStringLiteral("node"))) continue;
            item.node = static_cast<quint8>(cmd[QStringLiteral("node")].toInt());

            if (!cmd.contains(QStringLiteral("ch"))) continue;
            item.channel = static_cast<quint8>(cmd[QStringLiteral("ch")].toInt());
            if (item.channel > kMaxChannelId) continue;

            if (!cmd.contains(QStringLiteral("action"))) continue;
            bool okAction = false;
            item.action = context->parseAction(cmd[QStringLiteral("action")].toString(), &okAction);
            if (!okAction) continue;

            items.append(item);
        }

        if (items.isEmpty()) {
            return RpcHelpers::err(RpcError::BadParameterValue,
                QStringLiteral("no valid commands"));
        }

        const auto result = context->batchControl(items, QStringLiteral("rpc:relay.controlBatch"));

        QJsonArray jobIds;
        for (quint64 id : result.jobIds) {
            jobIds.append(QString::number(id));
        }

        QJsonObject response{
            {keyOk(), result.ok},
            {keyTotal(), result.total},
            {keyAccepted(), result.accepted},
            {QStringLiteral("failed"), result.failed},
            {QStringLiteral("originalFrames"), result.originalFrames},
            {QStringLiteral("optimizedFrames"), result.optimizedFrames},
            {QStringLiteral("framesSaved"), result.originalFrames - result.optimizedFrames},
            {keyJobIds(), jobIds}
        };

        if (context->canBus) {
            response[QStringLiteral("txQueueSize")] = context->canBus->txQueueSize();
        }

        return response;
    });

    // ===================== 新协议v1.2方法 =====================

    // 多通道控制
    dispatcher->registerMethod(QStringLiteral("relay.controlMulti"),
                                 [context](const QJsonObject &params) {
        quint8 node = 0;

        if (!RpcHelpers::getU8(params, "node", node))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing/invalid node"));

        auto *dev = context->relays.value(node, nullptr);
        if (!dev)
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("unknown node"));

        device::RelayProtocol::Action actions[4] = {
            device::RelayProtocol::Action::Stop,
            device::RelayProtocol::Action::Stop,
            device::RelayProtocol::Action::Stop,
            device::RelayProtocol::Action::Stop
        };

        if (params.contains(QStringLiteral("actions")) && params[QStringLiteral("actions")].isArray()) {
            const QJsonArray arr = params[QStringLiteral("actions")].toArray();
            for (int i = 0; i < 4 && i < arr.size(); ++i) {
                bool okAction = false;
                const auto action = context->parseAction(arr[i].toString(), &okAction);
                if (okAction) {
                    actions[i] = action;
                }
            }
        } else if (params.contains(keyCh()) && params.contains(keyAction())) {
            quint8 channel = 0;
            QString actionStr;
            if (!RpcHelpers::getU8(params, "ch", channel) || channel > kMaxChannelId)
                return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("invalid ch(0..3)"));
            if (!RpcHelpers::getString(params, "action", actionStr))
                return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing action"));

            bool okAction = false;
            const auto action = context->parseAction(actionStr, &okAction);
            if (!okAction)
                return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("invalid action (stop/fwd/rev)"));

            actions[channel] = action;
        } else {
            for (int i = 0; i < 4; ++i) {
                QString actionStr;
                if (RpcHelpers::getString(params, QStringLiteral("action%1").arg(i), actionStr)) {
                    bool okAction = false;
                    const auto action = context->parseAction(actionStr, &okAction);
                    if (okAction) {
                        actions[i] = action;
                    }
                }
            }
        }

        const bool ok = dev->controlMulti(actions);

        QJsonObject result{{keyOk(), ok}};

        if (context->canBus) {
            result[QStringLiteral("txQueueSize")] = context->canBus->txQueueSize();
        }

        return result;
    });

    // 查询所有通道
    dispatcher->registerMethod(QStringLiteral("relay.queryAllChannels"),
                                 [context](const QJsonObject &params) {
        quint8 node = 0;

        if (!RpcHelpers::getU8(params, "node", node))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing/invalid node"));

        auto *dev = context->relays.value(node, nullptr);
        if (!dev)
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("unknown node"));

        const bool ok = dev->queryAll();

        QJsonObject result{{keyOk(), ok}};

        if (context->canBus) {
            result[QStringLiteral("txQueueSize")] = context->canBus->txQueueSize();
        }

        return result;
    });

    // 自动状态上报
    dispatcher->registerMethod(QStringLiteral("relay.autoStatus"),
                                 [context](const QJsonObject &params) {
        quint8 node = 0;

        if (!RpcHelpers::getU8(params, "node", node))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing/invalid node"));

        auto *dev = context->relays.value(node, nullptr);
        if (!dev)
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("unknown node"));

        const auto report = dev->lastAutoStatus();
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const qint64 lastSeen = dev->lastSeenMs();
        qint64 ageMs = 0;
        bool online = false;
        calcDeviceOnlineStatus(lastSeen, now, ageMs, online);

        QJsonArray channels;
        for (int i = 0; i < 4; ++i) {
            QJsonObject ch;
            ch[keyCh()] = i;
            ch[QStringLiteral("status")] = static_cast<int>(report.status[i]);
            ch[keyPhaseLost()] = report.phaseLost[i];
            ch[QStringLiteral("overcurrent")] = report.overcurrent[i];
            ch[keyCurrentA()] = static_cast<double>(report.currentA[i]);
            channels.append(ch);
        }

        return QJsonObject{
            {keyOk(), true},
            {keyNode(), static_cast<int>(node)},
            {keyOnline(), online},
            {keyAgeMs(), (ageMs >= 0) ? static_cast<double>(ageMs) : QJsonValue()},
            {keyChannels(), channels}
        };
    });

    // 设置过流标志
    dispatcher->registerMethod(QStringLiteral("relay.setOvercurrent"),
                                 [context](const QJsonObject &params) {
        quint8 node = 0;
        qint32 channel = 0;
        qint32 flag = 0;

        if (!RpcHelpers::getU8(params, "node", node))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing/invalid node"));

        if (!RpcHelpers::getI32(params, "ch", channel))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing ch"));

        if (channel != -1 && channel != 255 && (channel < 0 || channel > kMaxChannelId))
            return RpcHelpers::err(RpcError::BadParameterValue,
                QStringLiteral("invalid ch (0-3 for single channel, -1 or 255 for all channels)"));

        if (!RpcHelpers::getI32(params, "flag", flag))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing flag"));

        if (flag < 0 || flag > 255)
            return RpcHelpers::err(RpcError::BadParameterValue,
                QStringLiteral("invalid flag (must be 0-255)"));

        auto *dev = context->relays.value(node, nullptr);
        if (!dev)
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("unknown node"));

        quint8 channelParam = (channel == -1 || channel == 255) ? 0xFF : static_cast<quint8>(channel);

        const bool ok = dev->setOvercurrentFlag(channelParam, static_cast<quint8>(flag));

        return QJsonObject{
            {keyOk(), ok},
            {keyChannel(), channel},
            {QStringLiteral("flag"), flag}
        };
    });

    // ===================== 传感器方法 =====================

    dispatcher->registerMethod(QStringLiteral("sensor.read"),
                                 [context](const QJsonObject &params) {
        quint8 nodeId = 0;

        if (!RpcHelpers::getU8(params, "nodeId", nodeId))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing/invalid nodeId"));

        auto config = context->getDeviceConfig(nodeId);
        if (config.nodeId < 0) {
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("sensor not found"));
        }

        if (!device::isSensorType(config.deviceType)) {
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("device is not a sensor"));
        }

        QJsonObject result{
            {keyOk(), true},
            {QStringLiteral("nodeId"), static_cast<int>(nodeId)},
            {keyName(), config.name},
            {QStringLiteral("type"), static_cast<int>(config.deviceType)},
            {QStringLiteral("typeName"), QString::fromLatin1(device::deviceTypeToString(config.deviceType))},
            {QStringLiteral("commType"), static_cast<int>(config.commType)},
            {QStringLiteral("commTypeName"), QString::fromLatin1(device::commTypeToString(config.commType))},
            {QStringLiteral("bus"), config.bus}
        };

        if (!config.params.isEmpty()) {
            result[QStringLiteral("params")] = config.params;
        }

        result[QStringLiteral("note")] = QStringLiteral("Sensor data reading requires device driver implementation");

        return result;
    });

    dispatcher->registerMethod(QStringLiteral("sensor.list"),
                                 [context](const QJsonObject &params) {
        QString commTypeFilter;
        RpcHelpers::getString(params, "commType", commTypeFilter);

        QJsonArray sensors;
        const auto devices = context->listDevices();

        for (const auto &dev : devices) {
            if (!device::isSensorType(dev.deviceType)) {
                continue;
            }

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
            sensorObj[QStringLiteral("nodeId")] = dev.nodeId;
            sensorObj[keyName()] = dev.name;
            sensorObj[QStringLiteral("type")] = static_cast<int>(dev.deviceType);
            sensorObj[QStringLiteral("typeName")] = QString::fromLatin1(device::deviceTypeToString(dev.deviceType));
            sensorObj[QStringLiteral("commType")] = static_cast<int>(dev.commType);
            sensorObj[QStringLiteral("commTypeName")] = QString::fromLatin1(device::commTypeToString(dev.commType));
            sensorObj[QStringLiteral("bus")] = dev.bus;

            if (!dev.params.isEmpty()) {
                sensorObj[QStringLiteral("params")] = dev.params;
            }

            sensors.append(sensorObj);
        }

        return QJsonObject{
            {keyOk(), true},
            {QStringLiteral("sensors"), sensors},
            {keyTotal(), sensors.size()}
        };
    });
}

}  // namespace rpc
}  // namespace fanzhou
