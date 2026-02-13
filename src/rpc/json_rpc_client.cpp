/**
 * @file json_rpc_client.cpp
 * @brief JSON-RPC TCP客户端实现
 */

#include "json_rpc_client.h"
#include "utils/logger.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QTimer>

namespace fanzhou {
namespace rpc {

namespace {
const char *const kLogSource = "RpcClient";
constexpr int kMaxRequestId = 2000000000;  ///< nextId_回绕阈值
}

JsonRpcClient::JsonRpcClient(QObject *parent)
    : QObject(parent)
{
    connect(&socket_, &QTcpSocket::readyRead, this, &JsonRpcClient::onReadyRead);
    connect(&socket_, &QTcpSocket::connected, this, &JsonRpcClient::connected);
    connect(&socket_, &QTcpSocket::disconnected, this, &JsonRpcClient::disconnected);

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(&socket_, &QAbstractSocket::errorOccurred,
            this, &JsonRpcClient::onSocketError);
#else
    connect(&socket_,
            QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &JsonRpcClient::onSocketError);
#endif

    LOG_DEBUG(kLogSource, QStringLiteral("RPC client initialized"));
}

void JsonRpcClient::setEndpoint(const QString &host, quint16 port)
{
    host_ = host;
    port_ = port;
    LOG_INFO(kLogSource,
             QStringLiteral("Set RPC endpoint: %1:%2").arg(host).arg(port));
}

bool JsonRpcClient::connectToServer(int timeoutMs)
{
    if (socket_.state() == QAbstractSocket::ConnectedState) {
        return true;
    }

    LOG_INFO(kLogSource,
             QStringLiteral("Connecting to RPC server: %1:%2").arg(host_).arg(port_));
    socket_.connectToHost(host_, port_);

    if (!socket_.waitForConnected(timeoutMs)) {
        LOG_ERROR(kLogSource,
                  QStringLiteral("Connection failed: %1").arg(socket_.errorString()));
        emit transportError(
            QStringLiteral("connectToHost failed: %1").arg(socket_.errorString()));
        return false;
    }

    LOG_INFO(kLogSource, QStringLiteral("RPC server connected"));
    return true;
}

void JsonRpcClient::disconnectFromServer()
{
    LOG_INFO(kLogSource, QStringLiteral("Disconnecting from RPC server"));
    socket_.disconnectFromHost();
}

bool JsonRpcClient::isConnected() const
{
    return socket_.state() == QAbstractSocket::ConnectedState;
}

QJsonObject JsonRpcClient::makeError(int code, const QString &message) const
{
    return QJsonObject{
        {QStringLiteral("code"), code},
        {QStringLiteral("message"), message}
    };
}

QByteArray JsonRpcClient::packRequest(int id, const QString &method,
                                        const QJsonObject &params) const
{
    QJsonObject request;
    request[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    request[QStringLiteral("id")] = id;
    request[QStringLiteral("method")] = method;
    request[QStringLiteral("params")] = params;

    return QJsonDocument(request).toJson(QJsonDocument::Compact) + "\n";
}

int JsonRpcClient::callAsync(const QString &method, const QJsonObject &params)
{
    if (!connectToServer()) {
        return -1;
    }

    const int id = nextId_++;
    if (nextId_ > kMaxRequestId) nextId_ = 1;  // 防止整数溢出
    pending_.insert(id, method);

    const QByteArray payload = packRequest(id, method, params);

    LOG_DEBUG(kLogSource,
              QStringLiteral("Sending RPC request [id=%1] method: %2")
                  .arg(id)
                  .arg(method));

    const qint64 n = socket_.write(payload);
    if (n != payload.size()) {
        LOG_ERROR(kLogSource,
                  QStringLiteral("RPC request send failed [id=%1]: %2")
                      .arg(id)
                      .arg(socket_.errorString()));
        emit transportError(
            QStringLiteral("write failed: %1").arg(socket_.errorString()));
        pending_.remove(id);
        return -1;
    }
    socket_.flush();
    return id;
}

int JsonRpcClient::callAsync(const QString &method, const QJsonObject &params,
                              Callback callback, int timeoutMs)
{
    const int id = callAsync(method, params);
    if (id < 0) {
        if (callback) {
            callback(QJsonValue(), makeError(-32000, QStringLiteral("transport write/connect failed")));
        }
        return -1;
    }

    if (callback) {
        callbacks_.insert(id, std::move(callback));
    }

    if (timeoutMs > 0) {
        QTimer::singleShot(timeoutMs, this, [this, id, method]() {
            if (!pending_.contains(id)) {
                return;
            }
            pending_.remove(id);

            LOG_WARNING(kLogSource,
                        QStringLiteral("RPC request timeout [id=%1] method: %2")
                            .arg(id)
                            .arg(method));

            auto it = callbacks_.find(id);
            if (it != callbacks_.end()) {
                auto cb = it.value();
                callbacks_.erase(it);
                if (cb) {
                    cb(QJsonValue(), makeError(-32001, QStringLiteral("timeout")));
                }
            }
        });
    }

    return id;
}

void JsonRpcClient::dispatchCallback(int id, const QJsonValue &result,
                                       const QJsonObject &error)
{
    auto it = callbacks_.find(id);
    if (it == callbacks_.end()) {
        return;
    }

    auto cb = it.value();
    callbacks_.erase(it);
    if (cb) {
        cb(result, error);
    }
}

QJsonValue JsonRpcClient::call(const QString &method, const QJsonObject &params,
                                 int timeoutMs)
{
    if (!connectToServer(timeoutMs)) {
        return QJsonObject{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("not connected")}
        };
    }

    const int id = nextId_++;
    if (nextId_ > kMaxRequestId) nextId_ = 1;  // 防止整数溢出
    pending_.insert(id, method);

    const QByteArray payload = packRequest(id, method, params);

    LOG_DEBUG(kLogSource,
              QStringLiteral("Sync RPC call [id=%1] method: %2").arg(id).arg(method));

    if (socket_.write(payload) != payload.size()) {
        pending_.remove(id);
        LOG_ERROR(kLogSource,
                  QStringLiteral("Sync RPC send failed [id=%1]: %2")
                      .arg(id)
                      .arg(socket_.errorString()));
        return QJsonObject{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"),
             QStringLiteral("write failed: %1").arg(socket_.errorString())}
        };
    }
    socket_.flush();

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QJsonValue outResult;
    QJsonObject outError;
    bool received = false;

    const auto conn = connect(this, &JsonRpcClient::callFinished, this,
                               [&](int rid, const QJsonValue &result,
                                   const QJsonObject &error) {
        if (rid != id) {
            return;
        }
        received = true;
        outResult = result;
        outError = error;
        loop.quit();
    });

    connect(&timer, &QTimer::timeout, &loop, [&]() {
        loop.quit();
    });

    timer.start(timeoutMs);
    loop.exec();

    disconnect(conn);
    pending_.remove(id);

    if (!received) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Sync RPC timeout [id=%1] method: %2")
                        .arg(id)
                        .arg(method));
        return QJsonObject{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("timeout")}
        };
    }

    if (!outError.isEmpty()) {
        const QString errorMsg = outError.value(QStringLiteral("message")).toString();
        LOG_WARNING(kLogSource,
                    QStringLiteral("Sync RPC error [id=%1]: %2")
                        .arg(id)
                        .arg(errorMsg));
        return QJsonObject{
            {QStringLiteral("ok"), false},
            {QStringLiteral("rpcError"), outError}
        };
    }

    LOG_DEBUG(kLogSource,
              QStringLiteral("Sync RPC success [id=%1] method: %2").arg(id).arg(method));
    return outResult;
}

void JsonRpcClient::onReadyRead()
{
    rxBuffer_ += socket_.readAll();

    while (true) {
        const int idx = rxBuffer_.indexOf('\n');
        if (idx < 0) {
            break;
        }

        const QByteArray line = rxBuffer_.left(idx).trimmed();
        rxBuffer_.remove(0, idx + 1);

        if (!line.isEmpty()) {
            handleLine(line);
        }
    }
}

void JsonRpcClient::handleLine(const QByteArray &line)
{
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(line, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        LOG_ERROR(kLogSource,
                  QStringLiteral("Parse RPC response failed: %1")
                      .arg(parseError.errorString()));
        emit transportError(
            QStringLiteral("parse response failed: %1 | line=%2")
                .arg(parseError.errorString(), QString::fromUtf8(line)));
        return;
    }

    const QJsonObject obj = doc.object();
    const int id = obj.value(QStringLiteral("id")).toInt(-1);

    QJsonValue result;
    QJsonObject error;

    if (obj.contains(QStringLiteral("error")) &&
        obj.value(QStringLiteral("error")).isObject()) {
        error = obj.value(QStringLiteral("error")).toObject();
        LOG_DEBUG(kLogSource,
                  QStringLiteral("Received RPC error response [id=%1]: %2")
                      .arg(id)
                      .arg(error.value(QStringLiteral("message")).toString()));
    } else {
        result = obj.value(QStringLiteral("result"));
        LOG_DEBUG(kLogSource, QStringLiteral("Received RPC response [id=%1]").arg(id));
    }

    emit callFinished(id, result, error);
    dispatchCallback(id, result, error);
    pending_.remove(id);
}

void JsonRpcClient::onSocketError(QAbstractSocket::SocketError)
{
    LOG_ERROR(kLogSource,
              QStringLiteral("RPC socket error: %1").arg(socket_.errorString()));
    emit transportError(socket_.errorString());
}

}  // namespace rpc
}  // namespace fanzhou
