#include "mqtt_client.h"
#include <QtMqtt/qmqttclient.h>

namespace fanzhou {
namespace cloud {

MqttClient::MqttClient(QObject *parent)
    : QObject(parent),
      m_client(new QMqttClient(this))
{
    connect(m_client, &QMqttClient::connected,
            this, &MqttClient::onConnected);

    connect(m_client, &QMqttClient::disconnected,
            this, &MqttClient::onDisconnected);

    connect(m_client, &QMqttClient::messageReceived,
            this, &MqttClient::onMessageReceived);
}


void MqttClient::setBroker(const QString &host, quint16 port)
{
    m_client->setHostname(host);
    m_client->setPort(port);
}

void MqttClient::connectToBroker()
{
    qDebug() << "MQTT connecting to"
             << m_client->hostname()
             << m_client->port();
    m_client->connectToHost();
}

void MqttClient::disconnectFromBroker()
{
    m_client->disconnectFromHost();
}

void MqttClient::publish(const QString &topic,
                         const QByteArray &payload,
                         int qos)
{
    if (m_client->state() == QMqttClient::Connected) {
        m_client->publish(topic, payload, qos);
    }
}

void MqttClient::subscribe(const QString &topic, int qos)
{
    if (m_client->state() == QMqttClient::Connected) {
        m_client->subscribe(topic, qos);
    }
}

void MqttClient::onConnected()
{
    qDebug() << "MQTT connected";
}

void MqttClient::onDisconnected()
{
    qDebug() << "MQTT disconnected";
}

void MqttClient::onMessageReceived(const QByteArray &message,
                                   const QMqttTopicName &topic)
{
    qDebug() << "MQTT message:"
             << topic.name()
             << message;
}


}
}


