#include "cloud_uploader.h"
#include "core/core_context.h"
#include "device/can/relay_gd427.h"
#include "cloud/mqtt/mqtt_channel_manager.h"
#include "utils/logger.h"
#include <QJsonDocument>
#include <QJsonArray>

namespace fanzhou {
namespace cloud {
namespace {
const char *const kLogSource = "CloudUploader";
}

CloudUploader::CloudUploader(core::CoreContext *ctx, QObject *parent)
    : QObject(parent),
      ctx_(ctx)
{
    Q_ASSERT(ctx_);
}

void CloudUploader::applyConfig(const core::CloudUploadConfig &cfg)
{
    cfg_ = const_cast<core::CloudUploadConfig *>(&cfg);

    LOG_INFO(kLogSource,
             QStringLiteral("CloudUploadConfig applied: enabled=%1, mode=%2, interval=%3s")
                 .arg(cfg_->enabled)
                 .arg(cfg_->uploadMode)
                 .arg(cfg_->intervalSec));
}


void CloudUploader::onDeviceStatusChanged(quint8 nodeId)
{
    if (!cfg_ || !cfg_->enabled) {
        return;
    }

    tryUploadNode(nodeId, cfg_->uploadMode != QStringLiteral("change"));
}

void CloudUploader::onChannelValueChanged(quint8 nodeId, quint8 channel)
{
    if (!cfg_ || !cfg_->enabled) {
        return;
    }

    tryUploadNode(nodeId, cfg_->uploadMode != QStringLiteral("change"));
}

void CloudUploader::tryUploadNode(quint8 nodeId, bool force)
{
    if (!cfg_ || !cfg_->enabled) {
        return;
    }

    auto &state = nodeStates_[nodeId];

    const QString formatId = QStringLiteral("fanzhou");
    QJsonObject payload = buildNodePayload(nodeId, formatId);

    if (payload.isEmpty()) {
        LOG_DEBUG(kLogSource,
                 QStringLiteral("Skip upload node %1: empty payload").arg(nodeId));
       return;
    }

    if (cfg_->uploadMode == QStringLiteral("change") &&
            !force &&
            !state.lastPayload.isEmpty() &&
            state.lastPayload == payload) {
        LOG_DEBUG(kLogSource,
                      QStringLiteral("Skip upload node %1: payload unchanged").arg(nodeId));
        return;
    }

    if (!ctx_->mqttManager) {
            LOG_DEBUG(kLogSource,
                     QStringLiteral("MQTT manager not initialized, skip upload"));
            return;
    }

    // @TODO TEST
    const QString topic = "$thing/down/property/LD1_Test";

    const QByteArray data =
         QJsonDocument(payload).toJson(QJsonDocument::Compact);
     ctx_->mqttManager->publishToAll(topic, "up test data", 1);
     const int sent = ctx_->mqttManager->publishToAll(topic, data, 1);
     LOG_INFO("CloudUploader",
              QString("publish sent=%1 topic=%2 payload=%3")
                  .arg(sent)
                  .arg(topic)
                  .arg(QString::fromUtf8(data)));
}

QJsonObject CloudUploader::buildNodePayload(quint8 nodeId,
                                            const QString &formatId) const
{
    Q_UNUSED(formatId)

    QJsonObject root;
    root["nodeId"] = nodeId;
    root["timestamp"] =
        QDateTime::currentDateTimeUtc().toSecsSinceEpoch();

    if (!ctx_ || !cfg_) {
        return root;
    }

    auto *dev = ctx_->relays.value(nodeId, nullptr);
    if (!dev) {
        root["error"] = "unknown node";
        return root;
    }

    QJsonArray channels;

    for (int ch = 0; ch < 4; ++ch) {
        QJsonObject chObj;
        chObj["channel"] = ch;

        const auto status = dev->lastStatus(ch);

        if (cfg_->uploadChannelStatus) {
            chObj["statusByte"] =
                static_cast<int>(status.statusByte);

            chObj["mode"] =
                static_cast<int>(
                    device::RelayProtocol::modeBits(status.statusByte));

            chObj["phaseLost"] =
                device::RelayProtocol::phaseLost(status.statusByte);
        }

        if (cfg_->uploadCurrent) {
            chObj["currentA"] =
                static_cast<double>(status.currentA);
        }

        channels.append(chObj);
    }

    root["channels"] = channels;

    return root;
}



} // namespace cloud
} // namespace fanzhou
