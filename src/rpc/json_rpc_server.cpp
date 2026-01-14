/**
 * @file json_rpc_server.cpp
 * @brief JSON-RPC TCP服务器实现
 */

#include "json_rpc_server.h"
#include "json_rpc_dispatcher.h"
#include "core/core_context.h"
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

void JsonRpcServer::setCoreContext(core::CoreContext *context)
{
    context_ = context;
    if (context_ && context_->authConfig.enabled) {
        LOG_INFO(kLogSource, QStringLiteral("Authentication enabled for RPC server"));
    }
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

bool JsonRpcServer::checkAuth(const QJsonObject &request, QTcpSocket *socket) const
{
    // 如果没有设置context或认证未启用，跳过认证检查
    if (!context_ || !context_->authConfig.enabled) {
        return true;
    }
    
    // 获取方法名
    const QString method = request.value(QStringLiteral("method")).toString();
    
    // 检查是否是公共方法（不需要认证）
    if (!context_->methodRequiresAuth(method)) {
        return true;
    }
    
    // 检查IP白名单
    if (socket) {
        const QString clientIp = socket->peerAddress().toString();
        if (context_->isIpWhitelisted(clientIp)) {
            return true;
        }
    }
    
    // 检查token认证
    // Token可以通过以下方式提供：
    // 1. 在请求的params中包含 "auth_token" 字段
    // 2. 在请求的顶层包含 "auth_token" 字段
    QString token;
    
    // 检查params中的token
    if (request.contains(QStringLiteral("params")) && request.value(QStringLiteral("params")).isObject()) {
        const auto params = request.value(QStringLiteral("params")).toObject();
        if (params.contains(QStringLiteral("auth_token"))) {
            token = params.value(QStringLiteral("auth_token")).toString();
        }
    }
    
    // 检查顶层的token
    if (token.isEmpty() && request.contains(QStringLiteral("auth_token"))) {
        token = request.value(QStringLiteral("auth_token")).toString();
    }
    
    // 检查socket是否已经通过认证（会话级别的认证）
    if (token.isEmpty() && socket && authenticatedTokens_.contains(socket)) {
        token = authenticatedTokens_.value(socket);
    }
    
    // 验证token
    return context_->verifyToken(token);
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

        // 检查认证
        if (!checkAuth(request, socket)) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("Authentication failed for method: %1 from %2")
                            .arg(method)
                            .arg(socket->peerAddress().toString()));
            
            QJsonObject response{
                {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                {QStringLiteral("id"), reqId.isUndefined() ? QJsonValue(QJsonValue::Null) : reqId},
                {QStringLiteral("error"), QJsonObject{
                    {QStringLiteral("code"), -32001},
                    {QStringLiteral("message"), QStringLiteral("Authentication required")}
                }}
            };
            socket->write(toLine(response));
            continue;
        }

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
                
                // 如果是auth.login方法成功，保存token到会话
                if (method == QStringLiteral("auth.login") && 
                    response.contains(QStringLiteral("result"))) {
                    const auto result = response.value(QStringLiteral("result")).toObject();
                    if (result.value(QStringLiteral("ok")).toBool() && 
                        result.contains(QStringLiteral("token"))) {
                        const QString token = result.value(QStringLiteral("token")).toString();
                        authenticatedTokens_.insert(socket, token);
                        LOG_DEBUG(kLogSource,
                                  QStringLiteral("Session authenticated for %1")
                                      .arg(socket->peerAddress().toString()));
                    }
                }
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
    authenticatedTokens_.remove(socket);
    socket->deleteLater();
}

}  // namespace rpc
}  // namespace fanzhou
