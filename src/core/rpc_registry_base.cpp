/**
 * @file rpc_registry_base.cpp
 * @brief 基础RPC方法注册
 */

#include "rpc_registry.h"
#include "core_context.h"
#include "rpc_registry_keys.h"

#include "rpc/json_rpc_dispatcher.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace fanzhou {
namespace core {

namespace {
const QString &kKeyOk = rpc_keys::Ok();
}  // namespace

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
        return QJsonObject{{kKeyOk, true}};
    });

    dispatcher_->registerMethod(QStringLiteral("echo"),
                                 [](const QJsonObject &params) {
        return QJsonValue(params);
    });

    dispatcher_->registerMethod(QStringLiteral("rpc.stats"),
                                 [this](const QJsonObject &params) {
        int limit = 20;
        if (params.contains(QStringLiteral("limit"))) {
            limit = params.value(QStringLiteral("limit")).toInt(20);
        }
        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("stats"), dispatcher_->stats(limit)},
            {QStringLiteral("methodCount"), dispatcher_->methods().size()}
        };
    });

    dispatcher_->registerMethod(QStringLiteral("rpc.stats.reset"),
                                 [this](const QJsonObject &) {
        dispatcher_->resetStats();
        return QJsonObject{{kKeyOk, true}};
    });
}

}  // namespace core
}  // namespace fanzhou
