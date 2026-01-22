#ifndef CLOUD_PLATFORM_H
#define CLOUD_PLATFORM_H

#include <QJsonObject>
#include <QString>
#include <cloud/cloud_types.h>


namespace fanzhou {
namespace cloud {

struct CloudSubscription {
    QString topicFilter;
    int qos = 0;
};

struct CloudDownlinkMethod {
    QString method;
    QString description;
};

class CloudPlatform
{
public:
    virtual ~CloudPlatform() = default;

    virtual CloudTypeId typeId() const = 0;


    virtual bool buildPropertyReport(quint8 deviceNode,
                                     const QJsonObject &properties,
                                     QString &outTopic,
                                     QByteArray &outPayload,
                                     int &outqos) = 0;

    virtual bool buildEventReport(quint8 deviceNode,
                                  const QString &eventId,
                                  const QJsonObject &params,
                                  QString &outTopic,
                                  QByteArray &outPayload,
                                  int &outqos) {
        Q_UNUSED(deviceNode)
        Q_UNUSED(eventId)
        Q_UNUSED(params)
        Q_UNUSED(outTopic)
        Q_UNUSED(outPayload)
        Q_UNUSED(outqos)
        return false;
    }

    // Subscription list
    virtual QList<CloudSubscription> subscriptions() const = 0;

    //
    virtual bool parseDownlink(const QString &topic,
                                   const QByteArray &payload,
                                   QString &outMethod,
                                   QJsonObject &outParams) = 0;

    virtual bool mapPolicyToLocalAction(const QJsonObject &cloudPolicy,
                                        QString &outMethod,
                                        QJsonObject &outParams) = 0;

};


}
}



#endif // CLOUD_PLATFORM_H
