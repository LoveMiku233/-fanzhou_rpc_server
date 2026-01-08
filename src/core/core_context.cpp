/**
 * @file core_context.cpp
 * @brief 核心系统上下文实现
 */

#include "core_context.h"
#include "comm/can_comm.h"
#include "config/system_settings.h"
#include "device/can/can_device_manager.h"
#include "device/can/relay_gd427.h"
#include "utils/logger.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>

#include <algorithm>

namespace fanzhou {
namespace core {

namespace {
const char *const kLogSource = "CoreContext";
const QString kErrUnknownNode = QStringLiteral("unknown node");
const QString kErrDeviceNotFound = QStringLiteral("device not found");
const QString kErrDeviceRejected = QStringLiteral("device rejected");
constexpr int kMaxChannelId = 3;  ///< 最大通道ID（0-3表示4个通道）
constexpr double kFloatCompareEpsilon = 0.001;  ///< 浮点数比较精度
}  // namespace

CoreContext::CoreContext(QObject *parent)
    : QObject(parent)
{
}

bool CoreContext::init()
{
    LOG_INFO(kLogSource, QStringLiteral("Initializing core context (default config)..."));

    if (!initSystemSettings()) {
        LOG_ERROR(kLogSource, QStringLiteral("Failed to initialize system settings"));
        return false;
    }
    if (!initCan()) {
        LOG_ERROR(kLogSource, QStringLiteral("Failed to initialize CAN bus"));
        return false;
    }
    if (!initDevices()) {
        LOG_ERROR(kLogSource, QStringLiteral("Failed to initialize devices"));
        return false;
    }
    initQueue();
    bindStrategies({});

    LOG_INFO(kLogSource, QStringLiteral("Core context initialization complete"));
    return true;
}

bool CoreContext::init(const CoreConfig &config)
{
    LOG_INFO(kLogSource, QStringLiteral("Initializing core context with config..."));
    LOG_DEBUG(kLogSource,
              QStringLiteral("RPC port: %1, CAN interface: %2, bitrate: %3")
                  .arg(config.main.rpcPort)
                  .arg(config.can.interface)
                  .arg(config.can.bitrate));

    rpcPort = config.main.rpcPort;
    canInterface = config.can.interface;
    canBitrate = config.can.bitrate;
    tripleSampling = config.can.tripleSampling;

    if (!initSystemSettings()) {
        LOG_ERROR(kLogSource, QStringLiteral("Failed to initialize system settings"));
        return false;
    }
    if (!initCan()) {
        LOG_ERROR(kLogSource, QStringLiteral("Failed to initialize CAN bus"));
        return false;
    }
    if (!initDevices(config)) {
        LOG_ERROR(kLogSource, QStringLiteral("Failed to initialize devices from config"));
        return false;
    }

    initQueue();
    bindStrategies(config.strategies);
    bindSensorStrategies(config.sensorStrategies);

    LOG_INFO(kLogSource, QStringLiteral("Core context initialization complete"));
    return true;
}

bool CoreContext::initSystemSettings()
{
    LOG_DEBUG(kLogSource, QStringLiteral("Initializing system settings..."));
    systemSettings = new config::SystemSettings(this);

    connect(systemSettings, &config::SystemSettings::commandOutput,
            this, [](const QString &output) {
        LOG_DEBUG("SystemSettings", QStringLiteral("[output] %1").arg(output));
    });

    connect(systemSettings, &config::SystemSettings::errorOccurred,
            this, [](const QString &error) {
        LOG_WARNING("SystemSettings", QStringLiteral("[error] %1").arg(error));
    });

    connect(systemSettings, &config::SystemSettings::candumpLine,
            this, [](const QString &line) {
        LOG_DEBUG("CANDump", line);
    });

    LOG_INFO(kLogSource,
             QStringLiteral("Setting CAN bitrate: interface=%1, bitrate=%2, tripleSampling=%3")
                 .arg(canInterface)
                 .arg(canBitrate)
                 .arg(tripleSampling));
    systemSettings->setCanBitrate(canInterface, canBitrate, tripleSampling);
    return true;
}

bool CoreContext::initCan()
{
    LOG_DEBUG(kLogSource, QStringLiteral("Initializing CAN bus..."));

    comm::CanConfig canConfig;
    canConfig.interface = canInterface;
    canConfig.canFd = false;

    canBus = new comm::CanComm(canConfig, this);
    connect(canBus, &comm::CanComm::errorOccurred, this, [](const QString &error) {
        LOG_ERROR("CAN", QStringLiteral("Error: %1").arg(error));
    });

    if (!canBus->open()) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("CAN open failed, RPC service will start but CAN methods will not work"));
    } else {
        LOG_INFO(kLogSource, QStringLiteral("CAN bus opened: %1").arg(canInterface));
    }

    canManager = new device::CanDeviceManager(canBus, this);
    LOG_DEBUG(kLogSource, QStringLiteral("CAN device manager created"));
    return true;
}

bool CoreContext::initDevices()
{
    LOG_DEBUG(kLogSource, QStringLiteral("Initializing devices (default mode)..."));

    const QList<quint8> nodes = {0x01};
    for (auto node : nodes) {
        auto *dev = new device::RelayGd427(node, canBus, this);
        dev->init();
        canManager->addDevice(dev);
        relays.insert(node, dev);
        LOG_INFO(kLogSource,
                 QStringLiteral("Relay device added: node=0x%1")
                     .arg(node, 2, 16, QChar('0')));
    }
    return true;
}

bool CoreContext::initDevices(const CoreConfig &config)
{
    LOG_DEBUG(kLogSource, QStringLiteral("Initializing devices from config..."));
    relays.clear();

    if (!config.devices.isEmpty()) {
        LOG_INFO(kLogSource,
                 QStringLiteral("Found %1 devices in config").arg(config.devices.size()));

        for (const auto &devConfig : config.devices) {
            const bool enabled = devConfig.params.value(QStringLiteral("enabled")).toBool(true);
            if (!enabled) {
                LOG_DEBUG(kLogSource,
                          QStringLiteral("Device '%1' disabled, skipping").arg(devConfig.name));
                continue;
            }

            if (devConfig.deviceType == device::DeviceTypeId::RelayGd427 &&
                devConfig.commType == device::CommTypeId::Can) {

                if (devConfig.nodeId < 1 || devConfig.nodeId > 255) {
                    LOG_WARNING(kLogSource,
                                QStringLiteral("Invalid node ID in config: %1, name=%2")
                                    .arg(devConfig.nodeId)
                                    .arg(devConfig.name));
                    continue;
                }

                const quint8 node = static_cast<quint8>(devConfig.nodeId);

                if (relays.contains(node)) {
                    LOG_WARNING(kLogSource,
                                QStringLiteral("Duplicate relay node in config: %1, skipping")
                                    .arg(static_cast<int>(node)));
                    continue;
                }

                auto *dev = new device::RelayGd427(node, canBus, this);
                dev->init();
                canManager->addDevice(dev);
                relays.insert(node, dev);
                deviceConfigs.insert(node, devConfig);

                LOG_INFO(kLogSource,
                         QStringLiteral("RelayGd427 added: node=0x%1, name=%2")
                             .arg(node, 2, 16, QChar('0'))
                             .arg(devConfig.name));
            } else {
                LOG_WARNING(kLogSource,
                            QStringLiteral("Unsupported device type/comm: %1/%2, name=%3")
                                .arg(static_cast<int>(devConfig.deviceType))
                                .arg(static_cast<int>(devConfig.commType))
                                .arg(devConfig.name));
            }
        }

        // Load device groups
        deviceGroups.clear();
        groupNames.clear();
        deviceGroups.reserve(config.groups.size());
        groupNames.reserve(config.groups.size());

        LOG_INFO(kLogSource,
                 QStringLiteral("Loading %1 device groups...").arg(config.groups.size()));

        for (const auto &grpConfig : config.groups) {
            if (!grpConfig.enabled) {
                LOG_DEBUG(kLogSource,
                          QStringLiteral("Device group '%1' disabled, skipping")
                              .arg(grpConfig.name));
                continue;
            }

            QList<quint8> nodes;
            nodes.reserve(grpConfig.deviceNodes.size());
            for (int nodeId : grpConfig.deviceNodes) {
                if (nodeId >= 1 && nodeId <= 255) {
                    nodes.append(static_cast<quint8>(nodeId));
                }
            }

            deviceGroups.insert(grpConfig.groupId, nodes);
            groupNames.insert(grpConfig.groupId, grpConfig.name);

            LOG_INFO(kLogSource,
                     QStringLiteral("Device group added: id=%1, name=%2, devices=%3")
                         .arg(grpConfig.groupId)
                         .arg(grpConfig.name)
                         .arg(nodes.size()));
        }

        return true;
    }

    LOG_WARNING(kLogSource, QStringLiteral("No devices configured (devices list empty)"));
    return true;
}

void CoreContext::initQueue()
{
    if (controlTimer_) return;
    controlTimer_ = new QTimer(this);
    controlTimer_->setInterval(kQueueTickMs);
    connect(controlTimer_, &QTimer::timeout, this, &CoreContext::processNextJob);
}

void CoreContext::startQueueProcessor()
{
    if (controlQueue_.isEmpty() || !controlTimer_) return;
    if (!controlTimer_->isActive()) {
        controlTimer_->start();
    }
}

ControlJobResult CoreContext::executeJob(const ControlJob &job)
{
    ControlJobResult result;
    result.finishedMs = QDateTime::currentMSecsSinceEpoch();

    auto *dev = relays.value(job.node, nullptr);
    if (!dev) {
        result.message = kErrDeviceNotFound;
        return result;
    }

    const bool ok = dev->control(job.channel, job.action);
    result.ok = ok;
    result.message = ok ? QStringLiteral("ok") : kErrDeviceRejected;
    jobResults_.insert(job.id, result);
    lastJobId_ = job.id;
    return result;
}

void CoreContext::processNextJob()
{
    if (processingQueue_) return;
    if (controlQueue_.isEmpty()) {
        if (controlTimer_) controlTimer_->stop();
        return;
    }

    processingQueue_ = true;
    const auto job = controlQueue_.dequeue();
    executeJob(job);
    processingQueue_ = false;

    if (controlQueue_.isEmpty() && controlTimer_) {
        controlTimer_->stop();
    }
}

EnqueueResult CoreContext::enqueueControl(quint8 node, quint8 channel,
                                            device::RelayProtocol::Action action,
                                            const QString &source, bool forceQueue)
{
    EnqueueResult result;
    if (!controlTimer_) initQueue();
    if (!relays.contains(node)) {
        result.error = kErrUnknownNode;
        return result;
    }

    ControlJob job;
    job.id = nextJobId_++;
    job.node = node;
    job.channel = channel;
    job.action = action;
    job.source = source;
    job.enqueuedMs = QDateTime::currentMSecsSinceEpoch();

    const bool immediate = controlQueue_.isEmpty() && !processingQueue_ && !forceQueue;
    result.accepted = true;
    result.jobId = job.id;

    if (immediate) {
        const auto jobResult = executeJob(job);
        result.executedImmediately = true;
        result.success = jobResult.ok;
        return result;
    }

    controlQueue_.enqueue(job);
    startQueueProcessor();
    return result;
}

GroupControlStats CoreContext::queueGroupControl(int groupId, quint8 channel,
                                                   device::RelayProtocol::Action action,
                                                   const QString &source)
{
    GroupControlStats stats;
    const QList<quint8> nodes = deviceGroups.value(groupId);
    stats.total = nodes.size();

    for (quint8 node : nodes) {
        const auto result = enqueueControl(node, channel, action, source, true);
        if (!result.accepted) {
            stats.missing++;
            continue;
        }
        stats.accepted++;
        stats.jobIds.append(result.jobId);
    }
    return stats;
}

QueueSnapshot CoreContext::queueSnapshot() const
{
    QueueSnapshot snapshot;
    snapshot.pending = controlQueue_.size();
    snapshot.active = controlTimer_ && controlTimer_->isActive();
    snapshot.lastJobId = lastJobId_;
    return snapshot;
}

ControlJobResult CoreContext::jobResult(quint64 jobId) const
{
    return jobResults_.value(jobId, ControlJobResult{});
}

bool CoreContext::createGroup(int groupId, const QString &name, QString *error)
{
    if (groupId < 1) {
        if (error) *error = QStringLiteral("groupId must be positive");
        return false;
    }
    if (deviceGroups.contains(groupId)) {
        if (error) *error = QStringLiteral("group exists");
        return false;
    }
    deviceGroups.insert(groupId, {});
    groupNames.insert(groupId, name);
    attachStrategiesForGroup(groupId);
    return true;
}

bool CoreContext::deleteGroup(int groupId, QString *error)
{
    if (!deviceGroups.contains(groupId)) {
        if (error) *error = QStringLiteral("group not found");
        return false;
    }
    detachStrategiesForGroup(groupId);
    deviceGroups.remove(groupId);
    groupNames.remove(groupId);
    return true;
}

bool CoreContext::addDeviceToGroup(int groupId, quint8 node, QString *error)
{
    if (!deviceGroups.contains(groupId)) {
        if (error) *error = QStringLiteral("group not found");
        return false;
    }
    if (!relays.contains(node)) {
        if (error) *error = QStringLiteral("device not found");
        return false;
    }
    QList<quint8> &devices = deviceGroups[groupId];
    if (!devices.contains(node)) {
        devices.append(node);
    }
    attachStrategiesForGroup(groupId);
    return true;
}

bool CoreContext::removeDeviceFromGroup(int groupId, quint8 node, QString *error)
{
    if (!deviceGroups.contains(groupId)) {
        if (error) *error = QStringLiteral("group not found");
        return false;
    }
    deviceGroups[groupId].removeAll(node);
    return true;
}

device::RelayProtocol::Action CoreContext::parseAction(const QString &str, bool *ok) const
{
    const QString a = str.trimmed().toLower();
    if (ok) *ok = true;

    if (a == QStringLiteral("stop") || a == QStringLiteral("0"))
        return device::RelayProtocol::Action::Stop;
    if (a == QStringLiteral("fwd") || a == QStringLiteral("forward") || a == QStringLiteral("1"))
        return device::RelayProtocol::Action::Forward;
    if (a == QStringLiteral("rev") || a == QStringLiteral("reverse") || a == QStringLiteral("2"))
        return device::RelayProtocol::Action::Reverse;

    if (ok) *ok = false;
    return device::RelayProtocol::Action::Stop;
}

int CoreContext::strategyIntervalMs(const AutoStrategyConfig &config) const
{
    return qMax(1, config.intervalSec) * kMsPerSec;
}

void CoreContext::bindStrategies(const QList<AutoStrategyConfig> &strategies)
{
    strategyConfigs_ = strategies;
    for (auto *timer : strategyTimers_) {
        if (!timer) continue;
        timer->stop();
        timer->deleteLater();
    }
    strategyTimers_.clear();

    for (auto it = deviceGroups.begin(); it != deviceGroups.end(); ++it) {
        attachStrategiesForGroup(it.key());
    }
}

void CoreContext::attachStrategiesForGroup(int groupId)
{
    for (const auto &config : strategyConfigs_) {
        if (!config.enabled || config.groupId != groupId) continue;

        bool okAction = false;
        const auto action = parseAction(config.action, &okAction);
        if (!okAction) continue;
        if (!deviceGroups.contains(config.groupId)) continue;

        QTimer *timer = strategyTimers_.value(config.strategyId, nullptr);
        if (!timer) {
            timer = new QTimer(this);
            timer->setInterval(strategyIntervalMs(config));

            const int strategyId = config.strategyId;
            const int targetGroupId = config.groupId;
            const quint8 channel = config.channel;
            const QString strategyName = config.name;

            connect(timer, &QTimer::timeout, this,
                    [this, targetGroupId, channel, strategyId, strategyName, action]() {
                const QString reason =
                    QStringLiteral("auto:%1")
                        .arg(strategyName.isEmpty() ? QString::number(strategyId) : strategyName);
                queueGroupControl(targetGroupId, channel, action, reason);
            });
            strategyTimers_.insert(config.strategyId, timer);
        } else {
            timer->setInterval(strategyIntervalMs(config));
        }

        if (!config.autoStart && timer->isActive()) {
            timer->stop();
        }
        if (config.autoStart && !timer->isActive()) {
            timer->start();
        }
    }
}

void CoreContext::detachStrategiesForGroup(int groupId)
{
    for (const auto &config : strategyConfigs_) {
        if (config.groupId != groupId) continue;
        if (auto *timer = strategyTimers_.value(config.strategyId, nullptr)) {
            timer->stop();
        }
    }
}

QList<AutoStrategyState> CoreContext::strategyStates() const
{
    QList<AutoStrategyState> list;
    for (const auto &config : strategyConfigs_) {
        AutoStrategyState state;
        state.config = config;
        auto *timer = strategyTimers_.value(config.strategyId, nullptr);
        state.attached = timer != nullptr && deviceGroups.contains(config.groupId);
        state.running = timer && timer->isActive();
        list.append(state);
    }
    return list;
}

bool CoreContext::setStrategyEnabled(int strategyId, bool enabled)
{
    bool found = false;
    int groupId = 0;
    for (auto &config : strategyConfigs_) {
        if (config.strategyId == strategyId) {
            config.enabled = enabled;
            found = true;
            groupId = config.groupId;
            break;
        }
    }
    if (!found) return false;

    if (enabled) {
        attachStrategiesForGroup(groupId);
    } else {
        if (auto *timer = strategyTimers_.value(strategyId, nullptr)) {
            timer->stop();
        }
    }
    return true;
}

bool CoreContext::triggerStrategy(int strategyId)
{
    for (const auto &config : strategyConfigs_) {
        if (config.strategyId != strategyId) continue;

        bool okAction = false;
        const auto action = parseAction(config.action, &okAction);
        if (!okAction) return false;
        if (!deviceGroups.contains(config.groupId)) return false;

        const auto stats = queueGroupControl(
            config.groupId, config.channel, action,
            QStringLiteral("manual-strategy:%1")
                .arg(config.name.isEmpty() ? QString::number(config.strategyId) : config.name));
        return stats.accepted > 0;
    }
    return false;
}

bool CoreContext::createStrategy(const AutoStrategyConfig &config, QString *error)
{
    // 检查策略ID是否已存在
    for (const auto &existing : strategyConfigs_) {
        if (existing.strategyId == config.strategyId) {
            if (error) *error = QStringLiteral("strategy id already exists");
            return false;
        }
    }

    // 检查分组是否存在
    if (!deviceGroups.contains(config.groupId)) {
        if (error) *error = QStringLiteral("group not found");
        return false;
    }

    // 添加策略配置
    strategyConfigs_.append(config);
    
    // 挂载策略
    if (config.enabled) {
        attachStrategiesForGroup(config.groupId);
    }

    LOG_INFO(kLogSource,
             QStringLiteral("Strategy created: id=%1, name=%2, groupId=%3")
                 .arg(config.strategyId)
                 .arg(config.name)
                 .arg(config.groupId));
    return true;
}

bool CoreContext::deleteStrategy(int strategyId, QString *error)
{
    // 查找并删除策略
    for (int i = 0; i < strategyConfigs_.size(); ++i) {
        if (strategyConfigs_[i].strategyId == strategyId) {
            // 停止定时器
            if (auto *timer = strategyTimers_.value(strategyId, nullptr)) {
                timer->stop();
                timer->deleteLater();
                strategyTimers_.remove(strategyId);
            }

            strategyConfigs_.removeAt(i);
            LOG_INFO(kLogSource, QStringLiteral("Strategy deleted: id=%1").arg(strategyId));
            return true;
        }
    }

    if (error) *error = QStringLiteral("strategy not found");
    return false;
}

QList<SensorStrategyState> CoreContext::sensorStrategyStates() const
{
    QList<SensorStrategyState> list;
    for (const auto &config : sensorStrategyConfigs_) {
        SensorStrategyState state;
        state.config = config;
        state.active = config.enabled && deviceGroups.contains(config.groupId);
        list.append(state);
    }
    return list;
}

bool CoreContext::createSensorStrategy(const SensorStrategyConfig &config, QString *error)
{
    // 检查策略ID是否已存在
    for (const auto &existing : sensorStrategyConfigs_) {
        if (existing.strategyId == config.strategyId) {
            if (error) *error = QStringLiteral("sensor strategy id already exists");
            return false;
        }
    }

    // 检查分组是否存在
    if (!deviceGroups.contains(config.groupId)) {
        if (error) *error = QStringLiteral("group not found");
        return false;
    }

    // 添加传感器策略配置
    sensorStrategyConfigs_.append(config);

    LOG_INFO(kLogSource,
             QStringLiteral("Sensor strategy created: id=%1, name=%2, sensor=%3, groupId=%4")
                 .arg(config.strategyId)
                 .arg(config.name)
                 .arg(config.sensorType)
                 .arg(config.groupId));
    return true;
}

bool CoreContext::deleteSensorStrategy(int strategyId, QString *error)
{
    for (int i = 0; i < sensorStrategyConfigs_.size(); ++i) {
        if (sensorStrategyConfigs_[i].strategyId == strategyId) {
            sensorStrategyConfigs_.removeAt(i);
            LOG_INFO(kLogSource, QStringLiteral("Sensor strategy deleted: id=%1").arg(strategyId));
            return true;
        }
    }

    if (error) *error = QStringLiteral("sensor strategy not found");
    return false;
}

bool CoreContext::setSensorStrategyEnabled(int strategyId, bool enabled)
{
    for (auto &config : sensorStrategyConfigs_) {
        if (config.strategyId == strategyId) {
            config.enabled = enabled;
            LOG_INFO(kLogSource,
                     QStringLiteral("Sensor strategy %1: id=%2")
                         .arg(enabled ? "enabled" : "disabled")
                         .arg(strategyId));
            return true;
        }
    }
    return false;
}

bool CoreContext::evaluateSensorCondition(const QString &condition, double value, double threshold) const
{
    if (condition == QStringLiteral("gt")) return value > threshold;
    if (condition == QStringLiteral("lt")) return value < threshold;
    if (condition == QStringLiteral("eq")) return qAbs(value - threshold) < kFloatCompareEpsilon;
    if (condition == QStringLiteral("gte")) return value >= threshold;
    if (condition == QStringLiteral("lte")) return value <= threshold;
    return false;
}

void CoreContext::checkSensorTriggers(const QString &sensorType, int sensorNode, double value)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (auto &config : sensorStrategyConfigs_) {
        // 检查是否匹配
        if (!config.enabled) continue;
        if (config.sensorType != sensorType) continue;
        if (config.sensorNode != sensorNode) continue;
        if (!deviceGroups.contains(config.groupId)) continue;

        // 检查冷却时间
        if (config.lastTriggerMs > 0) {
            const qint64 elapsed = now - config.lastTriggerMs;
            if (elapsed < config.cooldownSec * kMsPerSec) continue;
        }

        // 评估条件
        if (!evaluateSensorCondition(config.condition, value, config.threshold)) continue;

        // 触发控制
        bool okAction = false;
        const auto action = parseAction(config.action, &okAction);
        if (!okAction) continue;

        const QString reason = QStringLiteral("sensor:%1(%2)=%3")
            .arg(config.sensorType)
            .arg(sensorNode)
            .arg(value);

        // 控制指定通道或所有通道
        if (config.channel >= 0 && config.channel <= 3) {
            queueGroupControl(config.groupId, static_cast<quint8>(config.channel), action, reason);
        } else {
            // channel=-1 表示控制所有通道
            for (quint8 ch = 0; ch < 4; ++ch) {
                queueGroupControl(config.groupId, ch, action, reason);
            }
        }

        // 更新触发时间
        config.lastTriggerMs = now;

        LOG_INFO(kLogSource,
                 QStringLiteral("Sensor strategy triggered: id=%1, sensor=%2(%3)=%4, action=%5")
                     .arg(config.strategyId)
                     .arg(config.sensorType)
                     .arg(sensorNode)
                     .arg(value)
                     .arg(config.action));
    }
}

void CoreContext::bindSensorStrategies(const QList<SensorStrategyConfig> &strategies)
{
    sensorStrategyConfigs_ = strategies;
    LOG_INFO(kLogSource, QStringLiteral("Bound %1 sensor strategies").arg(strategies.size()));
}

// 继电器策略相关实现
int CoreContext::relayStrategyIntervalMs(const RelayStrategyConfig &config) const
{
    return qMax(1, config.intervalSec) * kMsPerSec;
}

QList<RelayStrategyConfig> CoreContext::relayStrategyStates() const
{
    return relayStrategyConfigs_;
}

void CoreContext::attachRelayStrategy(const RelayStrategyConfig &config)
{
    if (!config.enabled) return;
    if (!relays.contains(static_cast<quint8>(config.nodeId))) return;

    bool okAction = false;
    const auto action = parseAction(config.action, &okAction);
    if (!okAction) return;

    QTimer *timer = relayStrategyTimers_.value(config.strategyId, nullptr);
    if (!timer) {
        timer = new QTimer(const_cast<CoreContext*>(this));
        timer->setInterval(relayStrategyIntervalMs(config));

        const int strategyId = config.strategyId;
        const int targetNodeId = config.nodeId;
        const quint8 channel = config.channel;
        const QString strategyName = config.name;

        connect(timer, &QTimer::timeout, this,
                [this, targetNodeId, channel, strategyId, strategyName, action]() {
            const QString reason =
                QStringLiteral("auto-relay:%1")
                    .arg(strategyName.isEmpty() ? QString::number(strategyId) : strategyName);
            enqueueControl(static_cast<quint8>(targetNodeId), channel, action, reason);
        });
        relayStrategyTimers_.insert(config.strategyId, timer);
    } else {
        timer->setInterval(relayStrategyIntervalMs(config));
    }

    if (!config.autoStart && timer->isActive()) {
        timer->stop();
    }
    if (config.autoStart && !timer->isActive()) {
        timer->start();
    }
}

bool CoreContext::createRelayStrategy(const RelayStrategyConfig &config, QString *error)
{
    // 检查策略ID是否已存在
    for (const auto &existing : relayStrategyConfigs_) {
        if (existing.strategyId == config.strategyId) {
            if (error) *error = QStringLiteral("relay strategy id already exists");
            return false;
        }
    }

    // 检查设备是否存在
    if (!relays.contains(static_cast<quint8>(config.nodeId))) {
        if (error) *error = QStringLiteral("device not found");
        return false;
    }

    // 添加策略配置
    relayStrategyConfigs_.append(config);

    // 挂载策略
    if (config.enabled) {
        attachRelayStrategy(config);
    }

    LOG_INFO(kLogSource,
             QStringLiteral("Relay strategy created: id=%1, name=%2, nodeId=%3")
                 .arg(config.strategyId)
                 .arg(config.name)
                 .arg(config.nodeId));
    return true;
}

bool CoreContext::deleteRelayStrategy(int strategyId, QString *error)
{
    for (int i = 0; i < relayStrategyConfigs_.size(); ++i) {
        if (relayStrategyConfigs_[i].strategyId == strategyId) {
            // 停止定时器
            if (auto *timer = relayStrategyTimers_.value(strategyId, nullptr)) {
                timer->stop();
                timer->deleteLater();
                relayStrategyTimers_.remove(strategyId);
            }

            relayStrategyConfigs_.removeAt(i);
            LOG_INFO(kLogSource, QStringLiteral("Relay strategy deleted: id=%1").arg(strategyId));
            return true;
        }
    }

    if (error) *error = QStringLiteral("relay strategy not found");
    return false;
}

bool CoreContext::setRelayStrategyEnabled(int strategyId, bool enabled)
{
    for (auto &config : relayStrategyConfigs_) {
        if (config.strategyId == strategyId) {
            config.enabled = enabled;

            if (enabled) {
                attachRelayStrategy(config);
            } else {
                if (auto *timer = relayStrategyTimers_.value(strategyId, nullptr)) {
                    timer->stop();
                }
            }

            LOG_INFO(kLogSource,
                     QStringLiteral("Relay strategy %1: id=%2")
                         .arg(enabled ? "enabled" : "disabled")
                         .arg(strategyId));
            return true;
        }
    }
    return false;
}

// 传感器触发继电器策略相关实现
QList<SensorRelayStrategyConfig> CoreContext::sensorRelayStrategyStates() const
{
    return sensorRelayStrategyConfigs_;
}

bool CoreContext::createSensorRelayStrategy(const SensorRelayStrategyConfig &config, QString *error)
{
    // 检查策略ID是否已存在
    for (const auto &existing : sensorRelayStrategyConfigs_) {
        if (existing.strategyId == config.strategyId) {
            if (error) *error = QStringLiteral("sensor relay strategy id already exists");
            return false;
        }
    }

    // 检查设备是否存在
    if (!relays.contains(static_cast<quint8>(config.nodeId))) {
        if (error) *error = QStringLiteral("device not found");
        return false;
    }

    // 添加传感器触发继电器策略配置
    sensorRelayStrategyConfigs_.append(config);

    LOG_INFO(kLogSource,
             QStringLiteral("Sensor relay strategy created: id=%1, name=%2, sensor=%3, nodeId=%4")
                 .arg(config.strategyId)
                 .arg(config.name)
                 .arg(config.sensorType)
                 .arg(config.nodeId));
    return true;
}

bool CoreContext::deleteSensorRelayStrategy(int strategyId, QString *error)
{
    for (int i = 0; i < sensorRelayStrategyConfigs_.size(); ++i) {
        if (sensorRelayStrategyConfigs_[i].strategyId == strategyId) {
            sensorRelayStrategyConfigs_.removeAt(i);
            LOG_INFO(kLogSource, QStringLiteral("Sensor relay strategy deleted: id=%1").arg(strategyId));
            return true;
        }
    }

    if (error) *error = QStringLiteral("sensor relay strategy not found");
    return false;
}

bool CoreContext::setSensorRelayStrategyEnabled(int strategyId, bool enabled)
{
    for (auto &config : sensorRelayStrategyConfigs_) {
        if (config.strategyId == strategyId) {
            config.enabled = enabled;
            LOG_INFO(kLogSource,
                     QStringLiteral("Sensor relay strategy %1: id=%2")
                         .arg(enabled ? "enabled" : "disabled")
                         .arg(strategyId));
            return true;
        }
    }
    return false;
}

QStringList CoreContext::methodGroups() const
{
    return {QStringLiteral("rpc.*"), QStringLiteral("sys.*"), QStringLiteral("can.*"),
            QStringLiteral("relay.*"), QStringLiteral("group.*"),
            QStringLiteral("control.*"), QStringLiteral("auto.*"),
            QStringLiteral("device.*"), QStringLiteral("screen.*")};
}

bool CoreContext::addChannelToGroup(int groupId, quint8 node, int channel, QString *error)
{
    if (!deviceGroups.contains(groupId)) {
        if (error) *error = QStringLiteral("group not found");
        return false;
    }
    if (!relays.contains(node)) {
        if (error) *error = QStringLiteral("device not found");
        return false;
    }
    if (channel < 0 || channel > kMaxChannelId) {
        if (error) *error = QStringLiteral("invalid channel (0-%1)").arg(kMaxChannelId);
        return false;
    }

    // Add device to group if not already present
    QList<quint8> &devices = deviceGroups[groupId];
    if (!devices.contains(node)) {
        devices.append(node);
    }

    // Add channel to group channels list
    if (!groupChannels.contains(groupId)) {
        groupChannels.insert(groupId, {});
    }

    // Encode node+channel as unique key: node * 256 + channel
    const int channelKey = static_cast<int>(node) * 256 + channel;
    QList<int> &channels = groupChannels[groupId];
    if (!channels.contains(channelKey)) {
        channels.append(channelKey);
    }

    attachStrategiesForGroup(groupId);
    return true;
}

bool CoreContext::removeChannelFromGroup(int groupId, quint8 node, int channel, QString *error)
{
    if (!deviceGroups.contains(groupId)) {
        if (error) *error = QStringLiteral("group not found");
        return false;
    }

    const int channelKey = static_cast<int>(node) * 256 + channel;
    if (groupChannels.contains(groupId)) {
        groupChannels[groupId].removeAll(channelKey);
    }
    return true;
}

QList<int> CoreContext::getGroupChannels(int groupId) const
{
    return groupChannels.value(groupId, {});
}

bool CoreContext::addDevice(const DeviceConfig &config, QString *error)
{
    if (config.nodeId < 1 || config.nodeId > 255) {
        if (error) *error = QStringLiteral("invalid nodeId (1-255)");
        return false;
    }

    const quint8 node = static_cast<quint8>(config.nodeId);
    if (relays.contains(node)) {
        if (error) *error = QStringLiteral("device already exists");
        return false;
    }

    // Currently only support RelayGd427 device type
    if (config.deviceType == device::DeviceTypeId::RelayGd427 &&
        config.commType == device::CommTypeId::Can) {

        auto *dev = new device::RelayGd427(node, canBus, this);
        dev->init();
        if (canManager) {
            canManager->addDevice(dev);
        }
        relays.insert(node, dev);
        deviceConfigs.insert(node, config);

        LOG_INFO(kLogSource,
                 QStringLiteral("Device dynamically added: node=0x%1, name=%2")
                     .arg(node, 2, 16, QChar('0'))
                     .arg(config.name));
        return true;
    }

    // For sensor types, just register the config (no actual device driver yet)
    if (device::isSensorType(config.deviceType)) {
        deviceConfigs.insert(node, config);
        LOG_INFO(kLogSource,
                 QStringLiteral("Sensor device registered: node=0x%1, type=%2, name=%3")
                     .arg(node, 2, 16, QChar('0'))
                     .arg(device::deviceTypeToString(config.deviceType))
                     .arg(config.name));
        return true;
    }

    if (error) *error = QStringLiteral("unsupported device type");
    return false;
}

bool CoreContext::removeDevice(quint8 nodeId, QString *error)
{
    if (!relays.contains(nodeId) && !deviceConfigs.contains(nodeId)) {
        if (error) *error = QStringLiteral("device not found");
        return false;
    }

    // Remove from all groups
    for (auto it = deviceGroups.begin(); it != deviceGroups.end(); ++it) {
        it.value().removeAll(nodeId);
    }

    // Remove channel references
    for (auto it = groupChannels.begin(); it != groupChannels.end(); ++it) {
        QList<int> &channels = it.value();
        const int baseKey = static_cast<int>(nodeId) * 256;
        channels.erase(
            std::remove_if(channels.begin(), channels.end(),
                           [baseKey](int key) {
                               return key >= baseKey && key < baseKey + 256;
                           }),
            channels.end());
    }

    // Remove relay device if present
    if (relays.contains(nodeId)) {
        auto *dev = relays.take(nodeId);
        if (canManager) {
            canManager->removeDevice(dev);
        }
        dev->deleteLater();
    }

    deviceConfigs.remove(nodeId);

    LOG_INFO(kLogSource,
             QStringLiteral("Device removed: node=0x%1")
                 .arg(nodeId, 2, 16, QChar('0')));
    return true;
}

QList<DeviceConfig> CoreContext::listDevices() const
{
    return deviceConfigs.values();
}

DeviceConfig CoreContext::getDeviceConfig(quint8 nodeId) const
{
    return deviceConfigs.value(nodeId, DeviceConfig{});
}

ScreenConfig CoreContext::getScreenConfig() const
{
    return screenConfig;
}

bool CoreContext::setScreenConfig(const ScreenConfig &config, QString *error)
{
    if (config.brightness < 0 || config.brightness > 100) {
        if (error) *error = QStringLiteral("brightness must be 0-100");
        return false;
    }
    if (config.contrast < 0 || config.contrast > 100) {
        if (error) *error = QStringLiteral("contrast must be 0-100");
        return false;
    }
    if (config.sleepTimeoutSec < 0) {
        if (error) *error = QStringLiteral("sleepTimeoutSec must be >= 0");
        return false;
    }

    screenConfig = config;
    LOG_INFO(kLogSource,
             QStringLiteral("Screen config updated: brightness=%1, contrast=%2, enabled=%3")
                 .arg(config.brightness)
                 .arg(config.contrast)
                 .arg(config.enabled));
    return true;
}

// ===================== 配置保存/加载实现 =====================

/**
 * @brief 将当前运行时配置保存到文件
 * 
 * 这是解决"Web页面修改无法保存"问题的核心方法。
 * 
 * 为什么之前Web页面修改无法保存？
 * 原因：之前的实现中，通过RPC调用（如group.create、device.add等）
 * 创建的设备、分组、策略等只保存在内存中（存储在deviceGroups、
 * deviceConfigs、strategyConfigs_等成员变量中），服务重启后会丢失。
 * 
 * 解决方案：调用此方法将当前内存中的配置持久化到JSON配置文件。
 * 
 * @param path 配置文件路径，为空则使用configFilePath
 * @param error 错误信息输出
 * @return 成功返回true
 */
bool CoreContext::saveConfig(const QString &path, QString *error)
{
    const QString targetPath = path.isEmpty() ? configFilePath : path;
    
    if (targetPath.isEmpty()) {
        if (error) {
            *error = QStringLiteral("配置文件路径未设置，请先指定configFilePath或提供path参数");
        }
        LOG_WARNING(kLogSource, QStringLiteral("saveConfig failed: config file path not set"));
        return false;
    }

    // 构建配置对象
    CoreConfig config;
    
    // 主配置
    config.main.rpcPort = rpcPort;
    
    // CAN配置
    config.can.interface = canInterface;
    config.can.bitrate = canBitrate;
    config.can.tripleSampling = tripleSampling;
    
    // 屏幕配置
    config.screen = screenConfig;
    
    // 设备列表
    config.devices = deviceConfigs.values();
    
    // 设备分组
    config.groups.clear();
    for (auto it = deviceGroups.begin(); it != deviceGroups.end(); ++it) {
        DeviceGroupConfig grp;
        grp.groupId = it.key();
        grp.name = groupNames.value(it.key(), QString());
        grp.enabled = true;
        // 转换设备节点ID列表
        for (quint8 node : it.value()) {
            grp.deviceNodes.append(static_cast<int>(node));
        }
        // 保存分组通道
        grp.channels = groupChannels.value(it.key(), {});
        config.groups.append(grp);
    }
    
    // 定时策略
    config.strategies = strategyConfigs_;
    
    // 传感器策略
    config.sensorStrategies = sensorStrategyConfigs_;
    
    // 保存到文件
    QString saveError;
    if (!config.saveToFile(targetPath, &saveError)) {
        if (error) {
            *error = QStringLiteral("保存配置失败: %1").arg(saveError);
        }
        LOG_ERROR(kLogSource, QStringLiteral("saveConfig failed: %1").arg(saveError));
        return false;
    }
    
    LOG_INFO(kLogSource, QStringLiteral("配置已保存到: %1").arg(targetPath));
    return true;
}

/**
 * @brief 从文件重新加载配置
 * 
 * 注意：此操作会覆盖当前运行时配置，未保存的修改会丢失。
 * 
 * @param path 配置文件路径，为空则使用configFilePath
 * @param error 错误信息输出
 * @return 成功返回true
 */
bool CoreContext::reloadConfig(const QString &path, QString *error)
{
    const QString targetPath = path.isEmpty() ? configFilePath : path;
    
    if (targetPath.isEmpty()) {
        if (error) {
            *error = QStringLiteral("配置文件路径未设置");
        }
        return false;
    }

    CoreConfig config;
    QString loadError;
    if (!config.loadFromFile(targetPath, &loadError)) {
        if (error) {
            *error = QStringLiteral("加载配置失败: %1").arg(loadError);
        }
        LOG_ERROR(kLogSource, QStringLiteral("reloadConfig failed: %1").arg(loadError));
        return false;
    }
    
    // 注意：这里只更新部分配置，不重新初始化整个系统
    // 完整的重新初始化需要重启服务
    
    // 更新分组配置
    deviceGroups.clear();
    groupNames.clear();
    groupChannels.clear();
    
    for (const auto &grpConfig : config.groups) {
        if (!grpConfig.enabled) continue;
        
        QList<quint8> nodes;
        for (int nodeId : grpConfig.deviceNodes) {
            if (nodeId >= 1 && nodeId <= 255) {
                nodes.append(static_cast<quint8>(nodeId));
            }
        }
        deviceGroups.insert(grpConfig.groupId, nodes);
        groupNames.insert(grpConfig.groupId, grpConfig.name);
        groupChannels.insert(grpConfig.groupId, grpConfig.channels);
    }
    
    // 更新策略配置
    bindStrategies(config.strategies);
    bindSensorStrategies(config.sensorStrategies);
    
    // 更新屏幕配置
    screenConfig = config.screen;
    
    LOG_INFO(kLogSource, QStringLiteral("配置已重新加载: %1").arg(targetPath));
    return true;
}

/**
 * @brief 导出当前配置为JSON对象
 * 
 * 用于通过RPC获取当前运行时配置的完整内容。
 * 
 * @return 当前配置的JSON对象
 */
QJsonObject CoreContext::exportConfig() const
{
    QJsonObject root;
    
    // 主配置
    QJsonObject mainObj;
    mainObj[QStringLiteral("rpcPort")] = static_cast<int>(rpcPort);
    root[QStringLiteral("main")] = mainObj;
    
    // CAN配置
    QJsonObject canObj;
    canObj[QStringLiteral("interface")] = canInterface;
    canObj[QStringLiteral("bitrate")] = canBitrate;
    canObj[QStringLiteral("tripleSampling")] = tripleSampling;
    
    // CAN状态诊断信息（帮助诊断"CAN无法发送"的问题）
    if (canBus) {
        canObj[QStringLiteral("opened")] = canBus->isOpened();
        canObj[QStringLiteral("txQueueSize")] = canBus->txQueueSize();
    }
    root[QStringLiteral("can")] = canObj;
    
    // 设备列表
    QJsonArray devArr;
    for (const auto &dev : deviceConfigs) {
        QJsonObject obj;
        obj[QStringLiteral("nodeId")] = dev.nodeId;
        obj[QStringLiteral("name")] = dev.name;
        obj[QStringLiteral("type")] = static_cast<int>(dev.deviceType);
        obj[QStringLiteral("commType")] = static_cast<int>(dev.commType);
        obj[QStringLiteral("bus")] = dev.bus;
        if (!dev.params.isEmpty()) {
            obj[QStringLiteral("params")] = dev.params;
        }
        devArr.append(obj);
    }
    root[QStringLiteral("devices")] = devArr;
    
    // 设备分组
    QJsonArray groupArr;
    for (auto it = deviceGroups.begin(); it != deviceGroups.end(); ++it) {
        QJsonObject obj;
        obj[QStringLiteral("groupId")] = it.key();
        obj[QStringLiteral("name")] = groupNames.value(it.key(), QString());
        
        QJsonArray devNodes;
        for (quint8 node : it.value()) {
            devNodes.append(static_cast<int>(node));
        }
        obj[QStringLiteral("devices")] = devNodes;
        obj[QStringLiteral("deviceCount")] = it.value().size();
        
        // 导出分组通道
        const auto channels = groupChannels.value(it.key(), {});
        if (!channels.isEmpty()) {
            QJsonArray chArr;
            for (int ch : channels) {
                chArr.append(ch);
            }
            obj[QStringLiteral("channels")] = chArr;
        }
        
        groupArr.append(obj);
    }
    root[QStringLiteral("groups")] = groupArr;
    
    // 策略数量统计
    root[QStringLiteral("strategyCount")] = strategyConfigs_.size();
    root[QStringLiteral("sensorStrategyCount")] = sensorStrategyConfigs_.size();
    root[QStringLiteral("relayStrategyCount")] = relayStrategyConfigs_.size();
    
    // 屏幕配置
    QJsonObject screenObj;
    screenObj[QStringLiteral("brightness")] = screenConfig.brightness;
    screenObj[QStringLiteral("contrast")] = screenConfig.contrast;
    screenObj[QStringLiteral("enabled")] = screenConfig.enabled;
    screenObj[QStringLiteral("sleepTimeoutSec")] = screenConfig.sleepTimeoutSec;
    screenObj[QStringLiteral("orientation")] = screenConfig.orientation;
    root[QStringLiteral("screen")] = screenObj;
    
    // 配置文件路径
    root[QStringLiteral("configFilePath")] = configFilePath;
    
    return root;
}

}  // namespace core
}  // namespace fanzhou
