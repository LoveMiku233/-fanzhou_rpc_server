#ifndef CORE_CONTEXT_H
#define CORE_CONTEXT_H

#include <QObject>
#include <QHash>
#include <QQueue>
#include <QTimer>

#include "core/core_config.h"
#include "device/can/relay_can_protocol.h"

// 前向声明
class SystemSettings;
class CommCan;
class CanDeviceManager;
class RelayCanDeviceGD427;

struct CanConfig;

struct ControlJob {
    quint64 id = 0;
    quint8 node = 0;
    quint8 channel = 0;
    RelayCanProtocol::Action action = RelayCanProtocol::Action::Stop;
    QString source;
    qint64 enqueuedMs = 0;
};

struct ControlJobResult {
    bool ok = false;
    QString message;
    qint64 finishedMs = 0;
};

struct EnqueueResult {
    quint64 jobId = 0;
    bool accepted = false;
    bool executedImmediately = false;
    bool success = false;
    QString error;
};

struct GroupControlStats {
    int total = 0;
    int accepted = 0;
    int missing = 0;
    QList<quint64> jobIds;
};

struct QueueSnapshot {
    int pending = 0;
    bool active = false;
    quint64 lastJobId = 0;
};

struct AutoStrategyState {
    AutoStrategyConfig cfg;
    bool attached = false;
    bool running = false;
};

/**
 * @brief 核心上下文类
 * 
 * 管理大棚控制系统的核心组件，包括系统设置、CAN总线通讯、
 * 设备管理器以及继电器设备等
 */
class CoreContext : public QObject
{
    Q_OBJECT
public:
    explicit CoreContext(QObject* parent = nullptr);
    
    /**
     * @brief 使用默认配置初始化
     * @return 是否成功
     */
    bool init();
    
    /**
     * @brief 使用指定配置初始化
     * @param cfg 核心配置对象
     * @return 是否成功
     */
    bool init(const CoreConfig& cfg);
    
    /**
     * @brief 获取RPC方法组列表
     * @return 方法组名称列表
     */
    QStringList methodGroups() const;
    
    // 系统组件指针
    SystemSettings* sys = nullptr;          // 系统设置
    CommCan* canBus = nullptr;              // CAN总线通讯
    CanDeviceManager* canMgr = nullptr;     // CAN设备管理器

    // 设备注册表: 节点ID -> 设备
    QHash<quint8, RelayCanDeviceGD427*> relays;

    // 设备组: 组ID -> 节点ID列表
    QHash<int, QList<quint8>> deviceGroups;
    QHash<int, QString> groupNames;

    // CAN总线配置
    QString canIfname = "can0";             // CAN接口名称
    int canBitrate = 125000;                // 波特率
    bool tripleSampling = true;             // 三重采样

    // 服务器配置
    quint16 rpcPort = 12345;                // RPC服务端口

    // 组管理与控制队列接口
    bool createGroup(int groupId, const QString& name, QString* err=nullptr);
    bool deleteGroup(int groupId, QString* err=nullptr);
    bool addDeviceToGroup(int groupId, quint8 node, QString* err=nullptr);
    bool removeDeviceFromGroup(int groupId, quint8 node, QString* err=nullptr);

    EnqueueResult enqueueControl(quint8 node, quint8 ch, RelayCanProtocol::Action action, const QString& source, bool forceQueue=false);
    GroupControlStats queueGroupControl(int groupId, quint8 ch, RelayCanProtocol::Action action, const QString& source);
    QueueSnapshot queueSnapshot() const;
    ControlJobResult jobResult(quint64 jobId) const;
    RelayCanProtocol::Action parseAction(const QString& s, bool* ok=nullptr) const;

    QList<AutoStrategyState> strategyStates() const;
    bool setStrategyEnabled(int strategyId, bool enabled);
    bool triggerStrategy(int strategyId);

private:
    /**
     * @brief 初始化系统设置
     */
    bool initSystemSettings();
    
    /**
     * @brief 初始化CAN总线
     */
    bool initCan();

    /**
     * @brief 使用默认配置初始化设备
     */
    bool initDevices();
    
    /**
     * @brief 使用配置文件初始化设备
     * @param cfg 核心配置对象
     */
    bool initDevices(const CoreConfig& cfg);

    void initQueue();
    void startQueueProcessor();
    void processNextJob();
    ControlJobResult executeJob(const ControlJob& job);

    void bindStrategies(const QList<AutoStrategyConfig>& strategies);
    void attachStrategiesForGroup(int groupId);
    void detachStrategiesForGroup(int groupId);
    int strategyIntervalMs(const AutoStrategyConfig& cfg) const;

    QList<AutoStrategyConfig> strategyConfigs_;
    QHash<int, QTimer*> strategyTimers_; // strategyId -> timer

    QQueue<ControlJob> controlQueue_;
    QHash<quint64, ControlJobResult> jobResults_;
    QTimer* controlTimer_ = nullptr;
    bool processingQueue_ = false;
    quint64 nextJobId_ = 1;
    quint64 lastJobId_ = 0;
};

#endif // CORE_CONTEXT_H
