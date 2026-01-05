/**
 * @file json_rpc_dispatcher.cpp
 * @brief JSON-RPC方法分发器实现
 */

#include "json_rpc_dispatcher.h"
#include "utils/logger.h"

#include <algorithm>

namespace fanzhou {
namespace rpc {

namespace {
const char *const kLogSource = "RpcDispatcher";
}

void JsonRpcDispatcher::registerMethod(const QString &method, Handler handler)
{
    handlers_[method] = std::move(handler);
    LOG_DEBUG(kLogSource, QStringLiteral("Registered RPC method: %1").arg(method));
}

QStringList JsonRpcDispatcher::methods() const
{
    auto keys = handlers_.keys();
    std::sort(keys.begin(), keys.end());
    return keys;
}

QJsonObject JsonRpcDispatcher::makeError(const QJsonValue &id, int code,
                                          const QString &message)
{
    return QJsonObject{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), id.isUndefined() ? QJsonValue(QJsonValue::Null) : id},
        {QStringLiteral("error"), QJsonObject{
            {QStringLiteral("code"), code},
            {QStringLiteral("message"), message}
        }}
    };
}

QJsonObject JsonRpcDispatcher::makeResult(const QJsonValue &id,
                                           const QJsonValue &result)
{
    return QJsonObject{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), id},
        {QStringLiteral("result"), result}
    };
}

QJsonObject JsonRpcDispatcher::handle(const QJsonObject &request) const
{
    // 验证JSON-RPC版本
    if (request.value(QStringLiteral("jsonrpc")).toString() != QStringLiteral("2.0")) {
        LOG_WARNING(kLogSource, QStringLiteral("Invalid request: jsonrpc != 2.0"));
        return makeError(request.value(QStringLiteral("id")), -32600,
                        QStringLiteral("Invalid Request: jsonrpc must be '2.0'"));
    }

    // 检查是否为通知（无id）
    const bool isNotification = !request.contains(QStringLiteral("id"));
    const QJsonValue id = request.value(QStringLiteral("id"));

    // 获取方法名
    const QString method = request.value(QStringLiteral("method")).toString();
    if (method.isEmpty()) {
        LOG_WARNING(kLogSource, QStringLiteral("Invalid request: missing method"));
        return makeError(id, -32600, QStringLiteral("Invalid Request: method missing"));
    }

    // 查找处理器
    const auto it = handlers_.find(method);
    if (it == handlers_.end()) {
        LOG_WARNING(kLogSource, QStringLiteral("Method not found: %1").arg(method));
        return isNotification ? QJsonObject{}
                              : makeError(id, -32601, QStringLiteral("Method not found"));
    }

    // 解析参数
    QJsonObject params;
    if (request.contains(QStringLiteral("params"))) {
        if (!request.value(QStringLiteral("params")).isObject()) {
            LOG_WARNING(kLogSource,
                       QStringLiteral("Invalid params: must be object, method: %1").arg(method));
            return isNotification
                ? QJsonObject{}
                : makeError(id, -32602, QStringLiteral("Invalid params: must be object"));
        }
        params = request.value(QStringLiteral("params")).toObject();
    }

    // 执行处理器
    LOG_DEBUG(kLogSource, QStringLiteral("Executing method: %1").arg(method));
    const QJsonValue result = it.value()(params);

    return isNotification ? QJsonObject{} : makeResult(id, result);
}

}  // namespace rpc
}  // namespace fanzhou
