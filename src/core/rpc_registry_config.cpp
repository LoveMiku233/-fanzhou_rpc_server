/**
 * @file rpc_registry_config.cpp
 * @brief 配置RPC方法注册
 */

#include "rpc_registry.h"
#include "core_context.h"
#include "rpc_registry_keys.h"

#include "rpc/json_rpc_dispatcher.h"
#include "rpc/rpc_error_codes.h"
#include "rpc/rpc_helpers.h"

#include <QJsonObject>

namespace fanzhou {
namespace core {

namespace {
const QString &kKeyOk = rpc_keys::Ok();
const QString &kKeyMessage = rpc_keys::Message();
}  // namespace

void RpcRegistry::registerConfig()
{
    // 获取当前配置
    dispatcher_->registerMethod(QStringLiteral("config.get"),
                                 [this](const QJsonObject &) {
        QJsonObject config = context_->exportConfig();
        config[kKeyOk] = true;
        return config;
    });

    // 保存配置到文件
    dispatcher_->registerMethod(QStringLiteral("config.save"),
                                 [this](const QJsonObject &params) {
        QString path;
        rpc::RpcHelpers::getString(params, "path", path);  // 可选参数
        
        QString error;
        if (!context_->saveConfig(path, &error)) {
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, error);
        }
        
        return QJsonObject{
            {kKeyOk, true},
            {kKeyMessage, QStringLiteral("配置已保存")}
        };
    });

    // 从文件重新加载配置
    // 注意：这会覆盖当前未保存的修改
    dispatcher_->registerMethod(QStringLiteral("config.reload"),
                                 [this](const QJsonObject &params) {
        QString path;
        rpc::RpcHelpers::getString(params, "path", path);  // 可选参数
        
        QString error;
        if (!context_->reloadConfig(path, &error)) {
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, error);
        }
        
        return QJsonObject{
            {kKeyOk, true},
            {kKeyMessage, QStringLiteral("配置已重新加载")}
        };
    });
}

}  // namespace core
}  // namespace fanzhou
