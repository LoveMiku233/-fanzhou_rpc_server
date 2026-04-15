/**
 * @file device_tcp_server.h
 * @brief 设备协议TCP服务器（端口9000）
 */

#ifndef FANZHOU_DEVICE_TCP_SERVER_H
#define FANZHOU_DEVICE_TCP_SERVER_H

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QTcpServer>
#include <QByteArray>

class QTcpSocket;

namespace fanzhou {
namespace core {
class CoreContext;
}

namespace rpc {

/**
 * @brief 兼容 protocol_v1.2.1.md 的设备JSON TCP服务
 *
 * JSON流协议：支持半包/粘包和多行格式，按完整对象边界解析。
 */
class DeviceTcpServer : public QTcpServer
{
    Q_OBJECT

public:
    explicit DeviceTcpServer(QObject *parent = nullptr);

    void setCoreContext(core::CoreContext *context);

    QJsonObject connectionStatus() const;
    bool sendCommand(const QJsonObject &command, int targetDev, QString *error = nullptr);
    bool sendCommandAndWait(const QJsonObject &command,
                            int targetDev,
                            int timeoutMs,
                            QJsonObject *response,
                            QString *error = nullptr);

signals:
    void boardMessageReceived(int devId, QJsonObject message);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    struct RecentControlCommand {
        QByteArray signature;
        qint64 timestampMs = 0;
    };

    void processLines(QTcpSocket *socket);
    static int findJsonObjectEnd(const QByteArray &buffer, int startIndex);
    static QByteArray toLine(const QJsonObject &obj);
    static bool isControlCommand(const QJsonObject &obj);
    static QByteArray controlCommandSignature(const QJsonObject &obj);
    void bindDeviceSocket(QTcpSocket *socket, int devId);

    core::CoreContext *context_ = nullptr;
    QHash<QTcpSocket *, QByteArray> buffers_;
    QHash<QTcpSocket *, int> socketDevId_;
    QHash<int, QTcpSocket *> devSocket_;
    QHash<int, RecentControlCommand> recentControlByDev_;

    static constexpr int kMaxBufferSize = 1024 * 1024;
    static constexpr int kMaxIncompleteObjectSize = 4096;
    static constexpr int kMaxConnections = 64;
    static constexpr qint64 kControlDedupWindowMs = 1500;
};

}  // namespace rpc
}  // namespace fanzhou

#endif  // FANZHOU_DEVICE_TCP_SERVER_H
