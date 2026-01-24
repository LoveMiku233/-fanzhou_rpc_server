#include "message_handler.h"
#include "core/core_context.h"
#include "utils/logger.h"
#include "device/can/relay_gd427.h"
#include "device/can/relay_protocol.h"
#include "cloud/mqtt/mqtt_channel_manager.h"
#include "cloud/fanzhoucloud/setting_service.h"

#include <QJsonDocument>

namespace fanzhou {
namespace cloud {
namespace fanzhoucloud {
namespace  {
const char *const kLogSource = "CloudMessageHandler";
}



CloudMessageHandler::CloudMessageHandler(core::CoreContext *ctx, QObject *parent)
    : QObject(parent),
      ctx_(ctx)
{
    Q_ASSERT(ctx_);
}


void CloudMessageHandler::onMqttMessage(int channelId,
                                        const QString &topic,
                                        const QByteArray &payload)
{
    if (!ctx_ || !ctx_->mqttManager) {
        return;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Invalid JSON payload on topic '%1'").arg(topic));
        return;
    }

    const QJsonObject obj = doc.object();

    const QString controlTopic =
        ctx_->mqttManager->getControlTopicFromConfig(channelId);

    const QString settingTopic =
            ctx_->mqttManager->getSettingSubTopicFromConfig(channelId);

    // 分发处理
    if (!controlTopic.isEmpty() && topic == controlTopic) {
        handleControlCommand(channelId, obj);
        return;
    }

    if (!settingTopic.isEmpty() && topic == settingTopic) {
        handleSettingCommand(channelId, obj);
        return;
    }

    LOG_DEBUG(kLogSource,
              QStringLiteral("Unhandled MQTT topic: channel=%1 topic='%2'")
                  .arg(channelId)
                  .arg(topic));
}


void CloudMessageHandler::handleStrategyCommand(const int channelId, const QJsonObject &msg)
{
    LOG_DEBUG(kLogSource,QStringLiteral("Handled the %1 cloud Strategy commands").arg(channelId));
}

void CloudMessageHandler::handleControlCommand(const int channelId, const QJsonObject &msg)
{
    if (!ctx_) {
        LOG_WARNING(kLogSource, "CoreContext is null, cannot handle control command");
        return;
    }

    int successCount = 0;

    // 遍历所有 key-value
    for (auto it = msg.begin(); it != msg.end(); ++it) {
        const QString key = it.key();
        if (!key.startsWith(QStringLiteral("node_"))) {
            continue;
        }

        // 期望格式: node_<nodeId>_sw<ch>
        const QStringList parts = key.split('_');
        if (parts.size() != 3) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("invalid control key format: %1").arg(key));
            continue;
        }

        bool ok1 = false;
        bool ok2 = false;

        const int nodeId = parts[1].toInt(&ok1);  // node_<id>
        const QString swPart = parts[2];          // sw1

        if (!ok1 || !swPart.startsWith(QStringLiteral("sw"))) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("invalid control key format: %1").arg(key));
            continue;
        }

        const int chIndex = swPart.mid(2).toInt(&ok2); // sw1 -> 1
        if (!ok2 || chIndex <= 0) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("invalid channel index in key: %1").arg(key));
            continue;
        }


        // fix index
        const int channel = chIndex - 1;

        const int mode = it.value().toInt(-1);
        if (mode < 0) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("invalid mode value for key=%1").arg(key));
            continue;
        }

        // 查找设备
        auto *dev = ctx_->relays.value(static_cast<quint8>(nodeId), nullptr);
        if (!dev) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("Control command for unknown nodeId=%1").arg(nodeId));
            continue;
        }

        LOG_INFO(kLogSource,
                 QStringLiteral("Cloud control: nodeId=%1 ch=%2 mode=%3")
                     .arg(nodeId)
                     .arg(channel)
                     .arg(mode));

        // 执行控制
        dev->control(channel,
                     static_cast<device::RelayProtocol::Action>(mode));

        successCount++;
    }

    if (successCount == 0) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("No valid control command found in payload: %1")
                        .arg(QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact))));
    } else {
        LOG_INFO(kLogSource,
                 QStringLiteral("Handled the %1 cloud %2 control commands").arg(channelId).arg(successCount));
    }
}


void CloudMessageHandler::handleSettingCommand(const int channelId, const QJsonObject &msg)
{
    LOG_DEBUG(kLogSource,QStringLiteral("Handled the %1 cloud Setting commands").arg(channelId));

    const QString method = msg.value("method").toString();
    const QString type = msg.value("type").toString();
    const QJsonObject data = msg.value("data").toObject();
    const QString requestId = msg.value("requestId").toString();
    const qint64 timestamp = msg.value("timestamp").toVariant().toLongLong();

    if (method.isEmpty() || type.isEmpty() || requestId.isEmpty()) {
        LOG_WARNING(kLogSource, "invalid setting message: missing fields");
        return;
    }

    LOG_INFO(kLogSource,
             QStringLiteral("Setting request: method=%1 type=%2 requestId=%3")
             .arg(method)
             .arg(type)
             .arg(requestId));

    // resp
    QJsonObject resp;
    QString error;

    bool ok = ctx_->cloudSettingService->handleRequest(data, resp, error);

    if (ok || !error.isEmpty()) {
        resp["method"] = QStringLiteral("%1_response").arg(method);
        resp["type"] = type;
        resp["requestId"] = requestId;
        resp["timestamp"] = static_cast<qint64>(timestamp);

        ctx_->mqttManager->publishSetting(
                       channelId,
                       QJsonDocument(resp).toJson(QJsonDocument::Compact),
                       0);
    }
}




}
} // namespace cloud
} // namespace fanzhou
