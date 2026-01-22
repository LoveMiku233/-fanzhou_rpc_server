#ifndef FANZHOU_CLOUD_CLOUD_MESSAGE_HANDLER_H
#define FANZHOU_CLOUD_CLOUD_MESSAGE_HANDLER_H

#include <QObject>

namespace fanzhou {
namespace core {
class CoreContext;
}
namespace cloud {

class CloudMessageHandler : public QObject
{
    Q_OBJECT
public:
    explicit CloudMessageHandler(core::CoreContext *ctx, QObject *parent);


public slots:
    void onMqttMessage(int channelId, const QString &topic, const QByteArray &payload);

private:
    int channelId = -1;
    core::CoreContext *ctx_;
    void handleStrategyCommand(const int channelId, const QJsonObject &msg);
    void handleControlCommand(const int channelId, const QJsonObject &msg);
    void handleSettingCommand(const int channelId, const QJsonObject &msg);
    // TODO
};

} // namespace cloud
} // namespace fanzhou

#endif // FANZHOU_CLOUD_CLOUD_MESSAGE_HANDLER_H
