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


void CloudMessageHandler::onMqttMessage(int channelId,
                                       const QString &topic,
                                       const QByteArray &payload)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Invalid JSON from MQTT topic=%1 err=%2 payload=%3")
                        .arg(topic)
                        .arg(err.errorString())
                        .arg(QString::fromUtf8(payload)));
        return;
    }

    const QJsonObject msg = doc.object();

    LOG_INFO(kLogSource,
             QStringLiteral("MQTT downlink received: channel=%1 topic=%2 payload=%3")
                 .arg(channelId)
                 .arg(topic)
                 .arg(QString::fromUtf8(payload)));

    bool hasControlKey = false;
    for (auto it = msg.begin(); it != msg.end(); ++it) {
        if (it.key().startsWith(QStringLiteral("node_"))) {
            hasControlKey = true;
            break;
        }
    }

    if (hasControlKey) {
        handleControlCommand(msg);
        return;
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
                 QStringLiteral("Handled %1 cloud control commands").arg(successCount));
    }
}



} // namespace cloud
} // namespace fanzhou
