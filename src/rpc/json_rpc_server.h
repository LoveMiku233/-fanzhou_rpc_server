/**
 * @file json_rpc_server.h
 * @brief JSON-RPC TCP服务器
 *
 * 提供JSON-RPC 2.0请求的TCP服务器，支持可选的token认证。
 */

#ifndef FANZHOU_JSON_RPC_SERVER_H
#define FANZHOU_JSON_RPC_SERVER_H

#include <QHash>
#include <QObject>
#include <QTcpServer>

class QTcpSocket;

namespace fanzhou {

namespace core {
class CoreContext;
}

namespace rpc {

class JsonRpcDispatcher;

/**
 * @brief JSON-RPC 2.0 TCP服务器
 *
 * 接受TCP连接并使用行分隔JSON格式处理JSON-RPC请求。
 * 支持可选的token认证机制。
 */
class JsonRpcServer : public QTcpServer
{
    Q_OBJECT

public:
    /**
     * @brief 构造JSON-RPC服务器
     * @param dispatcher 请求分发器
     * @param parent 父对象
     */
    explicit JsonRpcServer(JsonRpcDispatcher *dispatcher,
                           QObject *parent = nullptr);

    /**
     * @brief 设置核心上下文（用于认证）
     * @param context 核心上下文指针
     */
    void setCoreContext(core::CoreContext *context);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void processLines(QTcpSocket *socket);
    static QByteArray toLine(const QJsonObject &obj);
    
    /**
     * @brief 检查请求是否通过认证
     * @param request JSON-RPC请求对象
     * @param socket 客户端socket（用于获取IP地址）
     * @return 通过认证返回true
     */
    bool checkAuth(const QJsonObject &request, QTcpSocket *socket) const;

    JsonRpcDispatcher *dispatcher_ = nullptr;
    core::CoreContext *context_ = nullptr;
    QHash<QTcpSocket *, QByteArray> buffers_;
    QHash<QTcpSocket *, QString> authenticatedTokens_;  ///< 已认证的socket -> token

    static constexpr int kMaxBufferSize = 1024 * 1024;  ///< 单个连接最大缓冲区1MB
    static constexpr int kMaxConnections = 64;          ///< 最大并发连接数
};

}  // namespace rpc
}  // namespace fanzhou

#endif  // FANZHOU_JSON_RPC_SERVER_H
