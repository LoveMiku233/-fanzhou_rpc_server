#include "core_config.h"


namespace fanzhou{
namespace core {

bool CoreConfig::loadStrategies(const QJsonObject &root)
{
    // 控制策略
    strategies.clear();
    if (root.contains(QStringLiteral("strategies")) &&
        root[QStringLiteral("strategies")].isArray()) {
        // @TODO
        const QJsonArray arr = root[QStringLiteral("strategies")].toArray();
        const QDate today = QDate::currentDate();

        for (const auto &v : arr) {

        }


    } else {
        return false;
    }
    return true;
}

void CoreConfig::saveStrategies(QJsonObject &root) const
{
    // 控制策略
    QJsonArray stratArr;
    for (const auto &strat : strategies) {
        QJsonObject obj;
        // @TODO
        stratArr.append(obj);
    }
    root[QStringLiteral("strategies")] = stratArr;
}

}
}
