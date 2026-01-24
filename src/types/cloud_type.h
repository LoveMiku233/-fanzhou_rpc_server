#ifndef CLOUD_CONFIG_H
#define CLOUD_CONFIG_H

#include <QString>
#include <QList>
#include <QJsonObject>

namespace fanzhou {
namespace core {


/**
 * @brief MQTT通道配置
 * 支持多MQTT通道连接
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

    QString topicControlSub;   ///< 订阅：云 → 本地 控制（可选）
    QString topicStrategySub;  ///< 订阅：云 → 本地 策略（可选）
    QString topicSettingSub;
    QString topicSettingPub;
    QString topicStatusPub;    ///< 发布：本地 → 云 状态（可选）
    QString topicEventPub;     ///< 发布：本地 → 云 事件（可选）

    /**
     * @brief 从JSON对象解析配置
     */
    static MqttChannelConfig fromJson(const QJsonObject &obj);

    /**
     * @brief 转换为JSON对象
     */
    QJsonObject toJson() const;
};


struct CloudNodeBinding {
    quint8 nodeId = 0;
    QString formatId;
};

struct CloudMqttChannelBinding {
    int channelId = 0;
    QString topic = "null";
    QList<CloudNodeBinding> nodes;
};

/**
 * @brief 云数据上传配置
 * 配置哪些数据上传到云端，以及上传模式
 */
struct CloudUploadConfig {
    bool enabled = true;                    ///< 是否启用云数据上传
    QString uploadMode = QStringLiteral("change");  ///< 上传模式: "interval"(定时上传) 或 "change"(变化上传)
    int intervalSec = 60;                    ///< 定时上传间隔（秒），仅当uploadMode为interval时使用

    // 上传的数据属性
    bool uploadChannelStatus = true;         ///< 是否上传通道状态（开/关/停止）
    bool uploadPhaseLoss = true;             ///< 是否上传缺相状态
    bool uploadCurrent = true;               ///< 是否上传电流值
    bool uploadOnlineStatus = true;          ///< 是否上传设备在线状态

    // 变化上传的阈值设置（仅当uploadMode为change时使用）
    double currentThreshold = 0.1;           ///< 电流变化阈值（安培），超过此值才上传
    bool statusChangeOnly = true;            ///< 状态变化时才上传（如开关状态改变）
    int minUploadIntervalSec = 5;           ///< 最小上传间隔（秒），防止频繁上传


    // device_bindings
    QList<CloudMqttChannelBinding> channelBindings;
};

}
}

#endif // CLOUD_CONFIG_H
