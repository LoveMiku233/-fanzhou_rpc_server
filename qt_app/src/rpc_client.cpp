/**
 * @file rpc_client.cpp
 * @brief JSON-RPC TCP客户端实现
 */

#include "rpc_client.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QTimer>
#include <QDateTime>

namespace {
constexpr int kMaxRequestId = 2000000000;  ///< nextId_回绕阈值
constexpr int kRequestTimeoutSec = 30;       ///< 请求超时清理时间(秒)
constexpr int kCleanupIntervalMs = 60000;    ///< 清理定时器间隔(1分钟)
constexpr int kMaxInflightRequests = 12;     ///< 并发在途请求上限
constexpr int kMaxQueuedRequests = 300;      ///< 本地排队上限
}

RpcClient::RpcClient(QObject *parent)
    : QObject(parent)
    , host_(QStringLiteral("127.0.0.1"))
    , port_(12345)
    , socket_(new QTcpSocket(this))
    , nextId_(1)
    , cleanupTimer_(new QTimer(this))
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

    // 设置清理定时器 - 防止长时间运行后内存泄漏
    connect(cleanupTimer_, &QTimer::timeout, this, &RpcClient::cleanupPendingRequests);
    cleanupTimer_->start(kCleanupIntervalMs);
}

RpcClient::~RpcClient()
{
    disconnectFromServer();
    clearAllCallbacks();
    if (cleanupTimer_) {
        cleanupTimer_->stop();
    }
}

void RpcClient::clearAllCallbacks()
{
    pending_.clear();
    callbacks_.clear();
    requestTimestamps_.clear();
    sendQueue_.clear();
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

void RpcClient::connectToServerAsync()
{
    if (socket_->state() == QAbstractSocket::ConnectedState) {
        emit connected();
        return;
    }
    if (socket_->state() == QAbstractSocket::ConnectingState) {
        return;  // 正在连接中，等待信号
    }

    log(QStringLiteral("[RPC] 正在异步连接服务器: %1:%2").arg(host_).arg(port_));
    socket_->connectToHost(host_, port_);
}

void RpcClient::disconnectFromServer()
{
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        log(QStringLiteral("[RPC] 断开服务器连接"));
        socket_->disconnectFromHost();

        // 断开时清理所有挂起的请求，防止内存泄漏
        clearAllCallbacks();
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

int RpcClient::allocateRequestId()
{
    int candidate = nextId_++;
    if (nextId_ > kMaxRequestId) {
        nextId_ = 1;
    }
    for (int guard = 0; guard < 1000; ++guard) {
        if (!pending_.contains(candidate) && !callbacks_.contains(candidate)) {
            return candidate;
        }
        candidate = nextId_++;
        if (nextId_ > kMaxRequestId) {
            nextId_ = 1;
        }
    }
    return -1;
}

void RpcClient::startRequestTimeout(int id, const QString &method, int timeoutMs)
{
    if (timeoutMs <= 0) {
        return;
    }
    QTimer::singleShot(timeoutMs, this, [this, id, method]() {
        if (!pending_.contains(id)) {
            return;
        }
        pending_.remove(id);
        requestTimestamps_.remove(id);
        log(QStringLiteral("[RPC] 请求超时 [id=%1] method: %2").arg(id).arg(method));

        auto it = callbacks_.find(id);
        if (it != callbacks_.end()) {
            auto cb = it.value();
            callbacks_.erase(it);
            if (cb) {
                cb(QJsonValue(), makeError(-32001, QStringLiteral("超时")));
            }
        }
        tryPumpQueue();
    });
}

int RpcClient::sendRequestNow(int id, const QString &method, const QJsonObject &params, int timeoutMs)
{
    if (!isConnected()) {
        return -1;
    }

    pending_.insert(id, method);
    requestTimestamps_.insert(id, QDateTime::currentMSecsSinceEpoch());
    const QByteArray payload = packRequest(id, method, params);
    log(QStringLiteral("[RPC] 发送请求 [id=%1] method: %2").arg(id).arg(method));
    const qint64 n = socket_->write(payload);
    if (n != payload.size()) {
        log(QStringLiteral("[RPC] 发送失败 [id=%1]: %2").arg(id).arg(socket_->errorString()));
        emit transportError(QStringLiteral("发送失败: %1").arg(socket_->errorString()));
        pending_.remove(id);
        requestTimestamps_.remove(id);
        return -1;
    }
    socket_->flush();
    startRequestTimeout(id, method, timeoutMs);
    return id;
}

void RpcClient::tryPumpQueue()
{
    if (!isConnected()) {
        return;
    }
    while (!sendQueue_.isEmpty() && pending_.size() < kMaxInflightRequests) {
        const OutgoingRequest req = sendQueue_.dequeue();
        if (sendRequestNow(req.id, req.method, req.params, req.timeoutMs) < 0) {
            auto it = callbacks_.find(req.id);
            if (it != callbacks_.end()) {
                auto cb = it.value();
                callbacks_.erase(it);
                if (cb) {
                    cb(QJsonValue(), makeError(-32000, QStringLiteral("传输连接/写入失败")));
                }
            }
        }
    }
}

int RpcClient::callAsync(const QString &method, const QJsonObject &params)
{
    if (!isConnected()) {
        log(QStringLiteral("[RPC] 异步调用失败：未连接服务器, method: %1").arg(method));
        return -1;
    }

    const int id = allocateRequestId();
    if (id < 0) {
        log(QStringLiteral("[RPC] 请求ID耗尽，拒绝请求 method: %1").arg(method));
        return -1;
    }

    if (pending_.size() >= kMaxInflightRequests) {
        if (sendQueue_.size() >= kMaxQueuedRequests) {
            log(QStringLiteral("[RPC] 请求队列已满，拒绝请求 method: %1").arg(method));
            return -1;
        }
        OutgoingRequest req;
        req.id = id;
        req.method = method;
        req.params = params;
        req.timeoutMs = 0;
        sendQueue_.enqueue(req);
        log(QStringLiteral("[RPC] 请求进入队列 [id=%1] method: %2 queue=%3")
            .arg(id).arg(method).arg(sendQueue_.size()));
        return id;
    }
    return sendRequestNow(id, method, params, 0);
}

int RpcClient::callAsync(const QString &method, const QJsonObject &params,
                          Callback callback, int timeoutMs)
{
    if (!isConnected()) {
        log(QStringLiteral("[RPC] 异步调用失败：未连接服务器, method: %1").arg(method));
        if (callback) {
            callback(QJsonValue(), makeError(-32000, QStringLiteral("传输连接/写入失败")));
        }
        return -1;
    }

    const int id = allocateRequestId();
    if (id < 0) {
        if (callback) {
            callback(QJsonValue(), makeError(-32003, QStringLiteral("请求ID耗尽")));
        }
        return -1;
    }

    if (callback) {
        callbacks_.insert(id, std::move(callback));
    }

    if (pending_.size() >= kMaxInflightRequests) {
        if (sendQueue_.size() >= kMaxQueuedRequests) {
            auto it = callbacks_.find(id);
            if (it != callbacks_.end()) {
                auto cb = it.value();
                callbacks_.erase(it);
                if (cb) {
                    cb(QJsonValue(), makeError(-32004, QStringLiteral("请求队列已满")));
                }
            }
            return -1;
        }
        OutgoingRequest req;
        req.id = id;
        req.method = method;
        req.params = params;
        req.timeoutMs = timeoutMs;
        sendQueue_.enqueue(req);
        log(QStringLiteral("[RPC] 请求进入队列 [id=%1] method: %2 queue=%3")
            .arg(id).arg(method).arg(sendQueue_.size()));
        return id;
    }

    if (sendRequestNow(id, method, params, timeoutMs) < 0) {
        auto it = callbacks_.find(id);
        if (it != callbacks_.end()) {
            auto cb = it.value();
            callbacks_.erase(it);
            if (cb) {
                cb(QJsonValue(), makeError(-32000, QStringLiteral("传输连接/写入失败")));
            }
        }
        return -1;
    }
    return id;
}

int RpcClient::callAsync(const QString &method,
                         const QJsonObject &params,
                         QObject *context,
                         Callback callback,
                         int timeoutMs)
{
    if (!context) {
        return callAsync(method, params, std::move(callback), timeoutMs);
    }

    const QPointer<QObject> aliveGuard(context);
    Callback safeCallback = [aliveGuard, callback](const QJsonValue &result, const QJsonObject &error) {
        if (!aliveGuard) {
            return;
        }
        if (callback) {
            callback(result, error);
        }
    };

    return callAsync(method, params, std::move(safeCallback), timeoutMs);
}

void RpcClient::cleanupPendingRequests()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 timeoutMs = kRequestTimeoutSec * 1000;

    QList<int> expiredIds;
    for (auto it = requestTimestamps_.begin(); it != requestTimestamps_.end(); ++it) {
        if (now - it.value() > timeoutMs) {
            expiredIds.append(it.key());
        }
    }

    for (int id : expiredIds) {
        QString method = pending_.value(id, QStringLiteral("unknown"));
        log(QStringLiteral("[RPC] 清理过期请求 [id=%1] method: %2").arg(id).arg(method));

        pending_.remove(id);
        requestTimestamps_.remove(id);

        auto it = callbacks_.find(id);
        if (it != callbacks_.end()) {
            auto cb = it.value();
            callbacks_.erase(it);
            if (cb) {
                cb(QJsonValue(), makeError(-32002, QStringLiteral("请求已过期")));
            }
        }
        tryPumpQueue();
    }

    // 防止哈希表无限增长
    if (pending_.size() > 1000) {
        log(QStringLiteral("[RPC] 警告: 挂起请求过多, 清理所有请求"));
        clearAllCallbacks();
    }
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
    if (nextId_ > kMaxRequestId) nextId_ = 1;  // 防止整数溢出
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
    requestTimestamps_.remove(id);
    tryPumpQueue();
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
    for (auto it = callbacks_.begin(); it != callbacks_.end(); ++it) {
        auto cb = it.value();
        if (cb) {
            cb(QJsonValue(), makeError(-32005, QStringLiteral("连接已断开")));
        }
    }
    clearAllCallbacks();
    emit disconnected();
}

void RpcClient::log(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    emit logMessage(QStringLiteral("[%1] %2").arg(timestamp, message));
}
