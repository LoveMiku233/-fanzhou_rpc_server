#ifndef FANZHOU_CLOUD_CLOUD_MESSAGE_HANDLER_H
#define FANZHOU_CLOUD_CLOUD_MESSAGE_HANDLER_H

#include <QObject>

namespace fanzhou {
namespace core {
class CoreContext;
struct AutoStrategy;
}
namespace cloud {
namespace fanzhoucloud {

enum class CloudMethod {
    Unknown,
    Get,
    GetResponse,
    Set,
    SetResponse,
    Delete,
    DeleteResponse,
};

class CloudMessageHandler : public QObject
{
    Q_OBJECT
public:
    explicit CloudMessageHandler(core::CoreContext *ctx, QObject *parent);

    void setChannelId(int channel);
    int getChannelId();

    /**
     * @brief Send strategy changes to cloud
     * @param strategy The strategy to sync
     * @param msg Message containing method (set/delete)
     * @return true if sent successfully
     */
    bool sendStrategyCommand(const core::AutoStrategy &strategy, const QJsonObject &msg);
    bool sendDeleteCommand(const int channelId, const QJsonObject &msg);

public slots:
    void onMqttMessage(int channelId, const QString &topic, const QByteArray &payload);

private:
    int channelId_ = -1;
    core::CoreContext *ctx_;
    //
    QList<core::AutoStrategy> parseStrategyByType(
        const QString &type,
        const QJsonObject &data,
        QString *error);
    bool applyStrategies(
        const QList<core::AutoStrategy> &list,
        int &lastId,
        int &lastVersion,
        QString &errMsg);


    bool handleStrategyCommand(const int channelId, const QJsonObject &msg);

    bool handleControlCommand(const int channelId, const QJsonObject &msg);
    bool handleSettingCommand(const int channelId, const QJsonObject &msg);
    bool sendStrategyReply(const QString &method,
                           const QString &type,
                           bool ok,
                           int objectId,
                           int version,
                           const QString &requestId,
                           int errCode,
                           const QString &errMsg);
};
}
} // namespace cloud
} // namespace fanzhou

#endif // FANZHOU_CLOUD_CLOUD_MESSAGE_HANDLER_H
