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
#include <cctype>

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

        // 工业环境下限制最大并发连接数，防止资源耗尽
        if (buffers_.size() >= kMaxConnections) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("Max connections reached (%1), rejecting %2:%3")
                            .arg(kMaxConnections)
                            .arg(socket->peerAddress().toString())
                            .arg(socket->peerPort()));
            socket->disconnectFromHost();
            socket->deleteLater();
            continue;
        }

        buffers_[socket] = QByteArray{};
        connect(socket, &QTcpSocket::readyRead, this, &JsonRpcServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &JsonRpcServer::onDisconnected);

        LOG_INFO(kLogSource,
                 QStringLiteral("New client connected: %1:%2 (total: %3)")
                     .arg(socket->peerAddress().toString())
                     .arg(socket->peerPort())
                     .arg(buffers_.size()));
    }
}

void JsonRpcServer::onReadyRead()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket || !buffers_.contains(socket)) {
        return;
    }

    auto &buf = buffers_[socket];
    buf.append(socket->readAll());

    // 防止单个连接缓冲区过大导致内存耗尽
    if (buf.size() > kMaxBufferSize) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Buffer overflow from %1:%2, dropping connection")
                        .arg(socket->peerAddress().toString())
                        .arg(socket->peerPort()));
        buffers_.remove(socket);
        authenticatedTokens_.remove(socket);
        socket->disconnectFromHost();
        return;
    }

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
    if (!socket || !buffers_.contains(socket)) {
        return;
    }
    auto &buffer = buffers_[socket];
    int processedCount = 0;

    while (processedCount < kMaxRequestsPerCycle) {
        int start = 0;
        while (start < buffer.size() && std::isspace(static_cast<unsigned char>(buffer[start])) != 0) {
            ++start;
        }
        if (start >= buffer.size()) {
            buffer.clear();
            break;
        }

        // 容忍流中的无效前缀，定位到下一个对象起点
        if (buffer[start] != '{') {
            const int nextObject = buffer.indexOf('{', start + 1);
            if (nextObject < 0) {
                LOG_WARNING(kLogSource,
                            QStringLiteral("Dropping non-json buffer from RPC stream (%1 bytes)")
                                .arg(buffer.size()));
                buffer.clear();
                break;
            }
            LOG_WARNING(kLogSource,
                        QStringLiteral("Dropping non-json prefix from RPC stream (%1 bytes)")
                            .arg(nextObject - start));
            buffer.remove(0, nextObject);
            start = 0;
        }

        const int end = findJsonObjectEnd(buffer, start);
        if (end < 0) {
            if (start > 0) {
                buffer.remove(0, start);
            }
            break;  // 半包，等待更多数据
        }

        const QByteArray payload = buffer.mid(start, end - start + 1);
        buffer.remove(0, end + 1);
        ++processedCount;

        QJsonParseError parseError {};
        const auto doc = QJsonDocument::fromJson(payload, &parseError);

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
        const bool isNotification = !request.contains(QStringLiteral("id"));

        LOG_DEBUG(kLogSource,
                  QStringLiteral("RPC request [id=%1] method: %2")
                      .arg(requestIdToString(reqId))
                      .arg(method));

        // 检查认证
        if (!checkAuth(request, socket)) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("Authentication failed for method: %1 from %2")
                            .arg(method)
                            .arg(socket->peerAddress().toString()));

            if (!isNotification) {
                QJsonObject response{
                    {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("id"), reqId.isUndefined() ? QJsonValue(QJsonValue::Null) : reqId},
                    {QStringLiteral("error"), QJsonObject{
                        {QStringLiteral("code"), -32001},
                        {QStringLiteral("message"), QStringLiteral("Authentication required")}
                    }}
                };
                socket->write(toLine(response));
            }
            continue;
        }

        const QJsonObject response = dispatcher_->handle(request);
        if (!response.isEmpty()) {
            socket->write(toLine(response));

            if (response.contains(QStringLiteral("error"))) {
                LOG_WARNING(kLogSource,
                          QStringLiteral("RPC error response [id=%1] method=%2: %3")
                              .arg(requestIdToString(reqId))
                              .arg(method)
                              .arg(response.value(QStringLiteral("error"))
                                       .toObject()
                                       .value(QStringLiteral("message"))
                                       .toString()));
            } else {
                LOG_DEBUG(kLogSource,
                          QStringLiteral("RPC success response [id=%1]")
                              .arg(requestIdToString(reqId)));
                
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

    if (processedCount >= kMaxRequestsPerCycle) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Processed %1 RPC requests in one cycle, yielding to event loop")
                        .arg(kMaxRequestsPerCycle));
    }
}

QByteArray JsonRpcServer::toLine(const QJsonObject &obj)
{
    return QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";
}

int JsonRpcServer::findJsonObjectEnd(const QByteArray &buffer, int startIndex)
{
    bool inString = false;
    bool escaped = false;
    int depth = 0;
    bool seenObjectStart = false;

    for (int i = startIndex; i < buffer.size(); ++i) {
        const char ch = buffer[i];
        if (!seenObjectStart) {
            if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                continue;
            }
            if (ch != '{') {
                return -1;
            }
            seenObjectStart = true;
            depth = 1;
            continue;
        }

        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
            continue;
        }
        if (ch == '{') {
            ++depth;
            continue;
        }
        if (ch == '}') {
            --depth;
            if (depth == 0) {
                return i;
            }
            continue;
        }
    }

    return -1;
}

QString JsonRpcServer::requestIdToString(const QJsonValue &id)
{
    if (id.isUndefined() || id.isNull()) {
        return QStringLiteral("null");
    }
    if (id.isString()) {
        return id.toString();
    }
    if (id.isDouble()) {
        return QString::number(id.toDouble(), 'g', 16);
    }
    return QStringLiteral("<non-scalar-id>");
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
