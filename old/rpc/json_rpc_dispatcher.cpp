/**
 * @file json_rpc_dispatcher.cpp
 * @brief JSON-RPC 方法分发器实现
 * 
 * 负责注册RPC方法处理器并分发请求到对应的处理函数。
 */

#include "json_rpc_dispatcher.h"
#include "utils/logger.h"

// 日志来源标识
static const QString LOG_SOURCE = "RPC分发器";


/**
 * @brief 注册RPC方法处理器
 * @param method 方法名称
 * @param handler 处理函数
 */
void JsonRpcDispatcher::registerMethod(const QString &method, Handler handler)
{
    handlers_[method] = std::move(handler);
    LOG_DEBUG(LOG_SOURCE, QString("注册RPC方法: %1").arg(method));
}


/**
 * @brief 获取所有已注册的方法名列表
 * @return 排序后的方法名列表
 */
QStringList JsonRpcDispatcher::methods() const {
    auto keys = handlers_.keys();
    std::sort(keys.begin(), keys.end());
    return keys;
}

/**
 * @brief 创建RPC错误响应对象
 * @param id 请求ID
 * @param code 错误代码
 * @param message 错误消息
 * @return JSON-RPC格式的错误响应
 */
QJsonObject JsonRpcDispatcher::makeError(const QJsonValue &id, int code, const QString &message)
{
    return QJsonObject {
        {"jsonrpc", "2.0"},
        {"id", id.isUndefined() ? QJsonValue(QJsonValue::Null) : id},
        {"error", QJsonObject{{"code", code}, {"message", message}}}
    };
}


/**
 * @brief 创建RPC成功响应对象
 * @param id 请求ID
 * @param result 返回结果
 * @return JSON-RPC格式的成功响应
 */
QJsonObject JsonRpcDispatcher::makeResult(const QJsonValue &id, const QJsonValue &result)
{
    return QJsonObject {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result}
    };
}


/**
 * @brief 处理RPC请求
 * @param req 请求对象
 * @return 响应对象（通知类型请求返回空对象）
 * 
 * 验证请求格式，查找对应的处理器并执行
 */
QJsonObject JsonRpcDispatcher::handle(const QJsonObject &req) const
{
    // 验证JSON-RPC版本
    if (req.value("jsonrpc").toString() != "2.0") {
        LOG_WARNING(LOG_SOURCE, "无效请求: jsonrpc版本不是2.0");
        return makeError(req.value("id"), -32600, "Invalid Request: jsonrpc must be '2.0'");
    }

    // 判断是否为通知（无id字段的请求）
    const bool isNotification = !req.contains("id");
    const QJsonValue id = req.value("id");

    // 获取方法名
    const QString method = req.value("method").toString();
    if (method.isEmpty()) {
        LOG_WARNING(LOG_SOURCE, "无效请求: 缺少method字段");
        return makeError(id, -32600, "Invalid Request: method missing");
    }

    // 查找方法处理器
    const auto it = handlers_.find(method);
    // 通知类型请求如果方法不存在则不返回错误
    if (it == handlers_.end()) {
        LOG_WARNING(LOG_SOURCE, QString("方法不存在: %1").arg(method));
        return isNotification ? QJsonObject{} : makeError(id, -32601, "Method not found");
    }

    // 解析参数
    QJsonObject paramsObj;
    if (req.contains("params")) {
        if (!req.value("params").isObject()) {
            LOG_WARNING(LOG_SOURCE, QString("无效参数: params必须是对象类型，方法: %1").arg(method));
            return isNotification ? QJsonObject{} : makeError(id, -32602, "Invalid params: must be object");
        }
        paramsObj = req.value("params").toObject();
    }

    // 执行方法处理器
    LOG_DEBUG(LOG_SOURCE, QString("执行方法: %1").arg(method));
    const QJsonValue result = it.value()(paramsObj);
    
    return isNotification ? QJsonObject{} : makeResult(id, result);
}
