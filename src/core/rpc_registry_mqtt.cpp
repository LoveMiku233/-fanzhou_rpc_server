/**
 * @file rpc_registry_mqtt.cpp
 * @brief MQTT多通道管理RPC方法注册
 */

#include "rpc_registry.h"
#include "core_context.h"
#include "rpc_registry_keys.h"

#include "cloud/cloud_types.h"
#include "cloud/mqtt/mqtt_channel_manager.h"
#include "rpc/json_rpc_dispatcher.h"
#include "rpc/rpc_error_codes.h"
#include "rpc/rpc_helpers.h"

#include <QJsonArray>
#include <QJsonObject>
#include <climits>

namespace fanzhou {
namespace core {

namespace {
const QString &kKeyOk = rpc_keys::Ok();
const QString &kKeyName = rpc_keys::Name();
const QString &kKeyEnabled = rpc_keys::Enabled();
const QString &kKeyChannels = rpc_keys::Channels();
const QString &kKeyTotal = rpc_keys::Total();
const QString &kKeyMessage = rpc_keys::Message();

constexpr int kMinChannelId = 1;
constexpr int kMinPort = 1;
constexpr int kMaxPort = 65535;
constexpr int kMinQos = 0;
constexpr int kMaxQos = 2;
constexpr int kMinKeepAliveSec = 1;
constexpr int kMaxKeepAliveSec = 3600;
constexpr int kMinReconnectIntervalSec = 1;
constexpr int kMaxReconnectIntervalSec = 300;

bool parseChannelId(const QJsonObject &params, qint32 &channelId, bool required,
                    QJsonObject &err)
{
    const bool hasKey = params.contains(QStringLiteral("channelId"));
    if (!hasKey && !required) {
        return false;
    }
    if (!rpc::RpcHelpers::getI32InRange(params, "channelId", channelId,
                                        kMinChannelId, INT_MAX)) {
        err = rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                   QStringLiteral("missing/invalid channelId (>0)"));
        return false;
    }
    return true;
}

bool parseOptionalI32InRange(const QJsonObject &params, const char *key,
                             qint32 &value, qint32 minValue, qint32 maxValue,
                             int code, const QString &msg, QJsonObject &err)
{
    if (!params.contains(QLatin1String(key))) {
        return true;
    }
    if (!rpc::RpcHelpers::getI32InRange(params, key, value, minValue, maxValue)) {
        err = rpc::RpcHelpers::err(code, msg);
        return false;
    }
    return true;
}

bool parseOptionalBool(const QJsonObject &params, const char *key, bool &value,
                       int code, const QString &msg, QJsonObject &err)
{
    if (!params.contains(QLatin1String(key))) {
        return true;
    }
    if (!rpc::RpcHelpers::getBool(params, key, value, value)) {
        err = rpc::RpcHelpers::err(code, msg);
        return false;
    }
    return true;
}
}  // namespace

void RpcRegistry::registerMqtt()
{
    // 获取MQTT通道列表
    dispatcher_->registerMethod(QStringLiteral("mqtt.channels.list"),
                                 [this](const QJsonObject &) {
        if (!context_->mqttManager) {
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("MQTT manager not available"));
        }

        QJsonArray channels;
        const auto statusList = context_->mqttManager->channelStatusList();
        for (const auto &status : statusList) {
            QJsonObject obj;
            obj[QStringLiteral("channelId")] = status.channelId;
            obj[kKeyName] = status.name;
            obj[kKeyEnabled] = status.enabled;
            obj[QStringLiteral("connected")] = status.connected;
            obj[QStringLiteral("broker")] = status.broker;
            obj[QStringLiteral("port")] = static_cast<int>(status.port);
            obj[QStringLiteral("messagesSent")] = static_cast<double>(status.messagesSent);
            obj[QStringLiteral("messagesReceived")] = static_cast<double>(status.messagesReceived);
            if (status.lastConnectedMs > 0) {
                obj[QStringLiteral("lastConnectedMs")] = static_cast<double>(status.lastConnectedMs);
            }
            channels.append(obj);
        }

        return QJsonObject{
            {kKeyOk, true},
            {kKeyChannels, channels},
            {kKeyTotal, channels.size()}
        };
    });

    // 添加MQTT通道
    dispatcher_->registerMethod(QStringLiteral("mqtt.channels.add"),
                                 [this](const QJsonObject &params) {
        if (!context_->mqttManager) {
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("MQTT manager not available"));
        }

        qint32 channelId = 0;
        QString name, broker, clientId, username, password, topicPrefix;
        int type = 0;
        qint32 port = 1883;
        bool enabled = true;
        qint32 keepAliveSec = 60;
        bool autoReconnect = true;
        qint32 reconnectIntervalSec = 5;
        qint32 qos = 0;
        QJsonObject parseErr;

        if (!parseChannelId(params, channelId, true, parseErr)) {
            return parseErr;
        }
        if (!rpc::RpcHelpers::getString(params, "broker", broker) || broker.isEmpty()) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing broker"));
        }

        rpc::RpcHelpers::getString(params, "name", name);
        if (!parseOptionalI32InRange(params, "port", port, kMinPort, kMaxPort,
                                     rpc::RpcError::BadParameterValue,
                                     QStringLiteral("invalid port (1-65535)"),
                                     parseErr)) {
            return parseErr;
        }
        if (!parseOptionalI32InRange(params, "type", type, 0, INT_MAX,
                                     rpc::RpcError::BadParameterValue,
                                     QStringLiteral("invalid type"), parseErr)) {
            return parseErr;
        }
        rpc::RpcHelpers::getString(params, "clientId", clientId);
        rpc::RpcHelpers::getString(params, "username", username);
        rpc::RpcHelpers::getString(params, "password", password);
        rpc::RpcHelpers::getString(params, "topicPrefix", topicPrefix);
        if (!parseOptionalBool(params, "enabled", enabled,
                               rpc::RpcError::BadParameterValue,
                               QStringLiteral("invalid enabled (bool)"), parseErr)) {
            return parseErr;
        }
        if (!parseOptionalI32InRange(params, "keepAliveSec", keepAliveSec,
                                     kMinKeepAliveSec, kMaxKeepAliveSec,
                                     rpc::RpcError::BadParameterValue,
                                     QStringLiteral("invalid keepAliveSec (1-3600)"),
                                     parseErr)) {
            return parseErr;
        }
        if (!parseOptionalBool(params, "autoReconnect", autoReconnect,
                               rpc::RpcError::BadParameterValue,
                               QStringLiteral("invalid autoReconnect (bool)"), parseErr)) {
            return parseErr;
        }
        if (!parseOptionalI32InRange(params, "reconnectIntervalSec", reconnectIntervalSec,
                                     kMinReconnectIntervalSec, kMaxReconnectIntervalSec,
                                     rpc::RpcError::BadParameterValue,
                                     QStringLiteral("invalid reconnectIntervalSec (1-300)"),
                                     parseErr)) {
            return parseErr;
        }
        if (!parseOptionalI32InRange(params, "qos", qos, kMinQos, kMaxQos,
                                     rpc::RpcError::BadParameterValue,
                                     QStringLiteral("invalid qos (0-2)"), parseErr)) {
            return parseErr;
        }

        MqttChannelConfig config;
        config.type = static_cast<cloud::CloudTypeId>(type);
        config.channelId = channelId;
        config.name = name.isEmpty() ? QStringLiteral("mqtt-%1").arg(channelId) : name;
        config.enabled = enabled;
        config.broker = broker;
        config.port = static_cast<quint16>(port);
        config.clientId = clientId;
        config.username = username;
        config.password = password;
        config.topicPrefix = topicPrefix;
        config.keepAliveSec = keepAliveSec;
        config.autoReconnect = autoReconnect;
        config.reconnectIntervalSec = reconnectIntervalSec;
        config.qos = qos;

        QString error;
        if (!context_->mqttManager->addChannel(config, &error)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, error);
        }

        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("channelId"), channelId},
            {kKeyMessage, QStringLiteral("MQTT通道添加成功")}
        };
    });

    // 删除MQTT通道
    dispatcher_->registerMethod(QStringLiteral("mqtt.channels.remove"),
                                 [this](const QJsonObject &params) {
        if (!context_->mqttManager) {
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("MQTT manager not available"));
        }

        qint32 channelId = 0;
        QJsonObject parseErr;
        if (!parseChannelId(params, channelId, true, parseErr)) {
            return parseErr;
        }

        QString error;
        if (!context_->mqttManager->removeChannel(channelId, &error)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, error);
        }

        return QJsonObject{
            {kKeyOk, true},
            {kKeyMessage, QStringLiteral("MQTT通道已删除")}
        };
    });

    // 连接MQTT通道
    dispatcher_->registerMethod(QStringLiteral("mqtt.channels.connect"),
                                 [this](const QJsonObject &params) {
        if (!context_->mqttManager) {
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("MQTT manager not available"));
        }

        qint32 channelId = 0;
        QJsonObject parseErr;
        if (!parseChannelId(params, channelId, false, parseErr)) {
            return parseErr;
        }
        if (channelId <= 0) {
            // 如果没有指定channelId，连接所有通道
            context_->mqttManager->connectAll();
            return QJsonObject{
                {kKeyOk, true},
                {kKeyMessage, QStringLiteral("正在连接所有MQTT通道")}
            };
        }

        if (!context_->mqttManager->hasChannel(channelId)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("channel not found"));
        }

        context_->mqttManager->connectChannel(channelId);
        return QJsonObject{
            {kKeyOk, true},
            {kKeyMessage, QStringLiteral("正在连接MQTT通道 %1").arg(channelId)}
        };
    });

    // 断开MQTT通道
    dispatcher_->registerMethod(QStringLiteral("mqtt.channels.disconnect"),
                                 [this](const QJsonObject &params) {
        if (!context_->mqttManager) {
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("MQTT manager not available"));
        }

        qint32 channelId = 0;
        QJsonObject parseErr;
        if (!parseChannelId(params, channelId, false, parseErr)) {
            return parseErr;
        }
        if (channelId <= 0) {
            context_->mqttManager->disconnectAll();
            return QJsonObject{
                {kKeyOk, true},
                {kKeyMessage, QStringLiteral("已断开所有MQTT通道")}
            };
        }

        context_->mqttManager->disconnectChannel(channelId);
        return QJsonObject{
            {kKeyOk, true},
            {kKeyMessage, QStringLiteral("已断开MQTT通道 %1").arg(channelId)}
        };
    });

    // 发布消息到MQTT通道
    dispatcher_->registerMethod(QStringLiteral("mqtt.publish"),
                                 [this](const QJsonObject &params) {
        if (!context_->mqttManager) {
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("MQTT manager not available"));
        }

        qint32 channelId = 0;
        QString topic, payload;
        qint32 qos = 0;
        QJsonObject parseErr;

        if (!rpc::RpcHelpers::getString(params, "topic", topic) || topic.isEmpty()) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing topic"));
        }
        if (!rpc::RpcHelpers::getString(params, "payload", payload)) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing payload"));
        }
        if (!parseOptionalI32InRange(params, "qos", qos, kMinQos, kMaxQos,
                                     rpc::RpcError::BadParameterValue,
                                     QStringLiteral("invalid qos (0-2)"), parseErr)) {
            return parseErr;
        }

        const QByteArray payloadBytes = payload.toUtf8();

        if (parseChannelId(params, channelId, false, parseErr) && channelId > 0) {
            // 发布到指定通道
            if (!context_->mqttManager->publish(channelId, topic, payloadBytes, qos)) {
                return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("publish failed"));
            }
            return QJsonObject{
                {kKeyOk, true},
                {QStringLiteral("channelId"), channelId}
            };
        } else if (!parseErr.isEmpty()) {
            return parseErr;
        } else {
            // 发布到所有通道
            const int count = context_->mqttManager->publishToAll(topic, payloadBytes, qos);
            return QJsonObject{
                {kKeyOk, true},
                {QStringLiteral("sentCount"), count}
            };
        }
    });

    // 订阅MQTT主题
    dispatcher_->registerMethod(QStringLiteral("mqtt.subscribe"),
                                 [this](const QJsonObject &params) {
        if (!context_->mqttManager) {
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("MQTT manager not available"));
        }

        qint32 channelId = 0;
        QString topic;
        qint32 qos = 0;
        QJsonObject parseErr;

        if (!parseChannelId(params, channelId, true, parseErr)) {
            return parseErr;
        }
        if (!rpc::RpcHelpers::getString(params, "topic", topic) || topic.isEmpty()) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing topic"));
        }
        if (!parseOptionalI32InRange(params, "qos", qos, kMinQos, kMaxQos,
                                     rpc::RpcError::BadParameterValue,
                                     QStringLiteral("invalid qos (0-2)"), parseErr)) {
            return parseErr;
        }

        if (!context_->mqttManager->hasChannel(channelId)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("channel not found"));
        }

        if (!context_->mqttManager->subscribe(channelId, topic, qos)) {
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("subscribe failed - channel may not be connected"));
        }

        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("channelId"), channelId},
            {QStringLiteral("topic"), topic},
            {kKeyMessage, QStringLiteral("订阅成功")}
        };
    });

    // 取消订阅MQTT主题
    dispatcher_->registerMethod(QStringLiteral("mqtt.unsubscribe"),
                                 [this](const QJsonObject &params) {
        if (!context_->mqttManager) {
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("MQTT manager not available"));
        }

        qint32 channelId = 0;
        QString topic;
        QJsonObject parseErr;

        if (!parseChannelId(params, channelId, true, parseErr)) {
            return parseErr;
        }
        if (!rpc::RpcHelpers::getString(params, "topic", topic) || topic.isEmpty()) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing topic"));
        }

        if (!context_->mqttManager->hasChannel(channelId)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("channel not found"));
        }

        if (!context_->mqttManager->unsubscribe(channelId, topic)) {
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("unsubscribe failed - channel may not be connected"));
        }

        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("channelId"), channelId},
            {QStringLiteral("topic"), topic},
            {kKeyMessage, QStringLiteral("取消订阅成功")}
        };
    });
}

// ===================== 系统资源监控RPC方法 =====================


}  // namespace core
}  // namespace fanzhou
