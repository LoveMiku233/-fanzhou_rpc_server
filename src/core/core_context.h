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
#include <QDateTime>

#include "core_config.h"
#include "device/can/relay_protocol.h"

namespace fanzhou {

namespace config {
class SystemSettings;
class SystemMonitor;
}

namespace cloud {
class MqttChannelManager;
namespace fanzhoucloud {
class CloudMessageHandler;
class CloudUploader;
class SettingService;
}
}

namespace comm {
class CanComm;
}

namespace device {
class CanDeviceManager;
class RelayGd427;
}

namespace core {

struct CloudNodeBinding;

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
    // 优化统计
    int originalFrameCount = 0;  ///< 优化前的CAN帧数
    int optimizedFrameCount = 0; ///< 优化后的CAN帧数
};

/**
 * @brief 批量控制请求项
 * 用于一次RPC调用控制多个节点/通道
 */
struct BatchControlItem {
    quint8 node = 0;
    quint8 channel = 0;
    device::RelayProtocol::Action action = device::RelayProtocol::Action::Stop;
};

/**
 * @brief 批量控制结果
 */
struct BatchControlResult {
    bool ok = true;
    int total = 0;           ///< 请求的总控制数
    int accepted = 0;        ///< 成功接受的控制数
    int failed = 0;          ///< 失败的控制数
    int originalFrames = 0;  ///< 优化前需要的CAN帧数
    int optimizedFrames = 0; ///< 优化后实际发送的CAN帧数
    QList<quint64> jobIds;
    QString error;
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
    AutoStrategy config;
    bool attached = false;
    bool running = false;
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
    config::SystemMonitor *systemMonitor = nullptr;      ///< 系统资源监控器
    comm::CanComm *canBus = nullptr;
    device::CanDeviceManager *canManager = nullptr;
    cloud::MqttChannelManager *mqttManager = nullptr;    ///< MQTT多通道管理器
    cloud::fanzhoucloud::CloudMessageHandler *cloudMessageHandler = nullptr; ///< 云平台消息处理器
    cloud::fanzhoucloud::CloudUploader *cloudUploader = nullptr;               ///<
    cloud::fanzhoucloud::SettingService *cloudSettingService = nullptr;

    //
//    QHash<quint8, QList<CloudNodeBinding>> cloud_binding_nodes;

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
    
    // 云数据上传配置
    CloudUploadConfig cloudUploadConfig;

    // CAN配置
    QString canInterface = QStringLiteral("can0");
    int canBitrate = 125000;
    bool tripleSampling = true;

    // 服务器配置
    quint16 rpcPort = 12345;

    // 认证配置
    AuthConfig authConfig;
    QHash<QString, qint64> validTokens;  ///< Token -> 过期时间戳(ms)，0表示永不过期

    // 配置文件路径（用于保存配置）
    QString configFilePath;

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
    
    /**
     * @brief 分组控制优化版 - 合并同一节点的多通道控制为单条CAN帧
     * 
     * 此方法会将同一节点的多个通道控制合并为一条 controlMulti CAN帧发送，
     * 显著减少CAN总线带宽占用。例如：控制4个通道原本需要4帧，现在只需1帧。
     * 
     * @param groupId 分组ID
     * @param channel 通道号（-1表示使用分组绑定的通道）
     * @param action 控制动作
     * @param source 命令来源
     * @return 控制统计信息（包含优化效果）
     */
    GroupControlStats queueGroupControlOptimized(int groupId, int channel,
                                                  device::RelayProtocol::Action action,
                                                  const QString &source);
    
    /**
     * @brief 批量控制 - 支持一次调用控制多个节点/通道
     * 
     * 此方法会自动将同一节点的多个通道控制合并为 controlMulti CAN帧，
     * 节约带宽，提高控制效率。
     * 
     * @param items 批量控制请求项列表
     * @param source 命令来源
     * @return 批量控制结果（包含优化统计）
     */
    BatchControlResult batchControl(const QList<BatchControlItem> &items,
                                     const QString &source);
    
    QueueSnapshot queueSnapshot() const;
    ControlJobResult jobResult(quint64 jobId) const;
    device::RelayProtocol::Action parseAction(const QString &str, bool *ok = nullptr) const;

    // 策略管理
    QList<AutoStrategyState> strategyStates() const;
    bool setStrategyEnabled(int strategyId, bool enabled);
    bool triggerStrategy(int strategyId);
    bool createStrategy(const AutoStrategy &config, QString *error = nullptr);
    bool deleteStrategy(int strategyId, QString *error = nullptr);
    //
    bool isInEffectiveTime(const AutoStrategy &s, const QDateTime &now) const;
    bool executeActions(const QList<StrategyAction> &actions);
    bool evaluateConditions(const QList<StrategyCondition> &conditions, qint8 matchType);
    // 认证管理
    /**
     * @brief 验证token是否有效
     * @param token 要验证的token
     * @return 有效返回true
     */
    bool verifyToken(const QString &token) const;

    /**
     * @brief 生成新的认证token
     * @param username 用户名（用于日志记录）
     * @param password 密码
     * @param outToken 输出生成的token
     * @param error 错误信息输出
     * @return 成功返回true
     */
    bool generateToken(const QString &username, const QString &password,
                       QString *outToken, QString *error = nullptr);

    /**
     * @brief 检查方法是否需要认证
     * @param method 方法名
     * @return 需要认证返回true
     */
    bool methodRequiresAuth(const QString &method) const;

    /**
     * @brief 检查IP地址是否在白名单中
     * @param ip IP地址
     * @return 在白名单中返回true
     */
    bool isIpWhitelisted(const QString &ip) const;

private:
    bool initSystemSettings();
    bool initCan();
    bool initDevices();
    bool initDevices(const CoreConfig &config);
    bool initMqtt();
    bool initStrategy();
    void initQueue();
    void startQueueProcessor();
    void processNextJob();
    void evaluateAllStrategies();
    ControlJobResult executeJob(const ControlJob &job);

    int strategyIntervalMs(const AutoStrategy &config) const;
    bool evaluateSensorCondition(const QString &condition, double value, double threshold) const;

    QList<AutoStrategy> strategys_;
    QTimer *autoStrategyScheduler_;

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
