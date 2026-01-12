/**
 * @file core_context.h
 * @brief 核心系统上下文
 *
 * 管理控制系统的核心组件。
 */

#ifndef FANZHOU_CORE_CONTEXT_H
#define FANZHOU_CORE_CONTEXT_H

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QQueue>
#include <QTimer>

#include "core_config.h"
#include "device/can/relay_protocol.h"

namespace fanzhou {

namespace config {
class SystemSettings;
}

namespace comm {
class CanComm;
}

namespace device {
class CanDeviceManager;
class RelayGd427;
}

namespace core {

/**
 * @brief 控制任务结构
 */
struct ControlJob {
    quint64 id = 0;
    quint8 node = 0;
    quint8 channel = 0;
    device::RelayProtocol::Action action = device::RelayProtocol::Action::Stop;
    QString source;
    qint64 enqueuedMs = 0;
};

/**
 * @brief 控制任务结果
 */
struct ControlJobResult {
    bool ok = false;
    QString message;
    qint64 finishedMs = 0;
};

/**
 * @brief 入队操作结果
 */
struct EnqueueResult {
    quint64 jobId = 0;
    bool accepted = false;
    bool executedImmediately = false;
    bool success = false;
    QString error;
};

/**
 * @brief 分组控制统计
 */
struct GroupControlStats {
    int total = 0;
    int accepted = 0;
    int missing = 0;
    QList<quint64> jobIds;
};

/**
 * @brief 队列状态快照
 */
struct QueueSnapshot {
    int pending = 0;
    bool active = false;
    quint64 lastJobId = 0;
};

/**
 * @brief 自动策略状态
 * 定时策略的运行时状态
 */
struct AutoStrategyState {
    AutoStrategyConfig config;
    bool attached = false;
    bool running = false;
};

/**
 * @brief 传感器策略状态
 * 传感器触发策略的运行时状态
 */
struct SensorStrategyState {
    SensorStrategyConfig config;
    bool active = false;  ///< 是否处于活动状态
};

/**
 * @brief 核心系统上下文
 *
 * 管理所有核心组件，包括系统设置、CAN总线、设备管理器和继电器设备。
 */
class CoreContext : public QObject
{
    Q_OBJECT

public:
    explicit CoreContext(QObject *parent = nullptr);

    /**
     * @brief 使用默认配置初始化
     * @return 成功返回true
     */
    bool init();

    /**
     * @brief 使用指定配置初始化
     * @param config 核心配置
     * @return 成功返回true
     */
    bool init(const CoreConfig &config);

    /**
     * @brief 获取RPC方法组名称
     * @return 方法组前缀列表
     */
    QStringList methodGroups() const;

    // 组件指针
    config::SystemSettings *systemSettings = nullptr;
    comm::CanComm *canBus = nullptr;
    device::CanDeviceManager *canManager = nullptr;

    // 设备注册表：节点ID -> 设备
    QHash<quint8, device::RelayGd427 *> relays;

    // 设备分组：分组ID -> 节点ID列表
    QHash<int, QList<quint8>> deviceGroups;
    QHash<int, QString> groupNames;
    QHash<int, QList<int>> groupChannels;  ///< 分组ID -> 指定通道列表

    // 设备配置记录（用于动态管理）
    QHash<quint8, DeviceConfig> deviceConfigs;

    // 屏幕配置
    ScreenConfig screenConfig;

    // CAN配置
    QString canInterface = QStringLiteral("can0");
    int canBitrate = 125000;
    bool tripleSampling = true;

    // 服务器配置
    quint16 rpcPort = 12345;

    // 配置文件路径（用于保存配置）
    QString configFilePath;

    // MQTT云平台配置
    bool mqttEnabled = false;
    QString mqttBroker;
    int mqttPort = 1883;
    QString mqttClientId;
    QString mqttUsername;
    QString mqttPassword;
    QString mqttTopic;
    bool mqttConnected = false;

    // 配置管理
    /**
     * @brief 将当前运行时配置保存到文件
     * 
     * 将当前内存中的设备、分组、策略等配置持久化保存到配置文件。
     * 这解决了"Web页面修改无法保存"的问题。
     * 
     * @param path 配置文件路径（可选，为空时使用configFilePath）
     * @param error 错误信息输出
     * @return 成功返回true
     */
    bool saveConfig(const QString &path = QString(), QString *error = nullptr);

    /**
     * @brief 从文件重新加载配置
     * 
     * 重新加载配置文件，会覆盖当前运行时配置。
     * 注意：这会丢失未保存的修改。
     * 
     * @param path 配置文件路径（可选，为空时使用configFilePath）
     * @param error 错误信息输出
     * @return 成功返回true
     */
    bool reloadConfig(const QString &path = QString(), QString *error = nullptr);

    /**
     * @brief 导出当前配置为JSON对象
     * 
     * 用于通过RPC获取当前配置的完整内容。
     * 
     * @return 当前配置的JSON对象
     */
    QJsonObject exportConfig() const;

    // 分组管理
    bool createGroup(int groupId, const QString &name, QString *error = nullptr);
    bool deleteGroup(int groupId, QString *error = nullptr);
    bool addDeviceToGroup(int groupId, quint8 node, QString *error = nullptr);
    bool removeDeviceFromGroup(int groupId, quint8 node, QString *error = nullptr);

    // 分组通道管理
    bool addChannelToGroup(int groupId, quint8 node, int channel, QString *error = nullptr);
    bool removeChannelFromGroup(int groupId, quint8 node, int channel, QString *error = nullptr);
    QList<int> getGroupChannels(int groupId) const;

    // 动态设备管理
    bool addDevice(const DeviceConfig &config, QString *error = nullptr);
    bool removeDevice(quint8 nodeId, QString *error = nullptr);
    QList<DeviceConfig> listDevices() const;
    DeviceConfig getDeviceConfig(quint8 nodeId) const;

    // 屏幕配置管理
    ScreenConfig getScreenConfig() const;
    bool setScreenConfig(const ScreenConfig &config, QString *error = nullptr);

    // 控制队列
    EnqueueResult enqueueControl(quint8 node, quint8 channel,
                                  device::RelayProtocol::Action action,
                                  const QString &source, bool forceQueue = false);
    GroupControlStats queueGroupControl(int groupId, quint8 channel,
                                         device::RelayProtocol::Action action,
                                         const QString &source);
    /**
     * @brief 按分组绑定的通道控制
     * 
     * 只控制分组中已绑定的特定通道，而不是控制整个节点的所有通道。
     * 这是为了解决"策略只能控制节点全部通道"的问题。
     * 
     * @param groupId 分组ID
     * @param action 控制动作
     * @param source 命令来源
     * @return 控制统计信息
     */
    GroupControlStats queueGroupBoundChannelsControl(int groupId,
                                                      device::RelayProtocol::Action action,
                                                      const QString &source);
    QueueSnapshot queueSnapshot() const;
    ControlJobResult jobResult(quint64 jobId) const;
    device::RelayProtocol::Action parseAction(const QString &str, bool *ok = nullptr) const;

    // 策略管理
    QList<AutoStrategyState> strategyStates() const;
    bool setStrategyEnabled(int strategyId, bool enabled);
    bool triggerStrategy(int strategyId);
    bool createStrategy(const AutoStrategyConfig &config, QString *error = nullptr);
    bool deleteStrategy(int strategyId, QString *error = nullptr);

    // 传感器策略管理
    QList<SensorStrategyState> sensorStrategyStates() const;
    bool createSensorStrategy(const SensorStrategyConfig &config, QString *error = nullptr);
    bool deleteSensorStrategy(int strategyId, QString *error = nullptr);
    bool setSensorStrategyEnabled(int strategyId, bool enabled);
    void checkSensorTriggers(const QString &sensorType, int sensorNode, double value);

    // 继电器策略管理（直接控制单个继电器，不需要分组）
    QList<RelayStrategyConfig> relayStrategyStates() const;
    bool createRelayStrategy(const RelayStrategyConfig &config, QString *error = nullptr);
    bool deleteRelayStrategy(int strategyId, QString *error = nullptr);
    bool setRelayStrategyEnabled(int strategyId, bool enabled);

    // 传感器触发继电器策略管理
    QList<SensorRelayStrategyConfig> sensorRelayStrategyStates() const;
    bool createSensorRelayStrategy(const SensorRelayStrategyConfig &config, QString *error = nullptr);
    bool deleteSensorRelayStrategy(int strategyId, QString *error = nullptr);
    bool setSensorRelayStrategyEnabled(int strategyId, bool enabled);

private:
    bool initSystemSettings();
    bool initCan();
    bool initDevices();
    bool initDevices(const CoreConfig &config);

    void initQueue();
    void startQueueProcessor();
    void processNextJob();
    ControlJobResult executeJob(const ControlJob &job);

    void bindStrategies(const QList<AutoStrategyConfig> &strategies);
    void bindSensorStrategies(const QList<SensorStrategyConfig> &strategies);
    void attachStrategiesForGroup(int groupId);
    void detachStrategiesForGroup(int groupId);
    void attachRelayStrategy(const RelayStrategyConfig &config);
    int strategyIntervalMs(const AutoStrategyConfig &config) const;
    int relayStrategyIntervalMs(const RelayStrategyConfig &config) const;
    bool evaluateSensorCondition(const QString &condition, double value, double threshold) const;

    QList<AutoStrategyConfig> strategyConfigs_;
    QList<SensorStrategyConfig> sensorStrategyConfigs_;  ///< 传感器策略配置列表
    QList<RelayStrategyConfig> relayStrategyConfigs_;    ///< 继电器策略配置列表
    QList<SensorRelayStrategyConfig> sensorRelayStrategyConfigs_;  ///< 传感器触发继电器策略列表
    QHash<int, QTimer *> strategyTimers_;
    QHash<int, QTimer *> relayStrategyTimers_;  ///< 继电器策略定时器

    QQueue<ControlJob> controlQueue_;
    QHash<quint64, ControlJobResult> jobResults_;
    QTimer *controlTimer_ = nullptr;
    bool processingQueue_ = false;
    quint64 nextJobId_ = 1;
    quint64 lastJobId_ = 0;

    static constexpr int kQueueTickMs = 10;
    static constexpr int kMsPerSec = 1000;
};

}  // namespace core
}  // namespace fanzhou

#endif  // FANZHOU_CORE_CONTEXT_H
