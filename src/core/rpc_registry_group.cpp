/**
 * @file rpc_registry_group.cpp
 * @brief 分组RPC方法注册
 */

#include "rpc_registry.h"
#include "core_context.h"
#include "rpc_registry_common.h"
#include "rpc_registry_keys.h"

#include "device/can/relay_gd427.h"
#include "rpc/json_rpc_dispatcher.h"
#include "rpc/rpc_error_codes.h"
#include "rpc/rpc_helpers.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <algorithm>
#include <climits>

namespace fanzhou {
namespace core {

namespace {
const QString &kKeyOk = rpc_keys::Ok();
const QString &kKeyChannel = rpc_keys::Channel();
const QString &kKeyNode = rpc_keys::Node();
const QString &kKeyChannels = rpc_keys::Channels();
const QString &kKeyGroupId = rpc_keys::GroupId();
const QString &kKeyName = rpc_keys::Name();
const QString &kKeyDevices = rpc_keys::Devices();
const QString &kKeyDeviceCount = rpc_keys::DeviceCount();
const QString &kKeyGroups = rpc_keys::Groups();
const QString &kKeyTotal = rpc_keys::Total();
const QString &kKeyAccepted = rpc_keys::Accepted();
const QString &kKeyMissing = rpc_keys::Missing();
const QString &kKeyJobIds = rpc_keys::JobIds();
const QString kKeySpecialId = QStringLiteral("specialId");
const QString kKeyCanOptimizeFrame = QStringLiteral("canOptimizeFrame");
constexpr int kChannelKeyMultiplier = 256;

QJsonArray buildGroupChannelsArray(const QList<int> &channelKeys)
{
    QJsonArray channels;
    QList<int> keys = channelKeys;
    std::sort(keys.begin(), keys.end());
    for (int key : keys) {
        const int node = key / kChannelKeyMultiplier;
        const int ch = key % kChannelKeyMultiplier;
        channels.append(QJsonObject{
            {kKeyNode, node},
            {kKeyChannel, ch}
        });
    }
    return channels;
}

bool saveIfRequested(CoreContext *context, const QJsonObject &params, QJsonObject *err)
{
    const bool save = params.value(QStringLiteral("save")).toBool(false);
    if (!save) {
        return true;
    }
    QString saveError;
    if (!context->saveConfig(QString(), &saveError)) {
        if (err) {
            *err = rpc::RpcHelpers::err(rpc::RpcError::InternalError,
                                        QStringLiteral("group updated but save failed: %1")
                                            .arg(saveError));
        }
        return false;
    }
    return true;
}

}  // namespace

void RpcRegistry::registerGroup()
{
    dispatcher_->registerMethod(QStringLiteral("group.list"),
                                 [this](const QJsonObject &) {
        QJsonArray arr;
        QList<int> groupIds = context_->deviceGroups.keys();
        std::sort(groupIds.begin(), groupIds.end());
        for (int groupId : groupIds) {
            QJsonObject obj;
            obj[kKeyGroupId] = groupId;
            obj[kKeyName] = context_->groupNames.value(groupId, QString());
            const QString specialId = context_->groupSpecialIds.value(groupId, QString());
            if (!specialId.isEmpty()) {
                obj[kKeySpecialId] = specialId;
            }
            obj[kKeyCanOptimizeFrame] = context_->groupCanOptimizeFrame.value(groupId, true);

            QJsonArray devices;
            QList<quint8> nodes = context_->deviceGroups.value(groupId);
            std::sort(nodes.begin(), nodes.end());
            for (quint8 node : nodes) {
                devices.append(static_cast<int>(node));
            }
            obj[kKeyDevices] = devices;
            obj[kKeyDeviceCount] = nodes.size();

            // 直接返回通道列表，减少额外的RPC调用
            const auto channelKeys = context_->getGroupChannels(groupId);
            const QJsonArray channels = buildGroupChannelsArray(channelKeys);
            obj[kKeyChannels] = channels;
            obj[QStringLiteral("channelCount")] = channels.size();

            arr.append(obj);
        }
        return QJsonObject{{kKeyOk, true}, {kKeyGroups, arr}};
    });

    // 获取指定分组详情
    dispatcher_->registerMethod(QStringLiteral("group.get"),
                                 [this](const QJsonObject &params) {
        qint32 groupId = 0;
        if (!rpc::RpcHelpers::getI32InRange(params, "groupId", groupId, 1, INT_MAX))
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
            {kKeySpecialId, context_->groupSpecialIds.value(groupId, QString())},
            {kKeyCanOptimizeFrame, context_->groupCanOptimizeFrame.value(groupId, true)},
            {kKeyDevices, devices},
            {kKeyDeviceCount, nodeList.size()},
            {QStringLiteral("onlineCount"), onlineCount},
            {kKeyChannels, buildGroupChannelsArray(context_->getGroupChannels(groupId))}
        };
    });

    dispatcher_->registerMethod(QStringLiteral("group.create"),
                                 [this](const QJsonObject &params) {
        qint32 groupId = 0;
        QString name;
        QString specialId;
        bool canOptimizeFrame = true;

        if (!rpc::RpcHelpers::getI32InRange(params, "groupId", groupId, 1, INT_MAX))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid groupId"));
        if (!rpc::RpcHelpers::getString(params, "name", name))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing name"));
        if (params.contains(kKeySpecialId)) {
            specialId = params.value(kKeySpecialId).toString().trimmed();
        }
        if (params.contains(kKeyCanOptimizeFrame) &&
            !rpc::RpcHelpers::getBool(params, "canOptimizeFrame", canOptimizeFrame, true)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType,
                                        QStringLiteral("invalid canOptimizeFrame"));
        }

        QString error;
        if (!context_->createGroup(groupId, name, &error))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, error);
        if (!specialId.isEmpty()) {
            context_->groupSpecialIds.insert(groupId, specialId);
        }
        context_->groupCanOptimizeFrame.insert(groupId, canOptimizeFrame);
        QJsonObject saveErr;
        if (!saveIfRequested(context_, params, &saveErr)) {
            return saveErr;
        }
        return QJsonObject{{kKeyOk, true}, {kKeyGroupId, groupId}};
    });

    dispatcher_->registerMethod(QStringLiteral("group.delete"),
                                 [this](const QJsonObject &params) {
        qint32 groupId = 0;

        if (!rpc::RpcHelpers::getI32InRange(params, "groupId", groupId, 1, INT_MAX))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing groupId"));

        QString error;
        if (!context_->deleteGroup(groupId, &error))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, error);
        QJsonObject saveErr;
        if (!saveIfRequested(context_, params, &saveErr)) {
            return saveErr;
        }
        return QJsonObject{{kKeyOk, true}};
    });

    dispatcher_->registerMethod(QStringLiteral("group.addDevice"),
                                 [this](const QJsonObject &params) {
        qint32 groupId = 0;
        quint8 node = 0;

        if (!rpc::RpcHelpers::getI32InRange(params, "groupId", groupId, 1, INT_MAX))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing groupId"));
        if (!rpc::RpcHelpers::getU8InRange(params, "node", node, 1, 255))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid node"));

        QString error;
        if (!context_->addDeviceToGroup(groupId, node, &error))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, error);
        QJsonObject saveErr;
        if (!saveIfRequested(context_, params, &saveErr)) {
            return saveErr;
        }
        return QJsonObject{{kKeyOk, true}};
    });

    dispatcher_->registerMethod(QStringLiteral("group.removeDevice"),
                                 [this](const QJsonObject &params) {
        qint32 groupId = 0;
        quint8 node = 0;

        if (!rpc::RpcHelpers::getI32InRange(params, "groupId", groupId, 1, INT_MAX))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing groupId"));
        if (!rpc::RpcHelpers::getU8InRange(params, "node", node, 1, 255))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid node"));

        QString error;
        if (!context_->removeDeviceFromGroup(groupId, node, &error))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, error);
        QJsonObject saveErr;
        if (!saveIfRequested(context_, params, &saveErr)) {
            return saveErr;
        }
        return QJsonObject{{kKeyOk, true}};
    });

    dispatcher_->registerMethod(QStringLiteral("group.control"),
                                 [this](const QJsonObject &params) {
        qint32 groupId = 0;
        qint32 channel = -1;  // 默认-1表示使用分组绑定的通道
        QString actionStr;

        if (!rpc::RpcHelpers::getI32InRange(params, "groupId", groupId, 1, INT_MAX))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing groupId"));
        // channel是可选参数，默认-1表示使用分组绑定的通道
        if (params.contains(QStringLiteral("ch")) &&
            !rpc::RpcHelpers::getI32InRange(params, "ch", channel, -1, kMaxChannelId)) {
            return rpc::RpcHelpers::err(
                rpc::RpcError::BadParameterValue,
                QStringLiteral("invalid ch (-1 for bound channels, or 0-%1)")
                    .arg(kMaxChannelId));
        }
        if (!rpc::RpcHelpers::getString(params, "action", actionStr))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing action"));

        bool okAction = false;
        const auto action = context_->parseAction(actionStr, &okAction);
        if (!okAction)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("invalid action (stop/fwd/rev)"));

        if (!context_->deviceGroups.contains(groupId))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("group not found"));

        // channel=-1 表示使用分组绑定的通道，0-kMaxChannelId 表示控制所有设备的指定通道
        GroupControlStats stats;
        if (channel >= 0 && channel <= kMaxChannelId) {
            // 指定通道：对分组中所有设备的指定通道发送控制命令
            stats = context_->queueGroupControl(groupId, static_cast<quint8>(channel), action, QStringLiteral("rpc:group.control"));
        } else {
            // channel=-1：只控制分组中绑定的特定通道
            stats = context_->queueGroupBoundChannelsControl(groupId, action, QStringLiteral("rpc:group.control"));
        }
        QJsonArray jobs;
        for (quint64 id : stats.jobIds) {
            jobs.append(QString::number(id));
        }

        return QJsonObject{
            {kKeyOk, true},
            {kKeyTotal, stats.total},
            {kKeyAccepted, stats.accepted},
            {kKeyMissing, stats.missing},
            {kKeyJobIds, jobs}
        };
    });

    // 添加通道到分组
    dispatcher_->registerMethod(QStringLiteral("group.addChannel"),
                                 [this](const QJsonObject &params) {
        qint32 groupId = 0;
        quint8 node = 0;
        qint32 channel = 0;

        if (!rpc::RpcHelpers::getI32InRange(params, "groupId", groupId, 1, INT_MAX))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing groupId"));
        if (!rpc::RpcHelpers::getU8InRange(params, "node", node, 1, 255))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid node"));
        if (!rpc::RpcHelpers::getI32InRange(params, "channel", channel, 0, kMaxChannelId))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing/invalid channel (0..%1)").arg(kMaxChannelId));

        QString error;
        if (!context_->addChannelToGroup(groupId, node, channel, &error))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, error);
        QJsonObject saveErr;
        if (!saveIfRequested(context_, params, &saveErr)) {
            return saveErr;
        }
        return QJsonObject{{kKeyOk, true}};
    });

    // 从分组移除通道
    dispatcher_->registerMethod(QStringLiteral("group.removeChannel"),
                                 [this](const QJsonObject &params) {
        qint32 groupId = 0;
        quint8 node = 0;
        qint32 channel = 0;

        if (!rpc::RpcHelpers::getI32InRange(params, "groupId", groupId, 1, INT_MAX))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing groupId"));
        if (!rpc::RpcHelpers::getU8InRange(params, "node", node, 1, 255))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid node"));
        if (!rpc::RpcHelpers::getI32InRange(params, "channel", channel, 0, kMaxChannelId))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing/invalid channel (0..%1)").arg(kMaxChannelId));

        QString error;
        if (!context_->removeChannelFromGroup(groupId, node, channel, &error))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, error);
        QJsonObject saveErr;
        if (!saveIfRequested(context_, params, &saveErr)) {
            return saveErr;
        }
        return QJsonObject{{kKeyOk, true}};
    });

    // 获取分组的通道列表
    dispatcher_->registerMethod(QStringLiteral("group.getChannels"),
                                 [this](const QJsonObject &params) {
        qint32 groupId = 0;

        if (!rpc::RpcHelpers::getI32InRange(params, "groupId", groupId, 1, INT_MAX))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing groupId"));

        if (!context_->deviceGroups.contains(groupId))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("group not found"));

        const QJsonArray arr = buildGroupChannelsArray(context_->getGroupChannels(groupId));

        return QJsonObject{
            {kKeyOk, true},
            {kKeyGroupId, groupId},
            {kKeyChannels, arr},
            {kKeyTotal, arr.size()}
        };
    });

    // 批量替换分组绑定通道
    dispatcher_->registerMethod(QStringLiteral("group.channels.replace"),
                                 [this](const QJsonObject &params) {
        qint32 groupId = 0;
        if (!rpc::RpcHelpers::getI32InRange(params, "groupId", groupId, 1, INT_MAX)) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing/invalid groupId"));
        }
        if (!context_->deviceGroups.contains(groupId)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue,
                                        QStringLiteral("group not found"));
        }
        if (!params.contains(kKeyChannels) || !params.value(kKeyChannels).isArray()) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing/invalid channels array"));
        }

        const QJsonArray channelsArr = params.value(kKeyChannels).toArray();
        QSet<int> newChannelKeySet;
        QSet<int> newNodeSet;
        newNodeSet.reserve(context_->deviceGroups.value(groupId).size() + channelsArr.size());
        for (quint8 node : context_->deviceGroups.value(groupId)) {
            newNodeSet.insert(static_cast<int>(node));
        }

        // 先全量校验和归一化，保证后续提交阶段不失败
        for (const QJsonValue &value : channelsArr) {
            if (!value.isObject()) {
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType,
                                            QStringLiteral("channels item must be object"));
            }
            const QJsonObject obj = value.toObject();
            qint32 node = 0;
            qint32 channel = 0;
            if (!rpc::RpcHelpers::getI32InRange(obj, "node", node, 1, 255)) {
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue,
                                            QStringLiteral("invalid node in channels item (1..255)"));
            }
            if (!rpc::RpcHelpers::getI32InRange(obj, "channel", channel, 0, kMaxChannelId)) {
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue,
                                            QStringLiteral("invalid channel in channels item (0..%1)")
                                                .arg(kMaxChannelId));
            }

            if (!context_->relays.contains(static_cast<quint8>(node))) {
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue,
                                            QStringLiteral("device not found: %1").arg(node));
            }
            newNodeSet.insert(node);
            newChannelKeySet.insert(node * kChannelKeyMultiplier + channel);
        }

        // 原子提交：全部校验通过后一次性覆盖
        QList<int> newKeys = newChannelKeySet.values();
        std::sort(newKeys.begin(), newKeys.end());
        context_->groupChannels[groupId] = newKeys;

        QList<quint8> newNodes;
        QList<int> nodeIds = newNodeSet.values();
        std::sort(nodeIds.begin(), nodeIds.end());
        newNodes.reserve(nodeIds.size());
        for (int node : nodeIds) {
            newNodes.append(static_cast<quint8>(node));
        }
        context_->deviceGroups[groupId] = newNodes;

        QJsonObject saveErr;
        if (!saveIfRequested(context_, params, &saveErr)) {
            return saveErr;
        }

        return QJsonObject{
            {kKeyOk, true},
            {kKeyGroupId, groupId},
            {kKeyTotal, newKeys.size()},
            {kKeyChannels, buildGroupChannelsArray(context_->getGroupChannels(groupId))}
        };
    });

    dispatcher_->registerMethod(QStringLiteral("group.setSpecialId"),
                                 [this](const QJsonObject &params) {
        qint32 groupId = 0;
        if (!rpc::RpcHelpers::getI32InRange(params, "groupId", groupId, 1, INT_MAX)) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing/invalid groupId"));
        }
        if (!context_->deviceGroups.contains(groupId)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue,
                                        QStringLiteral("group not found"));
        }
        if (!params.contains(kKeySpecialId)) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing specialId"));
        }
        const QString specialId = params.value(kKeySpecialId).toString().trimmed();
        if (specialId.isEmpty()) {
            context_->groupSpecialIds.remove(groupId);
        } else {
            context_->groupSpecialIds.insert(groupId, specialId);
        }
        QJsonObject saveErr;
        if (!saveIfRequested(context_, params, &saveErr)) {
            return saveErr;
        }
        return QJsonObject{
            {kKeyOk, true},
            {kKeyGroupId, groupId},
            {kKeySpecialId, context_->groupSpecialIds.value(groupId, QString())}
        };
    });

    dispatcher_->registerMethod(QStringLiteral("group.setOptimizeFrame"),
                                 [this](const QJsonObject &params) {
        qint32 groupId = 0;
        bool canOptimizeFrame = true;
        if (!rpc::RpcHelpers::getI32InRange(params, "groupId", groupId, 1, INT_MAX)) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing/invalid groupId"));
        }
        if (!context_->deviceGroups.contains(groupId)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue,
                                        QStringLiteral("group not found"));
        }
        if (!params.contains(kKeyCanOptimizeFrame) ||
            !rpc::RpcHelpers::getBool(params, "canOptimizeFrame", canOptimizeFrame, true)) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing/invalid canOptimizeFrame"));
        }

        context_->groupCanOptimizeFrame.insert(groupId, canOptimizeFrame);
        QJsonObject saveErr;
        if (!saveIfRequested(context_, params, &saveErr)) {
            return saveErr;
        }

        return QJsonObject{
            {kKeyOk, true},
            {kKeyGroupId, groupId},
            {kKeyCanOptimizeFrame, context_->groupCanOptimizeFrame.value(groupId, true)}
        };
    });

    dispatcher_->registerMethod(QStringLiteral("greenhouse.state.get"),
                                 [this](const QJsonObject &) {
        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("state"), context_->greenhouseState}
        };
    });

    dispatcher_->registerMethod(QStringLiteral("greenhouse.state.save"),
                                 [this](const QJsonObject &params) {
        if (!params.contains(QStringLiteral("state")) ||
            !params.value(QStringLiteral("state")).isObject()) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing/invalid state object"));
        }

        context_->greenhouseState = params.value(QStringLiteral("state")).toObject();

        QJsonObject saveParams = params;
        if (!saveParams.contains(QStringLiteral("save"))) {
            saveParams.insert(QStringLiteral("save"), true);
        }
        QJsonObject saveErr;
        if (!saveIfRequested(context_, saveParams, &saveErr)) {
            return saveErr;
        }
        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("state"), context_->greenhouseState}
        };
    });
}


}  // namespace core
}  // namespace fanzhou
