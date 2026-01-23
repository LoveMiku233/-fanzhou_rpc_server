/**
 * @file core_config.h
 * @brief 核心配置管理
 *
 * 定义系统的配置结构和加载/保存功能。
 */

#ifndef FANZHOU_CORE_CONFIG_H
#define FANZHOU_CORE_CONFIG_H

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QtGlobal>

#include "device/device_types.h"

namespace fanzhou {
namespace core {

/**
 * @brief 设备配置
 */
struct DeviceConfig {
    QString name;
    device::DeviceTypeId deviceType = device::DeviceTypeId::RelayGd427;
    device::CommTypeId commType = device::CommTypeId::Can;
    int nodeId = -1;
    QString bus = QStringLiteral("can0");
    QJsonObject params;
};

/**
 * @brief 继电器节点配置
 */
struct RelayNodeConfig {
    int nodeId = 1;
    bool enabled = true;
    int channels = 4;
    QString name;
};

/**
 * @brief CAN总线配置
 */
struct CanConfig {
    QString interface = QStringLiteral("can0");
    int bitrate = 250000;
    bool tripleSampling = true;
    bool canFd = false;
};

/**
 * @brief 日志配置
 */
struct LogConfig {
    bool logToConsole = true;
    bool logToFile = true;
    QString logFilePath = QStringLiteral("/var/log/fanzhou_core/core.log");
    int logLevel = 0;  // 0=Debug, 1=Info, 2=Warning, 3=Error, 4=Critical
};

/**
 * @brief 认证配置
 */
struct AuthConfig {
    bool enabled = false;                ///< 是否启用认证
    QString secret;                       ///< 认证密钥（用于生成token）
    QStringList allowedTokens;           ///< 预设的有效token列表
    int tokenExpireSec = 3600;           ///< Token过期时间（秒），0表示永不过期
    QStringList whitelist;               ///< IP白名单，在此列表中的IP不需要认证
    QStringList publicMethods = {        ///< 无需认证即可访问的公共方法
        QStringLiteral("rpc.ping"),
        QStringLiteral("rpc.list"),
        QStringLiteral("auth.login"),
        QStringLiteral("auth.verify"),
        QStringLiteral("auth.status")
    };
};

/**
 * @brief 主配置
 */
struct MainConfig {
    quint16 rpcPort = 12345;
    AuthConfig auth;                     ///< 认证配置
};

/**
 * @brief 设备组配置
 */
struct DeviceGroupConfig {
    int groupId = 0;
    QString name;
    QList<int> deviceNodes;
    QList<int> channels;  ///< 指定的通道列表，空表示所有通道
    bool enabled = true;
};

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
    quint8 nodeId = 0;          ///< 绑定的节点ID
    QString formatId;           ///< 使用的上传格式标识（决定JSON结构）
};

struct CloudMqttChannelBinding {
    int channelId = 0;          ///< MqttChannelManager 里的 channelId
    QString topic = "null";              ///< 该通道的发布 topic
    QList<CloudNodeBinding> nodes; ///< 这个通道绑定了哪些节点
};

/**
 * @brief 设备通道配置（用于分组）
 */
struct DeviceChannelRef {
    int nodeId = 0;    ///< 设备节点ID
    int channel = -1;  ///< 通道号，-1表示所有通道
};

/**
 * @brief 屏幕参数配置
 */
struct ScreenConfig {
    int brightness = 100;      ///< 亮度 (0-100)
    int contrast = 50;         ///< 对比度 (0-100)
    bool enabled = true;       ///< 屏幕开关
    int sleepTimeoutSec = 300; ///< 休眠超时（秒），0表示不休眠
    QString orientation = QStringLiteral("landscape");  ///< 屏幕方向
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


/**
  * @brief new 设备actions
  * @todo
  */
struct StrategyAction {
    QString identifier = "";        ///< "identifier": "node_1_sw1", 属性标识
    int identifierValue = 0.0f;  ///< "identifierValue": 1,	属性值
};

/**
  * @brief new 设备conditions
  * @todo
  */
struct StrategyCondition {
    QString identifier = "";        ///< "identifier": "airTemp", 属性标识
    double identifierValue = 0.0f;  ///< "identifierValue": 30,	属性值
    QString op = "";                ///< 条件操作符 eq等于 ne不等于 gt大于 lt小于 egt大等于 elt小等于
};


/**
  * @brief new 自动控制策略配置
  * @todo
  */
struct AutoStrategy {
    int strategyId = 0;
    int groupId = 0;                     ///< 绑定的分组ID
    int version = 1;                     ///< 场景版本号，通过版本号比对是否需要更新，每编辑一次，版本号+1
    bool enabled = true;                 ///< 是否启用
    QString type = "scene";              ///<
    QString updateTime = "";             ///< 最后更新时间
    QString strategyName = "";
    QString strategyType = "auto";       ///< auto自动场景，manual手动场景
    QString effectiveBeginTime = "";     ///< 生效开始时间
    QString effectiveEndTime = "";       ///< 生效结束时间
    qint8 matchType = 0;                 ///< 触发方式：0为需符合全部条件，1为任一条件符合即执行
    qint8 status = 0;                    ///< 场景状态：0正常，1关闭
    StrategyAction *actions;             ///< 执行设备
    StrategyCondition *conditions;       ///< 条件
};


/**
 * @brief old 自动控制策略配置
 * 定时策略，按间隔时间或每日固定时间自动触发分组控制
 */
struct AutoStrategyConfig {
    int strategyId = 0;          ///< 策略ID
    QString name;                 ///< 策略名称
    int groupId = 0;             ///< 绑定的分组ID
    qint8 channel = 0;           ///< 控制通道，-1表示所有通道
    QString action = QStringLiteral("stop");  ///< 控制动作
    int intervalSec = 60;        ///< 执行间隔（秒），仅当triggerType为interval时使用
    bool enabled = true;         ///< 是否启用
    bool autoStart = true;       ///< 是否自动启动
    QString triggerType = QStringLiteral("interval");  ///< 触发类型: "interval"(间隔执行) 或 "daily"(每日定时)
    QString dailyTime;           ///< 每日执行时间（格式: "HH:MM"），仅当triggerType为daily时使用
};


/**
 * @brief 核心系统配置
 *
 * 管理控制系统的所有配置，包括RPC设置、CAN总线参数、设备列表和分组。
 */
class CoreConfig
{
public:
    MainConfig main;
    CanConfig can;
    LogConfig log;
    ScreenConfig screen;  ///< 屏幕配置
    CloudUploadConfig cloudUpload;  ///< 云数据上传配置
    QList<DeviceConfig> devices;
    QList<DeviceGroupConfig> groups;
    QList<AutoStrategy> strategies;
    QList<MqttChannelConfig> mqttChannels;  ///< MQTT多通道配置列表

    /**
     * @brief 从文件加载配置
     * @param path 文件路径
     * @param error 错误信息输出
     * @return 成功返回true
     */
    bool loadFromFile(const QString &path, QString *error = nullptr);

    /**
     * @brief 保存配置到文件
     * @param path 文件路径
     * @param error 错误信息输出
     * @return 成功返回true
     */
    bool saveToFile(const QString &path, QString *error = nullptr) const;

    /**
     * @brief 创建默认配置
     * @return 默认配置对象
     */
    static CoreConfig makeDefault();
};

}  // namespace core
}  // namespace fanzhou

#endif  // FANZHOU_CORE_CONFIG_H
