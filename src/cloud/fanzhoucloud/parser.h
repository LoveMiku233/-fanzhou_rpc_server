#ifndef STRATEGYPARSER_H
#define STRATEGYPARSER_H

#include <QString>
#include <QJsonObject>

namespace fanzhou {
namespace core {
struct AutoStrategy;
}
namespace cloud {
namespace fanzhoucloud {

bool parseAutoStrategyFromJson(const QJsonObject &obj, core::AutoStrategy &out, QString *error = nullptr);
bool parseNodeChannelKey(const QString &key, quint8 &node, qint8 &channel);

}
}
}

#endif // STRATEGYPARSER_H
