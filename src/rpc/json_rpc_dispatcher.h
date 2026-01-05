/**
 * @file json_rpc_dispatcher.h
 * @brief JSON-RPC方法分发器
 *
 * 提供JSON-RPC 2.0的方法注册和请求分发功能。
 */

#ifndef FANZHOU_JSON_RPC_DISPATCHER_H
#define FANZHOU_JSON_RPC_DISPATCHER_H

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>

#include <functional>

namespace fanzhou {
namespace rpc {

/**
 * @brief JSON-RPC 2.0方法分发器
 *
 * 管理方法处理器并将传入请求分发到适当的处理器。
 */
class JsonRpcDispatcher : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 方法处理器函数类型
     *
     * 接收JSON参数对象并返回结果值。
     */
    using Handler = std::function<QJsonValue(const QJsonObject &params)>;

    explicit JsonRpcDispatcher(QObject *parent = nullptr)
        : QObject(parent) {}

    /**
     * @brief 注册方法处理器
     * @param method 方法名称
     * @param handler 处理器函数
     */
    void registerMethod(const QString &method, Handler handler);

    /**
     * @brief 获取已注册方法列表
     * @return 排序后的方法名称列表
     */
    QStringList methods() const;

    /**
     * @brief 处理JSON-RPC请求
     * @param request 请求对象
     * @return 响应对象（通知请求返回空对象）
     */
    QJsonObject handle(const QJsonObject &request) const;

private:
    static QJsonObject makeError(const QJsonValue &id, int code,
                                  const QString &message);
    static QJsonObject makeResult(const QJsonValue &id,
                                   const QJsonValue &result);

    QHash<QString, Handler> handlers_;
};

}  // namespace rpc
}  // namespace fanzhou

#endif  // FANZHOU_JSON_RPC_DISPATCHER_H
