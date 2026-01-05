/**
 * @file json_rpc_client.cpp
 * @brief JSON-RPC 客户端实现
 * 
 * 提供基于TCP的JSON-RPC 2.0客户端功能，支持同步和异步调用方式。
 */

#include "json_rpc_client.h"

#include <functional>
#include <QJsonDocument>
#include <QEventLoop>
#include <QTimer>
#include "utils/logger.h"

// 日志来源标识
static const QString LOG_SOURCE = "RPC客户端";


/**
 * @brief 构造函数，初始化RPC客户端
 * @param parent 父对象指针
 * 
 * 初始化TCP套接字并连接相关信号槽
 */
JsonRpcClient::JsonRpcClient(QObject* parent) : QObject(parent)
{
    // 连接套接字信号
    connect(&sock_, &QTcpSocket::readyRead, this, &JsonRpcClient::onReadyRead);
    connect(&sock_, &QTcpSocket::connected, this, &JsonRpcClient::connected);
    connect(&sock_, &QTcpSocket::disconnected, this, &JsonRpcClient::disconnected);
    // 兼容 Qt 5.12 的错误信号连接方式
    connect(&sock_,
               QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
               this,
               &JsonRpcClient::onSocketError);
    
    LOG_DEBUG(LOG_SOURCE, "RPC客户端已初始化");
}


/**
 * @brief 设置服务器端点
 * @param host 服务器主机名或IP地址
 * @param port 服务器端口号
 */
void JsonRpcClient::setEndpoint(const QString& host, quint16 port)
{
    host_ = host;
    port_ = port;
    LOG_INFO(LOG_SOURCE, QString("设置RPC服务器端点: %1:%2").arg(host).arg(port));
}

/**
 * @brief 连接到RPC服务器
 * @param timeoutMs 连接超时时间(毫秒)
 * @return 连接成功返回true，失败返回false
 */
bool JsonRpcClient::connectToServer(int timeoutMs)
{
    if (sock_.state() == QAbstractSocket::ConnectedState) return true;

    LOG_INFO(LOG_SOURCE, QString("正在连接RPC服务器: %1:%2").arg(host_).arg(port_));
    sock_.connectToHost(host_, port_);
    if (!sock_.waitForConnected(timeoutMs)) {
        LOG_ERROR(LOG_SOURCE, QString("连接失败: %1").arg(sock_.errorString()));
        emit transportError(QString("connectToHost failed: %1").arg(sock_.errorString()));
        return false;
    }
    LOG_INFO(LOG_SOURCE, "RPC服务器连接成功");
    return true;
}

/**
 * @brief 断开与服务器的连接
 */
void JsonRpcClient::disconnectFromServer()
{
    LOG_INFO(LOG_SOURCE, "断开RPC服务器连接");
    sock_.disconnectFromHost();
}

/**
 * @brief 检查是否已连接到服务器
 * @return 已连接返回true，否则返回false
 */
bool JsonRpcClient::isConnected() const
{
    return sock_.state() == QAbstractSocket::ConnectedState;
}


/**
 * @brief 创建RPC错误对象
 * @param code 错误代码
 * @param message 错误消息
 * @return JSON格式的错误对象
 */
QJsonObject JsonRpcClient::makeError(int code, const QString& message) const
{
    return QJsonObject{
        {"code", code},
        {"message", message},
    };
}

/**
 * @brief 打包RPC请求为JSON格式
 * @param id 请求ID
 * @param method 方法名
 * @param params 参数对象
 * @return 编码后的JSON数据（以换行符结尾）
 */
QByteArray JsonRpcClient::packRequest(int id, const QString& method, const QJsonObject& params) const
{
    QJsonObject req;
    req["jsonrpc"] = "2.0";
    req["id"] = id;
    req["method"] = method;
    req["params"] = params;

    // 每行一个JSON对象，便于解析
    QJsonDocument doc(req);
    return doc.toJson(QJsonDocument::Compact) + "\n";
}

/**
 * @brief 异步调用RPC方法（无回调）
 * @param method 方法名
 * @param params 参数对象
 * @return 请求ID，失败返回-1
 */
int JsonRpcClient::callAsync(const QString& method, const QJsonObject& params)
{
    if (!connectToServer()) return -1;

    const int id = nextId_++;
    pending_.insert(id, method);

    const QByteArray payload = packRequest(id, method, params);
    
    // 记录RPC调用日志
    LOG_DEBUG(LOG_SOURCE, QString("发送RPC请求 [id=%1] 方法: %2, 参数: %3")
        .arg(id).arg(method)
        .arg(QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact))));
    
    const qint64 n = sock_.write(payload);
    if (n != payload.size()) {
        LOG_ERROR(LOG_SOURCE, QString("RPC请求发送失败 [id=%1]: %2").arg(id).arg(sock_.errorString()));
        emit transportError(QString("write failed: %1").arg(sock_.errorString()));
        pending_.remove(id);
        return -1;
    }
    sock_.flush();
    return id;
}

/**
 * @brief 异步调用RPC方法（带回调）
 * @param method 方法名
 * @param params 参数对象
 * @param cb 回调函数
 * @param timeoutMs 超时时间(毫秒)
 * @return 请求ID，失败返回-1
 */
int JsonRpcClient::callAsync(const QString& method, const QJsonObject& params, Callback cb, int timeoutMs)
{
    const int id = callAsync(method, params);
    if (id < 0) {
        if (cb) cb(QJsonValue(), makeError(-32000, "transport write/connect failed"));
        return -1;
    }

    if (cb) callbacks_.insert(id, std::move(cb));

    if (timeoutMs > 0) {
        QTimer::singleShot(timeoutMs, this, [this, id, method](){
            if (!pending_.contains(id)) return;
            pending_.remove(id);
            
            LOG_WARNING(LOG_SOURCE, QString("RPC请求超时 [id=%1] 方法: %2").arg(id).arg(method));

            auto cbIt = callbacks_.find(id);
            if (cbIt != callbacks_.end()) {
                auto cb = cbIt.value();
                callbacks_.erase(cbIt);
                if (cb) cb(QJsonValue(), makeError(-32001, "timeout"));
            }
        });
    }

    return id;
}

/**
 * @brief 分发回调函数
 * @param id 请求ID
 * @param result 返回结果
 * @param error 错误信息
 */
void JsonRpcClient::dispatchCallback(int id, const QJsonValue& result, const QJsonObject& error)
{
    auto it = callbacks_.find(id);
    if (it == callbacks_.end()) return;

    auto cb = it.value();
    callbacks_.erase(it);
    if (cb) cb(result, error);
}



/**
 * @brief 同步调用RPC方法（阻塞等待结果）
 * @param method 方法名
 * @param params 参数对象
 * @param timeoutMs 超时时间(毫秒)
 * @return RPC调用结果
 */
QJsonValue JsonRpcClient::call(const QString& method, const QJsonObject& params, int timeoutMs)
{
    if (!connectToServer(timeoutMs)) {
        return QJsonObject{{"ok", false}, {"error", "not connected"}};
    }

    const int id = nextId_++;
    pending_.insert(id, method);

    const QByteArray payload = packRequest(id, method, params);
    
    // 记录同步RPC调用日志
    LOG_DEBUG(LOG_SOURCE, QString("同步RPC调用 [id=%1] 方法: %2").arg(id).arg(method));
    
    if (sock_.write(payload) != payload.size()) {
        pending_.remove(id);
        LOG_ERROR(LOG_SOURCE, QString("同步RPC发送失败 [id=%1]: %2").arg(id).arg(sock_.errorString()));
        return QJsonObject{{"ok", false}, {"error", QString("write failed: %1").arg(sock_.errorString())}};
    }
    sock_.flush();

    // 使用事件循环等待响应
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QJsonValue outResult;
    QJsonObject outError;
    bool got = false;

    const auto conn = connect(this, &JsonRpcClient::callFinished, this,
                              [&](int rid, const QJsonValue& result, const QJsonObject& error){
        if (rid != id) return;
        got = true;
        outResult = result;
        outError = error;
        loop.quit();
    });

    connect(&timer, &QTimer::timeout, &loop, [&](){
        loop.quit();
    });

    timer.start(timeoutMs);
    loop.exec();

    disconnect(conn);

    pending_.remove(id);

    if (!got) {
        LOG_WARNING(LOG_SOURCE, QString("同步RPC超时 [id=%1] 方法: %2").arg(id).arg(method));
        return QJsonObject{{"ok", false}, {"error", "timeout"}};
    }
    if (!outError.isEmpty()) {
        LOG_WARNING(LOG_SOURCE, QString("同步RPC错误 [id=%1]: %2").arg(id)
            .arg(QString::fromUtf8(QJsonDocument(outError).toJson(QJsonDocument::Compact))));
        return QJsonObject{{"ok", false}, {"rpcError", outError}};
    }
    
    LOG_DEBUG(LOG_SOURCE, QString("同步RPC成功 [id=%1] 方法: %2").arg(id).arg(method));
    return outResult;
}

/**
 * @brief 处理套接字数据就绪事件
 * 
 * 读取接收到的数据并解析JSON响应
 */
void JsonRpcClient::onReadyRead()
{
    rxBuf_ += sock_.readAll();

    while (true) {
        const int idx = rxBuf_.indexOf('\n');
        if (idx < 0) break;

        const QByteArray line = rxBuf_.left(idx).trimmed();
        rxBuf_.remove(0, idx + 1);

        if (!line.isEmpty()) handleLine(line);
    }
}

/**
 * @brief 处理接收到的JSON行数据
 * @param line 一行JSON数据
 */
void JsonRpcClient::handleLine(const QByteArray& line)
{
    QJsonParseError pe;
    const auto doc = QJsonDocument::fromJson(line, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        LOG_ERROR(LOG_SOURCE, QString("解析RPC响应失败: %1").arg(pe.errorString()));
        emit transportError(QString("parse response failed: %1 | line=%2")
                            .arg(pe.errorString(), QString::fromUtf8(line)));
        return;
    }

    const QJsonObject obj = doc.object();
    const int id = obj.value("id").toInt(-1);

    QJsonValue result;
    QJsonObject error;

    if (obj.contains("error") && obj.value("error").isObject()) {
        error = obj.value("error").toObject();
        LOG_DEBUG(LOG_SOURCE, QString("收到RPC错误响应 [id=%1]: %2")
            .arg(id).arg(error.value("message").toString()));
    } else {
        result = obj.value("result");
        LOG_DEBUG(LOG_SOURCE, QString("收到RPC响应 [id=%1]").arg(id));
    }

    emit callFinished(id, result, error);
    dispatchCallback(id, result, error);
    pending_.remove(id);
}

/**
 * @brief 处理套接字错误
 * @param socketError 套接字错误类型（未使用）
 */
void JsonRpcClient::onSocketError(QAbstractSocket::SocketError)
{
    LOG_ERROR(LOG_SOURCE, QString("RPC套接字错误: %1").arg(sock_.errorString()));
    qWarning() << "RPC socket error:" << sock_.errorString();
    emit transportError(sock_.errorString());
}





