/**
 * @file scene_manager.h
 * @brief 场景管理器
 *
 * 负责场景的存储、同步和执行。
 * 支持本地场景缓存和云端场景同步。
 */

#ifndef FANZHOU_CLOUD_SCENE_MANAGER_H
#define FANZHOU_CLOUD_SCENE_MANAGER_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>

#include "scene_types.h"

namespace fanzhou {
namespace cloud {

/**
 * @brief 场景管理器
 *
 * 管理场景的生命周期，包括：
 * - 场景的CRUD操作
 * - 场景条件的评估和触发
 * - 场景数据的本地存储和云端同步
 */
class SceneManager : public QObject
{
    Q_OBJECT

public:
    explicit SceneManager(QObject *parent = nullptr);
    ~SceneManager() override;

    /**
     * @brief 初始化场景管理器
     * @param storagePath 场景存储文件路径
     * @return 成功返回true
     */
    bool init(const QString &storagePath = QString());

    // ==================== 场景CRUD操作 ====================

    /**
     * @brief 获取场景
     * @param sceneId 场景ID，0表示获取所有场景
     * @return 场景列表
     */
    QList<SceneConfig> getScenes(int sceneId = 0) const;

    /**
     * @brief 获取单个场景
     * @param sceneId 场景ID
     * @return 场景配置，不存在则返回无效场景
     */
    SceneConfig getScene(int sceneId) const;

    /**
     * @brief 添加或更新场景
     * @param scene 场景配置（如果id为0则为新增，云端会分配id）
     * @param error 错误信息输出
     * @return 场景处理结果
     */
    SceneProcessResult saveScene(const SceneConfig &scene, QString *error = nullptr);

    /**
     * @brief 删除场景
     * @param sceneId 场景ID
     * @param error 错误信息输出
     * @return 场景处理结果
     */
    SceneProcessResult deleteScene(int sceneId, QString *error = nullptr);

    /**
     * @brief 批量更新场景（用于云端同步）
     *
     * 云端下发场景时调用，会进行版本比对并更新本地场景。
     *
     * @param scenes 场景列表
     * @return 处理结果列表
     */
    QList<SceneProcessResult> syncScenes(const QList<SceneConfig> &scenes);

    /**
     * @brief 批量删除场景（用于云端同步）
     * @param sceneIds 场景ID列表
     * @param deleteReason 删除原因
     * @return 处理结果列表
     */
    QList<SceneProcessResult> deleteScenes(const QList<int> &sceneIds,
                                            const QString &deleteReason = QString());

    // ==================== 场景状态 ====================

    /**
     * @brief 获取场景数量
     */
    int sceneCount() const;

    /**
     * @brief 检查场景是否存在
     */
    bool hasScene(int sceneId) const;

    /**
     * @brief 设置场景启用状态
     * @param sceneId 场景ID
     * @param enabled true-启用(status=0)，false-禁用(status=1)
     * @return 成功返回true
     */
    bool setSceneEnabled(int sceneId, bool enabled);

    // ==================== 场景执行 ====================

    /**
     * @brief 手动触发场景
     * @param sceneId 场景ID
     * @return 成功返回true
     */
    bool triggerScene(int sceneId);

    /**
     * @brief 更新设备属性值（用于条件评估）
     * @param deviceCode 设备编码
     * @param identifier 属性标识
     * @param value 属性值
     */
    void updateDeviceValue(const QString &deviceCode, const QString &identifier,
                            const QVariant &value);

    // ==================== 存储 ====================

    /**
     * @brief 保存场景到本地存储
     * @param error 错误信息输出
     * @return 成功返回true
     */
    bool saveToStorage(QString *error = nullptr);

    /**
     * @brief 从本地存储加载场景
     * @param error 错误信息输出
     * @return 成功返回true
     */
    bool loadFromStorage(QString *error = nullptr);

signals:
    /**
     * @brief 场景变更信号
     * @param sceneId 场景ID
     * @param action 操作类型：created/updated/deleted
     */
    void sceneChanged(int sceneId, const QString &action);

    /**
     * @brief 场景触发信号
     * @param sceneId 场景ID
     * @param sceneName 场景名称
     * @param actions 动作列表
     */
    void sceneTriggered(int sceneId, const QString &sceneName,
                         const QList<SceneAction> &actions);

    /**
     * @brief 需要执行动作信号
     * @param deviceCode 设备编码
     * @param identifier 属性标识
     * @param value 属性值
     */
    void actionRequired(const QString &deviceCode, const QString &identifier,
                         const QVariant &value);

private slots:
    void onConditionCheckTimer();

private:
    void checkAutoScenes();
    bool evaluateSceneConditions(const SceneConfig &scene) const;
    void executeSceneActions(const SceneConfig &scene);
    int allocateSceneId();

    QHash<int, SceneConfig> scenes_;      ///< 场景存储：sceneId -> SceneConfig
    QString storagePath_;                  ///< 本地存储路径
    QTimer *conditionCheckTimer_ = nullptr; ///< 条件检查定时器

    // 设备属性值缓存：deviceCode -> (identifier -> value)
    QHash<QString, QHash<QString, QVariant>> deviceValues_;

    int nextSceneId_ = 1;  ///< 下一个可用的场景ID（用于本地新增）
};

}  // namespace cloud
}  // namespace fanzhou

#endif  // FANZHOU_CLOUD_SCENE_MANAGER_H
