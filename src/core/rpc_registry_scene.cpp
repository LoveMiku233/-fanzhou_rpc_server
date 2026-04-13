/**
 * @file rpc_registry_scene.cpp
 * @brief 场景管理RPC方法注册
 */

#include "rpc_registry.h"
#include "core_context.h"
#include "rpc_registry_keys.h"

#include "cloud/fanzhoucloud/parser.h"
#include "rpc/json_rpc_dispatcher.h"
#include "rpc/rpc_error_codes.h"
#include "rpc/rpc_helpers.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>

namespace fanzhou {
namespace core {

namespace {
const QString &kKeyOk = rpc_keys::Ok();
}  // namespace

void RpcRegistry::registerScene()
{
    // 云端场景设置/更新 (cloud.scene.set)
    dispatcher_->registerMethod(QStringLiteral("cloud.scene.set"),
                                 [this](const QJsonObject &params) {
        // 验证必要参数
        if (!params.contains(QStringLiteral("data"))) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                QStringLiteral("missing data"));
        }

        const QJsonObject data = params.value(QStringLiteral("data")).toObject();
        const QString requestId = params.value(QStringLiteral("requestId")).toString();

        // 解析场景数据
        core::AutoStrategy strategy;
        QString parseError;
        if (!cloud::fanzhoucloud::parseSceneSetData(data, strategy, &parseError)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, parseError);
        }

        // 创建或更新策略
        bool isUpdate = false;
        QString createError;
        if (!context_->createStrategy(strategy, &isUpdate, &createError)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, createError);
        }

        // 保存配置
        QString saveError;
        context_->saveConfig(QString(), &saveError);

        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("sceneId"), strategy.strategyId},
            {QStringLiteral("version"), strategy.version},
            {QStringLiteral("isUpdate"), isUpdate},
            {QStringLiteral("requestId"), requestId}
        };
    });

    // 云端场景删除 (cloud.scene.delete)
    dispatcher_->registerMethod(QStringLiteral("cloud.scene.delete"),
                                 [this](const QJsonObject &params) {
        // 验证必要参数
        if (!params.contains(QStringLiteral("data"))) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                QStringLiteral("missing data"));
        }

        const QString requestId = params.value(QStringLiteral("requestId")).toString();
        const QJsonValue dataValue = params.value(QStringLiteral("data"));

        // 解析场景ID列表
        QList<int> sceneIds;
        QString parseError;
        if (!cloud::fanzhoucloud::parseDeleteCommand(
                QStringLiteral("scene"), dataValue, sceneIds, &parseError)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, parseError);
        }

        // 删除场景
        QJsonArray deletedIds;
        QJsonArray failedIds;
        for (int id : sceneIds) {
            QString deleteError;
            bool alreadyDeleted = false;
            if (context_->deleteStrategy(id, &deleteError, &alreadyDeleted)) {
                deletedIds.append(id);
            } else {
                if (!alreadyDeleted) {
                    failedIds.append(QJsonObject{
                        {QStringLiteral("id"), id},
                        {QStringLiteral("error"), deleteError}
                    });
                } else {
                    // 已删除的也算成功
                    deletedIds.append(id);
                }
            }
        }

        // 保存配置
        QString saveError;
        context_->saveConfig(QString(), &saveError);

        const bool allSuccess = failedIds.isEmpty();

        return QJsonObject{
            {kKeyOk, allSuccess},
            {QStringLiteral("deletedIds"), deletedIds},
            {QStringLiteral("failedIds"), failedIds},
            {QStringLiteral("requestId"), requestId}
        };
    });
}

}  // namespace core
}  // namespace fanzhou
