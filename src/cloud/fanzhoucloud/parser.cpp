#include "parser.h"
#include "types/strategy_type.h"

#include <QJsonDocument>
#include <QJsonArray>

namespace fanzhou {
namespace cloud {
namespace fanzhoucloud {


bool parseAutoStrategyFromJson(const QJsonObject &obj, core::AutoStrategy &s, QString *error)
{
    auto fail = [&](const QString &msg) {
        if (error) *error = msg;
        return false;
    };

    if (!obj.contains("id"))            return fail("missing id");
    if (!obj.contains("sceneName"))     return fail("missing sceneName");
    if (!obj.contains("sceneType"))     return fail("missing sceneType");

    // 必填字段
    s.strategyId   = obj.value("id").toInt();
    s.strategyName = obj.value("sceneName").toString();
    s.strategyType = obj.value("sceneType").toString(); // auto / manual
    s.matchType    = static_cast<qint8>(obj.value("matchType").toInt(0));
    s.version      = obj.value("version").toInt(1);
    s.updateTime   = obj.value("updateTime").toString();

    // status: 0正常, 1关闭
    int status = obj.value("status").toInt(0);
    s.enabled = (status == 0);

    // 时间窗口
    s.effectiveBeginTime = obj.value("effectiveBeginTime").toString(); // "06:00"
    s.effectiveEndTime   = obj.value("effectiveEndTime").toString();   // "18:00"

    // 简单校验
    if (!s.effectiveBeginTime.isEmpty()) {
        if (!QTime::fromString(s.effectiveBeginTime, "HH:mm").isValid())
            return fail("invalid effectiveBeginTime format");
    }
    if (!s.effectiveEndTime.isEmpty()) {
        if (!QTime::fromString(s.effectiveEndTime, "HH:mm").isValid())
            return fail("invalid effectiveEndTime format");
    }

    // actions
    s.actions.clear();
    if (obj.contains("actions") && obj["actions"].isArray()) {
        const QJsonArray actArr = obj["actions"].toArray();

        for (const auto &av : actArr) {
            if (!av.isObject())
                continue;

            const QJsonObject aobj = av.toObject();

            // 必填字段校验
            if (!aobj.contains("identifier"))
                return fail("action missing identifier");
            if (!aobj.contains("identifierValue"))
                return fail("action missing identifierValue");

            const QString identifier = aobj.value("identifier").toString();
            const int value = aobj.value("identifierValue").toInt();

            quint8 node = 0;
            qint8 channel = -1;

            // 解析 "node_1_sw1"
            if (!parseNodeChannelKey(identifier, node, channel)) {
                return fail(QStringLiteral("invalid action identifier format: %1").arg(identifier));
            }

            core::StrategyAction a;
            a.identifier = identifier;
            a.node = node;
            a.channel = channel;
            a.identifierValue = value;

            // 保存 deviceCode
            if (aobj.contains("deviceCode"))
                a.action_dev = aobj.value("deviceCode").toString();

            s.actions.append(a);
        }
    }

    // conditions(manual 场景可为空)
    s.conditions.clear();
    if (obj.contains("conditions") && obj["conditions"].isArray()) {
        const QJsonArray condArr = obj["conditions"].toArray();
        for (const auto &cv : condArr) {
            if (!cv.isObject()) continue;
            const QJsonObject cobj = cv.toObject();

            core::StrategyCondition c;

            c.op = cobj.value("op").toString();  // gt lt eq egt elt
            c.identifierValue = cobj.value("identifierValue").toDouble();
            c.sensor_dev = cobj.value("deviceCode").toString();
            c.identifier = cobj.value("identifier").toString();

            s.conditions.append(c);
        }
    }

    // 运行时字段初始化
    s.lastTriggered = QDateTime();

    return true;
}


bool parseNodeChannelKey(const QString &key, quint8 &node, qint8 &channel)
{
    // 期望格式: node_<nodeId>_sw<ch>
    if (!key.startsWith(QStringLiteral("node_")))
        return false;

    const QStringList parts = key.split('_');
    if (parts.size() != 3)
        return false;

    bool ok1 = false;
    bool ok2 = false;

    const int nodeId = parts[1].toInt(&ok1);   // node_<id>
    const QString swPart = parts[2];           // sw1

    if (!ok1 || !swPart.startsWith(QStringLiteral("sw")))
        return false;

    const int chIndex = swPart.mid(2).toInt(&ok2); // sw1 -> 1
    if (!ok2 || chIndex <= 0)
        return false;

    node = static_cast<quint8>(nodeId);
    channel = static_cast<qint8>(chIndex - 1); // 转成 0-based

    return true;

}

}
}
}
