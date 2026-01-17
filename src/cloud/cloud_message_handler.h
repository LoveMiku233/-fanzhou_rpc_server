/**
 * @file cloud_message_handler.h
 * @brief 云平台消息处理器
 *
 * 处理泛舟云平台MQTT消息中的setting字段，
 * 实现场景的查询、同步、删除等操作。
 */

#ifndef FANZHOU_CLOUD_MESSAGE_HANDLER_H
#define FANZHOU_CLOUD_MESSAGE_HANDLER_H

#include <QJsonObject>
#include <QObject>

namespace fanzhou {
namespace cloud {

class SceneManager;

/**
 * @brief 云平台消息处理器
 *
 * 负责解析和处理云平台MQTT消息，特别是setting字段的处理。
 * 按照泛舟云平台大棚智能控制柜系统技术文档实现。
 */
class CloudMessageHandler : public QObject
{
    Q_OBJECT

public:
    explicit CloudMessageHandler(SceneManager *sceneManager, QObject *parent = nullptr);

    /**
     * @brief 处理接收到的MQTT消息
     * @param topic 消息主题
     * @param payload 消息内容
     * @return 需要回复的消息（空则不需要回复）
     */
    QByteArray handleMessage(const QString &topic, const QByteArray &payload);

    /**
     * @brief 处理setting字段
     * @param settingStr setting字段的JSON字符串
     * @return 响应的setting字段JSON对象
     */
    QJsonObject handleSetting(const QString &settingStr);

    /**
     * @brief 处理setting字段（JSON对象形式）
     * @param settingObj setting字段的JSON对象
     * @return 响应的setting字段JSON对象
     */
    QJsonObject handleSettingObject(const QJsonObject &settingObj);

    // ==================== 构建请求消息 ====================

    /**
     * @brief 构建场景查询请求
     * @param sceneId 场景ID，0表示获取所有场景
     * @return 请求消息的JSON字节数据
     */
    QByteArray buildGetScenesRequest(int sceneId = 0);

    /**
     * @brief 构建场景设置请求
     * @param scene 场景配置
     * @return 请求消息的JSON字节数据
     */
    QByteArray buildSetSceneRequest(const QJsonObject &scene);

    /**
     * @brief 构建场景删除请求
     * @param sceneIds 要删除的场景ID列表
     * @return 请求消息的JSON字节数据
     */
    QByteArray buildDeleteScenesRequest(const QList<int> &sceneIds);

signals:
    /**
     * @brief 需要发送MQTT消息
     * @param payload 消息内容
     */
    void sendMessage(const QByteArray &payload);

    /**
     * @brief 场景配置已更新（从云端同步）
     */
    void scenesUpdated();

private:
    /**
     * @brief 处理get请求（柜端请求拉取场景）
     */
    QJsonObject handleGetRequest(const QJsonObject &data, const QString &requestId);

    /**
     * @brief 处理get_response响应（云端返回场景数据）
     */
    void handleGetResponse(const QJsonObject &data, const QString &requestId,
                            const QJsonObject &error);

    /**
     * @brief 处理set请求（云端下发场景或柜端上报场景）
     */
    QJsonObject handleSetRequest(const QJsonObject &data, const QString &requestId);

    /**
     * @brief 处理set_response响应
     */
    void handleSetResponse(const QJsonObject &data, const QString &requestId);

    /**
     * @brief 处理delete请求（柜端请求删除场景）
     */
    QJsonObject handleDeleteRequest(const QJsonObject &data, const QString &requestId);

    /**
     * @brief 处理delete_response响应
     */
    void handleDeleteResponse(const QJsonObject &data, const QString &requestId);

    /**
     * @brief 处理delete_sync请求（云端主动删除同步）
     */
    QJsonObject handleDeleteSyncRequest(const QJsonObject &data, const QString &requestId);

    SceneManager *sceneManager_;
};

}  // namespace cloud
}  // namespace fanzhou

#endif  // FANZHOU_CLOUD_MESSAGE_HANDLER_H
