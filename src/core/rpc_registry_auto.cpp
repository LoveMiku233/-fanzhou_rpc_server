/**
 * @file rpc_registry_auto.cpp
 * @brief 自动策略与控制队列RPC方法注册
 */

#include "rpc_registry.h"
#include "core_context.h"
#include "rpc_registry_keys.h"

#include "cloud/fanzhoucloud/parser.h"
#include "cloud/fanzhoucloud/message_handler.h"
#include "rpc/json_rpc_dispatcher.h"
#include "rpc/rpc_error_codes.h"
#include "rpc/rpc_helpers.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <climits>

namespace fanzhou {
namespace core {

namespace {
const QString &kKeyOk = rpc_keys::Ok();
const QString &kKeyMessage = rpc_keys::Message();
const QString &kKeyEnabled = rpc_keys::Enabled();
const QString &kKeyNode = rpc_keys::Node();
const QString &kKeyChannel = rpc_keys::Channel();
const QString &kKeyName = rpc_keys::Name();
const QString &kKeyJobId = rpc_keys::JobId();
const QString &kKeyGroupId = rpc_keys::GroupId();
}  // namespace

void RpcRegistry::registerAuto()
{
    dispatcher_->registerMethod(QStringLiteral("control.queue.status"),
                                 [this](const QJsonObject &) {
        const auto snapshot = context_->queueSnapshot();
        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("pending"), snapshot.pending},
            {QStringLiteral("active"), snapshot.active},
            {QStringLiteral("lastJobId"),
             snapshot.lastJobId ? QJsonValue(QString::number(snapshot.lastJobId)) : QJsonValue()}
        };
    });

    dispatcher_->registerMethod(QStringLiteral("control.queue.result"),
                                 [this](const QJsonObject &params) {
        if (!params.contains(kKeyJobId))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing jobId"));

        quint64 jobId = 0;
        bool okId = false;
        const auto jobVal = params.value(kKeyJobId);
        if (jobVal.isString()) {
            jobId = jobVal.toString().toULongLong(&okId);
        } else if (jobVal.isDouble()) {
            const double v = jobVal.toDouble();
            okId = true;
            if (v >= 0) {
                jobId = static_cast<quint64>(v);
            }
        }

        if (!okId)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType,
                                        QStringLiteral("jobId must be integer or string"));
        if (jobId == 0)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue,
                                        QStringLiteral("jobId must be positive"));

        const auto result = context_->jobResult(jobId);
        return QJsonObject{{kKeyJobId, QString::number(jobId)},
                           {kKeyOk, result.ok},
                           {kKeyMessage, result.message},
                           {QStringLiteral("finishedMs"), static_cast<double>(result.finishedMs)}};
    });

    dispatcher_->registerMethod(QStringLiteral("auto.strategy.list"),
                                 [this](const QJsonObject &) {
        QJsonArray arr;
        const auto states = context_->strategyStates();
        for (const auto &state : states) {
            const auto &s = state.config;

            QJsonObject obj;
            obj[QStringLiteral("id")] = s.strategyId;
            obj[kKeyName] = s.strategyName;
            obj[QStringLiteral("type")] = s.strategyType;
            obj[kKeyEnabled] = s.enabled;
            obj[QStringLiteral("matchType")] = s.matchType;
            obj[QStringLiteral("version")] = s.version;
            obj[QStringLiteral("updateTime")] = s.updateTime;

            obj[QStringLiteral("effectiveBeginTime")] = s.effectiveBeginTime;
            obj[QStringLiteral("effectiveEndTime")] = s.effectiveEndTime;

            obj[QStringLiteral("attached")] = state.attached;
            obj[QStringLiteral("running")] = state.running;

            QJsonArray actArr;
            for (const auto &a : s.actions) {
                QJsonObject aobj;
                aobj[kKeyNode] = a.node;
                aobj[kKeyChannel] = a.channel;
                aobj[QStringLiteral("value")] = a.identifierValue;
                actArr.append(aobj);
            }
            obj[QStringLiteral("actions")] = actArr;

            QJsonArray condArr;
            for (const auto &c : s.conditions) {
                QJsonObject cobj;
                cobj[QStringLiteral("device")] = c.sensor_dev;
                cobj[QStringLiteral("identifier")] = c.identifier;
                cobj[QStringLiteral("op")] = c.op;
                cobj[QStringLiteral("value")] = c.identifierValue;
                condArr.append(cobj);
            }
            obj[QStringLiteral("conditions")] = condArr;

            arr.append(obj);
        }
        return QJsonObject{{kKeyOk, true}, {QStringLiteral("strategies"), arr}};
    });

    dispatcher_->registerMethod(QStringLiteral("auto.strategy.enable"),
                                 [this](const QJsonObject &params) {
        qint32 id = 0;
        bool enabled = true;

        if (!rpc::RpcHelpers::getI32InRange(params, "id", id, 1, INT_MAX))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing/invalid id"));
        if (!params.contains(kKeyEnabled))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing enabled"));
        if (!rpc::RpcHelpers::getBool(params, "enabled", enabled, true))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType,
                                        QStringLiteral("invalid enabled"));

        if (!context_->setStrategyEnabled(id, enabled))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue,
                                        QStringLiteral("strategy not found"));
        return QJsonObject{{kKeyOk, true}};
    });

    dispatcher_->registerMethod(QStringLiteral("auto.strategy.trigger"),
                                 [this](const QJsonObject &params) {
        qint32 id = 0;

        if (!rpc::RpcHelpers::getI32InRange(params, "id", id, 1, INT_MAX))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing/invalid id"));

        if (!context_->triggerStrategy(id))
            return rpc::RpcHelpers::err(
                rpc::RpcError::BadParameterValue,
                QStringLiteral("strategy not found or not attached"));
        return QJsonObject{{kKeyOk, true}};
    });

    dispatcher_->registerMethod(QStringLiteral("auto.strategy.create"),
                                 [this](const QJsonObject &params) {
        core::AutoStrategy s;
        QString err;
        bool isUpdate = false;

        if (!cloud::fanzhoucloud::parseAutoStrategyFromJson(params, s, &err)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, err);
        }

        if (!context_->createStrategy(s, &isUpdate, &err)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, err);
        }

        if (!context_->ensureGroupForStrategy(s, &err)) {
            return rpc::RpcHelpers::err(
                rpc::RpcError::BadParameterValue,
                QStringLiteral("create group failed: %1").arg(err));
        }

        return QJsonObject{{kKeyOk, true},
                           {QStringLiteral("id"), s.strategyId},
                           {kKeyGroupId, s.groupId}};
    });

    dispatcher_->registerMethod(QStringLiteral("auto.strategy.delete"),
                                 [this](const QJsonObject &params) {
        qint32 id = 0;

        if (!rpc::RpcHelpers::getI32InRange(params, "id", id, 1, INT_MAX))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing/invalid id"));

        QString error;
        if (!context_->deleteStrategy(id, &error))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, error);
        return QJsonObject{{kKeyOk, true}};
    });

    dispatcher_->registerMethod(QStringLiteral("auto.strategy.syncToCloud"),
                                 [this](const QJsonObject &params) {
        if (!context_->cloudMessageHandler) {
            return rpc::RpcHelpers::err(
                rpc::RpcError::InvalidState,
                QStringLiteral("Cloud message handler not available"));
        }

        QString syncMethod = QStringLiteral("set");
        rpc::RpcHelpers::getString(params, "method", syncMethod);

        qint32 id = -1;
        bool hasId = false;
        if (params.contains(QStringLiteral("id"))) {
            if (!rpc::RpcHelpers::getI32InRange(params, "id", id, 1, INT_MAX)) {
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue,
                                            QStringLiteral("invalid id"));
            }
            hasId = true;
        }

        int syncedCount = 0;
        QJsonArray syncedIds;

        const auto states = context_->strategyStates();

        for (const auto &state : states) {
            const auto &s = state.config;

            if (hasId && s.strategyId != id) {
                continue;
            }

            QJsonObject msgObj;
            msgObj.insert(QStringLiteral("method"), syncMethod);

            if (context_->cloudMessageHandler->sendStrategyCommand(s, msgObj)) {
                syncedCount++;
                syncedIds.append(s.strategyId);
            }
        }

        return QJsonObject{{kKeyOk, true},
                           {QStringLiteral("syncedCount"), syncedCount},
                           {QStringLiteral("syncedIds"), syncedIds},
                           {QStringLiteral("method"), syncMethod}};
    });
}

}  // namespace core
}  // namespace fanzhou
