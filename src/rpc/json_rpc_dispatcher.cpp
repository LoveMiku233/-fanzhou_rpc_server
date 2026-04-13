/**
 * @file json_rpc_dispatcher.cpp
 * @brief JSON-RPC方法分发器实现
 */

#include "json_rpc_dispatcher.h"
#include "utils/logger.h"

#include <QElapsedTimer>
#include <QJsonArray>

#include <algorithm>
#include <stdexcept>

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

QJsonArray JsonRpcDispatcher::stats(int limit) const
{
    struct Item {
        QString method;
        MethodStats stat;
    };
    QList<Item> items;
    items.reserve(stats_.size());
    for (auto it = stats_.cbegin(); it != stats_.cend(); ++it) {
        items.append(Item{it.key(), it.value()});
    }

    std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
        return a.stat.totalUs > b.stat.totalUs;
    });

    if (limit <= 0) {
        limit = 20;
    }

    QJsonArray arr;
    const int n = std::min(limit, items.size());
    for (int i = 0; i < n; ++i) {
        const Item &item = items.at(i);
        const double avgUs = item.stat.calls > 0
            ? static_cast<double>(item.stat.totalUs) / static_cast<double>(item.stat.calls)
            : 0.0;
        arr.append(QJsonObject{
            {QStringLiteral("method"), item.method},
            {QStringLiteral("calls"), static_cast<double>(item.stat.calls)},
            {QStringLiteral("errors"), static_cast<double>(item.stat.errors)},
            {QStringLiteral("totalUs"), static_cast<double>(item.stat.totalUs)},
            {QStringLiteral("avgUs"), avgUs},
            {QStringLiteral("maxUs"), static_cast<double>(item.stat.maxUs)},
            {QStringLiteral("lastUs"), static_cast<double>(item.stat.lastUs)}
        });
    }
    return arr;
}

void JsonRpcDispatcher::resetStats()
{
    stats_.clear();
}

void JsonRpcDispatcher::recordMethodCall(const QString &method, qint64 elapsedUs, bool isError)
{
    MethodStats &st = stats_[method];
    st.calls++;
    if (isError) {
        st.errors++;
    }
    st.lastUs = elapsedUs;
    st.totalUs += elapsedUs;
    if (elapsedUs > st.maxUs) {
        st.maxUs = elapsedUs;
    }
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

QJsonObject JsonRpcDispatcher::handle(const QJsonObject &request)
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
    QElapsedTimer timer;
    timer.start();
    try {
        const QJsonValue result = it.value()(params);
        recordMethodCall(method, timer.nsecsElapsed() / 1000, false);
        return isNotification ? QJsonObject{} : makeResult(id, result);
    } catch (const std::exception &e) {
        recordMethodCall(method, timer.nsecsElapsed() / 1000, true);
        LOG_ERROR(kLogSource,
                  QStringLiteral("Handler exception for method %1: %2")
                      .arg(method, QString::fromLocal8Bit(e.what())));
        return isNotification ? QJsonObject{}
                              : makeError(id, -32603, QStringLiteral("Internal error"));
    } catch (...) {
        recordMethodCall(method, timer.nsecsElapsed() / 1000, true);
        LOG_ERROR(kLogSource,
                  QStringLiteral("Unknown handler exception for method %1").arg(method));
        return isNotification ? QJsonObject{}
                              : makeError(id, -32603, QStringLiteral("Internal error"));
    }
}

}  // namespace rpc
}  // namespace fanzhou
