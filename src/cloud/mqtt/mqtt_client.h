#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <QObject>
#include <QtMqtt/qmqttclient.h>

namespace fanzhou {
namespace cloud{

class MqttClient : public QObject
{
    Q_OBJECT

public:
    explicit MqttClient(QObject *parent = nullptr);

    // setBroker
    void setBroker(const QString &host, quint16 port = 1883);
    void connectToBroker();
    void disconnectFromBroker();

    void publish(const QString &topic, const QByteArray &payload, int qos = 0);
    void subscribe(const QString &topic, int qos = 0);

private slots:
    void onConnected();
    void onDisconnected();
    void onMessageReceived(const QByteArray &message,
                           const QMqttTopicName &topic);
private:
    QMqttClient *m_client;
};


}
}

#endif // MQTT_CLIENT_H
