/**
 * @file json_rpc_client.h
 * @brief JSON-RPC TCP client
 *
 * Provides a TCP client for JSON-RPC 2.0 requests.
 */

#ifndef FANZHOU_JSON_RPC_CLIENT_H
#define FANZHOU_JSON_RPC_CLIENT_H

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QTcpSocket>

#include <functional>

namespace fanzhou {
namespace rpc {

/**
 * @brief JSON-RPC 2.0 TCP client
 *
 * Provides synchronous and asynchronous RPC calls over TCP.
 */
class JsonRpcClient : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Callback type for asynchronous calls
     */
    using Callback = std::function<void(const QJsonValue &result,
                                         const QJsonObject &error)>;

    explicit JsonRpcClient(QObject *parent = nullptr);

    /**
     * @brief Set server endpoint
     * @param host Server host
     * @param port Server port
     */
    void setEndpoint(const QString &host, quint16 port);

    /**
     * @brief Connect to server
     * @param timeoutMs Connection timeout in milliseconds
     * @return True if connected
     */
    bool connectToServer(int timeoutMs = 1500);

    /**
     * @brief Disconnect from server
     */
    void disconnectFromServer();

    /**
     * @brief Check if connected
     * @return True if connected
     */
    bool isConnected() const;

    /**
     * @brief Synchronous RPC call (blocking)
     * @param method Method name
     * @param params Parameters
     * @param timeoutMs Timeout in milliseconds
     * @return Result value
     */
    QJsonValue call(const QString &method,
                    const QJsonObject &params = QJsonObject(),
                    int timeoutMs = 1500);

    /**
     * @brief Asynchronous RPC call
     * @param method Method name
     * @param params Parameters
     * @return Request ID, or -1 on failure
     */
    int callAsync(const QString &method,
                  const QJsonObject &params = QJsonObject());

    /**
     * @brief Asynchronous RPC call with callback
     * @param method Method name
     * @param params Parameters
     * @param callback Result callback
     * @param timeoutMs Timeout in milliseconds
     * @return Request ID, or -1 on failure
     */
    int callAsync(const QString &method,
                  const QJsonObject &params,
                  Callback callback,
                  int timeoutMs = 1500);

signals:
    void connected();
    void disconnected();
    void transportError(const QString &error);
    void callFinished(int id, const QJsonValue &result, const QJsonObject &error);

private slots:
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    QJsonObject makeError(int code, const QString &message) const;
    QByteArray packRequest(int id, const QString &method,
                           const QJsonObject &params) const;
    void handleLine(const QByteArray &line);
    void dispatchCallback(int id, const QJsonValue &result,
                          const QJsonObject &error);

    QString host_ = QStringLiteral("127.0.0.1");
    quint16 port_ = 12345;

    QTcpSocket socket_;
    QByteArray rxBuffer_;

    int nextId_ = 1;
    QHash<int, QString> pending_;
    QHash<int, Callback> callbacks_;
};

}  // namespace rpc
}  // namespace fanzhou

#endif  // FANZHOU_JSON_RPC_CLIENT_H
