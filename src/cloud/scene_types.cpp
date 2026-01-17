/**
 * @file scene_types.cpp
 * @brief 场景数据类型实现
 */

#include "scene_types.h"

#include <QDateTime>
#include <QRandomGenerator>
#include <QTime>

namespace fanzhou {
namespace cloud {

// ==================== SceneCondition ====================

SceneCondition SceneCondition::fromJson(const QJsonObject &obj)
{
    SceneCondition cond;
    cond.deviceCode = obj.value(QStringLiteral("deviceCode")).toString();
    cond.identifier = obj.value(QStringLiteral("identifier")).toString();
    cond.identifierValue = obj.value(QStringLiteral("identifierValue")).toVariant();
    cond.op = parseConditionOp(obj.value(QStringLiteral("op")).toString());
    return cond;
}

QJsonObject SceneCondition::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("deviceCode")] = deviceCode;
    obj[QStringLiteral("identifier")] = identifier;
    obj[QStringLiteral("identifierValue")] = QJsonValue::fromVariant(identifierValue);
    obj[QStringLiteral("op")] = conditionOpToString(op);
    return obj;
}

bool SceneCondition::evaluate(const QVariant &currentValue) const
{
    // 尝试转换为数值进行比较
    bool ok1 = false, ok2 = false;
    double current = currentValue.toDouble(&ok1);
    double target = identifierValue.toDouble(&ok2);

    if (ok1 && ok2) {
        // 数值比较 - 使用epsilon比较以避免qFuzzyCompare对零值的问题
        constexpr double epsilon = 0.0001;
        switch (op) {
        case SceneConditionOp::Equal:
            return qAbs(current - target) < epsilon;
        case SceneConditionOp::NotEqual:
            return qAbs(current - target) >= epsilon;
        case SceneConditionOp::GreaterThan:
            return current > target;
        case SceneConditionOp::LessThan:
            return current < target;
        case SceneConditionOp::GreaterOrEqual:
            return current >= target - epsilon;
        case SceneConditionOp::LessOrEqual:
            return current <= target + epsilon;
        }
    } else {
        // 字符串比较
        QString currentStr = currentValue.toString();
        QString targetStr = identifierValue.toString();

        switch (op) {
        case SceneConditionOp::Equal:
            return currentStr == targetStr;
        case SceneConditionOp::NotEqual:
            return currentStr != targetStr;
        default:
            return false; // 字符串不支持大小比较
        }
    }

    return false;
}

// ==================== SceneAction ====================

SceneAction SceneAction::fromJson(const QJsonObject &obj)
{
    SceneAction action;
    action.deviceCode = obj.value(QStringLiteral("deviceCode")).toString();
    action.identifier = obj.value(QStringLiteral("identifier")).toString();
    action.identifierValue = obj.value(QStringLiteral("identifierValue")).toVariant();
    return action;
}

QJsonObject SceneAction::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("deviceCode")] = deviceCode;
    obj[QStringLiteral("identifier")] = identifier;
    obj[QStringLiteral("identifierValue")] = QJsonValue::fromVariant(identifierValue);
    return obj;
}

// ==================== SceneConfig ====================

SceneConfig SceneConfig::fromJson(const QJsonObject &obj)
{
    SceneConfig scene;
    scene.id = obj.value(QStringLiteral("id")).toInt(0);
    scene.sceneName = obj.value(QStringLiteral("sceneName")).toString();

    // 解析场景类型
    QString typeStr = obj.value(QStringLiteral("sceneType")).toString().toLower();
    scene.sceneType = (typeStr == QStringLiteral("manual")) ? SceneType::Manual : SceneType::Auto;

    // 解析触发方式
    scene.matchType = static_cast<SceneMatchType>(obj.value(QStringLiteral("matchType")).toInt(0));

    scene.effectiveBeginTime = obj.value(QStringLiteral("effectiveBeginTime")).toString();
    scene.effectiveEndTime = obj.value(QStringLiteral("effectiveEndTime")).toString();
    scene.status = obj.value(QStringLiteral("status")).toInt(0);
    scene.version = obj.value(QStringLiteral("version")).toInt(1);
    scene.createTime = obj.value(QStringLiteral("createTime")).toString();
    scene.updateTime = obj.value(QStringLiteral("updateTime")).toString();

    // 解析动作列表
    if (obj.contains(QStringLiteral("actions")) && obj[QStringLiteral("actions")].isArray()) {
        const QJsonArray actionsArr = obj[QStringLiteral("actions")].toArray();
        for (const auto &val : actionsArr) {
            if (val.isObject()) {
                scene.actions.append(SceneAction::fromJson(val.toObject()));
            }
        }
    }

    // 解析条件列表
    if (obj.contains(QStringLiteral("conditions")) && obj[QStringLiteral("conditions")].isArray()) {
        const QJsonArray conditionsArr = obj[QStringLiteral("conditions")].toArray();
        for (const auto &val : conditionsArr) {
            if (val.isObject()) {
                scene.conditions.append(SceneCondition::fromJson(val.toObject()));
            }
        }
    }

    return scene;
}

QJsonObject SceneConfig::toJson() const
{
    QJsonObject obj;

    if (id > 0) {
        obj[QStringLiteral("id")] = id;
    }
    obj[QStringLiteral("sceneName")] = sceneName;
    obj[QStringLiteral("sceneType")] = (sceneType == SceneType::Manual)
                                            ? QStringLiteral("manual")
                                            : QStringLiteral("auto");
    obj[QStringLiteral("matchType")] = static_cast<int>(matchType);
    obj[QStringLiteral("effectiveBeginTime")] = effectiveBeginTime;
    obj[QStringLiteral("effectiveEndTime")] = effectiveEndTime;
    obj[QStringLiteral("status")] = status;
    obj[QStringLiteral("version")] = version;

    if (!createTime.isEmpty()) {
        obj[QStringLiteral("createTime")] = createTime;
    }
    if (!updateTime.isEmpty()) {
        obj[QStringLiteral("updateTime")] = updateTime;
    }

    // 动作列表
    QJsonArray actionsArr;
    for (const auto &action : actions) {
        actionsArr.append(action.toJson());
    }
    obj[QStringLiteral("actions")] = actionsArr;

    // 条件列表（手动场景可以省略）
    if (!conditions.isEmpty()) {
        QJsonArray conditionsArr;
        for (const auto &cond : conditions) {
            conditionsArr.append(cond.toJson());
        }
        obj[QStringLiteral("conditions")] = conditionsArr;
    }

    return obj;
}

bool SceneConfig::isValid() const
{
    // 检查基本字段
    if (sceneName.isEmpty()) {
        return false;
    }

    // 检查动作列表
    if (actions.isEmpty()) {
        return false;
    }

    // 自动场景必须有条件
    if (sceneType == SceneType::Auto && conditions.isEmpty()) {
        return false;
    }

    return true;
}

bool SceneConfig::isInEffectiveTime() const
{
    if (effectiveBeginTime.isEmpty() || effectiveEndTime.isEmpty()) {
        return true; // 未设置时间范围则始终有效
    }

    QTime beginTime = QTime::fromString(effectiveBeginTime, QStringLiteral("HH:mm"));
    QTime endTime = QTime::fromString(effectiveEndTime, QStringLiteral("HH:mm"));
    QTime currentTime = QTime::currentTime();

    if (!beginTime.isValid() || !endTime.isValid()) {
        return true; // 时间格式无效则始终有效
    }

    if (beginTime <= endTime) {
        // 正常时间范围（如 06:00 - 18:00）
        return currentTime >= beginTime && currentTime <= endTime;
    } else {
        // 跨夜时间范围（如 22:00 - 06:00）
        return currentTime >= beginTime || currentTime <= endTime;
    }
}

// ==================== SettingMessage ====================

SettingMessage SettingMessage::fromJson(const QJsonObject &obj)
{
    SettingMessage msg;
    msg.method = parseSettingMethod(obj.value(QStringLiteral("method")).toString());
    msg.data = obj.value(QStringLiteral("data")).toObject();
    msg.requestId = obj.value(QStringLiteral("requestId")).toString();
    msg.responseId = obj.value(QStringLiteral("responseId")).toString();
    msg.timestamp = static_cast<qint64>(obj.value(QStringLiteral("timestamp")).toDouble(0));
    msg.error = obj.value(QStringLiteral("error")).toObject();
    return msg;
}

QJsonObject SettingMessage::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("method")] = settingMethodToString(method);
    obj[QStringLiteral("data")] = data;

    if (!requestId.isEmpty()) {
        obj[QStringLiteral("requestId")] = requestId;
    }
    if (!responseId.isEmpty()) {
        obj[QStringLiteral("responseId")] = responseId;
    }
    if (timestamp > 0) {
        obj[QStringLiteral("timestamp")] = static_cast<double>(timestamp);
    }
    if (!error.isEmpty()) {
        obj[QStringLiteral("error")] = error;
    }

    return obj;
}

bool SettingMessage::hasError() const
{
    return !error.isEmpty();
}

// ==================== SceneProcessResult ====================

QJsonObject SceneProcessResult::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = id;

    if (!sceneName.isEmpty()) {
        obj[QStringLiteral("sceneName")] = sceneName;
    }
    obj[QStringLiteral("status")] = status;

    if (version > 0) {
        obj[QStringLiteral("version")] = version;
    }
    if (errorCode != 0) {
        obj[QStringLiteral("errorCode")] = errorCode;
        obj[QStringLiteral("errorMsg")] = errorMsg;
    }
    if (!createTime.isEmpty()) {
        obj[QStringLiteral("createTime")] = createTime;
    }
    if (!updateTime.isEmpty()) {
        obj[QStringLiteral("updateTime")] = updateTime;
    }
    if (!deleteTime.isEmpty()) {
        obj[QStringLiteral("deleteTime")] = deleteTime;
    }

    return obj;
}

// ==================== 工具函数 ====================

SettingMethod parseSettingMethod(const QString &method)
{
    const QString m = method.toLower();
    if (m == QStringLiteral("get")) return SettingMethod::Get;
    if (m == QStringLiteral("get_response")) return SettingMethod::GetResponse;
    if (m == QStringLiteral("set")) return SettingMethod::Set;
    if (m == QStringLiteral("set_response")) return SettingMethod::SetResponse;
    if (m == QStringLiteral("delete")) return SettingMethod::Delete;
    if (m == QStringLiteral("delete_response")) return SettingMethod::DeleteResponse;
    if (m == QStringLiteral("delete_sync")) return SettingMethod::DeleteSync;
    if (m == QStringLiteral("delete_ack")) return SettingMethod::DeleteAck;
    return SettingMethod::Unknown;
}

QString settingMethodToString(SettingMethod method)
{
    switch (method) {
    case SettingMethod::Get: return QStringLiteral("get");
    case SettingMethod::GetResponse: return QStringLiteral("get_response");
    case SettingMethod::Set: return QStringLiteral("set");
    case SettingMethod::SetResponse: return QStringLiteral("set_response");
    case SettingMethod::Delete: return QStringLiteral("delete");
    case SettingMethod::DeleteResponse: return QStringLiteral("delete_response");
    case SettingMethod::DeleteSync: return QStringLiteral("delete_sync");
    case SettingMethod::DeleteAck: return QStringLiteral("delete_ack");
    default: return QStringLiteral("unknown");
    }
}

SceneConditionOp parseConditionOp(const QString &op)
{
    const QString o = op.toLower();
    if (o == QStringLiteral("eq")) return SceneConditionOp::Equal;
    if (o == QStringLiteral("ne")) return SceneConditionOp::NotEqual;
    if (o == QStringLiteral("gt")) return SceneConditionOp::GreaterThan;
    if (o == QStringLiteral("lt")) return SceneConditionOp::LessThan;
    if (o == QStringLiteral("egt")) return SceneConditionOp::GreaterOrEqual;
    if (o == QStringLiteral("elt")) return SceneConditionOp::LessOrEqual;
    return SceneConditionOp::Equal;
}

QString conditionOpToString(SceneConditionOp op)
{
    switch (op) {
    case SceneConditionOp::Equal: return QStringLiteral("eq");
    case SceneConditionOp::NotEqual: return QStringLiteral("ne");
    case SceneConditionOp::GreaterThan: return QStringLiteral("gt");
    case SceneConditionOp::LessThan: return QStringLiteral("lt");
    case SceneConditionOp::GreaterOrEqual: return QStringLiteral("egt");
    case SceneConditionOp::LessOrEqual: return QStringLiteral("elt");
    default: return QStringLiteral("eq");
    }
}

QString generateRequestId()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const quint32 random = QRandomGenerator::global()->generate();
    return QStringLiteral("req_%1_%2")
        .arg(QDateTime::fromMSecsSinceEpoch(now).toString(QStringLiteral("yyyyMMddHHmmss")))
        .arg(random % 1000000, 6, 10, QChar('0'));  // 使用6位随机数提高唯一性
}

QString generateResponseId()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const quint32 random = QRandomGenerator::global()->generate();
    return QStringLiteral("resp_%1_%2")
        .arg(QDateTime::fromMSecsSinceEpoch(now).toString(QStringLiteral("yyyyMMddHHmmss")))
        .arg(random % 1000000, 6, 10, QChar('0'));
}

qint64 currentTimestampMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

QString currentTimeString()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

}  // namespace cloud
}  // namespace fanzhou
