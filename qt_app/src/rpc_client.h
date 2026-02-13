/**
 * @file rpc_client.h
 * @brief JSON-RPC TCP客户端封装
 *
 * 提供JSON-RPC 2.0请求的TCP客户端封装类。
 */

#ifndef RPC_CLIENT_H
#define RPC_CLIENT_H

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QTcpSocket>

#include <functional>

/**
 * @brief JSON-RPC 2.0 TCP客户端
 *
 * 提供同步和异步RPC调用功能。
 */
class RpcClient : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 异步调用回调类型
     */
    using Callback = std::function<void(const QJsonValue &result,
                                         const QJsonObject &error)>;

    explicit RpcClient(QObject *parent = nullptr);
    ~RpcClient();

    /**
     * @brief 设置服务器端点
     * @param host 服务器主机
     * @param port 服务器端口
     */
    void setEndpoint(const QString &host, quint16 port);

    /**
     * @brief 获取主机地址
     */
    QString host() const { return host_; }

    /**
     * @brief 获取端口
     */
    quint16 port() const { return port_; }

    /**
     * @brief 连接到服务器（阻塞）
     * @param timeoutMs 连接超时（毫秒）
     * @return 连接成功返回true
     */
    bool connectToServer(int timeoutMs = 3000);

    /**
     * @brief 异步连接到服务器（非阻塞）
     *
     * 连接结果通过 connected() 或 transportError() 信号通知。
     */
    void connectToServerAsync();

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
                    int timeoutMs = 3000);

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
                  int timeoutMs = 3000);

signals:
    void connected();
    void disconnected();
    void transportError(const QString &error);
    void callFinished(int id, const QJsonValue &result, const QJsonObject &error);
    void logMessage(const QString &message);

private slots:
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onConnected();
    void onDisconnected();

private:
    QJsonObject makeError(int code, const QString &message) const;
    QByteArray packRequest(int id, const QString &method,
                           const QJsonObject &params) const;
    void handleLine(const QByteArray &line);
    void dispatchCallback(int id, const QJsonValue &result,
                          const QJsonObject &error);
    void log(const QString &message);

    QString host_;
    quint16 port_;

    QTcpSocket *socket_;
    QByteArray rxBuffer_;

    int nextId_;
    QHash<int, QString> pending_;
    QHash<int, Callback> callbacks_;
};

#endif // RPC_CLIENT_H
