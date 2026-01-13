/**
 * @file mqtt_channel_manager.h
 * @brief MQTT多通道管理器
 *
 * 支持多MQTT通道连接，每个通道可配置不同的broker、topic等参数。
 * 当设备值有变化时，可通过指定通道上报数据。
 */

#ifndef MQTT_CHANNEL_MANAGER_H
#define MQTT_CHANNEL_MANAGER_H

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

namespace fanzhou {
namespace cloud {

class MqttClient;

/**
 * @brief MQTT通道配置
 */
struct MqttChannelConfig {
    int channelId = 0;              ///< 通道ID
    QString name;                    ///< 通道名称
    bool enabled = true;             ///< 是否启用
    QString broker;                  ///< Broker地址
    quint16 port = 1883;            ///< Broker端口
    QString clientId;               ///< 客户端ID
    QString username;               ///< 用户名（可选）
    QString password;               ///< 密码（可选）
    QString topicPrefix;            ///< 主题前缀
    int keepAliveSec = 60;          ///< 心跳间隔（秒）
    bool autoReconnect = true;      ///< 是否自动重连
    int reconnectIntervalSec = 5;   ///< 重连间隔（秒）
    int qos = 0;                    ///< 默认QoS级别

    /**
     * @brief 从JSON对象解析配置
     */
    static MqttChannelConfig fromJson(const QJsonObject &obj);

    /**
     * @brief 转换为JSON对象
     */
    QJsonObject toJson() const;
};

/**
 * @brief MQTT通道状态
 */
struct MqttChannelStatus {
    int channelId = 0;
    QString name;
    bool enabled = false;
    bool connected = false;
    QString broker;
    quint16 port = 0;
    qint64 lastConnectedMs = 0;
    qint64 lastMessageMs = 0;
    quint64 messagesSent = 0;
    quint64 messagesReceived = 0;
};

/**
 * @brief MQTT多通道管理器
 *
 * 管理多个MQTT通道连接，支持：
 * - 动态添加/删除通道
 * - 按通道ID或名称发布消息
 * - 设备值变化自动上报
 */
class MqttChannelManager : public QObject
{
    Q_OBJECT

public:
    explicit MqttChannelManager(QObject *parent = nullptr);
    ~MqttChannelManager() override;

    /**
     * @brief 添加MQTT通道
     * @param config 通道配置
     * @param error 错误信息输出
     * @return 成功返回true
     */
    bool addChannel(const MqttChannelConfig &config, QString *error = nullptr);

    /**
     * @brief 移除MQTT通道
     * @param channelId 通道ID
     * @param error 错误信息输出
     * @return 成功返回true
     */
    bool removeChannel(int channelId, QString *error = nullptr);

    /**
     * @brief 更新MQTT通道配置
     * @param config 新配置
     * @param error 错误信息输出
     * @return 成功返回true
     */
    bool updateChannel(const MqttChannelConfig &config, QString *error = nullptr);

    /**
     * @brief 连接指定通道
     * @param channelId 通道ID
     * @return 成功返回true
     */
    bool connectChannel(int channelId);

    /**
     * @brief 断开指定通道
     * @param channelId 通道ID
     */
    void disconnectChannel(int channelId);

    /**
     * @brief 连接所有已启用的通道
     */
    void connectAll();

    /**
     * @brief 断开所有通道
     */
    void disconnectAll();

    /**
     * @brief 向指定通道发布消息
     * @param channelId 通道ID
     * @param topic 主题（相对于topicPrefix）
     * @param payload 消息内容
     * @param qos QoS级别（-1使用通道默认值）
     * @return 成功返回true
     */
    bool publish(int channelId, const QString &topic,
                 const QByteArray &payload, int qos = -1);

    /**
     * @brief 向所有已连接通道发布消息
     * @param topic 主题（相对于topicPrefix）
     * @param payload 消息内容
     * @param qos QoS级别
     * @return 发布成功的通道数
     */
    int publishToAll(const QString &topic, const QByteArray &payload, int qos = 0);

    /**
     * @brief 在指定通道订阅主题
     * @param channelId 通道ID
     * @param topic 主题（相对于topicPrefix）
     * @param qos QoS级别
     * @return 成功返回true
     */
    bool subscribe(int channelId, const QString &topic, int qos = 0);

    /**
     * @brief 上报设备值变化
     * @param deviceNode 设备节点ID
     * @param channel 通道号
     * @param value 新值
     * @param oldValue 旧值
     */
    void reportDeviceValueChange(quint8 deviceNode, quint8 channel,
                                   const QJsonObject &value,
                                   const QJsonObject &oldValue = QJsonObject());

    /**
     * @brief 获取通道状态列表
     */
    QList<MqttChannelStatus> channelStatusList() const;

    /**
     * @brief 获取指定通道的配置
     */
    MqttChannelConfig getChannelConfig(int channelId) const;

    /**
     * @brief 获取所有通道配置
     */
    QList<MqttChannelConfig> allChannelConfigs() const;

    /**
     * @brief 检查通道是否存在
     */
    bool hasChannel(int channelId) const;

    /**
     * @brief 获取通道数量
     */
    int channelCount() const;

signals:
    /**
     * @brief 通道连接状态变化
     */
    void channelConnected(int channelId);
    void channelDisconnected(int channelId);

    /**
     * @brief 收到消息
     */
    void messageReceived(int channelId, const QString &topic, const QByteArray &payload);

    /**
     * @brief 发生错误
     */
    void errorOccurred(int channelId, const QString &error);

private slots:
    void onClientConnected();
    void onClientDisconnected();
    void onClientMessageReceived(const QByteArray &message, const QString &topic);

private:
    struct ChannelData {
        MqttChannelConfig config;
        MqttClient *client = nullptr;
        MqttChannelStatus status;
    };

    QHash<int, ChannelData> channels_;
    int nextChannelId_ = 1;

    QString buildFullTopic(const QString &prefix, const QString &topic) const;
};

}  // namespace cloud
}  // namespace fanzhou

#endif  // MQTT_CHANNEL_MANAGER_H
