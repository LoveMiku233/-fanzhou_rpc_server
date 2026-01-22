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
    // fix
     cfg_ = std::make_unique<core::CloudUploadConfig>(cfg);  // 深拷贝

    // clear map
    nodeToChannels_.clear();

    for (const auto &binding : cfg.channelBindings) {
        for (const auto &nodeBinding : binding.nodes) {
            nodeToChannels_[nodeBinding.nodeId].append(qMakePair(binding.channelId, nodeBinding.formatId));
        }
    }

    LOG_INFO(kLogSource,
             QStringLiteral("applyConfig: channelBindings.size = %1")
                 .arg(cfg.channelBindings.size()));

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
    Q_UNUSED(channel)

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

    if (!ctx_ || !ctx_->mqttManager) {
        LOG_DEBUG(kLogSource,
                  QStringLiteral("MQTT manager not initialized, skip upload"));
        return;
    }


    const auto it = nodeToChannels_.find(nodeId);
    if (it == nodeToChannels_.end()) {
        LOG_DEBUG(kLogSource,
                  QStringLiteral("Node %1 has no cloud bindings, skip upload")
                      .arg(nodeId));
        return;
    }


    auto &state = nodeStates_[nodeId];

    if (cfg_->uploadMode == QStringLiteral("change") &&
        !force &&
        state.lastUpload.isValid()) {

        const qint64 elapsed =
            state.lastUpload.secsTo(QDateTime::currentDateTimeUtc());

        if (elapsed < cfg_->minUploadIntervalSec) {
            LOG_DEBUG(kLogSource,
                      QStringLiteral("Skip upload node %1: min interval not reached (%2 s)")
                          .arg(nodeId)
                          .arg(elapsed));
            return;
        }
    }

    QHash<QString, QJsonObject> payloadCache;

    int sentCount = 0;
    for (const auto &pair : it.value()) {
        const int channelId = pair.first;
        const QString &formatIdForThisChannel = pair.second;

        if (!payloadCache.contains(formatIdForThisChannel)) {
            QJsonObject payload =
                buildNodePayload(nodeId, formatIdForThisChannel);

            if (payload.isEmpty()) {
                LOG_DEBUG(kLogSource,
                          QStringLiteral("Node %1 format %2 payload empty, skip")
                              .arg(nodeId)
                              .arg(formatIdForThisChannel));
                continue;
            }

            if (cfg_->uploadMode == QStringLiteral("change") &&
                !force &&
                !state.lastPayload.isEmpty() &&
                state.lastPayload == payload) {

                LOG_DEBUG(kLogSource,
                          QStringLiteral("Skip upload node %1: payload unchanged")
                              .arg(nodeId));
                return;
            }

            payloadCache.insert(formatIdForThisChannel, payload);
        }

        const QJsonObject &payload = payloadCache[formatIdForThisChannel];

        const QByteArray data =
            QJsonDocument(payload).toJson(QJsonDocument::Compact);

        const bool ok = ctx_->mqttManager->publishStatus(
            channelId,
            data,
            0
        );

        if (ok) {
            sentCount++;
            LOG_DEBUG(kLogSource,
                      QStringLiteral("Node %1 uploaded to MQTT channel %2 (format=%3)")
                          .arg(nodeId)
                          .arg(channelId)
                          .arg(formatIdForThisChannel));
        } else {
            LOG_DEBUG(kLogSource,
                      QStringLiteral("Node %1 failed to upload to MQTT channel %2")
                          .arg(nodeId)
                          .arg(channelId));
        }
    }
    if (sentCount == 0) {
        LOG_DEBUG(kLogSource,
                  QStringLiteral("Node %1 upload finished: no channel succeeded")
                      .arg(nodeId));
        return;
    }

    // 更新状态
    state.lastUpload = QDateTime::currentDateTimeUtc();

    // 缓存
    if (!payloadCache.isEmpty()) {
        state.lastPayload = payloadCache.begin().value();
    }

    LOG_INFO(kLogSource,
             QStringLiteral("Node %1 uploaded to %2 MQTT channels, formats=%3")
                 .arg(nodeId)
                 .arg(sentCount)
                 .arg(payloadCache.size()));
}


QJsonObject CloudUploader::buildNodePayload(quint8 nodeId,
                                            const QString &formatId) const
{
    Q_UNUSED(formatId)

    QJsonObject root;
//    root["last_updata_nodeId"] = static_cast<int>(nodeId);
//    root["last_updata_timestamp"] =
//        QDateTime::currentDateTimeUtc().toSecsSinceEpoch();

    if (!ctx_ || !cfg_) {
        return root;
    }

    auto *dev = ctx_->relays.value(nodeId, nullptr);
    if (!dev) {
        root["error"] = "unknown node";
        return root;
    }

    for (int ch = 0; ch < 4; ++ch) {
        const auto status = dev->lastStatus(ch);
        const int chIndex = ch + 1;
        const QString prefix =
            QStringLiteral("node_%1_").arg(nodeId);

        // 开关
        if (cfg_->uploadChannelStatus) {
            const int mode =
                static_cast<int>(
                    device::RelayProtocol::modeBits(status.statusByte));

            root[prefix + QStringLiteral("sw%1").arg(chIndex)] = mode;

            // 缺相
            root[prefix + QStringLiteral("phaseLost%1").arg(chIndex)] =
                device::RelayProtocol::phaseLost(status.statusByte);
        }

        // 电流
        if (cfg_->uploadCurrent) {
            root[prefix + QStringLiteral("current%1").arg(chIndex)] =
                static_cast<double>(status.currentA);
        }
    }

    return root;
}




} // namespace cloud
} // namespace fanzhou
