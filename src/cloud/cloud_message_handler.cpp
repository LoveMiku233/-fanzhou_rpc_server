/**
 * @file cloud_message_handler.cpp
 * @brief 云平台消息处理器实现
 */

#include "cloud_message_handler.h"
#include "scene_manager.h"
#include "scene_types.h"
#include "utils/logger.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace fanzhou {
namespace cloud {

namespace {
const char *const kLogSource = "CloudMessageHandler";
const QString kKeySetting = QStringLiteral("setting");
const QString kKeyMethod = QStringLiteral("method");
const QString kKeyData = QStringLiteral("data");
const QString kKeyScene = QStringLiteral("scene");
const QString kKeyRequestId = QStringLiteral("requestId");
const QString kKeyResponseId = QStringLiteral("responseId");
const QString kKeyTimestamp = QStringLiteral("timestamp");
const QString kKeyError = QStringLiteral("error");
const QString kKeyDeleteReason = QStringLiteral("deleteReason");
}  // namespace

CloudMessageHandler::CloudMessageHandler(SceneManager *sceneManager, QObject *parent)
    : QObject(parent)
    , sceneManager_(sceneManager)
{
}

QByteArray CloudMessageHandler::handleMessage(const QString &topic, const QByteArray &payload)
{
    Q_UNUSED(topic);

    // 解析JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Failed to parse MQTT message: %1")
                        .arg(parseError.errorString()));
        return QByteArray();
    }

    if (!doc.isObject()) {
        return QByteArray();
    }

    QJsonObject root = doc.object();

    // 检查是否包含setting字段
    if (!root.contains(kKeySetting)) {
        return QByteArray();
    }

    // setting可能是字符串（需要再次解析）或直接是对象
    QJsonObject settingObj;
    if (root[kKeySetting].isString()) {
        settingObj = handleSetting(root[kKeySetting].toString());
    } else if (root[kKeySetting].isObject()) {
        settingObj = handleSettingObject(root[kKeySetting].toObject());
    } else {
        LOG_WARNING(kLogSource, QStringLiteral("Invalid setting field type"));
        return QByteArray();
    }

    // 如果有响应，构建响应消息
    if (!settingObj.isEmpty()) {
        QJsonObject response;
        // 复制其他字段（如传感器数据）
        for (auto it = root.begin(); it != root.end(); ++it) {
            if (it.key() != kKeySetting) {
                response[it.key()] = it.value();
            }
        }
        // 添加响应的setting字段
        response[kKeySetting] = settingObj;

        return QJsonDocument(response).toJson(QJsonDocument::Compact);
    }

    return QByteArray();
}

QJsonObject CloudMessageHandler::handleSetting(const QString &settingStr)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(settingStr.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Failed to parse setting string: %1")
                        .arg(parseError.errorString()));
        return QJsonObject();
    }

    if (!doc.isObject()) {
        return QJsonObject();
    }

    return handleSettingObject(doc.object());
}

QJsonObject CloudMessageHandler::handleSettingObject(const QJsonObject &settingObj)
{
    SettingMessage msg = SettingMessage::fromJson(settingObj);

    LOG_DEBUG(kLogSource,
              QStringLiteral("Handling setting: method=%1, requestId=%2")
                  .arg(settingMethodToString(msg.method))
                  .arg(msg.requestId));

    switch (msg.method) {
    case SettingMethod::Get:
        return handleGetRequest(msg.data, msg.requestId);

    case SettingMethod::GetResponse:
        handleGetResponse(msg.data, msg.requestId, msg.error);
        return QJsonObject();

    case SettingMethod::Set:
        return handleSetRequest(msg.data, msg.requestId);

    case SettingMethod::SetResponse:
        handleSetResponse(msg.data, msg.requestId);
        return QJsonObject();

    case SettingMethod::Delete:
        return handleDeleteRequest(msg.data, msg.requestId);

    case SettingMethod::DeleteResponse:
        handleDeleteResponse(msg.data, msg.requestId);
        return QJsonObject();

    case SettingMethod::DeleteSync:
        return handleDeleteSyncRequest(msg.data, msg.requestId);

    case SettingMethod::DeleteAck:
        // 柜端发送的确认，通常不需要处理
        return QJsonObject();

    default:
        LOG_WARNING(kLogSource,
                    QStringLiteral("Unknown setting method: %1")
                        .arg(settingObj.value(kKeyMethod).toString()));
        return QJsonObject();
    }
}

// ==================== 处理请求和响应 ====================

QJsonObject CloudMessageHandler::handleGetRequest(const QJsonObject &data,
                                                    const QString &requestId)
{
    // 解析场景ID
    int sceneId = 0;
    if (data.contains(kKeyScene)) {
        sceneId = data.value(kKeyScene).toInt(0);
    }

    LOG_INFO(kLogSource,
             QStringLiteral("Processing get request: sceneId=%1, requestId=%2")
                 .arg(sceneId)
                 .arg(requestId));

    // 获取场景数据
    QList<SceneConfig> scenes = sceneManager_->getScenes(sceneId);

    // 构建响应
    QJsonObject response;
    response[kKeyMethod] = QStringLiteral("get_response");

    QJsonObject responseData;
    QJsonArray sceneArr;
    for (const auto &scene : scenes) {
        sceneArr.append(scene.toJson());
    }
    responseData[kKeyScene] = sceneArr;
    response[kKeyData] = responseData;

    if (!requestId.isEmpty()) {
        response[kKeyRequestId] = requestId;
    }
    response[kKeyTimestamp] = static_cast<double>(currentTimestampMs());

    // 如果请求的场景不存在且sceneId > 0，添加错误信息
    if (sceneId > 0 && scenes.isEmpty()) {
        QJsonObject error;
        error[QStringLiteral("code")] = 2001;
        error[QStringLiteral("message")] = QStringLiteral("场景不存在");
        response[kKeyError] = error;
    }

    return response;
}

void CloudMessageHandler::handleGetResponse(const QJsonObject &data,
                                             const QString &requestId,
                                             const QJsonObject &error)
{
    Q_UNUSED(requestId);

    if (!error.isEmpty()) {
        int code = error.value(QStringLiteral("code")).toInt(0);
        QString message = error.value(QStringLiteral("message")).toString();
        LOG_WARNING(kLogSource,
                    QStringLiteral("Get response error: code=%1, message=%2")
                        .arg(code)
                        .arg(message));
        return;
    }

    // 解析场景数据
    if (!data.contains(kKeyScene) || !data[kKeyScene].isArray()) {
        LOG_WARNING(kLogSource, QStringLiteral("Get response missing scene array"));
        return;
    }

    QJsonArray sceneArr = data[kKeyScene].toArray();
    QList<SceneConfig> scenes;
    for (const auto &val : sceneArr) {
        if (val.isObject()) {
            scenes.append(SceneConfig::fromJson(val.toObject()));
        }
    }

    LOG_INFO(kLogSource,
             QStringLiteral("Received %1 scenes from cloud").arg(scenes.size()));

    // 同步场景
    if (!scenes.isEmpty()) {
        sceneManager_->syncScenes(scenes);
        emit scenesUpdated();
    }
}

QJsonObject CloudMessageHandler::handleSetRequest(const QJsonObject &data,
                                                    const QString &requestId)
{
    // 解析场景数据
    if (!data.contains(kKeyScene) || !data[kKeyScene].isArray()) {
        QJsonObject response;
        response[kKeyMethod] = QStringLiteral("set_response");
        QJsonObject error;
        error[QStringLiteral("code")] = 1001;
        error[QStringLiteral("message")] = QStringLiteral("缺少场景数据");
        response[kKeyError] = error;
        if (!requestId.isEmpty()) {
            response[kKeyRequestId] = requestId;
        }
        response[kKeyTimestamp] = static_cast<double>(currentTimestampMs());
        return response;
    }

    QJsonArray sceneArr = data[kKeyScene].toArray();
    QList<SceneConfig> scenes;
    for (const auto &val : sceneArr) {
        if (val.isObject()) {
            scenes.append(SceneConfig::fromJson(val.toObject()));
        }
    }

    LOG_INFO(kLogSource,
             QStringLiteral("Processing set request: %1 scenes, requestId=%2")
                 .arg(scenes.size())
                 .arg(requestId));

    // 同步场景并获取结果
    QList<SceneProcessResult> results = sceneManager_->syncScenes(scenes);

    // 构建响应
    QJsonObject response;
    response[kKeyMethod] = QStringLiteral("set_response");

    QJsonObject responseData;
    QJsonArray resultArr;
    for (const auto &result : results) {
        resultArr.append(result.toJson());
    }
    responseData[kKeyScene] = resultArr;
    response[kKeyData] = responseData;

    if (!requestId.isEmpty()) {
        response[kKeyRequestId] = requestId;
    }
    response[kKeyResponseId] = generateRequestId().replace(QStringLiteral("req_"),
                                                            QStringLiteral("resp_"));
    response[kKeyTimestamp] = static_cast<double>(currentTimestampMs());

    emit scenesUpdated();
    return response;
}

void CloudMessageHandler::handleSetResponse(const QJsonObject &data,
                                             const QString &requestId)
{
    Q_UNUSED(requestId);

    // 解析场景处理结果
    if (!data.contains(kKeyScene) || !data[kKeyScene].isArray()) {
        LOG_WARNING(kLogSource, QStringLiteral("Set response missing scene array"));
        return;
    }

    QJsonArray sceneArr = data[kKeyScene].toArray();
    for (const auto &val : sceneArr) {
        if (!val.isObject()) continue;

        QJsonObject obj = val.toObject();
        int id = obj.value(QStringLiteral("id")).toInt(0);
        QString status = obj.value(QStringLiteral("status")).toString();
        int version = obj.value(QStringLiteral("version")).toInt(0);

        if (status == QStringLiteral("success")) {
            LOG_INFO(kLogSource,
                     QStringLiteral("Scene set success: id=%1, version=%2")
                         .arg(id)
                         .arg(version));

            // 如果是新增场景，更新本地场景ID
            // 云端返回的id可能与本地临时id不同
        } else {
            int errorCode = obj.value(QStringLiteral("errorCode")).toInt(0);
            QString errorMsg = obj.value(QStringLiteral("errorMsg")).toString();
            LOG_WARNING(kLogSource,
                        QStringLiteral("Scene set failed: id=%1, error=%2 - %3")
                            .arg(id)
                            .arg(errorCode)
                            .arg(errorMsg));
        }
    }
}

QJsonObject CloudMessageHandler::handleDeleteRequest(const QJsonObject &data,
                                                       const QString &requestId)
{
    // 解析要删除的场景ID
    QList<int> sceneIds;
    if (data.contains(kKeyScene) && data[kKeyScene].isArray()) {
        QJsonArray idArr = data[kKeyScene].toArray();
        for (const auto &val : idArr) {
            int id = val.toInt(0);
            if (id > 0) {
                sceneIds.append(id);
            }
        }
    }

    LOG_INFO(kLogSource,
             QStringLiteral("Processing delete request: %1 scenes, requestId=%2")
                 .arg(sceneIds.size())
                 .arg(requestId));

    // 删除场景并获取结果
    QList<SceneProcessResult> results = sceneManager_->deleteScenes(sceneIds);

    // 构建响应
    QJsonObject response;
    response[kKeyMethod] = QStringLiteral("delete_response");

    QJsonObject responseData;
    QJsonArray resultArr;
    for (const auto &result : results) {
        resultArr.append(result.toJson());
    }
    responseData[kKeyScene] = resultArr;
    response[kKeyData] = responseData;

    if (!requestId.isEmpty()) {
        response[kKeyRequestId] = requestId;
    }
    response[kKeyResponseId] = generateRequestId().replace(QStringLiteral("req_"),
                                                            QStringLiteral("resp_"));
    response[kKeyTimestamp] = static_cast<double>(currentTimestampMs());

    emit scenesUpdated();
    return response;
}

void CloudMessageHandler::handleDeleteResponse(const QJsonObject &data,
                                                const QString &requestId)
{
    Q_UNUSED(requestId);

    // 解析删除结果
    if (!data.contains(kKeyScene) || !data[kKeyScene].isArray()) {
        LOG_WARNING(kLogSource, QStringLiteral("Delete response missing scene array"));
        return;
    }

    QJsonArray sceneArr = data[kKeyScene].toArray();
    for (const auto &val : sceneArr) {
        if (!val.isObject()) continue;

        QJsonObject obj = val.toObject();
        int id = obj.value(QStringLiteral("id")).toInt(0);
        QString status = obj.value(QStringLiteral("status")).toString();

        if (status == QStringLiteral("deleted")) {
            LOG_INFO(kLogSource,
                     QStringLiteral("Scene delete confirmed: id=%1").arg(id));
        } else {
            int errorCode = obj.value(QStringLiteral("errorCode")).toInt(0);
            QString errorMsg = obj.value(QStringLiteral("errorMsg")).toString();
            LOG_WARNING(kLogSource,
                        QStringLiteral("Scene delete failed: id=%1, error=%2 - %3")
                            .arg(id)
                            .arg(errorCode)
                            .arg(errorMsg));
        }
    }
}

QJsonObject CloudMessageHandler::handleDeleteSyncRequest(const QJsonObject &data,
                                                          const QString &requestId)
{
    // 云端主动删除同步
    QList<int> sceneIds;
    if (data.contains(kKeyScene) && data[kKeyScene].isArray()) {
        QJsonArray idArr = data[kKeyScene].toArray();
        for (const auto &val : idArr) {
            int id = val.toInt(0);
            if (id > 0) {
                sceneIds.append(id);
            }
        }
    }

    QString deleteReason = data.value(kKeyDeleteReason).toString();

    LOG_INFO(kLogSource,
             QStringLiteral("Processing delete_sync: %1 scenes, reason=%2, requestId=%3")
                 .arg(sceneIds.size())
                 .arg(deleteReason)
                 .arg(requestId));

    // 删除场景
    QList<SceneProcessResult> results = sceneManager_->deleteScenes(sceneIds, deleteReason);

    // 构建确认响应
    QJsonObject response;
    response[kKeyMethod] = QStringLiteral("delete_ack");

    QJsonObject responseData;
    QJsonArray resultArr;
    for (const auto &result : results) {
        resultArr.append(result.toJson());
    }
    responseData[kKeyScene] = resultArr;
    response[kKeyData] = responseData;

    if (!requestId.isEmpty()) {
        response[kKeyRequestId] = requestId;
    }
    response[kKeyTimestamp] = static_cast<double>(currentTimestampMs());

    emit scenesUpdated();
    return response;
}

// ==================== 构建请求消息 ====================

QByteArray CloudMessageHandler::buildGetScenesRequest(int sceneId)
{
    QJsonObject root;

    QJsonObject setting;
    setting[kKeyMethod] = QStringLiteral("get");

    QJsonObject data;
    data[kKeyScene] = sceneId;
    setting[kKeyData] = data;

    setting[kKeyRequestId] = generateRequestId();
    setting[kKeyTimestamp] = static_cast<double>(currentTimestampMs());

    root[kKeySetting] = setting;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray CloudMessageHandler::buildSetSceneRequest(const QJsonObject &scene)
{
    QJsonObject root;

    QJsonObject setting;
    setting[kKeyMethod] = QStringLiteral("set");

    QJsonObject data;
    QJsonArray sceneArr;
    sceneArr.append(scene);
    data[kKeyScene] = sceneArr;
    setting[kKeyData] = data;

    setting[kKeyRequestId] = generateRequestId();
    setting[kKeyTimestamp] = static_cast<double>(currentTimestampMs());

    root[kKeySetting] = setting;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray CloudMessageHandler::buildDeleteScenesRequest(const QList<int> &sceneIds)
{
    QJsonObject root;

    QJsonObject setting;
    setting[kKeyMethod] = QStringLiteral("delete");

    QJsonObject data;
    QJsonArray idArr;
    for (int id : sceneIds) {
        idArr.append(id);
    }
    data[kKeyScene] = idArr;
    setting[kKeyData] = data;

    setting[kKeyRequestId] = generateRequestId();
    setting[kKeyTimestamp] = static_cast<double>(currentTimestampMs());

    root[kKeySetting] = setting;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

}  // namespace cloud
}  // namespace fanzhou
