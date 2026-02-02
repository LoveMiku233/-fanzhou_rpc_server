#include "mqtt_client.h"
#include <QtMqtt/qmqttclient.h>
#include <QDebug>

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

    connect(m_client, &QMqttClient::errorChanged,
            this, &MqttClient::onError);

    // 3.1.1
    setMqttVer(QMqttClient::MQTT_3_1_1);
}

void MqttClient::setBroker(const QString &host, quint16 port)
{
    m_client->setHostname(host);
    m_client->setPort(port);
}

void MqttClient::setCredentials(const QString &username, const QString &password)
{
    if (!username.isEmpty()) {
        m_client->setUsername(username);
    }
    if (!password.isEmpty()) {
        m_client->setPassword(password);
    }
}

void MqttClient::setMqttVer(QMqttClient::ProtocolVersion ver) {
    m_client->setProtocolVersion(ver);
}

void MqttClient::setCloudType(const CloudTypeId type)
{
    if (type >= CloudTypeId::MAX || type < CloudTypeId::Unknown) {
        return;
    }
    cloud_type = type;
}

CloudTypeId MqttClient::getCloudType()
{
    return cloud_type;
}


void MqttClient::setClientId(const QString &clientId)
{
    if (!clientId.isEmpty()) {
        m_client->setClientId(clientId);
    }
}

void MqttClient::setKeepAlive(int seconds)
{
    if (seconds > 0) {
        m_client->setKeepAlive(seconds);
    }
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

void MqttClient::unsubscribe(const QString &topic)
{
    if (m_client->state() == QMqttClient::Connected) {
        m_client->unsubscribe(topic);
    }
}

bool MqttClient::isConnected() const
{
    return m_client->state() == QMqttClient::Connected;
}

QString MqttClient::hostname() const
{
    return m_client->hostname();
}

quint16 MqttClient::port() const
{
    return m_client->port();
}

void MqttClient::onConnected()
{
    qDebug() << "MQTT connected";
    emit connected();
}

void MqttClient::onDisconnected()
{
    qDebug() << "MQTT disconnected";
    emit disconnected();
}

void MqttClient::onMessageReceived(const QByteArray &message,
                                   const QMqttTopicName &topic)
{
    qDebug() << "MQTT message:"
             << topic.name()
             << message;
    emit messageReceived(message, topic.name());
}

void MqttClient::onError(QMqttClient::ClientError error)
{
    if (error != QMqttClient::NoError) {
        QString errorStr;
        switch (error) {
        case QMqttClient::InvalidProtocolVersion:
            errorStr = QStringLiteral("Invalid protocol version");
            break;
        case QMqttClient::IdRejected:
            errorStr = QStringLiteral("Client ID rejected");
            break;
        case QMqttClient::ServerUnavailable:
            errorStr = QStringLiteral("Server unavailable");
            break;
        case QMqttClient::BadUsernameOrPassword:
            errorStr = QStringLiteral("Bad username or password");
            break;
        case QMqttClient::NotAuthorized:
            errorStr = QStringLiteral("Not authorized");
            break;
        case QMqttClient::TransportInvalid:
            errorStr = QStringLiteral("Transport invalid");
            break;
        case QMqttClient::ProtocolViolation:
            errorStr = QStringLiteral("Protocol violation");
            break;
        case QMqttClient::UnknownError:
        default:
            errorStr = QStringLiteral("Unknown error");
            break;
        }
        qDebug() << "MQTT error:" << errorStr;
        emit errorOccurred(errorStr);
    }
}

}  // namespace cloud
}  // namespace fanzhou


