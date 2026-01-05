#ifndef JSON_RPC_CLIENT_H
#define JSON_RPC_CLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QHash>
#include <QJsonValue>
#include <QJsonObject>

class JsonRpcClient : public QObject
{
    Q_OBJECT
public:
    using Callback = std::function<void(const QJsonValue& result, const QJsonObject& error)>;

    explicit JsonRpcClient(QObject *parent = nullptr);

    void setEndpoint(const QString& host, quint16 port);
    bool connectToServer(int timeoutMs = 1500);
    void disconnectFromServer();
    bool isConnected() const;

    //
    QJsonValue call(const QString& method,
                    const QJsonObject& params = QJsonObject(),
                    int timeoutMs = 1500);

    int callAsync(const QString& method, const QJsonObject& params = QJsonObject());
    int callAsync(const QString& method,
                      const QJsonObject& params,
                      Callback cb,
                      int timeoutMs = 1500);


signals:
    void connected();
    void disconnected();
    void transportError(const QString& err);
    void callFinished(int id, const QJsonValue& result, const QJsonObject& error);

private slots:
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError);

private:
    QJsonObject makeError(int code, const QString& message) const;
    QByteArray packRequest(int id, const QString& method, const QJsonObject& params) const;
    void handleLine(const QByteArray& line);

    void dispatchCallback(int id, const QJsonValue& result, const QJsonObject& error);


    QString host_ = "127.0.0.1";
    quint16 port_ = 12345;

    QTcpSocket sock_;
    QByteArray rxBuf_;

    int nextId_ = 1;
    // TODO
    QHash<int, QString> pending_;
    QHash<int, Callback> callbacks_; // id -> once-callback
};

#endif // JSON_RPC_CLIENT_H
