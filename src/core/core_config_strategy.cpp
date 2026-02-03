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

        // 必填字段
        obj[QStringLiteral("sceneId")] = strat.strategyId;
        obj[QStringLiteral("sceneName")] = strat.strategyName;
        obj[QStringLiteral("sceneType")] = strat.strategyType;

        // 其他字段
        obj[QStringLiteral("matchType")] = static_cast<int>(strat.matchType);
        obj[QStringLiteral("version")] = strat.version;
        obj[QStringLiteral("updateTime")] = strat.updateTime;
        obj[QStringLiteral("cloudChannelId")] = strat.cloudChannelId;

        // status: 0正常, 1关闭
        obj[QStringLiteral("status")] = strat.enabled ? 0 : 1;

        // 时间窗口
        obj[QStringLiteral("effectiveBeginTime")] = strat.effectiveBeginTime;
        obj[QStringLiteral("effectiveEndTime")] = strat.effectiveEndTime;

        // actions
        QJsonArray actArr;
        for (const auto &act : strat.actions) {
            QJsonObject actObj;
            actObj[QStringLiteral("identifier")] = act.identifier;
            actObj[QStringLiteral("identifierValue")] = act.identifierValue;
            if (!act.action_dev.isEmpty()) {
                actObj[QStringLiteral("deviceCode")] = act.action_dev;
            }
            actArr.append(actObj);
        }
        obj[QStringLiteral("actions")] = actArr;

        // conditions
        QJsonArray condArr;
        for (const auto &cond : strat.conditions) {
            QJsonObject condObj;
            condObj[QStringLiteral("identifier")] = cond.identifier;
            condObj[QStringLiteral("identifierValue")] = cond.identifierValue;
            condObj[QStringLiteral("op")] = cond.op;
            if (!cond.sensor_dev.isEmpty()) {
                condObj[QStringLiteral("deviceCode")] = cond.sensor_dev;
            }
            condArr.append(condObj);
        }
        obj[QStringLiteral("conditions")] = condArr;

        stratArr.append(obj);
    }
    root[QStringLiteral("strategies")] = stratArr;
}

}
}
