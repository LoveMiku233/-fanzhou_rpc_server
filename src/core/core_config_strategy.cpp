#include "core_config.h"
#include "cloud/fanzhoucloud/parser.h"


namespace fanzhou{
namespace core {

bool CoreConfig::loadStrategies(const QJsonObject &root)
{
    strategies.clear();

    if (!root.contains(QStringLiteral("strategies")) ||
        !root[QStringLiteral("strategies")].isArray()) {
        return false;
    }

    const QJsonArray arr = root[QStringLiteral("strategies")].toArray();

    for (const auto &v : arr) {
        if (!v.isObject())
            continue;

        const QJsonObject obj = v.toObject();

        core::AutoStrategy s;
        QString err;
        if (!cloud::fanzhoucloud::parseAutoStrategyFromJson(obj, s, &err)) {
            continue;   // 跳过坏策略，不影响其他
        }
        strategies.append(s);
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
