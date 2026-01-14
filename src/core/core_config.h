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
    int bitrate = 125000;
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
 * @brief 自动控制策略配置
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
 * @brief 传感器触发策略配置
 * 当传感器数值满足阈值条件时，自动触发分组控制
 */
struct SensorStrategyConfig {
    int strategyId = 0;          ///< 策略ID
    QString name;                 ///< 策略名称
    QString sensorType;          ///< 传感器类型 (temperature/humidity/light/pressure/soil_moisture/co2)
    int sensorNode = 0;          ///< 传感器节点ID
    QString condition;           ///< 阈值条件 (gt/lt/eq/gte/lte)
    double threshold = 0.0;      ///< 阈值
    int groupId = 0;             ///< 绑定的分组ID
    int channel = 0;             ///< 控制通道，-1表示所有通道
    QString action = QStringLiteral("stop");  ///< 触发动作
    int cooldownSec = 60;        ///< 冷却时间（秒），防止频繁触发
    bool enabled = true;         ///< 是否启用
    qint64 lastTriggerMs = 0;    ///< 上次触发时间（内部使用）
};

/**
 * @brief 定时继电器策略配置
 * 按间隔时间自动触发单个继电器控制（不需要分组）
 */
struct RelayStrategyConfig {
    int strategyId = 0;          ///< 策略ID
    QString name;                 ///< 策略名称
    int nodeId = 0;              ///< 目标设备节点ID
    qint8 channel = 0;           ///< 控制通道，-1表示所有通道
    QString action = QStringLiteral("stop");  ///< 控制动作
    int intervalSec = 60;        ///< 执行间隔（秒）
    bool enabled = true;         ///< 是否启用
    bool autoStart = true;       ///< 是否自动启动
};

/**
 * @brief 传感器触发继电器策略配置
 * 当传感器数值满足阈值条件时，直接触发单个继电器控制
 */
struct SensorRelayStrategyConfig {
    int strategyId = 0;          ///< 策略ID
    QString name;                 ///< 策略名称
    QString sensorType;          ///< 传感器类型
    int sensorNode = 0;          ///< 传感器节点ID
    QString condition;           ///< 阈值条件
    double threshold = 0.0;      ///< 阈值
    int nodeId = 0;              ///< 目标设备节点ID
    int channel = 0;             ///< 控制通道
    QString action = QStringLiteral("stop");  ///< 触发动作
    int cooldownSec = 60;        ///< 冷却时间（秒）
    bool enabled = true;         ///< 是否启用
    qint64 lastTriggerMs = 0;    ///< 上次触发时间（内部使用）
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
    QList<DeviceConfig> devices;
    QList<DeviceGroupConfig> groups;
    QList<AutoStrategyConfig> strategies;
    QList<SensorStrategyConfig> sensorStrategies;  ///< 传感器触发策略列表
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
