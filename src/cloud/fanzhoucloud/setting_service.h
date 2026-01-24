#ifndef FANZHOU_CLOUD_FANZHOUCLOUD_SETTING_SERVICE_H
#define FANZHOU_CLOUD_FANZHOUCLOUD_SETTING_SERVICE_H

#include <QString>
#include <QJsonDocument>

namespace fanzhou {
namespace cloud {
namespace fanzhoucloud {

class SettingService
{
public:
    bool handleRequest(const QJsonObject &req,
                       QJsonObject &resp,
                       QString &error);

private:
    bool handleScene(const QString &method,
                     const QJsonObject &data,
                     QJsonObject &resp,
                     QString &error);

    bool handleTimer(const QString &method,
                     const QJsonObject &data,
                     QJsonObject &resp,
                     QString &error);
};

} // namespace fanzhoucloud
} // namespace cloud
} // namespace fanzhou

#endif // FANZHOU_CLOUD_FANZHOUCLOUD_SETTING_SERVICE_H
