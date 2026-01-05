/**
 * @file core_context.h
 * @brief Core system context
 *
 * Manages the core components of the control system.
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
 * @brief Control job structure
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
 * @brief Control job result
 */
struct ControlJobResult {
    bool ok = false;
    QString message;
    qint64 finishedMs = 0;
};

/**
 * @brief Enqueue operation result
 */
struct EnqueueResult {
    quint64 jobId = 0;
    bool accepted = false;
    bool executedImmediately = false;
    bool success = false;
    QString error;
};

/**
 * @brief Group control statistics
 */
struct GroupControlStats {
    int total = 0;
    int accepted = 0;
    int missing = 0;
    QList<quint64> jobIds;
};

/**
 * @brief Queue status snapshot
 */
struct QueueSnapshot {
    int pending = 0;
    bool active = false;
    quint64 lastJobId = 0;
};

/**
 * @brief Auto strategy state
 */
struct AutoStrategyState {
    AutoStrategyConfig config;
    bool attached = false;
    bool running = false;
};

/**
 * @brief Core system context
 *
 * Manages all core components including system settings, CAN bus,
 * device manager, and relay devices.
 */
class CoreContext : public QObject
{
    Q_OBJECT

public:
    explicit CoreContext(QObject *parent = nullptr);

    /**
     * @brief Initialize with default configuration
     * @return True if successful
     */
    bool init();

    /**
     * @brief Initialize with specified configuration
     * @param config Core configuration
     * @return True if successful
     */
    bool init(const CoreConfig &config);

    /**
     * @brief Get RPC method group names
     * @return List of method group prefixes
     */
    QStringList methodGroups() const;

    // Component pointers
    config::SystemSettings *systemSettings = nullptr;
    comm::CanComm *canBus = nullptr;
    device::CanDeviceManager *canManager = nullptr;

    // Device registry: node ID -> device
    QHash<quint8, device::RelayGd427 *> relays;

    // Device groups: group ID -> node IDs
    QHash<int, QList<quint8>> deviceGroups;
    QHash<int, QString> groupNames;

    // CAN configuration
    QString canInterface = QStringLiteral("can0");
    int canBitrate = 125000;
    bool tripleSampling = true;

    // Server configuration
    quint16 rpcPort = 12345;

    // Group management
    bool createGroup(int groupId, const QString &name, QString *error = nullptr);
    bool deleteGroup(int groupId, QString *error = nullptr);
    bool addDeviceToGroup(int groupId, quint8 node, QString *error = nullptr);
    bool removeDeviceFromGroup(int groupId, quint8 node, QString *error = nullptr);

    // Control queue
    EnqueueResult enqueueControl(quint8 node, quint8 channel,
                                  device::RelayProtocol::Action action,
                                  const QString &source, bool forceQueue = false);
    GroupControlStats queueGroupControl(int groupId, quint8 channel,
                                         device::RelayProtocol::Action action,
                                         const QString &source);
    QueueSnapshot queueSnapshot() const;
    ControlJobResult jobResult(quint64 jobId) const;
    device::RelayProtocol::Action parseAction(const QString &str, bool *ok = nullptr) const;

    // Strategy management
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
