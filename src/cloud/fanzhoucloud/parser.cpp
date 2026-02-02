#include "parser.h"
#include "types/strategy_type.h"
#include "utils/logger.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <cmath>
#include <limits>

namespace fanzhou {
namespace cloud {
namespace fanzhoucloud {
namespace  {
const char *const kLogSource = "FanzhouParser";
}


bool parseAutoStrategyFromJson(const QJsonObject &obj, core::AutoStrategy &s, QString *error)
{
    auto fail = [&](const QString &msg) {
        if (error) *error = msg;
        return false;
    };

    if (!obj.contains("sceneId"))            return fail("missing sceneId");
    if (!obj.contains("sceneName"))     return fail("missing sceneName");
    if (!obj.contains("sceneType"))     return fail("missing sceneType");

    // 必填字段
    s.strategyId   = obj.value("sceneId").toInt();
    if (s.strategyId <= 0)
        return fail("invalid id");
    s.strategyName = obj.value("sceneName").toString();
    s.strategyType = obj.value("sceneType").toString(); // auto / manual
    s.matchType    = static_cast<qint8>(obj.value("matchType").toInt(0));
    s.version      = obj.value("version").toInt(1);
    s.updateTime   = obj.value("updateTime").toString();
    s.cloudChannelId = obj.value("cloudChannelId").toInt(0);

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

            int node = 0;
            int channel = -1;

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


bool parseNodeChannelKey(const QString &key, int &node, int &channel)
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


QList<core::AutoStrategy> parseSceneDataFromJson(const QJsonObject &obj)
{
    // 基本参数
    const int code = obj.value("code").toInt(999);
    const QString message = obj.value("message").toString();
    const QJsonArray data_arr = obj.value("result").toArray();

    QList<core::AutoStrategy> st;

    if (code > 0 || data_arr.isEmpty()) {
        LOG_WARNING(kLogSource, QStringLiteral("code:%1, message:%2").arg(code).arg(message));
        return st;
    }

    //
    for (auto s : data_arr) {
        if (s.isObject()) {
            QJsonObject jobj = s.toObject();
            core::AutoStrategy tmp;
            // 处理
            if (parseAutoStrategyFromJson(jobj, tmp)) {
                st.append(tmp);
            }
        }
    }
    LOG_WARNING(kLogSource, QStringLiteral("parse cnt: %1").arg(st.size()));
    return st;
}


bool parseSceneActions(const QJsonArray &arr,
                       QList<core::StrategyAction> &out,
                       QString *error)
{
    out.clear();

    auto fail = [&](const QString &msg) {
        if (error) *error = msg;
        return false;
    };

    for (const auto &v : arr) {
        if (!v.isObject())
            continue;

        const QJsonObject obj = v.toObject();

        if (!obj.contains("identifier") || !obj.contains("identifierValue"))
            return fail("action missing identifier or identifierValue");

        const QString identifier = obj.value("identifier").toString();
        const int value = obj.value("identifierValue").toInt();

        int node = 0;
        int channel = -1;
        if (!parseNodeChannelKey(identifier, node, channel))
            return fail(QString("invalid identifier: %1").arg(identifier));

        core::StrategyAction a;
        a.identifier = identifier;
        a.node = node;
        a.channel = channel;
        a.identifierValue = value;

        if (obj.contains("deviceCode"))
            a.action_dev = obj.value("deviceCode").toString();

        out.append(a);
    }

    return true;
}

bool parseSetCommand(const QString &type,
                     const QJsonObject &data,
                     core::AutoStrategy &out,
                     QString *error)
{
    if (type == "scene") {
        return parseSceneSetData(data, out, error);
    }
    if (type == "timer") {
        return parseTimerSetData(data, out, error);
    }

    if (error)
        *error = QString("unsupported set type: %1").arg(type);
    return false;
}

QList<core::AutoStrategy> parseSyncData(
    const QString &type,
    const QJsonObject &obj,
    QString *error)
{
    if (type == "scene") {
        return parseSceneSyncData(obj);
    }

    if (type == "timer") {
        if (error) *error = "timer strategy not supported";
        return {};
    }

    if (error) *error = "unknown strategy type";
    return {};
}

bool parseSceneSetData(const QJsonObject &obj,
                       core::AutoStrategy &out,
                       QString *error)
{
    if (!parseAutoStrategyFromJson(obj, out, error))
        return false;

    out.type = "scene";
    return true;
}


QList<core::AutoStrategy> parseSceneSyncData(const QJsonObject &obj)
{
    QList<core::AutoStrategy> list;

    const int code = obj.value("code").toInt(-1);
    if (code != 0) {
        LOG_WARNING(kLogSource, "scene sync code != 0");
        return list;
    }

    const QJsonArray arr = obj.value("result").toArray();
    for (const auto &v : arr) {
        if (!v.isObject())
            continue;

        core::AutoStrategy s;
        if (parseAutoStrategyFromJson(v.toObject(), s)) {
            s.type = "scene";
            list.append(s);
        }
    }

    return list;
}

bool parseTimerSetData(const QJsonObject &obj,
                       core::AutoStrategy &out,
                       QString *error)
{
    // 先复用 scene
    if (!parseAutoStrategyFromJson(obj, out, error))
        return false;

    out.type = "timer";

    // @TODO:

    return true;
}

QList<core::AutoStrategy> parseTimerSyncData(const QJsonObject &obj)
{
    QList<core::AutoStrategy> list;

    const QJsonArray arr = obj.value("result").toArray();
    for (const auto &v : arr) {
        if (!v.isObject())
            continue;

        core::AutoStrategy s;
        if (parseAutoStrategyFromJson(v.toObject(), s)) {
            s.type = "timer";
            list.append(s);
        }
    }

    return list;
}


bool parseDeleteCommand(const QString &type,
                        const QJsonValue &data,
                        QList<int> &sceneIds,
                        QString *error)
{
    sceneIds.clear();

    if (type != "scene" && type != "timer") {
        if (error) *error = "unsupported strategy type";
        return false;
    }

    if (data.isDouble()) {
        int id = data.toInt();
        if (id <= 0) {
            if (error) *error = "invalid strategy id";
            return false;
        }
        sceneIds.append(id);
        return true;
    }

    if (data.isArray()) {
        const QJsonArray arr = data.toArray();
        for (const auto &value : arr) {
            if (!value.isDouble()) {
                if (error) *error = "scene id must be numeric";
                return false;
            }
            const double rawId = value.toDouble();
            const double truncatedId = std::trunc(rawId);
            if (truncatedId != rawId) {
                if (error) *error = "scene id must be an integer";
                return false;
            }
            if (truncatedId <= 0) {
                if (error) *error = "scene id must be positive";
                return false;
            }
            if (truncatedId > static_cast<double>(std::numeric_limits<int>::max())) {
                if (error) *error = "scene id out of range";
                return false;
            }
            const int id = static_cast<int>(truncatedId);
            sceneIds.append(id);
        }
        if (sceneIds.isEmpty()) {
            if (error) *error = "scene id array cannot be empty";
            return false;
        }
        return true;
    }

    if (error) *error = "invalid delete data format";
    return false;
}


}
}
}
