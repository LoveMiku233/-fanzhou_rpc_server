/**
 * @file device_tcp_server.cpp
 * @brief 设备协议TCP服务器实现（端口9000）
 */

#include "device_tcp_server.h"

#include "core/core_context.h"
#include "utils/logger.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QEventLoop>
#include <QTcpSocket>
#include <QTimer>
#include <QDateTime>
#include <cctype>

namespace fanzhou {
namespace rpc {

namespace {
const char *const kLogSource = "DeviceTcpServer";

bool jsonValueEquals(const QJsonValue &a, const QJsonValue &b)
{
    if (a.type() == b.type()) {
        return a == b;
    }
    if (a.isDouble() && b.isString()) {
        bool ok = false;
        const double v = b.toString().toDouble(&ok);
        return ok && qFuzzyCompare(a.toDouble() + 1.0, v + 1.0);
    }
    if (a.isString() && b.isDouble()) {
        bool ok = false;
        const double v = a.toString().toDouble(&ok);
        return ok && qFuzzyCompare(v + 1.0, b.toDouble() + 1.0);
    }
    return false;
}
}

DeviceTcpServer::DeviceTcpServer(QObject *parent)
    : QTcpServer(parent)
{
    connect(this, &QTcpServer::newConnection, this, &DeviceTcpServer::onNewConnection);
}

void DeviceTcpServer::setCoreContext(core::CoreContext *context)
{
    context_ = context;
}

QJsonObject DeviceTcpServer::connectionStatus() const
{
    QJsonArray clients;
    for (auto it = buffers_.constBegin(); it != buffers_.constEnd(); ++it) {
        auto *socket = it.key();
        if (!socket) {
            continue;
        }
        QJsonObject obj{
            {QStringLiteral("peerIp"), socket->peerAddress().toString()},
            {QStringLiteral("peerPort"), static_cast<int>(socket->peerPort())}
        };
        const int devId = socketDevId_.value(socket, -1);
        if (devId >= 0) {
            obj[QStringLiteral("dev")] = devId;
        }
        clients.append(obj);
    }

    return QJsonObject{
        {QStringLiteral("ok"), true},
        {QStringLiteral("clientCount"), buffers_.size()},
        {QStringLiteral("clients"), clients}
    };
}

bool DeviceTcpServer::sendCommand(const QJsonObject &command, int targetDev, QString *error)
{
    return sendCommandAndWait(command, targetDev, 0, nullptr, error);
}

bool DeviceTcpServer::sendCommandAndWait(const QJsonObject &command,
                                         int targetDev,
                                         int timeoutMs,
                                         QJsonObject *response,
                                         QString *error)
{
    if (buffers_.isEmpty()) {
        if (error) {
            *error = QStringLiteral("no device tcp client connected");
        }
        return false;
    }

    QTcpSocket *target = nullptr;
    if (targetDev >= 0) {
        target = devSocket_.value(targetDev, nullptr);
        if (!target) {
            if (error) {
                *error = QStringLiteral("target device not connected");
            }
            return false;
        }
    } else {
        target = buffers_.constBegin().key();
    }

    if (!target || target->state() != QAbstractSocket::ConnectedState) {
        if (error) {
            *error = QStringLiteral("target socket disconnected");
        }
        return false;
    }

    const bool waitMode = (timeoutMs > 0 && response != nullptr);
    if (waitMode && !command.contains(QStringLiteral("id"))) {
        if (error) {
            *error = QStringLiteral("missing id in command for wait-response mode");
        }
        return false;
    }
    const QJsonValue expectedId = waitMode ? command.value(QStringLiteral("id")) : QJsonValue();
    const int expectedDev = waitMode ? ((targetDev >= 0) ? targetDev : socketDevId_.value(target, -1)) : -1;

    // 控制命令去重：防止上层超时重复触发导致继电器被重复打开。
    if (expectedDev >= 0 && isControlCommand(command)) {
        const QByteArray signature = controlCommandSignature(command);
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const RecentControlCommand last = recentControlByDev_.value(expectedDev);
        if (!last.signature.isEmpty() && signature == last.signature &&
            (nowMs - last.timestampMs) >= 0 && (nowMs - last.timestampMs) <= kControlDedupWindowMs) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("dedup duplicate control command for dev=%1 within %2ms, suppressed")
                            .arg(expectedDev)
                            .arg(kControlDedupWindowMs));
            return true;
        }
        recentControlByDev_[expectedDev] = RecentControlCommand{signature, nowMs};
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    bool matched = false;
    QMetaObject::Connection c;
    if (waitMode) {
        c = connect(
            this, &DeviceTcpServer::boardMessageReceived, this,
            [&](int devId, const QJsonObject &msg) {
                if (expectedDev >= 0 && devId != expectedDev) {
                    return;
                }
                if (!msg.contains(QStringLiteral("id"))) {
                    return;
                }
                if (!jsonValueEquals(msg.value(QStringLiteral("id")), expectedId)) {
                    return;
                }
                *response = msg;
                matched = true;
                loop.quit();
            });
    }

    const qint64 bytes = target->write(toLine(command));
    if (bytes < 0) {
        if (waitMode) {
            disconnect(c);
        }
        if (error) {
            *error = QStringLiteral("write command failed");
        }
        return false;
    }
    if (!waitMode) {
        return true;
    }

    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
    disconnect(c);

    if (!matched) {
        if (error) {
            *error = QStringLiteral("wait response timeout");
        }
        return false;
    }

    return true;
}

void DeviceTcpServer::onNewConnection()
{
    while (hasPendingConnections()) {
        auto *socket = nextPendingConnection();
        if (!socket) {
            continue;
        }

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

        buffers_.insert(socket, QByteArray{});
        connect(socket, &QTcpSocket::readyRead, this, &DeviceTcpServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &DeviceTcpServer::onDisconnected);

        LOG_INFO(kLogSource,
                 QStringLiteral("Board TCP client connected: %1:%2 (total: %3)")
                     .arg(socket->peerAddress().toString())
                     .arg(socket->peerPort())
                     .arg(buffers_.size()));
    }
}

void DeviceTcpServer::onReadyRead()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket || !buffers_.contains(socket)) {
        return;
    }

    const QByteArray chunk = socket->readAll();
    if (!chunk.isEmpty()) {
        QString chunkText = QString::fromUtf8(chunk);
        chunkText.replace(QStringLiteral("\r"), QStringLiteral("\\r"));
        chunkText.replace(QStringLiteral("\n"), QStringLiteral("\\n"));
        if (chunkText.size() > 256) {
            chunkText = chunkText.left(256) + QStringLiteral("...(truncated)");
        }
        LOG_DEBUG(kLogSource,
                 QStringLiteral("rx chunk from %1:%2 => %3")
                     .arg(socket->peerAddress().toString())
                     .arg(socket->peerPort())
                     .arg(chunkText));
    }

    auto &buffer = buffers_[socket];
    buffer.append(chunk);
    if (buffer.size() > kMaxBufferSize) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Buffer overflow from %1:%2, dropping connection")
                        .arg(socket->peerAddress().toString())
                        .arg(socket->peerPort()));
        buffers_.remove(socket);
        socketDevId_.remove(socket);
        socket->disconnectFromHost();
        return;
    }

    processLines(socket);
}

void DeviceTcpServer::onDisconnected()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) {
        return;
    }

    const int devId = socketDevId_.value(socket, -1);
    if (devId >= 0 && devSocket_.value(devId) == socket) {
        devSocket_.remove(devId);
    }

    LOG_INFO(kLogSource,
             QStringLiteral("Board TCP client disconnected: %1:%2")
                 .arg(socket->peerAddress().toString())
                 .arg(socket->peerPort()));

    buffers_.remove(socket);
    socketDevId_.remove(socket);
    socket->deleteLater();
}

void DeviceTcpServer::processLines(QTcpSocket *socket)
{
    auto &buffer = buffers_[socket];

    for (;;) {
        int start = 0;
        while (start < buffer.size() && std::isspace(static_cast<unsigned char>(buffer[start])) != 0) {
            ++start;
        }
        if (start >= buffer.size()) {
            buffer.clear();
            break;
        }

        if (buffer[start] != '{') {
            const int nextObject = buffer.indexOf('{', start + 1);
            if (nextObject < 0) {
                LOG_WARNING(kLogSource,
                            QStringLiteral("dropping non-json buffer from board stream (%1 bytes)")
                                .arg(buffer.size()));
                buffer.clear();
                break;
            }
            LOG_WARNING(kLogSource,
                        QStringLiteral("dropping non-json prefix from board stream (%1 bytes)")
                            .arg(nextObject - start));
            buffer.remove(0, nextObject);
            start = 0;
        }

        const int end = findJsonObjectEnd(buffer, start);
        if (end < 0) {
            if (start > 0) {
                buffer.remove(0, start);
            }
            if (buffer.size() > kMaxIncompleteObjectSize) {
                LOG_WARNING(kLogSource,
                            QStringLiteral("dropping oversized incomplete json object (%1 bytes) from %2:%3")
                                .arg(buffer.size())
                                .arg(socket->peerAddress().toString())
                                .arg(socket->peerPort()));
                buffer.clear();
            }
            break;  // Wait for more bytes (half packet)
        }

        const QByteArray payload = buffer.mid(start, end - start + 1);
        buffer.remove(0, end + 1);
        LOG_INFO(kLogSource,
                 QStringLiteral("rx message from %1:%2 => %3")
                     .arg(socket->peerAddress().toString())
                     .arg(socket->peerPort())
                     .arg(QString::fromUtf8(payload)));

        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("invalid json from board: %1")
                            .arg(parseError.errorString()));
            continue;
        }

        const QJsonObject msg = doc.object();
        const QString event = msg.value(QStringLiteral("event")).toString();
        const int devId = msg.value(QStringLiteral("dev")).toInt(-1);
        int mappedDevId = devId;
        if ((event == QStringLiteral("hello") || event == QStringLiteral("heartbeat")) && devId >= 0) {
            bindDeviceSocket(socket, devId);
            LOG_INFO(kLogSource,
                     QStringLiteral("Board event: event=%1 dev=%2 peer=%3:%4")
                         .arg(event)
                         .arg(devId)
                         .arg(socket->peerAddress().toString())
                         .arg(socket->peerPort()));
        } else {
            if (mappedDevId < 0) {
                mappedDevId = socketDevId_.value(socket, -1);
            }
            LOG_DEBUG(kLogSource,
                      QStringLiteral("Board message: %1")
                          .arg(QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact))));
        }

        if (mappedDevId >= 0) {
            emit boardMessageReceived(mappedDevId, msg);
        }
    }
}

int DeviceTcpServer::findJsonObjectEnd(const QByteArray &buffer, int startIndex)
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

QByteArray DeviceTcpServer::toLine(const QJsonObject &obj)
{
    return QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
}

bool DeviceTcpServer::isControlCommand(const QJsonObject &obj)
{
    const QString cmd = obj.value(QStringLiteral("cmd")).toString();
    return cmd == QStringLiteral("relay.set") ||
           cmd == QStringLiteral("relay.stopall");
}

QByteArray DeviceTcpServer::controlCommandSignature(const QJsonObject &obj)
{
    QJsonObject normalized = obj;
    normalized.remove(QStringLiteral("id"));
    return QJsonDocument(normalized).toJson(QJsonDocument::Compact);
}

void DeviceTcpServer::bindDeviceSocket(QTcpSocket *socket, int devId)
{
    QTcpSocket *old = devSocket_.value(devId, nullptr);
    if (old && old != socket) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Device %1 reconnected, closing old socket %2:%3")
                        .arg(devId)
                        .arg(old->peerAddress().toString())
                        .arg(old->peerPort()));
        buffers_.remove(old);
        socketDevId_.remove(old);
        old->disconnectFromHost();
        old->deleteLater();
    }
    socketDevId_[socket] = devId;
    devSocket_[devId] = socket;
    recentControlByDev_.remove(devId);
}

}  // namespace rpc
}  // namespace fanzhou
