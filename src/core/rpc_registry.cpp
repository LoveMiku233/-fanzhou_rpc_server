/**
 * @file rpc_registry.cpp
 * @brief RPC方法注册器实现
 */

#include "rpc_registry.h"
#include "core_context.h"

#include "comm/can_comm.h"
#include "config/system_settings.h"
#include "device/can/relay_gd427.h"
#include "rpc/json_rpc_dispatcher.h"
#include "rpc/rpc_error_codes.h"
#include "rpc/rpc_helpers.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <limits>

namespace fanzhou {
namespace core {

namespace {
// 通用JSON键 - 避免重复的QStringLiteral分配
const QString kKeyOk = QStringLiteral("ok");
const QString kKeyCh = QStringLiteral("ch");
const QString kKeyChannel = QStringLiteral("channel");
const QString kKeyStatusByte = QStringLiteral("statusByte");
const QString kKeyCurrentA = QStringLiteral("currentA");
const QString kKeyMode = QStringLiteral("mode");
const QString kKeyPhaseLost = QStringLiteral("phaseLost");
const QString kKeyNode = QStringLiteral("node");
const QString kKeyOnline = QStringLiteral("online");
const QString kKeyAgeMs = QStringLiteral("ageMs");
const QString kKeyChannels = QStringLiteral("channels");
const QString kKeyNodes = QStringLiteral("nodes");
const QString kKeyJobId = QStringLiteral("jobId");
const QString kKeyQueued = QStringLiteral("queued");
const QString kKeySuccess = QStringLiteral("success");
const QString kKeyGroupId = QStringLiteral("groupId");
const QString kKeyName = QStringLiteral("name");
const QString kKeyDevices = QStringLiteral("devices");
const QString kKeyDeviceCount = QStringLiteral("deviceCount");
const QString kKeyGroups = QStringLiteral("groups");
const QString kKeyTotal = QStringLiteral("total");
const QString kKeyAccepted = QStringLiteral("accepted");
const QString kKeyMissing = QStringLiteral("missing");
const QString kKeyJobIds = QStringLiteral("jobIds");
const QString kKeyPending = QStringLiteral("pending");
const QString kKeyActive = QStringLiteral("active");
const QString kKeyLastJobId = QStringLiteral("lastJobId");
const QString kKeyMessage = QStringLiteral("message");
const QString kKeyFinishedMs = QStringLiteral("finishedMs");
const QString kKeyId = QStringLiteral("id");
const QString kKeyAction = QStringLiteral("action");
const QString kKeyIntervalSec = QStringLiteral("intervalSec");
const QString kKeyEnabled = QStringLiteral("enabled");
const QString kKeyAutoStart = QStringLiteral("autoStart");
const QString kKeyAttached = QStringLiteral("attached");
const QString kKeyRunning = QStringLiteral("running");
const QString kKeyStrategies = QStringLiteral("strategies");

// 设备在线超时阈值（毫秒）：30秒内收到过响应认为在线
constexpr qint64 kOnlineTimeoutMs = 30000;

/**
 * @brief 计算设备在线状态信息
 * @param lastSeenMs 设备最后响应时间戳（毫秒）
 * @param now 当前时间戳（毫秒）
 * @param outAgeMs 输出：设备响应时间间隔（毫秒），-1表示从未响应
 * @param outOnline 输出：设备是否在线
 */
void calcDeviceOnlineStatus(qint64 lastSeenMs, qint64 now, qint64 &outAgeMs, bool &outOnline)
{
    if (lastSeenMs > 0) {
        outAgeMs = now - lastSeenMs;
        outOnline = (outAgeMs <= kOnlineTimeoutMs);
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
QJsonObject buildDeviceStatusObject(quint8 node, qint64 ageMs, bool online)
{
    QJsonObject obj;
    obj[kKeyNode] = static_cast<int>(node);
    obj[kKeyOnline] = online;
    // ageMs为-1时表示从未响应，返回null
    obj[kKeyAgeMs] = (ageMs >= 0) ? static_cast<double>(ageMs) : QJsonValue();
    return obj;
}

}  // namespace

RpcRegistry::RpcRegistry(CoreContext *context, rpc::JsonRpcDispatcher *dispatcher,
                         QObject *parent)
    : QObject(parent)
    , context_(context)
    , dispatcher_(dispatcher)
{
}

void RpcRegistry::registerAll()
{
    registerBase();
    registerSystem();
    registerCan();
    registerRelay();
    registerGroup();
    registerAuto();
}

void RpcRegistry::registerBase()
{
    dispatcher_->registerMethod(QStringLiteral("rpc.list"),
                                 [this](const QJsonObject &) {
        QJsonArray arr;
        for (const auto &m : dispatcher_->methods()) {
            arr.append(m);
        }
        return QJsonValue(arr);
    });

    dispatcher_->registerMethod(QStringLiteral("rpc.ping"),
                                 [](const QJsonObject &) {
        return QJsonObject{{QStringLiteral("ok"), true}};
    });

    dispatcher_->registerMethod(QStringLiteral("echo"),
                                 [](const QJsonObject &params) {
        return QJsonValue(params);
    });
}

void RpcRegistry::registerSystem()
{
    // 获取系统信息
    dispatcher_->registerMethod(QStringLiteral("sys.info"),
                                 [this](const QJsonObject &) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        return QJsonObject{
            {QStringLiteral("ok"), true},
            {QStringLiteral("serverVersion"), QStringLiteral("1.0.0")},
            {QStringLiteral("serverTime"), QString::number(now)},
            {QStringLiteral("rpcPort"), context_->rpcPort},
            {QStringLiteral("canInterface"), context_->canInterface},
            {QStringLiteral("canBitrate"), context_->canBitrate},
            {QStringLiteral("deviceCount"), context_->relays.size()},
            {QStringLiteral("groupCount"), context_->deviceGroups.size()}
        };
    });

    dispatcher_->registerMethod(QStringLiteral("sys.can.setBitrate"),
                                 [this](const QJsonObject &params) {
        QString interface;
        qint32 bitrate = 0;
        bool tripleSampling = false;

        if (!rpc::RpcHelpers::getString(params, "ifname", interface))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing ifname"));
        if (!rpc::RpcHelpers::getI32(params, "bitrate", bitrate) || bitrate <= 0)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("missing/invalid bitrate"));
        if (!rpc::RpcHelpers::getBool(params, "tripleSampling", tripleSampling, false))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid tripleSampling"));

        if (!context_->systemSettings)
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("SystemSettings not ready"));

        const bool ok = context_->systemSettings->setCanBitrate(interface, bitrate, tripleSampling);
        return QJsonObject{{QStringLiteral("ok"), ok}};
    });

    dispatcher_->registerMethod(QStringLiteral("sys.can.dump.start"),
                                 [this](const QJsonObject &params) {
        QString interface;
        if (!rpc::RpcHelpers::getString(params, "ifname", interface))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing ifname"));
        const bool ok = context_->systemSettings && context_->systemSettings->startCanDump(interface);
        return QJsonObject{{QStringLiteral("ok"), ok}};
    });

    dispatcher_->registerMethod(QStringLiteral("sys.can.dump.stop"),
                                 [this](const QJsonObject &) {
        if (context_->systemSettings)
            context_->systemSettings->stopCanDump();
        return rpc::RpcHelpers::ok(true);
    });
}

void RpcRegistry::registerCan()
{
    dispatcher_->registerMethod(QStringLiteral("can.send"),
                                 [this](const QJsonObject &params) {
        if (!context_->canBus)
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("CAN not ready"));

        qint32 id = 0;
        QByteArray data;
        bool extended = false;

        if (!rpc::RpcHelpers::getI32(params, "id", id) || id < 0)
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid id"));
        if (!rpc::RpcHelpers::getHexBytes(params, "dataHex", data))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid dataHex"));
        if (!rpc::RpcHelpers::getBool(params, "extended", extended, false))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid extended"));
        if (data.size() > 8)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("payload too long (>8)"));

        const bool ok = context_->canBus->sendFrame(static_cast<quint32>(id), data, extended, false);
        return QJsonObject{{QStringLiteral("ok"), ok}};
    });
}

void RpcRegistry::registerRelay()
{
    dispatcher_->registerMethod(QStringLiteral("relay.control"),
                                 [this](const QJsonObject &params) {
        quint8 node = 0, channel = 0;
        QString actionStr;

        if (!rpc::RpcHelpers::getU8(params, "node", node))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid node"));
        if (!rpc::RpcHelpers::getU8(params, "ch", channel) || channel > 3)
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
            {QStringLiteral("ok"), true},
            {QStringLiteral("jobId"), QString::number(result.jobId)},
            {QStringLiteral("queued"), !result.executedImmediately}
        };
        if (result.executedImmediately) {
            obj[QStringLiteral("success")] = result.success;
        }
        return obj;
    });

    dispatcher_->registerMethod(QStringLiteral("relay.query"),
                                 [this](const QJsonObject &params) {
        quint8 node = 0, channel = 0;

        if (!rpc::RpcHelpers::getU8(params, "node", node))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid node"));
        if (!rpc::RpcHelpers::getU8(params, "ch", channel) || channel > 3)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("missing/invalid ch(0..3)"));

        auto *dev = context_->relays.value(node, nullptr);
        if (!dev)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("unknown node"));

        return QJsonObject{{QStringLiteral("ok"), dev->query(channel)}};
    });

    dispatcher_->registerMethod(QStringLiteral("relay.status"),
                                 [this](const QJsonObject &params) {
        quint8 node = 0, channel = 0;

        if (!rpc::RpcHelpers::getU8(params, "node", node))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid node"));
        if (!rpc::RpcHelpers::getU8(params, "ch", channel) || channel > 3)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("missing/invalid ch(0..3)"));

        auto *dev = context_->relays.value(node, nullptr);
        if (!dev)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("unknown node"));

        const auto status = dev->lastStatus(channel);
        return QJsonObject{
            {QStringLiteral("ok"), true},
            {QStringLiteral("channel"), static_cast<int>(status.channel)},
            {QStringLiteral("statusByte"), static_cast<int>(status.statusByte)},
            {QStringLiteral("currentA"), static_cast<double>(status.currentA)},
            {QStringLiteral("mode"), static_cast<int>(device::RelayProtocol::modeBits(status.statusByte))},
            {QStringLiteral("phaseLost"), device::RelayProtocol::phaseLost(status.statusByte)}
        };
    });

    dispatcher_->registerMethod(QStringLiteral("relay.statusAll"),
                                 [this](const QJsonObject &params) {
        quint8 node = 0;
        if (!rpc::RpcHelpers::getU8(params, "node", node))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid node"));

        auto *dev = context_->relays.value(node, nullptr);
        if (!dev)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("unknown node"));

        QJsonArray channels;
        for (quint8 ch = 0; ch < 4; ++ch) {
            const auto status = dev->lastStatus(ch);
            QJsonObject obj;
            obj[kKeyCh] = static_cast<int>(ch);
            obj[kKeyChannel] = static_cast<int>(status.channel);
            obj[kKeyStatusByte] = static_cast<int>(status.statusByte);
            obj[kKeyCurrentA] = static_cast<double>(status.currentA);
            obj[kKeyMode] = static_cast<int>(device::RelayProtocol::modeBits(status.statusByte));
            obj[kKeyPhaseLost] = device::RelayProtocol::phaseLost(status.statusByte);
            channels.append(obj);
        }

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const qint64 lastSeen = dev->lastSeenMs();
        qint64 ageMs = 0;
        bool online = false;
        calcDeviceOnlineStatus(lastSeen, now, ageMs, online);

        return QJsonObject{
            {kKeyOk, true},
            {kKeyNode, static_cast<int>(node)},
            {kKeyOnline, online},
            {kKeyAgeMs, (ageMs >= 0) ? static_cast<double>(ageMs) : QJsonValue()},
            {kKeyChannels, channels}
        };
    });

    // 获取所有继电器节点列表，包含在线状态
    dispatcher_->registerMethod(QStringLiteral("relay.nodes"),
                                 [this](const QJsonObject &) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        QJsonArray arr;
        for (auto it = context_->relays.begin(); it != context_->relays.end(); ++it) {
            const quint8 node = it.key();
            auto *dev = it.value();
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
        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("queriedDevices"), queriedCount}
        };
    });
}

void RpcRegistry::registerGroup()
{
    dispatcher_->registerMethod(QStringLiteral("group.list"),
                                 [this](const QJsonObject &) {
        QJsonArray arr;
        for (auto it = context_->deviceGroups.begin(); it != context_->deviceGroups.end(); ++it) {
            QJsonObject obj;
            obj[QStringLiteral("groupId")] = it.key();
            obj[QStringLiteral("name")] = context_->groupNames.value(it.key(), QString());

            QJsonArray devices;
            for (quint8 node : it.value()) {
                devices.append(static_cast<int>(node));
            }
            obj[QStringLiteral("devices")] = devices;
            obj[QStringLiteral("deviceCount")] = it.value().size();

            arr.append(obj);
        }
        return QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("groups"), arr}};
    });

    // 获取指定分组详情
    dispatcher_->registerMethod(QStringLiteral("group.get"),
                                 [this](const QJsonObject &params) {
        qint32 groupId = 0;
        if (!rpc::RpcHelpers::getI32(params, "groupId", groupId))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing groupId"));

        if (!context_->deviceGroups.contains(groupId))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("group not found"));

        const auto &nodeList = context_->deviceGroups.value(groupId);
        const qint64 now = QDateTime::currentMSecsSinceEpoch();

        QJsonArray devices;
        int onlineCount = 0;
        for (quint8 node : nodeList) {
            auto *dev = context_->relays.value(node, nullptr);
            const qint64 lastSeen = dev ? dev->lastSeenMs() : 0;
            
            qint64 ageMs = 0;
            bool online = false;
            calcDeviceOnlineStatus(lastSeen, now, ageMs, online);
            if (online) onlineCount++;

            devices.append(buildDeviceStatusObject(node, ageMs, online));
        }

        return QJsonObject{
            {kKeyOk, true},
            {kKeyGroupId, groupId},
            {kKeyName, context_->groupNames.value(groupId, QString())},
            {kKeyDevices, devices},
            {kKeyDeviceCount, nodeList.size()},
            {QStringLiteral("onlineCount"), onlineCount}
        };
    });

    dispatcher_->registerMethod(QStringLiteral("group.create"),
                                 [this](const QJsonObject &params) {
        qint32 groupId = 0;
        QString name;

        if (!rpc::RpcHelpers::getI32(params, "groupId", groupId) || groupId <= 0)
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid groupId"));
        if (!rpc::RpcHelpers::getString(params, "name", name))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing name"));

        QString error;
        if (!context_->createGroup(groupId, name, &error))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, error);
        return QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("groupId"), groupId}};
    });

    dispatcher_->registerMethod(QStringLiteral("group.delete"),
                                 [this](const QJsonObject &params) {
        qint32 groupId = 0;

        if (!rpc::RpcHelpers::getI32(params, "groupId", groupId))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing groupId"));

        QString error;
        if (!context_->deleteGroup(groupId, &error))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, error);
        return QJsonObject{{QStringLiteral("ok"), true}};
    });

    dispatcher_->registerMethod(QStringLiteral("group.addDevice"),
                                 [this](const QJsonObject &params) {
        qint32 groupId = 0;
        quint8 node = 0;

        if (!rpc::RpcHelpers::getI32(params, "groupId", groupId))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing groupId"));
        if (!rpc::RpcHelpers::getU8(params, "node", node))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid node"));

        QString error;
        if (!context_->addDeviceToGroup(groupId, node, &error))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, error);
        return QJsonObject{{QStringLiteral("ok"), true}};
    });

    dispatcher_->registerMethod(QStringLiteral("group.removeDevice"),
                                 [this](const QJsonObject &params) {
        qint32 groupId = 0;
        quint8 node = 0;

        if (!rpc::RpcHelpers::getI32(params, "groupId", groupId))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing groupId"));
        if (!rpc::RpcHelpers::getU8(params, "node", node))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid node"));

        QString error;
        if (!context_->removeDeviceFromGroup(groupId, node, &error))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, error);
        return QJsonObject{{QStringLiteral("ok"), true}};
    });

    dispatcher_->registerMethod(QStringLiteral("group.control"),
                                 [this](const QJsonObject &params) {
        qint32 groupId = 0;
        quint8 channel = 0;
        QString actionStr;

        if (!rpc::RpcHelpers::getI32(params, "groupId", groupId))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing groupId"));
        if (!rpc::RpcHelpers::getU8(params, "ch", channel) || channel > 3)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("missing/invalid ch(0..3)"));
        if (!rpc::RpcHelpers::getString(params, "action", actionStr))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing action"));

        bool okAction = false;
        const auto action = context_->parseAction(actionStr, &okAction);
        if (!okAction)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("invalid action (stop/fwd/rev)"));

        if (!context_->deviceGroups.contains(groupId))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("group not found"));

        const auto stats = context_->queueGroupControl(groupId, channel, action, QStringLiteral("rpc:group.control"));
        QJsonArray jobs;
        for (quint64 id : stats.jobIds) {
            jobs.append(QString::number(id));
        }

        return QJsonObject{
            {QStringLiteral("ok"), true},
            {QStringLiteral("total"), stats.total},
            {QStringLiteral("accepted"), stats.accepted},
            {QStringLiteral("missing"), stats.missing},
            {QStringLiteral("jobIds"), jobs}
        };
    });
}

void RpcRegistry::registerAuto()
{
    dispatcher_->registerMethod(QStringLiteral("control.queue.status"),
                                 [this](const QJsonObject &) {
        const auto snapshot = context_->queueSnapshot();
        return QJsonObject{
            {QStringLiteral("ok"), true},
            {QStringLiteral("pending"), snapshot.pending},
            {QStringLiteral("active"), snapshot.active},
            {QStringLiteral("lastJobId"),
             snapshot.lastJobId ? QJsonValue(QString::number(snapshot.lastJobId)) : QJsonValue()}
        };
    });

    dispatcher_->registerMethod(QStringLiteral("control.queue.result"),
                                 [this](const QJsonObject &params) {
        if (!params.contains(QStringLiteral("jobId")))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing jobId"));

        quint64 jobId = 0;
        bool okId = false;
        const auto jobVal = params.value(QStringLiteral("jobId"));
        if (jobVal.isString()) {
            jobId = jobVal.toString().toULongLong(&okId);
        } else if (jobVal.isDouble()) {
            const double v = jobVal.toDouble();
            okId = true;
            if (v >= 0) jobId = static_cast<quint64>(v);
        }

        if (!okId)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("jobId must be integer or string"));
        if (jobId == 0)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("jobId must be positive"));

        const auto result = context_->jobResult(jobId);
        return QJsonObject{
            {QStringLiteral("jobId"), QString::number(jobId)},
            {QStringLiteral("ok"), result.ok},
            {QStringLiteral("message"), result.message},
            {QStringLiteral("finishedMs"), static_cast<double>(result.finishedMs)}
        };
    });

    dispatcher_->registerMethod(QStringLiteral("auto.strategy.list"),
                                 [this](const QJsonObject &) {
        QJsonArray arr;
        const auto states = context_->strategyStates();
        for (const auto &state : states) {
            QJsonObject obj;
            obj[QStringLiteral("id")] = state.config.strategyId;
            obj[QStringLiteral("name")] = state.config.name;
            obj[QStringLiteral("groupId")] = state.config.groupId;
            obj[QStringLiteral("channel")] = static_cast<int>(state.config.channel);
            obj[QStringLiteral("action")] = state.config.action;
            obj[QStringLiteral("intervalSec")] = state.config.intervalSec;
            obj[QStringLiteral("enabled")] = state.config.enabled;
            obj[QStringLiteral("autoStart")] = state.config.autoStart;
            obj[QStringLiteral("attached")] = state.attached;
            obj[QStringLiteral("running")] = state.running;
            arr.append(obj);
        }
        return QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("strategies"), arr}};
    });

    dispatcher_->registerMethod(QStringLiteral("auto.strategy.enable"),
                                 [this](const QJsonObject &params) {
        qint32 id = 0;
        bool enabled = true;

        if (!rpc::RpcHelpers::getI32(params, "id", id))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing id"));
        if (!params.contains(QStringLiteral("enabled")))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing enabled"));
        if (!rpc::RpcHelpers::getBool(params, "enabled", enabled, true))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid enabled"));

        if (!context_->setStrategyEnabled(id, enabled))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("strategy not found"));
        return QJsonObject{{QStringLiteral("ok"), true}};
    });

    dispatcher_->registerMethod(QStringLiteral("auto.strategy.trigger"),
                                 [this](const QJsonObject &params) {
        qint32 id = 0;

        if (!rpc::RpcHelpers::getI32(params, "id", id))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing id"));

        if (!context_->triggerStrategy(id))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("strategy not found or not attached"));
        return QJsonObject{{QStringLiteral("ok"), true}};
    });
}

}  // namespace core
}  // namespace fanzhou
