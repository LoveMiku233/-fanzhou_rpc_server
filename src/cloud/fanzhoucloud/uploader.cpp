#include "uploader.h"
#include "core/core_context.h"
#include "device/can/relay_gd427.h"
#include "cloud/mqtt/mqtt_channel_manager.h"
#include "utils/logger.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <cmath>

namespace fanzhou {
namespace cloud {
namespace fanzhoucloud {
namespace {
const char *const kLogSource = "CloudUploader";

// Tolerance for current comparison (in A, 0.01A = 10mA)
constexpr double kCurrentTolerance = 0.01;

// Key prefix for identifying current fields in payload
const QString kCurrentKeyPrefix = QStringLiteral("current");
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

bool CloudUploader::payloadsEqual(const QJsonObject &a, const QJsonObject &b, double currentTolerance) const
{
    // Fast path: check if key counts are the same first
    if (a.size() != b.size()) {
        return false;
    }
    
    for (const QString &key : a.keys()) {
        // Check if b contains this key
        if (!b.contains(key)) {
            return false;
        }
        
        const QJsonValue &valA = a[key];
        const QJsonValue &valB = b[key];
        
        // If types are different, not equal
        if (valA.type() != valB.type()) {
            return false;
        }
        
        // For current fields (keys starting with "current" followed by a digit), use tolerance comparison
        // Format: "node_X_currentY" where X is node id and Y is channel index (1-4)
        if (key.contains(kCurrentKeyPrefix) && valA.isDouble() && valB.isDouble()) {
            // Verify it's actually a current field (ends with digit after "current")
            int currentPos = key.indexOf(kCurrentKeyPrefix);
            if (currentPos >= 0) {
                int afterCurrentPos = currentPos + kCurrentKeyPrefix.length();
                if (afterCurrentPos < key.length() && key[afterCurrentPos].isDigit()) {
                    double diff = std::fabs(valA.toDouble() - valB.toDouble());
                    if (diff > currentTolerance) {
                        return false;
                    }
                    continue;  // Skip the default comparison below
                }
            }
        }
        
        if (valA.isDouble() && valB.isDouble()) {
            // For other double values, use exact comparison
            if (valA.toDouble() != valB.toDouble()) {
                return false;
            }
        } else if (valA != valB) {
            // For non-double values, use exact comparison
            return false;
        }
    }
    
    return true;
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

            // Use tolerance-based comparison for change mode
            if (cfg_->uploadMode == QStringLiteral("change") &&
                !force &&
                !state.lastPayload.isEmpty() &&
                payloadsEqual(state.lastPayload, payload, kCurrentTolerance)) {

                LOG_DEBUG(kLogSource,
                          QStringLiteral("Skip upload node %1: payload unchanged (within tolerance)")
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

        // 电流 - 使用四舍五入到小数点后两位，减少因浮点精度导致的重复上传
        if (cfg_->uploadCurrent) {
            double currentA = static_cast<double>(status.currentA);
            // Round to 2 decimal places to avoid floating-point precision issues
            currentA = std::round(currentA * 100.0) / 100.0;
            root[prefix + QStringLiteral("current%1").arg(chIndex)] = currentA;
        }
    }

    return root;
}



}
} // namespace cloud
} // namespace fanzhou
