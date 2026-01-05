/**
 * @file json_rpc_server.cpp
 * @brief JSON-RPC TCP server implementation
 */

#include "json_rpc_server.h"
#include "json_rpc_dispatcher.h"
#include "utils/logger.h"

#include <QJsonDocument>
#include <QTcpSocket>

namespace fanzhou {
namespace rpc {

namespace {
const char *const kLogSource = "RpcServer";
}

JsonRpcServer::JsonRpcServer(JsonRpcDispatcher *dispatcher, QObject *parent)
    : QTcpServer(parent)
    , dispatcher_(dispatcher)
{
    connect(this, &QTcpServer::newConnection, this, &JsonRpcServer::onNewConnection);
    LOG_DEBUG(kLogSource, QStringLiteral("RPC server initialized"));
}

void JsonRpcServer::onNewConnection()
{
    while (hasPendingConnections()) {
        auto *socket = nextPendingConnection();
        buffers_[socket] = QByteArray{};
        connect(socket, &QTcpSocket::readyRead, this, &JsonRpcServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &JsonRpcServer::onDisconnected);

        LOG_INFO(kLogSource,
                 QStringLiteral("New client connected: %1:%2")
                     .arg(socket->peerAddress().toString())
                     .arg(socket->peerPort()));
    }
}

void JsonRpcServer::onReadyRead()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) {
        return;
    }

    buffers_[socket].append(socket->readAll());
    processLines(socket);
}

void JsonRpcServer::processLines(QTcpSocket *socket)
{
    auto &buffer = buffers_[socket];

    for (;;) {
        const int nlIndex = buffer.indexOf('\n');
        if (nlIndex < 0) {
            break;
        }

        const QByteArray line = buffer.left(nlIndex);
        buffer.remove(0, nlIndex + 1);

        const QByteArray trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        QJsonParseError parseError {};
        const auto doc = QJsonDocument::fromJson(trimmed, &parseError);

        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("JSON parse error: %1").arg(parseError.errorString()));
            QJsonObject response{
                {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                {QStringLiteral("id"), QJsonValue(QJsonValue::Null)},
                {QStringLiteral("error"), QJsonObject{
                    {QStringLiteral("code"), -32700},
                    {QStringLiteral("message"), QStringLiteral("Parse error")}
                }}
            };
            socket->write(toLine(response));
            continue;
        }

        const QJsonObject request = doc.object();
        const QString method = request.value(QStringLiteral("method")).toString();
        const QJsonValue reqId = request.value(QStringLiteral("id"));

        LOG_DEBUG(kLogSource,
                  QStringLiteral("RPC request [id=%1] method: %2")
                      .arg(reqId.isNull() ? QStringLiteral("null")
                                          : QString::number(reqId.toInt()))
                      .arg(method));

        const QJsonObject response = dispatcher_->handle(request);
        if (!response.isEmpty()) {
            socket->write(toLine(response));

            if (response.contains(QStringLiteral("error"))) {
                LOG_DEBUG(kLogSource,
                          QStringLiteral("RPC error response [id=%1]: %2")
                              .arg(reqId.isNull() ? QStringLiteral("null")
                                                  : QString::number(reqId.toInt()))
                              .arg(response.value(QStringLiteral("error"))
                                       .toObject()
                                       .value(QStringLiteral("message"))
                                       .toString()));
            } else {
                LOG_DEBUG(kLogSource,
                          QStringLiteral("RPC success response [id=%1]")
                              .arg(reqId.isNull() ? QStringLiteral("null")
                                                  : QString::number(reqId.toInt())));
            }
        }
    }
}

QByteArray JsonRpcServer::toLine(const QJsonObject &obj)
{
    return QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";
}

void JsonRpcServer::onDisconnected()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) {
        return;
    }

    LOG_INFO(kLogSource,
             QStringLiteral("Client disconnected: %1:%2")
                 .arg(socket->peerAddress().toString())
                 .arg(socket->peerPort()));

    buffers_.remove(socket);
    socket->deleteLater();
}

}  // namespace rpc
}  // namespace fanzhou
