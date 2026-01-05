#ifndef JSON_RPC_DISPATCHER_H
#define JSON_RPC_DISPATCHER_H

#include <functional>

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonValue>
#include <QHash>

class JsonRpcDispatcher : public QObject
{
    Q_OBJECT

public:

    // define function callback
    using Handler = std::function<QJsonValue(const QJsonObject& params)>;

    explicit JsonRpcDispatcher(QObject* parent = nullptr)
        : QObject(parent) {}

    void registerMethod(const QString& method, Handler handler);

    QStringList methods() const;

    // in : requset object, out: response object
    QJsonObject handle(const QJsonObject& req) const;

private:
    QHash<QString, Handler> handlers_;

    static QJsonObject makeError(const QJsonValue& id, int code, const QString& message);
    static QJsonObject makeResult(const QJsonValue& id, const QJsonValue& result);
};

#endif // JSON_RPC_DISPATCHER_H
