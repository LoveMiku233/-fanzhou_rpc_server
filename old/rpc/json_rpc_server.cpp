/**
 * @file json_rpc_server.cpp
 * @brief JSON-RPC 服务器实现
 * 
 * 提供基于TCP的JSON-RPC 2.0服务器功能，接收并处理客户端请求。
 */

#include "json_rpc_server.h"
#include "json_rpc_dispatcher.h"

#include <QTcpSocket>
#include <QJsonDocument>
#include "utils/logger.h"

// 日志来源标识
static const QString LOG_SOURCE = "RPC服务器";


/**
 * @brief 构造函数，初始化RPC服务器
 * @param dispatcher RPC方法分发器指针
 * @param parent 父对象指针
 */
JsonRpcServer::JsonRpcServer(JsonRpcDispatcher* dispatcher, QObject* parent)
    : QTcpServer(parent), dispatcher_(dispatcher)
{
    connect(this, &QTcpServer::newConnection, this, &JsonRpcServer::onNewConnection);
    LOG_DEBUG(LOG_SOURCE, "RPC服务器已初始化");
}

/**
 * @brief 处理新客户端连接
 * 
 * 当有新客户端连接时，创建接收缓冲区并绑定信号槽
 */
void JsonRpcServer::onNewConnection()
{
    // 处理所有等待中的连接
    while (hasPendingConnections()) {
        auto* s = nextPendingConnection();
        buffers_[s] = QByteArray{};
        connect(s, &QTcpSocket::readyRead, this, &JsonRpcServer::onReadyRead);
        connect(s, &QTcpSocket::disconnected, this, &JsonRpcServer::onDisconnected);
        
        LOG_INFO(LOG_SOURCE, QString("新客户端连接: %1:%2")
            .arg(s->peerAddress().toString()).arg(s->peerPort()));
    }
}

/**
 * @brief 处理套接字数据就绪事件
 * 
 * 读取客户端发送的数据并进行解析处理
 */
void JsonRpcServer::onReadyRead()
{
    // 确定是哪个套接字发送的数据
    // sender() 返回 QObject*，表示"谁触发了这个槽"
    // qobject_cast 是Qt的动态类型转换，如果不是 QTcpSocket 则返回 nullptr
    auto* s = qobject_cast<QTcpSocket*>(sender());
    if (!s) return;

    buffers_[s].append(s->readAll());
    // 尝试解析"完整的消息"
    processLines(s);
}

/**
 * @brief 处理缓冲区中的数据行
 * @param s 客户端套接字指针
 * 
 * 逐行解析JSON-RPC请求并返回响应
 */
void JsonRpcServer::processLines(QTcpSocket *s)
{
    // 获取该套接字对应的缓冲区
    auto& buf = buffers_[s];

    for (;;) {
        // 查找第一条完整消息的结束位置
        const int nl = buf.indexOf('\n');
        if (nl < 0) break;
        // 提取这一行并从缓冲区中删除
        const QByteArray line = buf.left(nl);
        buf.remove(0, nl + 1);

        // 去除空白字符
        const QByteArray trimmed = line.trimmed();
        // 跳过空行
        if (trimmed.isEmpty()) continue;

        // 解析JSON
        QJsonParseError err {};
        const auto doc = QJsonDocument::fromJson(trimmed, &err);
        // 处理解析错误
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            LOG_WARNING(LOG_SOURCE, QString("JSON解析错误: %1").arg(err.errorString()));
            QJsonObject resp {
                {"jsonrpc", "2.0"},
                {"id", QJsonValue(QJsonValue::Null)},
                {"error", QJsonObject{{"code", -32700}, {"message", "Parse error"}}}
            };
            s->write(toLine(resp));
            continue;
        }

        // 正常处理请求
        const QJsonObject req = doc.object();
        const QString method = req.value("method").toString();
        const QJsonValue reqId = req.value("id");
        
        LOG_DEBUG(LOG_SOURCE, QString("收到RPC请求 [id=%1] 方法: %2")
            .arg(reqId.isNull() ? "null" : QString::number(reqId.toInt()))
            .arg(method));
        
        const QJsonObject resp = dispatcher_->handle(req);
        if (!resp.isEmpty()) {
            s->write(toLine(resp));
            
            // 记录响应日志
            if (resp.contains("error")) {
                LOG_DEBUG(LOG_SOURCE, QString("RPC响应错误 [id=%1]: %2")
                    .arg(reqId.isNull() ? "null" : QString::number(reqId.toInt()))
                    .arg(resp.value("error").toObject().value("message").toString()));
            } else {
                LOG_DEBUG(LOG_SOURCE, QString("RPC响应成功 [id=%1]")
                    .arg(reqId.isNull() ? "null" : QString::number(reqId.toInt())));
            }
        }
    }
}

/**
 * @brief 将JSON对象转换为可发送的数据行
 * @param obj JSON对象
 * @return 编码后的JSON数据（以换行符结尾）
 */
QByteArray JsonRpcServer::toLine(const QJsonObject& obj)
{
    return QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";
}

/**
 * @brief 处理客户端断开连接
 * 
 * 清理该客户端对应的缓冲区并释放资源
 */
void JsonRpcServer::onDisconnected()
{
    auto* s = qobject_cast<QTcpSocket*>(sender());
    if (!s) return;
    
    LOG_INFO(LOG_SOURCE, QString("客户端断开连接: %1:%2")
        .arg(s->peerAddress().toString()).arg(s->peerPort()));
    
    buffers_.remove(s);
    s->deleteLater();
}
