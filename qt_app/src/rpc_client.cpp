/**
 * @file rpc_client.cpp
 * @brief JSON-RPC TCP客户端实现
 */

#include "rpc_client.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QTimer>
#include <QDateTime>

RpcClient::RpcClient(QObject *parent)
    : QObject(parent)
    , host_(QStringLiteral("127.0.0.1"))
    , port_(12345)
    , socket_(new QTcpSocket(this))
    , nextId_(1)
{
    connect(socket_, &QTcpSocket::readyRead, this, &RpcClient::onReadyRead);
    connect(socket_, &QTcpSocket::connected, this, &RpcClient::onConnected);
    connect(socket_, &QTcpSocket::disconnected, this, &RpcClient::onDisconnected);

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(socket_, &QAbstractSocket::errorOccurred,
            this, &RpcClient::onSocketError);
#else
    connect(socket_,
            QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &RpcClient::onSocketError);
#endif
}

RpcClient::~RpcClient()
{
    disconnectFromServer();
}

void RpcClient::setEndpoint(const QString &host, quint16 port)
{
    host_ = host;
    port_ = port;
    log(QStringLiteral("[RPC] 设置服务器端点: %1:%2").arg(host).arg(port));
}

bool RpcClient::connectToServer(int timeoutMs)
{
    if (socket_->state() == QAbstractSocket::ConnectedState) {
        return true;
    }

    log(QStringLiteral("[RPC] 正在连接服务器: %1:%2").arg(host_).arg(port_));
    socket_->connectToHost(host_, port_);

    if (!socket_->waitForConnected(timeoutMs)) {
        QString errorStr = socket_->errorString();
        log(QStringLiteral("[RPC] 连接失败: %1").arg(errorStr));
        emit transportError(QStringLiteral("连接失败: %1").arg(errorStr));
        return false;
    }

    log(QStringLiteral("[RPC] 服务器连接成功"));
    return true;
}

void RpcClient::disconnectFromServer()
{
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        log(QStringLiteral("[RPC] 断开服务器连接"));
        socket_->disconnectFromHost();
    }
}

bool RpcClient::isConnected() const
{
    return socket_->state() == QAbstractSocket::ConnectedState;
}

QJsonObject RpcClient::makeError(int code, const QString &message) const
{
    return QJsonObject{
        {QStringLiteral("code"), code},
        {QStringLiteral("message"), message}
    };
}

QByteArray RpcClient::packRequest(int id, const QString &method,
                                   const QJsonObject &params) const
{
    QJsonObject request;
    request[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    request[QStringLiteral("id")] = id;
    request[QStringLiteral("method")] = method;
    request[QStringLiteral("params")] = params;

    return QJsonDocument(request).toJson(QJsonDocument::Compact) + "\n";
}

int RpcClient::callAsync(const QString &method, const QJsonObject &params)
{
    if (!connectToServer()) {
        return -1;
    }

    const int id = nextId_++;
    pending_.insert(id, method);

    const QByteArray payload = packRequest(id, method, params);

    log(QStringLiteral("[RPC] 发送请求 [id=%1] method: %2").arg(id).arg(method));

    const qint64 n = socket_->write(payload);
    if (n != payload.size()) {
        log(QStringLiteral("[RPC] 发送失败 [id=%1]: %2").arg(id).arg(socket_->errorString()));
        emit transportError(QStringLiteral("发送失败: %1").arg(socket_->errorString()));
        pending_.remove(id);
        return -1;
    }
    socket_->flush();
    return id;
}

int RpcClient::callAsync(const QString &method, const QJsonObject &params,
                          Callback callback, int timeoutMs)
{
    const int id = callAsync(method, params);
    if (id < 0) {
        if (callback) {
            callback(QJsonValue(), makeError(-32000, QStringLiteral("传输连接/写入失败")));
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

            log(QStringLiteral("[RPC] 请求超时 [id=%1] method: %2").arg(id).arg(method));

            auto it = callbacks_.find(id);
            if (it != callbacks_.end()) {
                auto cb = it.value();
                callbacks_.erase(it);
                if (cb) {
                    cb(QJsonValue(), makeError(-32001, QStringLiteral("超时")));
                }
            }
        });
    }

    return id;
}

void RpcClient::dispatchCallback(int id, const QJsonValue &result,
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

QJsonValue RpcClient::call(const QString &method, const QJsonObject &params,
                            int timeoutMs)
{
    if (!connectToServer(timeoutMs)) {
        return QJsonObject{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("未连接")}
        };
    }

    const int id = nextId_++;
    pending_.insert(id, method);

    const QByteArray payload = packRequest(id, method, params);

    log(QStringLiteral("[RPC] 同步调用 [id=%1] method: %2").arg(id).arg(method));

    if (socket_->write(payload) != payload.size()) {
        pending_.remove(id);
        log(QStringLiteral("[RPC] 发送失败 [id=%1]: %2").arg(id).arg(socket_->errorString()));
        return QJsonObject{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("发送失败: %1").arg(socket_->errorString())}
        };
    }
    socket_->flush();

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QJsonValue outResult;
    QJsonObject outError;
    bool received = false;

    const auto conn = connect(this, &RpcClient::callFinished, this,
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
        log(QStringLiteral("[RPC] 同步调用超时 [id=%1] method: %2").arg(id).arg(method));
        return QJsonObject{
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("超时")}
        };
    }

    if (!outError.isEmpty()) {
        const QString errorMsg = outError.value(QStringLiteral("message")).toString();
        log(QStringLiteral("[RPC] 调用错误 [id=%1]: %2").arg(id).arg(errorMsg));
        return QJsonObject{
            {QStringLiteral("ok"), false},
            {QStringLiteral("rpcError"), outError}
        };
    }

    log(QStringLiteral("[RPC] 调用成功 [id=%1] method: %2").arg(id).arg(method));
    return outResult;
}

void RpcClient::onReadyRead()
{
    rxBuffer_ += socket_->readAll();

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

void RpcClient::handleLine(const QByteArray &line)
{
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(line, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        log(QStringLiteral("[RPC] 解析响应失败: %1").arg(parseError.errorString()));
        emit transportError(QStringLiteral("解析响应失败: %1").arg(parseError.errorString()));
        return;
    }

    const QJsonObject obj = doc.object();
    const int id = obj.value(QStringLiteral("id")).toInt(-1);

    QJsonValue result;
    QJsonObject error;

    if (obj.contains(QStringLiteral("error")) &&
        obj.value(QStringLiteral("error")).isObject()) {
        error = obj.value(QStringLiteral("error")).toObject();
        log(QStringLiteral("[RPC] 收到错误响应 [id=%1]: %2")
                .arg(id)
                .arg(error.value(QStringLiteral("message")).toString()));
    } else {
        result = obj.value(QStringLiteral("result"));
        log(QStringLiteral("[RPC] 收到响应 [id=%1]").arg(id));
    }

    emit callFinished(id, result, error);
    dispatchCallback(id, result, error);
    pending_.remove(id);
}

void RpcClient::onSocketError(QAbstractSocket::SocketError)
{
    log(QStringLiteral("[RPC] Socket错误: %1").arg(socket_->errorString()));
    emit transportError(socket_->errorString());
}

void RpcClient::onConnected()
{
    log(QStringLiteral("[RPC] 已连接到服务器"));
    emit connected();
}

void RpcClient::onDisconnected()
{
    log(QStringLiteral("[RPC] 服务器连接已断开"));
    emit disconnected();
}

void RpcClient::log(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    emit logMessage(QStringLiteral("[%1] %2").arg(timestamp, message));
}
