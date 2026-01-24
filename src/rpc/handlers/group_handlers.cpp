/**
 * @file group_handlers.cpp
 * @brief 设备分组相关RPC处理器实现
 */

#include "group_handlers.h"
#include "rpc_handler_base.h"

#include "comm/can/can_comm.h"

namespace fanzhou {
namespace rpc {

using namespace RpcKeys;
using namespace RpcConst;
using namespace RpcUtils;

void registerGroupHandlers(core::CoreContext *context, JsonRpcDispatcher *dispatcher)
{
    // ===================== 分组列表和详情 =====================

    dispatcher->registerMethod(QStringLiteral("group.list"),
                                 [context](const QJsonObject &) {
        QJsonArray arr;
        for (auto it = context->deviceGroups.begin(); it != context->deviceGroups.end(); ++it) {
            QJsonObject obj;
            obj[QStringLiteral("groupId")] = it.key();
            obj[QStringLiteral("name")] = context->groupNames.value(it.key(), QString());

            QJsonArray devices;
            for (quint8 node : it.value()) {
                devices.append(static_cast<int>(node));
            }
            obj[QStringLiteral("devices")] = devices;
            obj[QStringLiteral("deviceCount")] = it.value().size();

            const auto channelKeys = context->getGroupChannels(it.key());
            QJsonArray channels;
            for (int key : channelKeys) {
                const int node = key / 256;
                const int ch = key % 256;
                channels.append(QJsonObject{
                    {keyNode(), node},
                    {keyChannel(), ch}
                });
            }
            obj[keyChannels()] = channels;
            obj[QStringLiteral("channelCount")] = channels.size();

            arr.append(obj);
        }
        return QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("groups"), arr}};
    });

    dispatcher->registerMethod(QStringLiteral("group.get"),
                                 [context](const QJsonObject &params) {
        qint32 groupId = 0;
        if (!RpcHelpers::getI32(params, "groupId", groupId))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing groupId"));

        if (!context->deviceGroups.contains(groupId))
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("group not found"));

        QJsonObject obj;
        obj[keyOk()] = true;
        obj[keyGroupId()] = groupId;
        obj[keyName()] = context->groupNames.value(groupId, QString());

        QJsonArray devices;
        for (quint8 node : context->deviceGroups.value(groupId)) {
            devices.append(static_cast<int>(node));
        }
        obj[keyDevices()] = devices;
        obj[keyDeviceCount()] = devices.size();

        const auto channelKeys = context->getGroupChannels(groupId);
        QJsonArray channels;
        for (int key : channelKeys) {
            const int node = key / 256;
            const int ch = key % 256;
            channels.append(QJsonObject{
                {keyNode(), node},
                {keyChannel(), ch}
            });
        }
        obj[keyChannels()] = channels;
        obj[QStringLiteral("channelCount")] = channels.size();

        return obj;
    });

    // ===================== 分组管理 =====================

    dispatcher->registerMethod(QStringLiteral("group.create"),
                                 [context](const QJsonObject &params) {
        qint32 groupId = 0;
        QString name;

        if (!RpcHelpers::getI32(params, "groupId", groupId) || groupId <= 0)
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing/invalid groupId"));
        if (!RpcHelpers::getString(params, "name", name))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing name"));

        QString error;
        if (!context->createGroup(groupId, name, &error))
            return RpcHelpers::err(RpcError::BadParameterValue, error);
        return QJsonObject{{keyOk(), true}, {keyGroupId(), groupId}};
    });

    dispatcher->registerMethod(QStringLiteral("group.delete"),
                                 [context](const QJsonObject &params) {
        qint32 groupId = 0;

        if (!RpcHelpers::getI32(params, "groupId", groupId))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing groupId"));

        QString error;
        if (!context->deleteGroup(groupId, &error))
            return RpcHelpers::err(RpcError::BadParameterValue, error);
        return QJsonObject{{keyOk(), true}};
    });

    dispatcher->registerMethod(QStringLiteral("group.addDevice"),
                                 [context](const QJsonObject &params) {
        qint32 groupId = 0;
        quint8 node = 0;

        if (!RpcHelpers::getI32(params, "groupId", groupId))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing groupId"));
        if (!RpcHelpers::getU8(params, "node", node))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing node"));

        QString error;
        if (!context->addDeviceToGroup(groupId, node, &error))
            return RpcHelpers::err(RpcError::BadParameterValue, error);
        return QJsonObject{{keyOk(), true}};
    });

    dispatcher->registerMethod(QStringLiteral("group.removeDevice"),
                                 [context](const QJsonObject &params) {
        qint32 groupId = 0;
        quint8 node = 0;

        if (!RpcHelpers::getI32(params, "groupId", groupId))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing groupId"));
        if (!RpcHelpers::getU8(params, "node", node))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing node"));

        QString error;
        if (!context->removeDeviceFromGroup(groupId, node, &error))
            return RpcHelpers::err(RpcError::BadParameterValue, error);
        return QJsonObject{{keyOk(), true}};
    });

    // ===================== 通道管理 =====================

    dispatcher->registerMethod(QStringLiteral("group.addChannel"),
                                 [context](const QJsonObject &params) {
        qint32 groupId = 0;
        quint8 node = 0;
        qint32 channel = 0;

        if (!RpcHelpers::getI32(params, "groupId", groupId))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing groupId"));
        if (!RpcHelpers::getU8(params, "node", node))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing node"));
        if (!RpcHelpers::getI32(params, "channel", channel) || channel < 0 || channel > kMaxChannelId)
            return RpcHelpers::err(RpcError::BadParameterValue,
                QStringLiteral("missing/invalid channel (0-%1)").arg(kMaxChannelId));

        QString error;
        if (!context->addChannelToGroup(groupId, node, channel, &error))
            return RpcHelpers::err(RpcError::BadParameterValue, error);
        return QJsonObject{{keyOk(), true}};
    });

    dispatcher->registerMethod(QStringLiteral("group.removeChannel"),
                                 [context](const QJsonObject &params) {
        qint32 groupId = 0;
        quint8 node = 0;
        qint32 channel = 0;

        if (!RpcHelpers::getI32(params, "groupId", groupId))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing groupId"));
        if (!RpcHelpers::getU8(params, "node", node))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing node"));
        if (!RpcHelpers::getI32(params, "channel", channel))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing channel"));

        QString error;
        if (!context->removeChannelFromGroup(groupId, node, channel, &error))
            return RpcHelpers::err(RpcError::BadParameterValue, error);
        return QJsonObject{{keyOk(), true}};
    });

    // ===================== 分组控制 =====================

    dispatcher->registerMethod(QStringLiteral("group.control"),
                                 [context](const QJsonObject &params) {
        qint32 groupId = 0;
        quint8 channel = 0;
        QString actionStr;

        if (!RpcHelpers::getI32(params, "groupId", groupId))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing groupId"));
        if (!RpcHelpers::getU8(params, "ch", channel) || channel > kMaxChannelId)
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("missing/invalid ch(0..3)"));
        if (!RpcHelpers::getString(params, "action", actionStr))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing action"));

        bool okAction = false;
        const auto action = context->parseAction(actionStr, &okAction);
        if (!okAction)
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("invalid action (stop/fwd/rev)"));

        if (!context->deviceGroups.contains(groupId))
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("group not found"));

        const auto stats = context->queueGroupControl(groupId, channel, action,
            QStringLiteral("rpc:group.control"));

        QJsonArray jobs;
        for (quint64 id : stats.jobIds) {
            jobs.append(QString::number(id));
        }

        return QJsonObject{
            {keyOk(), true},
            {keyTotal(), stats.total},
            {keyAccepted(), stats.accepted},
            {keyMissing(), stats.missing},
            {keyJobIds(), jobs}
        };
    });

    // 分组控制优化版
    dispatcher->registerMethod(QStringLiteral("group.controlOptimized"),
                                 [context](const QJsonObject &params) {
        qint32 groupId = 0;
        qint32 channel = -1;
        QString actionStr;

        if (!RpcHelpers::getI32(params, "groupId", groupId))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing groupId"));
        RpcHelpers::getI32(params, "ch", channel);
        if (channel < -1 || channel > kMaxChannelId)
            return RpcHelpers::err(RpcError::BadParameterValue,
                QStringLiteral("invalid ch (-1 for bound channels, or 0-%1)").arg(kMaxChannelId));
        if (!RpcHelpers::getString(params, "action", actionStr))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing action"));

        bool okAction = false;
        const auto action = context->parseAction(actionStr, &okAction);
        if (!okAction)
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("invalid action (stop/fwd/rev)"));

        if (!context->deviceGroups.contains(groupId))
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("group not found"));

        const auto stats = context->queueGroupControlOptimized(groupId, channel, action,
            QStringLiteral("rpc:group.controlOptimized"));

        QJsonArray jobs;
        for (quint64 id : stats.jobIds) {
            jobs.append(QString::number(id));
        }

        QJsonObject result{
            {keyOk(), true},
            {keyTotal(), stats.total},
            {keyAccepted(), stats.accepted},
            {keyMissing(), stats.missing},
            {keyJobIds(), jobs},
            {QStringLiteral("originalFrames"), stats.originalFrameCount},
            {QStringLiteral("optimizedFrames"), stats.optimizedFrameCount},
            {QStringLiteral("framesSaved"), stats.originalFrameCount - stats.optimizedFrameCount}
        };

        if (context->canBus) {
            result[QStringLiteral("txQueueSize")] = context->canBus->txQueueSize();
        }

        return result;
    });

    // ===================== 控制队列 =====================

    dispatcher->registerMethod(QStringLiteral("control.queue"),
                                 [context](const QJsonObject &) {
        const auto snapshot = context->queueSnapshot();
        return QJsonObject{
            {keyOk(), true},
            {keyPending(), snapshot.pending},
            {keyActive(), snapshot.active},
            {keyLastJobId(), QString::number(snapshot.lastJobId)}
        };
    });

    dispatcher->registerMethod(QStringLiteral("control.job"),
                                 [context](const QJsonObject &params) {
        QString jobIdStr;
        if (!RpcHelpers::getString(params, "jobId", jobIdStr))
            return RpcHelpers::err(RpcError::MissingParameter, QStringLiteral("missing jobId"));

        bool ok = false;
        const quint64 jobId = jobIdStr.toULongLong(&ok);
        if (!ok)
            return RpcHelpers::err(RpcError::BadParameterValue, QStringLiteral("invalid jobId"));

        const auto result = context->jobResult(jobId);
        return QJsonObject{
            {keyOk(), result.ok},
            {keyMessage(), result.message},
            {keyFinishedMs(), result.finishedMs > 0 ? QString::number(result.finishedMs) : QString()}
        };
    });
}

}  // namespace rpc
}  // namespace fanzhou
