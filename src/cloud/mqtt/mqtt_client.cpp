#include "mqtt_client.h"
#include <QtMqtt/qmqttclient.h>
#include "utils/logger.h"

namespace fanzhou {
namespace cloud {

namespace {
const char *const kLogSource = "MqttClient";
}

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

    connect(m_client, &QMqttClient::stateChanged,
            this, &MqttClient::onStateChanged);

    // 使用 MQTT 3.1.1 协议版本
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
    LOG_INFO(kLogSource,
             QStringLiteral("MQTT connecting to %1:%2 (clientId=%3, username=%4)")
                 .arg(m_client->hostname())
                 .arg(m_client->port())
                 .arg(m_client->clientId())
                 .arg(m_client->username().isEmpty() ? QStringLiteral("(none)") : m_client->username()));
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
    LOG_INFO(kLogSource,
             QStringLiteral("MQTT connected to %1:%2")
                 .arg(m_client->hostname())
                 .arg(m_client->port()));
    emit connected();
}

void MqttClient::onDisconnected()
{
    LOG_INFO(kLogSource,
             QStringLiteral("MQTT disconnected from %1:%2")
                 .arg(m_client->hostname())
                 .arg(m_client->port()));
    emit disconnected();
}

void MqttClient::onMessageReceived(const QByteArray &message,
                                   const QMqttTopicName &topic)
{
    LOG_DEBUG(kLogSource,
              QStringLiteral("MQTT message received: topic=%1, size=%2 bytes")
                  .arg(topic.name())
                  .arg(message.size()));
    emit messageReceived(message, topic.name());
}

void MqttClient::onStateChanged(QMqttClient::ClientState state)
{
    QString stateStr;
    switch (state) {
    case QMqttClient::Disconnected:
        stateStr = QStringLiteral("Disconnected");
        break;
    case QMqttClient::Connecting:
        stateStr = QStringLiteral("Connecting");
        break;
    case QMqttClient::Connected:
        stateStr = QStringLiteral("Connected");
        break;
    default:
        stateStr = QStringLiteral("Unknown");
        break;
    }
    LOG_DEBUG(kLogSource,
              QStringLiteral("MQTT state changed: %1 (%2:%3)")
                  .arg(stateStr)
                  .arg(m_client->hostname())
                  .arg(m_client->port()));
    emit stateChanged(state);
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
            errorStr = QStringLiteral("Transport invalid - check network connection");
            break;
        case QMqttClient::ProtocolViolation:
            errorStr = QStringLiteral("Protocol violation");
            break;
        case QMqttClient::UnknownError:
        default:
            errorStr = QStringLiteral("Unknown error (code=%1)").arg(static_cast<int>(error));
            break;
        }
        LOG_ERROR(kLogSource,
                  QStringLiteral("MQTT error: %1 (broker=%2:%3)")
                      .arg(errorStr)
                      .arg(m_client->hostname())
                      .arg(m_client->port()));
        emit errorOccurred(errorStr);
    }
}

}  // namespace cloud
}  // namespace fanzhou


