/**
 * @file core_context.h
 * @brief 核心系统上下文
 *
 * 管理控制系统的核心组件。
 */

#ifndef FANZHOU_CORE_CONTEXT_H
#define FANZHOU_CORE_CONTEXT_H

#include <QHash>
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
 */
struct AutoStrategyState {
    AutoStrategyConfig config;
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
    comm::CanComm *canBus = nullptr;
    device::CanDeviceManager *canManager = nullptr;

    // 设备注册表：节点ID -> 设备
    QHash<quint8, device::RelayGd427 *> relays;

    // 设备分组：分组ID -> 节点ID列表
    QHash<int, QList<quint8>> deviceGroups;
    QHash<int, QString> groupNames;

    // CAN配置
    QString canInterface = QStringLiteral("can0");
    int canBitrate = 125000;
    bool tripleSampling = true;

    // 服务器配置
    quint16 rpcPort = 12345;

    // 分组管理
    bool createGroup(int groupId, const QString &name, QString *error = nullptr);
    bool deleteGroup(int groupId, QString *error = nullptr);
    bool addDeviceToGroup(int groupId, quint8 node, QString *error = nullptr);
    bool removeDeviceFromGroup(int groupId, quint8 node, QString *error = nullptr);

    // 控制队列
    EnqueueResult enqueueControl(quint8 node, quint8 channel,
                                  device::RelayProtocol::Action action,
                                  const QString &source, bool forceQueue = false);
    GroupControlStats queueGroupControl(int groupId, quint8 channel,
                                         device::RelayProtocol::Action action,
                                         const QString &source);
    QueueSnapshot queueSnapshot() const;
    ControlJobResult jobResult(quint64 jobId) const;
    device::RelayProtocol::Action parseAction(const QString &str, bool *ok = nullptr) const;

    // 策略管理
    QList<AutoStrategyState> strategyStates() const;
    bool setStrategyEnabled(int strategyId, bool enabled);
    bool triggerStrategy(int strategyId);

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
    void attachStrategiesForGroup(int groupId);
    void detachStrategiesForGroup(int groupId);
    int strategyIntervalMs(const AutoStrategyConfig &config) const;

    QList<AutoStrategyConfig> strategyConfigs_;
    QHash<int, QTimer *> strategyTimers_;

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
