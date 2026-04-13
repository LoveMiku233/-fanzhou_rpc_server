#include "setting_service.h"

namespace fanzhou {
namespace cloud {
namespace fanzhoucloud {

namespace {
bool notImplemented(QString &error, const QString &message)
{
    error = message;
    return false;
}
}  // namespace

// return true: handled and should respond; false: not handled / no response
bool SettingService::handleRequest(const QJsonObject &req, QJsonObject &resp, QString &error)
{
    Q_UNUSED(req);
    Q_UNUSED(resp);

    return notImplemented(error, QStringLiteral("setting service not implemented"));
}


bool SettingService::handleScene(const QString &method,
                 const QJsonObject &data,
                 QJsonObject &resp,
                 QString &error)
{
    Q_UNUSED(method);
    Q_UNUSED(data);
    Q_UNUSED(resp);
    return notImplemented(error, QStringLiteral("scene setting handler not implemented"));
}

bool SettingService::handleTimer(const QString &method,
                 const QJsonObject &data,
                 QJsonObject &resp,
                 QString &error)
{
    Q_UNUSED(method);
    Q_UNUSED(data);
    Q_UNUSED(resp);
    return notImplemented(error, QStringLiteral("timer setting handler not implemented"));
}


} // namespace fanzhoucloud
} // namespace cloud
} // namespace fanzhou
