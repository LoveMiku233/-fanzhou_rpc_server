#ifndef FANZHOU_CLOUD_CLOUD_UPLOADER_H
#define FANZHOU_CLOUD_CLOUD_UPLOADER_H

#include <QObject>
#include <QHash>
#include <QDateTime>
#include <QJsonObject>

#include <memory>

#include "core/core_config.h"

namespace fanzhou {
namespace core {
class CoreContext;
}
namespace cloud {

class CloudUploader : public QObject {
    Q_OBJECT
public:
    explicit CloudUploader(core::CoreContext *ctx, QObject *parent = nullptr);

    void applyConfig(const core::CloudUploadConfig &cfg);

    void onDeviceStatusChanged(quint8 nodeId);
    void onChannelValueChanged(quint8 nodeId, quint8 channel);

private:
    struct NodeUploadState {
        QDateTime lastUpload;
        QJsonObject lastPayload;
    };

    core::CoreContext *ctx_;
    std::unique_ptr<core::CloudUploadConfig> cfg_;

    // idx map
    QHash<quint8, QList<QPair<int, QString>>> nodeToChannels_;

    QHash<quint8, NodeUploadState> nodeStates_;

    void tryUploadNode(quint8 nodeId, bool force = false);
    QJsonObject buildNodePayload(quint8 nodeId, const QString &formatId) const;
};

} // namespace cloud
} // namespace fanzhou

#endif // FANZHOU_CLOUD_CLOUD_UPLOADER_H
