/**
 * @file mqtt_channel_manager.cpp
 * @brief MQTT多通道管理器实现
 */

#include "mqtt_channel_manager.h"
#include "mqtt_client.h"
#include "utils/logger.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>

namespace fanzhou {
namespace cloud {

namespace {
const char *const kLogSource = "MqttChannelManager";
}

// ==================== MqttChannelManager ====================

MqttChannelManager::MqttChannelManager(QObject *parent)
    : QObject(parent)
{
}

MqttChannelManager::~MqttChannelManager()
{
    disconnectAll();
    for (auto it = channels_.begin(); it != channels_.end(); ++it) {
        if (it->client) {
            it->client->deleteLater();
        }
    }
    channels_.clear();
}

bool MqttChannelManager::addChannel(const core::MqttChannelConfig &config, QString *error)
{
    if (channels_.contains(config.channelId)) {
        if (error) {
            *error = QStringLiteral("Channel ID %1 already exists").arg(config.channelId);
        }
        return false;
    }

    if (config.broker.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Broker address is required");
        }
        return false;
    }

    ChannelData data;
    data.config = config;
    data.status.channelId = config.channelId;
    data.status.name = config.name;
    data.status.enabled = config.enabled;
    data.status.broker = config.broker;
    data.status.port = config.port;
//    data.platform = CloudPlatformRegistry::instance()
//            .platform(config.cloudid);

    // 创建MQTT客户端
    data.client = new MqttClient(this);
    data.client->setProperty("channelId", config.channelId);

    // 连接信号
    connect(data.client, &MqttClient::connected,
            this, &MqttChannelManager::onClientConnected);
    connect(data.client, &MqttClient::disconnected,
            this, &MqttChannelManager::onClientDisconnected);
    connect(data.client, &MqttClient::messageReceived,
            this, &MqttChannelManager::onClientMessageReceived);

    channels_.insert(config.channelId, data);

    LOG_INFO(kLogSource,
             QStringLiteral("MQTT channel added: id=%1, name=%2, broker=%3:%4")
                 .arg(config.channelId)
                 .arg(config.name)
                 .arg(config.broker)
                 .arg(config.port));

    // 如果启用了自动连接，立即连接
    if (config.enabled) {
        connectChannel(config.channelId);
    }

    return true;
}

bool MqttChannelManager::removeChannel(int channelId, QString *error)
{
    if (!channels_.contains(channelId)) {
        if (error) {
            *error = QStringLiteral("Channel ID %1 not found").arg(channelId);
        }
        return false;
    }

    disconnectChannel(channelId);
    auto &data = channels_[channelId];
    if (data.client) {
        data.client->deleteLater();
    }
    channels_.remove(channelId);

    LOG_INFO(kLogSource, QStringLiteral("MQTT channel removed: id=%1").arg(channelId));
    return true;
}

bool MqttChannelManager::updateChannel(const core::MqttChannelConfig &config, QString *error)
{
    if (!channels_.contains(config.channelId)) {
        if (error) {
            *error = QStringLiteral("Channel ID %1 not found").arg(config.channelId);
        }
        return false;
    }

    auto &data = channels_[config.channelId];
    const bool wasConnected = data.status.connected;

    // 断开旧连接
    if (wasConnected) {
        disconnectChannel(config.channelId);
    }

    // 更新配置
    data.config = config;
    data.status.name = config.name;
    data.status.enabled = config.enabled;
    data.status.broker = config.broker;
    data.status.port = config.port;

    // 如果之前是连接的且新配置启用，重新连接
    if (wasConnected && config.enabled) {
        connectChannel(config.channelId);
    }

    LOG_INFO(kLogSource,
             QStringLiteral("MQTT channel updated: id=%1, name=%2")
                 .arg(config.channelId)
                 .arg(config.name));
    return true;
}

bool MqttChannelManager::connectChannel(int channelId)
{
    if (!channels_.contains(channelId)) {
        return false;
    }

    auto &data = channels_[channelId];
    if (!data.config.enabled) {
        LOG_DEBUG(kLogSource,
                  QStringLiteral("Channel %1 is disabled, skip connect").arg(channelId));
        return false;
    }

    if (data.status.connected) {
        return true;  // 已经连接
    }

    // 设置MQTT客户端参数
    data.client->setBroker(data.config.broker, data.config.port);
    data.client->setCredentials(data.config.username, data.config.password);
    data.client->setClientId(data.config.clientId);
    data.client->setKeepAlive(data.config.keepAliveSec);

    LOG_INFO(kLogSource,
             QStringLiteral("Connecting MQTT channel %1 to %2:%3...")
                 .arg(channelId)
                 .arg(data.config.broker)
                 .arg(data.config.port));

    data.client->connectToBroker();
    return true;
}

void MqttChannelManager::disconnectChannel(int channelId)
{
    if (!channels_.contains(channelId)) {
        return;
    }

    auto &data = channels_[channelId];
    if (data.client && data.status.connected) {
        data.client->disconnectFromBroker();
        data.status.connected = false;
    }
}

void MqttChannelManager::connectAll()
{
    for (auto it = channels_.begin(); it != channels_.end(); ++it) {
        if (it->config.enabled) {
            connectChannel(it.key());
        }
    }
}

void MqttChannelManager::disconnectAll()
{
    for (auto it = channels_.begin(); it != channels_.end(); ++it) {
        disconnectChannel(it.key());
    }
}

bool MqttChannelManager::publish(int channelId, const QString &topic,
                                   const QByteArray &payload, int qos)
{
    if (!channels_.contains(channelId)) {
        return false;
    }

    auto &data = channels_[channelId];
    if (!data.status.connected) {
        return false;
    }

    const int actualQos = (qos >= 0) ? qos : data.config.qos;
    const QString fullTopic = buildFullTopic(data.config.topicPrefix, topic);

    data.client->publish(fullTopic, payload, actualQos);
    data.status.messagesSent++;
    data.status.lastMessageMs = QDateTime::currentMSecsSinceEpoch();

    return true;
}

int MqttChannelManager::publishToAll(const QString &topic,
                                       const QByteArray &payload, int qos)
{
    int count = 0;
    for (auto it = channels_.begin(); it != channels_.end(); ++it) {
        if (it->status.connected && it->config.enabled) {
            if (publish(it.key(), topic, payload, qos)) {
                count++;
            }
        }
    }
    return count;
}

/*
 *  QString topicControlSub;   ///< 订阅：云 → 本地 控制（可选）
 *  QString topicStrategySub;  ///< 订阅：云 → 本地 策略（可选）
 *  QString topicStatusPub;    ///< 发布：本地 → 云 状态（可选）
 *  QString topicEventPub;     ///< 发布：本地 → 云 事件（可选）
 *
 */
bool MqttChannelManager::publishStatus(int channelId,
                                      const QByteArray &payload,
                                      int qos)
{
    if (!channels_.contains(channelId)) {
        return false;
    }

    auto &data = channels_[channelId];
    if (!data.status.connected) {
        return false;
    }

    const QString topic = data.config.topicStatusPub.trimmed();
    if (topic.isEmpty()) {
        LOG_DEBUG(kLogSource,
                  QStringLiteral("Channel %1 has no topicStatusPub configured, skip publishStatus")
                      .arg(channelId));
        return false;
    }

    const int actualQos = (qos >= 0) ? qos : data.config.qos;
    const QString fullTopic = buildFullTopic(data.config.topicPrefix, topic);

    data.client->publish(fullTopic, payload, actualQos);

    data.status.messagesSent++;
    data.status.lastMessageMs = QDateTime::currentMSecsSinceEpoch();

    LOG_DEBUG(kLogSource,
              QStringLiteral("publishStatus: channel=%1 topic=%2 payload=%3")
                  .arg(channelId)
                  .arg(fullTopic)
                  .arg(QString::fromUtf8(payload)));

    return true;
}


bool MqttChannelManager::publishEvent(int channelId,
                                     const QByteArray &payload,
                                     int qos)
{
    if (!channels_.contains(channelId)) {
        return false;
    }

    auto &data = channels_[channelId];
    if (!data.status.connected) {
        return false;
    }

    const QString topic = data.config.topicEventPub.trimmed();
    if (topic.isEmpty()) {
        LOG_DEBUG(kLogSource,
                  QStringLiteral("Channel %1 has no topicEventPub configured, skip publishEvent")
                      .arg(channelId));
        return false;
    }

    const int actualQos = (qos >= 0) ? qos : data.config.qos;
    const QString fullTopic = buildFullTopic(data.config.topicPrefix, topic);

    data.client->publish(fullTopic, payload, actualQos);

    data.status.messagesSent++;
    data.status.lastMessageMs = QDateTime::currentMSecsSinceEpoch();

    LOG_DEBUG(kLogSource,
              QStringLiteral("publishEvent: channel=%1 topic=%2 payload=%3")
                  .arg(channelId)
                  .arg(fullTopic)
                  .arg(QString::fromUtf8(payload)));

    return true;
}

bool MqttChannelManager::subscribeControlSub(int channelId, int qos)
{
    if (!channels_.contains(channelId)) {
        return false;
    }

    auto &data = channels_[channelId];
    if (!data.status.connected) {
        return false;
    }

    const QString topic = data.config.topicControlSub.trimmed();
    if (topic.isEmpty()) {
        LOG_DEBUG(kLogSource,
                  QStringLiteral("Channel %1 has no topicControlSub configured, skip subscribe")
                      .arg(channelId));
        return false;
    }

    const QString fullTopic = buildFullTopic(data.config.topicPrefix, topic);
    const int actualQos = (qos >= 0) ? qos : data.config.qos;

    data.client->subscribe(fullTopic, actualQos);

    LOG_INFO(kLogSource,
             QStringLiteral("subscribeControlSub: channel=%1 topic=%2")
                 .arg(channelId)
                 .arg(fullTopic));

    return true;
}

bool MqttChannelManager::subscribeStrategySub(int channelId, int qos)
{
    if (!channels_.contains(channelId)) {
        return false;
    }

    auto &data = channels_[channelId];
    if (!data.status.connected) {
        return false;
    }

    const QString topic = data.config.topicStrategySub.trimmed();
    if (topic.isEmpty()) {
        LOG_DEBUG(kLogSource,
                  QStringLiteral("Channel %1 has no topicStrategySub configured, skip subscribe")
                      .arg(channelId));
        return false;
    }

    const QString fullTopic = buildFullTopic(data.config.topicPrefix, topic);
    const int actualQos = (qos >= 0) ? qos : data.config.qos;

    data.client->subscribe(fullTopic, actualQos);

    LOG_INFO(kLogSource,
             QStringLiteral("subscribeStrategySub: channel=%1 topic=%2")
                 .arg(channelId)
                 .arg(fullTopic));

    return true;
}



bool MqttChannelManager::subscribe(int channelId, const QString &topic, int qos)
{
    if (!channels_.contains(channelId)) {
        return false;
    }

    auto &data = channels_[channelId];
    if (!data.status.connected) {
        return false;
    }

    const QString fullTopic = buildFullTopic(data.config.topicPrefix, topic);
    data.client->subscribe(fullTopic, qos);
    return true;
}

bool MqttChannelManager::unsubscribe(int channelId, const QString &topic)
{
    if (!channels_.contains(channelId)) {
        return false;
    }

    auto &data = channels_[channelId];
    if (!data.status.connected) {
        return false;
    }

    const QString fullTopic = buildFullTopic(data.config.topicPrefix, topic);
    data.client->unsubscribe(fullTopic);
    return true;
}

void MqttChannelManager::reportDeviceValueChange(quint8 deviceNode, quint8 channel,
                                                   const QJsonObject &value,
                                                   const QJsonObject &oldValue)
{
    // 构建上报消息
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("device_value_change");
    msg[QStringLiteral("timestamp")] = QDateTime::currentMSecsSinceEpoch();
    msg[QStringLiteral("deviceNode")] = static_cast<int>(deviceNode);
    msg[QStringLiteral("channel")] = static_cast<int>(channel);
    msg[QStringLiteral("value")] = value;
}

QList<MqttChannelStatus> MqttChannelManager::channelStatusList() const
{
    QList<MqttChannelStatus> list;
    for (auto it = channels_.constBegin(); it != channels_.constEnd(); ++it) {
        list.append(it->status);
    }
    return list;
}

core::MqttChannelConfig MqttChannelManager::getChannelConfig(int channelId) const
{
    if (channels_.contains(channelId)) {
        return channels_[channelId].config;
    }
    return core::MqttChannelConfig();
}

QList<core::MqttChannelConfig> MqttChannelManager::allChannelConfigs() const
{
    QList<core::MqttChannelConfig> list;
    for (auto it = channels_.constBegin(); it != channels_.constEnd(); ++it) {
        list.append(it->config);
    }
    return list;
}

bool MqttChannelManager::hasChannel(int channelId) const
{
    return channels_.contains(channelId);
}

int MqttChannelManager::channelCount() const
{
    return channels_.size();
}

void MqttChannelManager::onClientConnected()
{
    auto *client = qobject_cast<MqttClient *>(sender());
    if (!client) return;

    const int channelId = client->property("channelId").toInt();
    if (!channels_.contains(channelId)) return;

    auto &data = channels_[channelId];
    data.status.connected = true;
    data.status.lastConnectedMs = QDateTime::currentMSecsSinceEpoch();

    subscribeControlSub(channelId, 1);
    subscribeStrategySub(channelId, 1);

    LOG_INFO(kLogSource,
             QStringLiteral("MQTT channel %1 connected to %2:%3")
                 .arg(channelId)
                 .arg(data.config.broker)
                 .arg(data.config.port));

    emit channelConnected(channelId);
}

void MqttChannelManager::onClientDisconnected()
{
    auto *client = qobject_cast<MqttClient *>(sender());
    if (!client) return;

    const int channelId = client->property("channelId").toInt();
    if (!channels_.contains(channelId)) return;

    auto &data = channels_[channelId];
    data.status.connected = false;

    LOG_INFO(kLogSource, QStringLiteral("MQTT channel %1 disconnected").arg(channelId));

    emit channelDisconnected(channelId);

    // 如果启用了自动重连
    if (data.config.autoReconnect && data.config.enabled) {
        LOG_INFO(kLogSource,
                 QStringLiteral("MQTT channel %1 will reconnect in %2 seconds")
                     .arg(channelId)
                     .arg(data.config.reconnectIntervalSec));

        // 使用QTimer实现延迟重连
        QTimer::singleShot(data.config.reconnectIntervalSec * 1000, this, [this, channelId]() {
            if (channels_.contains(channelId)) {
                auto &channelData = channels_[channelId];
                if (channelData.config.enabled && channelData.config.autoReconnect &&
                    !channelData.status.connected) {
                    LOG_INFO(kLogSource,
                             QStringLiteral("Attempting to reconnect MQTT channel %1...")
                                 .arg(channelId));
                    connectChannel(channelId);
                }
            }
        });
    }
}

void MqttChannelManager::onClientMessageReceived(const QByteArray &message,
                                                    const QString &topic)
{
    auto *client = qobject_cast<MqttClient *>(sender());
    if (!client) return;

    const int channelId = client->property("channelId").toInt();
    if (!channels_.contains(channelId)) return;

    auto &data = channels_[channelId];
    data.status.messagesReceived++;
    data.status.lastMessageMs = QDateTime::currentMSecsSinceEpoch();

    emit messageReceived(channelId, topic, message);
}

QString MqttChannelManager::buildFullTopic(const QString &prefix, const QString &topic) const
{
    if (prefix.isEmpty()) {
        return topic;
    }
    if (prefix.endsWith('/')) {
        return prefix + topic;
    }
    return prefix + '/' + topic;
}

}  // namespace cloud
}  // namespace fanzhou
