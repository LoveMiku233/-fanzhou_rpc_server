#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <QObject>
#include <QtMqtt/qmqttclient.h>
#include "cloud/cloud_types.h"

namespace fanzhou {
namespace cloud {

class MqttClient : public QObject
{
    Q_OBJECT

public:
    explicit MqttClient(QObject *parent = nullptr);

    // 设置Broker地址
    void setBroker(const QString &host, quint16 port = 1883);

    // 设置认证信息
    void setCredentials(const QString &username, const QString &password);

    void setCloudType(const CloudTypeId type);
    CloudTypeId getCloudType();

    // 设置客户端ID
    void setClientId(const QString &clientId);

    // 设置心跳间隔
    void setKeepAlive(int seconds);

    // 连接和断开
    void connectToBroker();
    void disconnectFromBroker();

    // 发布消息
    void publish(const QString &topic, const QByteArray &payload, int qos = 0);

    // 订阅主题
    void subscribe(const QString &topic, int qos = 0);

    // mqtt ver
    void setMqttVer(QMqttClient::ProtocolVersion ver);

    // 取消订阅主题
    void unsubscribe(const QString &topic);

    // 获取连接状态
    bool isConnected() const;

    // 获取Broker信息
    QString hostname() const;
    quint16 port() const;

signals:
    void connected();
    void disconnected();
    void messageReceived(const QByteArray &message, const QString &topic);
    void errorOccurred(const QString &error);
    void stateChanged(QMqttClient::ClientState state);

private slots:
    void onConnected();
    void onDisconnected();
    void onMessageReceived(const QByteArray &message,
                           const QMqttTopicName &topic);
    void onError(QMqttClient::ClientError error);
    void onStateChanged(QMqttClient::ClientState state);

private:
    QMqttClient *m_client;
    CloudTypeId cloud_type = CloudTypeId::Unknown;
};

}  // namespace cloud
}  // namespace fanzhou

#endif  // MQTT_CLIENT_H
