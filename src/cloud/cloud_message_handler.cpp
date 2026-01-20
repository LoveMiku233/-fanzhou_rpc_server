#include "cloud_message_handler.h"
#include "core/core_context.h"
#include "utils/logger.h"
#include "device/can/relay_gd427.h"
#include "device/can/relay_protocol.h"

#include <QJsonDocument>

namespace fanzhou {
namespace cloud {

namespace  {
const char *const kLogSource = "CloudMessageHandler";
}



CloudMessageHandler::CloudMessageHandler(core::CoreContext *ctx, QObject *parent)
    : QObject(parent),
      ctx_(ctx)
{
    Q_ASSERT(ctx_);
}


void CloudMessageHandler::onMqttMessage(int channelId, const QString &topic, const QByteArray &payload)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("invalid JSON From MQTT topic=%1 err=%2 payload=%3")
                    .arg(topic)
                    .arg(err.errorString())
                    .arg(QString::fromUtf8(payload)));
        return;
    }

    const QJsonObject msg = doc.object();

    const QString type = msg.value(QStringLiteral("type")).toString();
    if (type.isEmpty()) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("MQTT message missing 'type': topic=%1 payload=%2")
                    .arg(topic)
                    .arg(QString::fromUtf8(payload)));
        return;
    }

    LOG_INFO(kLogSource,
             QStringLiteral("MQTT downlink received: type=%1 topic=%2")
             .arg(type)
             .arg(topic));

    if (type == QStringLiteral("strategy")) {
        handleStrategyCommand(msg);
    } else if (type == QStringLiteral("control")) {
        handleControlCommand(msg);
    } else {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Unknown MQTT command type=%1").arg(type));
    }

}


void CloudMessageHandler::handleStrategyCommand(const QJsonObject &msg)
{
    LOG_DEBUG(kLogSource, QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void CloudMessageHandler::handleControlCommand(const QJsonObject &msg)
{
    if (!ctx_) {
        LOG_WARNING(kLogSource, "CoreContext is null, cannot handle control command");
        return;
    }


    const int nodeId = msg.value(QStringLiteral("nodeId")).toInt(-1);
    const int channel = msg.value(QStringLiteral("channel")).toInt(-1);
    const int mode = msg.value(QStringLiteral("mode")).toInt(-1);


    if (nodeId < 0 || channel < 0 || mode < 0) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("invalid control command: %1")
                    .arg(QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact))));
        return;
    }

    auto *dev = ctx_->relays.value(static_cast<quint8>(nodeId), nullptr);
    if (!dev) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Control command for unknown nodeId=%1").arg(nodeId));
        return;
    }

    LOG_INFO(kLogSource,
             QStringLiteral("Control nodeId=%1 ch=%2 mode=%3")
             .arg(nodeId)
             .arg(channel)
             .arg(mode));

    dev->control(channel, static_cast<device::RelayProtocol::Action>(mode));
}





} // namespace cloud
} // namespace fanzhou
