/**
 * @file scene_manager.cpp
 * @brief 场景管理器实现
 */

#include "scene_manager.h"
#include "utils/logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

namespace fanzhou {
namespace cloud {

namespace {
const char *const kLogSource = "SceneManager";
constexpr int kConditionCheckIntervalMs = 1000; // 条件检查间隔（毫秒）
}  // namespace

SceneManager::SceneManager(QObject *parent)
    : QObject(parent)
{
}

SceneManager::~SceneManager()
{
    if (conditionCheckTimer_) {
        conditionCheckTimer_->stop();
        delete conditionCheckTimer_;
    }
}

bool SceneManager::init(const QString &storagePath)
{
    storagePath_ = storagePath;

    // 尝试从本地存储加载场景
    if (!storagePath_.isEmpty()) {
        QString error;
        if (!loadFromStorage(&error)) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("Failed to load scenes from storage: %1").arg(error));
        }
    }

    // 启动条件检查定时器
    conditionCheckTimer_ = new QTimer(this);
    conditionCheckTimer_->setInterval(kConditionCheckIntervalMs);
    connect(conditionCheckTimer_, &QTimer::timeout, this, &SceneManager::onConditionCheckTimer);
    conditionCheckTimer_->start();

    LOG_INFO(kLogSource,
             QStringLiteral("Scene manager initialized with %1 scenes").arg(scenes_.size()));
    return true;
}

// ==================== 场景CRUD操作 ====================

QList<SceneConfig> SceneManager::getScenes(int sceneId) const
{
    QList<SceneConfig> result;

    if (sceneId == 0) {
        // 返回所有场景
        result = scenes_.values();
    } else if (scenes_.contains(sceneId)) {
        result.append(scenes_.value(sceneId));
    }

    return result;
}

SceneConfig SceneManager::getScene(int sceneId) const
{
    return scenes_.value(sceneId, SceneConfig());
}

SceneProcessResult SceneManager::saveScene(const SceneConfig &scene, QString *error)
{
    SceneProcessResult result;
    result.sceneName = scene.sceneName;

    // 验证场景
    if (!scene.isValid()) {
        result.status = QStringLiteral("fail");
        result.errorCode = 3001;
        result.errorMsg = QStringLiteral("场景格式错误");
        if (error) *error = result.errorMsg;
        return result;
    }

    // 检查场景名称是否已存在（排除自身）
    for (auto it = scenes_.begin(); it != scenes_.end(); ++it) {
        if (it->sceneName == scene.sceneName && it->id != scene.id) {
            result.status = QStringLiteral("fail");
            result.errorCode = 3002;
            result.errorMsg = QStringLiteral("场景名称已存在");
            if (error) *error = result.errorMsg;
            return result;
        }
    }

    SceneConfig newScene = scene;
    const QString timeNow = currentTimeString();

    if (scene.id == 0 || !scenes_.contains(scene.id)) {
        // 新增场景
        if (scene.id == 0) {
            newScene.id = allocateSceneId();
        }
        newScene.version = 1;
        newScene.createTime = timeNow;
        newScene.updateTime = timeNow;

        scenes_.insert(newScene.id, newScene);

        result.id = newScene.id;
        result.status = QStringLiteral("success");
        result.version = newScene.version;
        result.createTime = newScene.createTime;

        LOG_INFO(kLogSource,
                 QStringLiteral("Scene created: id=%1, name=%2")
                     .arg(newScene.id)
                     .arg(newScene.sceneName));
        emit sceneChanged(newScene.id, QStringLiteral("created"));
    } else {
        // 更新场景
        SceneConfig &existing = scenes_[scene.id];
        existing.sceneName = scene.sceneName;
        existing.sceneType = scene.sceneType;
        existing.matchType = scene.matchType;
        existing.effectiveBeginTime = scene.effectiveBeginTime;
        existing.effectiveEndTime = scene.effectiveEndTime;
        existing.status = scene.status;
        existing.version += 1;
        existing.updateTime = timeNow;
        existing.actions = scene.actions;
        existing.conditions = scene.conditions;

        result.id = existing.id;
        result.status = QStringLiteral("success");
        result.version = existing.version;
        result.updateTime = existing.updateTime;

        LOG_INFO(kLogSource,
                 QStringLiteral("Scene updated: id=%1, name=%2, version=%3")
                     .arg(existing.id)
                     .arg(existing.sceneName)
                     .arg(existing.version));
        emit sceneChanged(existing.id, QStringLiteral("updated"));
    }

    // 保存到本地存储
    if (!storagePath_.isEmpty()) {
        QString saveError;
        if (!saveToStorage(&saveError)) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("Failed to save scenes: %1").arg(saveError));
        }
    }

    return result;
}

SceneProcessResult SceneManager::deleteScene(int sceneId, QString *error)
{
    SceneProcessResult result;
    result.id = sceneId;

    if (!scenes_.contains(sceneId)) {
        result.status = QStringLiteral("fail");
        result.errorCode = 4001;
        result.errorMsg = QStringLiteral("场景不存在");
        if (error) *error = result.errorMsg;
        return result;
    }

    const SceneConfig scene = scenes_.take(sceneId);
    result.sceneName = scene.sceneName;
    result.status = QStringLiteral("deleted");
    result.deleteTime = currentTimeString();

    LOG_INFO(kLogSource,
             QStringLiteral("Scene deleted: id=%1, name=%2").arg(sceneId).arg(scene.sceneName));
    emit sceneChanged(sceneId, QStringLiteral("deleted"));

    // 保存到本地存储
    if (!storagePath_.isEmpty()) {
        QString saveError;
        if (!saveToStorage(&saveError)) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("Failed to save scenes: %1").arg(saveError));
        }
    }

    return result;
}

QList<SceneProcessResult> SceneManager::syncScenes(const QList<SceneConfig> &scenes)
{
    QList<SceneProcessResult> results;

    for (const auto &scene : scenes) {
        SceneProcessResult result;
        result.id = scene.id;
        result.sceneName = scene.sceneName;
        result.version = scene.version;

        if (scene.id <= 0) {
            result.status = QStringLiteral("fail");
            result.errorCode = 1001;
            result.errorMsg = QStringLiteral("场景格式错误");
            results.append(result);
            continue;
        }

        // 检查本地是否存在该场景
        if (scenes_.contains(scene.id)) {
            SceneConfig &existing = scenes_[scene.id];

            // 版本比对
            if (scene.version > existing.version) {
                // 云端版本更新，更新本地场景
                existing = scene;
                result.status = QStringLiteral("success");
                LOG_INFO(kLogSource,
                         QStringLiteral("Scene synced (updated): id=%1, version=%2")
                             .arg(scene.id).arg(scene.version));
                emit sceneChanged(scene.id, QStringLiteral("updated"));
            } else if (scene.version < existing.version) {
                // 本地版本更新，可能存在冲突
                result.status = QStringLiteral("fail");
                result.errorCode = 1005;
                result.errorMsg = QStringLiteral("版本冲突");
                LOG_WARNING(kLogSource,
                            QStringLiteral("Scene sync conflict: id=%1, local=%2, remote=%3")
                                .arg(scene.id).arg(existing.version).arg(scene.version));
            } else {
                // 版本相同，无需更新
                result.status = QStringLiteral("success");
            }
        } else {
            // 新增场景
            scenes_.insert(scene.id, scene);
            result.status = QStringLiteral("success");
            LOG_INFO(kLogSource,
                     QStringLiteral("Scene synced (created): id=%1, name=%2")
                         .arg(scene.id).arg(scene.sceneName));
            emit sceneChanged(scene.id, QStringLiteral("created"));
        }

        // 更新下一个可用ID
        if (scene.id >= nextSceneId_) {
            nextSceneId_ = scene.id + 1;
        }

        results.append(result);
    }

    // 保存到本地存储
    if (!storagePath_.isEmpty()) {
        QString saveError;
        if (!saveToStorage(&saveError)) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("Failed to save scenes: %1").arg(saveError));
        }
    }

    return results;
}

QList<SceneProcessResult> SceneManager::deleteScenes(const QList<int> &sceneIds,
                                                      const QString &deleteReason)
{
    Q_UNUSED(deleteReason);

    QList<SceneProcessResult> results;

    for (int sceneId : sceneIds) {
        SceneProcessResult result;
        result.id = sceneId;

        if (scenes_.contains(sceneId)) {
            const SceneConfig scene = scenes_.take(sceneId);
            result.sceneName = scene.sceneName;
            result.status = QStringLiteral("deleted");
            result.deleteTime = currentTimeString();

            LOG_INFO(kLogSource,
                     QStringLiteral("Scene deleted by sync: id=%1, name=%2")
                         .arg(sceneId).arg(scene.sceneName));
            emit sceneChanged(sceneId, QStringLiteral("deleted"));
        } else {
            result.status = QStringLiteral("not_found");
            result.errorCode = 0;
            result.errorMsg = QStringLiteral("场景不存在于本地");
        }

        results.append(result);
    }

    // 保存到本地存储
    if (!storagePath_.isEmpty()) {
        QString saveError;
        if (!saveToStorage(&saveError)) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("Failed to save scenes: %1").arg(saveError));
        }
    }

    return results;
}

// ==================== 场景状态 ====================

int SceneManager::sceneCount() const
{
    return scenes_.size();
}

bool SceneManager::hasScene(int sceneId) const
{
    return scenes_.contains(sceneId);
}

bool SceneManager::setSceneEnabled(int sceneId, bool enabled)
{
    if (!scenes_.contains(sceneId)) {
        return false;
    }

    scenes_[sceneId].status = enabled ? 0 : 1;
    scenes_[sceneId].updateTime = currentTimeString();
    scenes_[sceneId].version += 1;

    LOG_INFO(kLogSource,
             QStringLiteral("Scene %1: id=%2")
                 .arg(enabled ? "enabled" : "disabled")
                 .arg(sceneId));
    emit sceneChanged(sceneId, QStringLiteral("updated"));

    // 保存到本地存储
    if (!storagePath_.isEmpty()) {
        QString saveError;
        if (!saveToStorage(&saveError)) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("Failed to save scenes: %1").arg(saveError));
        }
    }

    return true;
}

// ==================== 场景执行 ====================

bool SceneManager::triggerScene(int sceneId)
{
    if (!scenes_.contains(sceneId)) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Scene not found: id=%1").arg(sceneId));
        return false;
    }

    const SceneConfig &scene = scenes_.value(sceneId);

    // 检查场景是否启用
    if (scene.status != 0) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Scene disabled: id=%1, name=%2")
                        .arg(sceneId).arg(scene.sceneName));
        return false;
    }

    // 执行场景动作
    executeSceneActions(scene);
    return true;
}

void SceneManager::updateDeviceValue(const QString &deviceCode, const QString &identifier,
                                      const QVariant &value)
{
    deviceValues_[deviceCode][identifier] = value;
}

// ==================== 存储 ====================

bool SceneManager::saveToStorage(QString *error)
{
    if (storagePath_.isEmpty()) {
        if (error) *error = QStringLiteral("存储路径未设置");
        return false;
    }

    // 确保目录存在
    QFileInfo fileInfo(storagePath_);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (error) *error = QStringLiteral("无法创建存储目录");
        return false;
    }

    // 构建JSON文档
    QJsonObject root;
    root[QStringLiteral("nextSceneId")] = nextSceneId_;

    QJsonArray scenesArr;
    for (auto it = scenes_.begin(); it != scenes_.end(); ++it) {
        scenesArr.append(it->toJson());
    }
    root[QStringLiteral("scenes")] = scenesArr;

    QJsonDocument doc(root);

    // 写入文件
    QFile file(storagePath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = QStringLiteral("无法打开存储文件: %1").arg(file.errorString());
        return false;
    }

    if (file.write(doc.toJson(QJsonDocument::Indented)) < 0) {
        if (error) *error = QStringLiteral("写入失败: %1").arg(file.errorString());
        return false;
    }

    LOG_DEBUG(kLogSource,
              QStringLiteral("Saved %1 scenes to %2").arg(scenes_.size()).arg(storagePath_));
    return true;
}

bool SceneManager::loadFromStorage(QString *error)
{
    if (storagePath_.isEmpty()) {
        if (error) *error = QStringLiteral("存储路径未设置");
        return false;
    }

    QFile file(storagePath_);
    if (!file.exists()) {
        // 文件不存在不是错误，首次运行时正常
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("无法打开存储文件: %1").arg(file.errorString());
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error) *error = QStringLiteral("JSON解析错误: %1").arg(parseError.errorString());
        return false;
    }

    if (!doc.isObject()) {
        if (error) *error = QStringLiteral("无效的JSON格式");
        return false;
    }

    QJsonObject root = doc.object();
    nextSceneId_ = root.value(QStringLiteral("nextSceneId")).toInt(1);

    scenes_.clear();
    if (root.contains(QStringLiteral("scenes")) && root[QStringLiteral("scenes")].isArray()) {
        QJsonArray scenesArr = root[QStringLiteral("scenes")].toArray();
        for (const auto &val : scenesArr) {
            if (val.isObject()) {
                SceneConfig scene = SceneConfig::fromJson(val.toObject());
                if (scene.id > 0) {
                    scenes_.insert(scene.id, scene);
                }
            }
        }
    }

    LOG_INFO(kLogSource,
             QStringLiteral("Loaded %1 scenes from %2").arg(scenes_.size()).arg(storagePath_));
    return true;
}

// ==================== 私有方法 ====================

void SceneManager::onConditionCheckTimer()
{
    checkAutoScenes();
}

void SceneManager::checkAutoScenes()
{
    for (auto it = scenes_.begin(); it != scenes_.end(); ++it) {
        const SceneConfig &scene = it.value();

        // 跳过禁用的场景
        if (scene.status != 0) {
            continue;
        }

        // 跳过手动场景
        if (scene.sceneType == SceneType::Manual) {
            continue;
        }

        // 检查生效时间
        if (!scene.isInEffectiveTime()) {
            continue;
        }

        // 评估条件
        if (evaluateSceneConditions(scene)) {
            executeSceneActions(scene);
        }
    }
}

bool SceneManager::evaluateSceneConditions(const SceneConfig &scene) const
{
    if (scene.conditions.isEmpty()) {
        return false; // 没有条件则不触发
    }

    bool matchAll = (scene.matchType == SceneMatchType::All);
    bool hasMatch = false;

    for (const auto &cond : scene.conditions) {
        // 获取设备当前值
        QVariant currentValue;
        if (deviceValues_.contains(cond.deviceCode)) {
            const auto &deviceProps = deviceValues_.value(cond.deviceCode);
            if (deviceProps.contains(cond.identifier)) {
                currentValue = deviceProps.value(cond.identifier);
            }
        }

        bool conditionMet = cond.evaluate(currentValue);

        if (matchAll) {
            // 需要全部条件满足
            if (!conditionMet) {
                return false;
            }
            hasMatch = true;
        } else {
            // 任一条件满足即可
            if (conditionMet) {
                return true;
            }
        }
    }

    return matchAll ? hasMatch : false;
}

void SceneManager::executeSceneActions(const SceneConfig &scene)
{
    LOG_INFO(kLogSource,
             QStringLiteral("Executing scene: id=%1, name=%2, actions=%3")
                 .arg(scene.id)
                 .arg(scene.sceneName)
                 .arg(scene.actions.size()));

    emit sceneTriggered(scene.id, scene.sceneName, scene.actions);

    for (const auto &action : scene.actions) {
        emit actionRequired(action.deviceCode, action.identifier, action.identifierValue);
    }
}

int SceneManager::allocateSceneId()
{
    return nextSceneId_++;
}

}  // namespace cloud
}  // namespace fanzhou
