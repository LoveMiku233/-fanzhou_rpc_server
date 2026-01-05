/**
 * @file json_rpc_server.h
 * @brief JSON-RPC TCP server
 *
 * Provides a TCP server for JSON-RPC 2.0 requests.
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
 * @brief JSON-RPC 2.0 TCP server
 *
 * Accepts TCP connections and processes JSON-RPC requests using
 * line-delimited JSON format.
 */
class JsonRpcServer : public QTcpServer
{
    Q_OBJECT

public:
    /**
     * @brief Construct a JSON-RPC server
     * @param dispatcher Request dispatcher
     * @param parent Parent object
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
