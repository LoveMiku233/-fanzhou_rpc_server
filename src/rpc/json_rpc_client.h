/**
 * @file json_rpc_client.h
 * @brief JSON-RPC TCP客户端
 *
 * 提供JSON-RPC 2.0请求的TCP客户端。
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
 * @brief JSON-RPC 2.0 TCP客户端
 *
 * 提供同步和异步RPC调用功能。
 */
class JsonRpcClient : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 异步调用回调类型
     */
    using Callback = std::function<void(const QJsonValue &result,
                                         const QJsonObject &error)>;

    explicit JsonRpcClient(QObject *parent = nullptr);

    /**
     * @brief 设置服务器端点
     * @param host 服务器主机
     * @param port 服务器端口
     */
    void setEndpoint(const QString &host, quint16 port);

    /**
     * @brief 连接到服务器
     * @param timeoutMs 连接超时（毫秒）
     * @return 连接成功返回true
     */
    bool connectToServer(int timeoutMs = 1500);

    /**
     * @brief 断开服务器连接
     */
    void disconnectFromServer();

    /**
     * @brief 检查是否已连接
     * @return 已连接返回true
     */
    bool isConnected() const;

    /**
     * @brief 同步RPC调用（阻塞）
     * @param method 方法名
     * @param params 参数
     * @param timeoutMs 超时（毫秒）
     * @return 结果值
     */
    QJsonValue call(const QString &method,
                    const QJsonObject &params = QJsonObject(),
                    int timeoutMs = 1500);

    /**
     * @brief 异步RPC调用
     * @param method 方法名
     * @param params 参数
     * @return 请求ID，失败返回-1
     */
    int callAsync(const QString &method,
                  const QJsonObject &params = QJsonObject());

    /**
     * @brief 带回调的异步RPC调用
     * @param method 方法名
     * @param params 参数
     * @param callback 结果回调
     * @param timeoutMs 超时（毫秒）
     * @return 请求ID，失败返回-1
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
