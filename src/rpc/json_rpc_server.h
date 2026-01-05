/**
 * @file json_rpc_server.h
 * @brief JSON-RPC TCP服务器
 *
 * 提供JSON-RPC 2.0请求的TCP服务器。
 */

#ifndef FANZHOU_JSON_RPC_SERVER_H
#define FANZHOU_JSON_RPC_SERVER_H

#include <QHash>
#include <QObject>
#include <QTcpServer>

class QTcpSocket;

namespace fanzhou {
namespace rpc {

class JsonRpcDispatcher;

/**
 * @brief JSON-RPC 2.0 TCP服务器
 *
 * 接受TCP连接并使用行分隔JSON格式处理JSON-RPC请求。
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

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void processLines(QTcpSocket *socket);
    static QByteArray toLine(const QJsonObject &obj);

    JsonRpcDispatcher *dispatcher_ = nullptr;
    QHash<QTcpSocket *, QByteArray> buffers_;
};

}  // namespace rpc
}  // namespace fanzhou

#endif  // FANZHOU_JSON_RPC_SERVER_H
