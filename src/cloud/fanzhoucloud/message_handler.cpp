#include "message_handler.h"
#include "core/core_context.h"
#include "utils/logger.h"
#include "device/can/relay_gd427.h"
#include "device/can/relay_protocol.h"
#include "cloud/mqtt/mqtt_channel_manager.h"
#include "cloud/fanzhoucloud/setting_service.h"
#include "cloud/fanzhoucloud/parser.h"

#include <QJsonDocument>

namespace fanzhou {
namespace cloud {
namespace fanzhoucloud {
namespace  {
const char *const kLogSource = "CloudMessageHandler";
}



CloudMessageHandler::CloudMessageHandler(core::CoreContext *ctx, QObject *parent)
    : QObject(parent),
      ctx_(ctx)
{
    Q_ASSERT(ctx_);
}


void CloudMessageHandler::onMqttMessage(int channelId,
                                        const QString &topic,
                                        const QByteArray &payload)
{
    if (!ctx_ || !ctx_->mqttManager || channelId != channelId_) {
        return;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Invalid JSON payload on topic '%1'").arg(topic));
        return;
    }

    const QJsonObject obj = doc.object();

    const QString controlTopic =
        ctx_->mqttManager->getControlTopicFromConfig(channelId);

    const QString settingTopic =
            ctx_->mqttManager->getSettingSubTopicFromConfig(channelId);

    // 分发处理
    if (!controlTopic.isEmpty() && topic == controlTopic) {
        handleControlCommand(channelId, obj);
        return;
    }

    if (!settingTopic.isEmpty() && topic == settingTopic) {
        handleStrategyCommand(channelId, obj);
        return;
    }

    LOG_DEBUG(kLogSource,
              QStringLiteral("Unhandled MQTT topic: channel=%1 topic='%2'")
                  .arg(channelId)
                  .arg(topic));
}

void CloudMessageHandler::setChannelId(int channel)
{
    channelId_ = channel;
}


QList<core::AutoStrategy> parseStrategyByType(
    const QString &type,
    const QJsonObject &data,
    QString *error)
{
    if (type == "scene") {
        return parseSceneDataFromJson(data);
    }

    if (type == "timer") {
        if (error) *error = "timer strategy not supported";
        return {};
    }

    if (error) *error = "unknown strategy type";
    return {};
}

bool CloudMessageHandler::applyStrategies(
    const QList<core::AutoStrategy> &list,
    int &lastId,
    int &lastVersion,
    QString &errMsg)
{
    for (const auto &cfg : list) {
        bool isUpdate = false;
        QString err;

        if (!ctx_->checkActionValid(cfg, &err)) {
            errMsg = err;
            return false;
        }

        if (!ctx_->createStrategy(cfg, &isUpdate, &err)) {
            errMsg = err;
            return false;
        }

        lastId = cfg.strategyId;
        lastVersion = cfg.version;
    }
    return true;
}

bool CloudMessageHandler::sendStrategyCommand(const core::AutoStrategy &a, const QJsonObject &msg)
{

}

bool CloudMessageHandler::handleStrategyCommand(
        const int channelId,
        const QJsonObject &msg)
{
    LOG_DEBUG(kLogSource,
              QStringLiteral("Handled the %1 cloud Strategy commands, msg: %2")
              .arg(channelId)
              .arg(QString(QJsonDocument(msg).toJson(QJsonDocument::Compact))));

    if (channelId != channelId_) return false;

    if (!msg.contains("method") ||
            !msg.contains("type") ||
            !msg.contains("requestId")) {
        LOG_WARNING(kLogSource, QStringLiteral("bad strategy packet"));
        return false;
    }

    const QString method = msg.value("method").toString();
    const QString type = msg.value("type").toString();
    const QString requestId = msg.value("requestId").toString();

    int lastId = 0;
    int lastVersion = 0;
    int errCode = 0;
    QString errMsg;
    bool ok = false;

    QList<core::AutoStrategy> strategies;
    QString parseErr;

    // parse
    if (method == "get_response") {
        strategies = parseSyncData(
                    type,
                    msg.value("data").toObject(),
                    &parseErr);

        if (!parseErr.isEmpty()) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("parse get_response failed: %1")
                        .arg(parseErr));
            return false;
        }

        // is response, no reply
        if (!applyStrategies(strategies, lastId, lastVersion, errMsg)) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("apply get_response failed: %1")
                        .arg(errMsg));
            return false;
        }

        return true;
    }

    else if (method == "set") {
        core::AutoStrategy one;
        if (!parseSetCommand(
                    type,
                    msg.value("data").toObject(),
                    one,
                    &parseErr)) {
            errCode = 1001;
            errMsg = parseErr;
            goto REPLY;
        }

        strategies = { one };
    }

    else if (method == "delete") {
        QList<int> ids;
        if (!parseDeleteCommand(
                    type,
                    msg.value("data"),
                    ids,
                    &parseErr)) {
            errCode = 4001; // 场景ID为空
            errMsg = parseErr;
            goto REPLY;
        }

        QString delErr;
        for (int id : ids) {
            bool alreadyDeleted = false;
            if (!ctx_->deleteStrategy(id, &delErr, &alreadyDeleted)) {
                if (alreadyDeleted) {
                    LOG_INFO(kLogSource,
                             QStringLiteral("Skip delete for %1: %2")
                                 .arg(id)
                                 .arg(delErr));
                    continue;
                }
                ok = false;
                errCode = 4002;     // 场景不存在或其他删除错误
                errMsg = delErr;
                goto REPLY;
            }
        }


        // 暂时删除一个
        lastId = ids.first();
        ok = true;
        errMsg = QStringLiteral("删除成功");
        goto REPLY;
    }

    else if (method == "get") {

    }

    else {
        errCode = 1000;
        errMsg = QStringLiteral("unsupported method");
        goto REPLY;
    }

    // apply
    if (!applyStrategies(strategies, lastId, lastVersion, errMsg)) {
        errCode = 1002;
        goto REPLY;
    }

    ok = true;

    // reply
REPLY:
    return sendStrategyReply(
                method,
                type,
                ok,
                lastId,
                lastVersion,
                requestId,
                errCode,
                errMsg);
}



bool CloudMessageHandler::sendStrategyReply(const QString &method,
                                            const QString &type,
                                            bool ok,
                                            int objectId,
                                            int version,
                                            const QString &requestId,
                                            int errCode,
                                            const QString &errMsg)
{
    // 防止对 response 再回复
    if (method.contains("_response")) {
        LOG_WARNING(kLogSource, QStringLiteral("Already response method, skip reply"));
        return false;
    }

    QJsonObject report;

    // method: get -> get_response, set -> set_response, delete -> delete_response
    const QString rp_method = method + "_response";
    report.insert("method", rp_method);
    report.insert("type", type);

    // ===== 构造 data =====
    QJsonObject data;

    if (ok) {
        data.insert("code", 0);
        data.insert("message", "");

        QJsonObject result;

        // 根据 type 决定返回字段名
        if (type == "scene") {
            result.insert("sceneId", objectId);
        } else if (type == "timer") {
            result.insert("timerId", objectId);
        }

        // get / set 才需要 version / updateTime
        if (method == "get" || method == "set") {
            result.insert("version", version);
            result.insert("updateTime",
                          QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
        }

        data.insert("result", result);
    } else {
        data.insert("code", errCode);
        data.insert("message", errMsg);
        data.insert("result", QJsonValue()); // null
    }

    report.insert("data", data);

    // ===== requestId 原样返回 =====
    report.insert("requestId", requestId);

    // ===== timestamp：毫秒 =====
    const qint64 ts = QDateTime::currentMSecsSinceEpoch();
    report.insert("timestamp", ts);

    // report
    QByteArray report_str = QJsonDocument(report).toJson(QJsonDocument::Compact);

    // 打 log
    LOG_DEBUG(kLogSource,
              QStringLiteral("Send strategy reply: %1")
              .arg(QString(report_str)));

    // 实际发送
    return ctx_->mqttManager->publishSetting(
                channelId_,
                report_str,
                0
                );
}


bool CloudMessageHandler::handleControlCommand(const int channelId, const QJsonObject &msg)
{
    if (!ctx_) {
        LOG_WARNING(kLogSource, "CoreContext is null, cannot handle control command");
        return false;
    }

    int successCount = 0;

    // 遍历所有 key-value
    for (auto it = msg.begin(); it != msg.end(); ++it) {
        const QString key = it.key();
        if (!key.startsWith(QStringLiteral("node_"))) {
            continue;
        }

        // 期望格式: node_<nodeId>_sw<ch>
        const QStringList parts = key.split('_');
        if (parts.size() != 3) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("invalid control key format: %1").arg(key));
            continue;
        }

        bool ok1 = false;
        bool ok2 = false;

        const int nodeId = parts[1].toInt(&ok1);  // node_<id>
        const QString swPart = parts[2];          // sw1

        if (!ok1 || !swPart.startsWith(QStringLiteral("sw"))) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("invalid control key format: %1").arg(key));
            continue;
        }

        const int chIndex = swPart.mid(2).toInt(&ok2); // sw1 -> 1
        if (!ok2 || chIndex <= 0) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("invalid channel index in key: %1").arg(key));
            continue;
        }


        // fix index
        const int channel = chIndex - 1;

        const int mode = it.value().toInt(-1);
        if (mode < 0) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("invalid mode value for key=%1").arg(key));
            continue;
        }

        // 查找设备
        auto *dev = ctx_->relays.value(static_cast<quint8>(nodeId), nullptr);
        if (!dev) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("Control command for unknown nodeId=%1").arg(nodeId));
            continue;
        }

        LOG_INFO(kLogSource,
                 QStringLiteral("Cloud control: nodeId=%1 ch=%2 mode=%3")
                     .arg(nodeId)
                     .arg(channel)
                     .arg(mode));

        // 执行控制
        dev->control(channel,
                     static_cast<device::RelayProtocol::Action>(mode));

        successCount++;
    }

    if (successCount == 0) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("No valid control command found in payload: %1")
                        .arg(QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact))));
        ctx_->onMqttSensorMessage(channelId, "", msg);
    } else {
        LOG_INFO(kLogSource,
                 QStringLiteral("Handled the %1 cloud %2 control commands").arg(channelId).arg(successCount));
        return true;
    }

    return false;
}


bool CloudMessageHandler::handleSettingCommand(const int channelId, const QJsonObject &msg)
{
    LOG_DEBUG(kLogSource,QStringLiteral("Handled the %1 cloud Setting commands").arg(channelId));

    const QString method = msg.value("method").toString();
    const QString type = msg.value("type").toString();
    const QJsonObject data = msg.value("data").toObject();
    const QString requestId = msg.value("requestId").toString();
    const qint64 timestamp = msg.value("timestamp").toVariant().toLongLong();

    if (method.isEmpty() || type.isEmpty() || requestId.isEmpty()) {
        LOG_WARNING(kLogSource, "invalid setting message: missing fields");
        return false;
    }

    LOG_INFO(kLogSource,
             QStringLiteral("Setting request: method=%1 type=%2 requestId=%3")
             .arg(method)
             .arg(type)
             .arg(requestId));

    // resp
    QJsonObject resp;
    QString error;

    bool ok = ctx_->cloudSettingService->handleRequest(data, resp, error);

    if (ok || !error.isEmpty()) {
        resp["method"] = QStringLiteral("%1_response").arg(method);
        resp["type"] = type;
        resp["requestId"] = requestId;
        resp["timestamp"] = static_cast<qint64>(timestamp);

        ctx_->mqttManager->publishSetting(
                       channelId,
                       QJsonDocument(resp).toJson(QJsonDocument::Compact),
                       0);
        return true;
    }
    return false;
}




}
} // namespace cloud
} // namespace fanzhou
