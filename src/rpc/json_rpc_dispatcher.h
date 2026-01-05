/**
 * @file json_rpc_dispatcher.h
 * @brief JSON-RPC method dispatcher
 *
 * Provides method registration and request dispatching for JSON-RPC 2.0.
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
 * @brief JSON-RPC 2.0 method dispatcher
 *
 * Manages method handlers and dispatches incoming requests to the
 * appropriate handler.
 */
class JsonRpcDispatcher : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Method handler function type
     *
     * Takes a JSON object of parameters and returns a result value.
     */
    using Handler = std::function<QJsonValue(const QJsonObject &params)>;

    explicit JsonRpcDispatcher(QObject *parent = nullptr)
        : QObject(parent) {}

    /**
     * @brief Register a method handler
     * @param method Method name
     * @param handler Handler function
     */
    void registerMethod(const QString &method, Handler handler);

    /**
     * @brief Get list of registered methods
     * @return Sorted list of method names
     */
    QStringList methods() const;

    /**
     * @brief Handle a JSON-RPC request
     * @param request Request object
     * @return Response object (empty for notifications)
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
