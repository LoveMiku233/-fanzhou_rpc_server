/**
 * @file rpc_registry_auth.cpp
 * @brief 认证RPC方法注册
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
const QString &kKeyEnabled = rpc_keys::Enabled();
}  // namespace

void RpcRegistry::registerAuth()
{
    // 获取认证配置状态（不含敏感信息）
    dispatcher_->registerMethod(QStringLiteral("auth.status"),
                                 [this](const QJsonObject &) {
        return QJsonObject{
            {kKeyOk, true},
            {kKeyEnabled, context_->authConfig.enabled},
            {QStringLiteral("tokenExpireSec"), context_->authConfig.tokenExpireSec},
            {QStringLiteral("publicMethodsCount"), context_->authConfig.publicMethods.size()},
            {QStringLiteral("whitelistCount"), context_->authConfig.whitelist.size()}
        };
    });

    // 登录获取token
    dispatcher_->registerMethod(QStringLiteral("auth.login"),
                                 [this](const QJsonObject &params) {
        QString username;
        QString password;

        if (!rpc::RpcHelpers::getString(params, "username", username)) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing username"));
        }
        if (!rpc::RpcHelpers::getString(params, "password", password)) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing password"));
        }

        QString token;
        QString error;
        if (!context_->generateToken(username, password, &token, &error)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, error);
        }

        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("token"), token},
            {QStringLiteral("expiresIn"), context_->authConfig.tokenExpireSec},
            {kKeyMessage, QStringLiteral("Authentication successful")}
        };
    });

    // 验证token是否有效
    dispatcher_->registerMethod(QStringLiteral("auth.verify"),
                                 [this](const QJsonObject &params) {
        QString token;

        if (!rpc::RpcHelpers::getString(params, "token", token)) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing token"));
        }

        const bool valid = context_->verifyToken(token);
        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("valid"), valid}
        };
    });

    // 设置认证配置（需要已认证）
    dispatcher_->registerMethod(QStringLiteral("auth.configure"),
                                 [this](const QJsonObject &params) {
        bool enabled = context_->authConfig.enabled;
        QString secret;
        qint32 tokenExpireSec = context_->authConfig.tokenExpireSec;

        // 可选参数
        if (params.contains(kKeyEnabled)) {
            if (!rpc::RpcHelpers::getBool(params, "enabled", enabled, true)) {
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid enabled"));
            }
        }
        if (params.contains(QStringLiteral("secret"))) {
            rpc::RpcHelpers::getString(params, "secret", secret);
        }
        if (params.contains(QStringLiteral("tokenExpireSec"))) {
            rpc::RpcHelpers::getI32(params, "tokenExpireSec", tokenExpireSec);
        }

        // 更新配置
        context_->authConfig.enabled = enabled;
        if (!secret.isEmpty()) {
            context_->authConfig.secret = secret;
        }
        context_->authConfig.tokenExpireSec = tokenExpireSec;

        return QJsonObject{
            {kKeyOk, true},
            {kKeyMessage, QStringLiteral("Authentication configuration updated")}
        };
    });

    // 添加允许的token
    dispatcher_->registerMethod(QStringLiteral("auth.addToken"),
                                 [this](const QJsonObject &params) {
        QString token;

        if (!rpc::RpcHelpers::getString(params, "token", token) || token.isEmpty()) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid token"));
        }

        if (!context_->authConfig.allowedTokens.contains(token)) {
            context_->authConfig.allowedTokens.append(token);
        }

        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("totalTokens"), context_->authConfig.allowedTokens.size()}
        };
    });

    // 删除允许的token
    dispatcher_->registerMethod(QStringLiteral("auth.removeToken"),
                                 [this](const QJsonObject &params) {
        QString token;

        if (!rpc::RpcHelpers::getString(params, "token", token)) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing token"));
        }

        context_->authConfig.allowedTokens.removeAll(token);

        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("totalTokens"), context_->authConfig.allowedTokens.size()}
        };
    });

    // 添加IP白名单
    dispatcher_->registerMethod(QStringLiteral("auth.addWhitelist"),
                                 [this](const QJsonObject &params) {
        QString ip;

        if (!rpc::RpcHelpers::getString(params, "ip", ip) || ip.isEmpty()) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing/invalid ip"));
        }

        if (!context_->authConfig.whitelist.contains(ip)) {
            context_->authConfig.whitelist.append(ip);
        }

        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("totalWhitelist"), context_->authConfig.whitelist.size()}
        };
    });

    // 从IP白名单移除
    dispatcher_->registerMethod(QStringLiteral("auth.removeWhitelist"),
                                 [this](const QJsonObject &params) {
        QString ip;

        if (!rpc::RpcHelpers::getString(params, "ip", ip)) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing ip"));
        }

        context_->authConfig.whitelist.removeAll(ip);

        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("totalWhitelist"), context_->authConfig.whitelist.size()}
        };
    });
}


}  // namespace core
}  // namespace fanzhou
