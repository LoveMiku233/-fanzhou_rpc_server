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

namespace fanzhou {
namespace core {

namespace {
const char *const kLogSource = "CoreContext";
const QString kErrUnknownNode = QStringLiteral("unknown node");
const QString kErrDeviceNotFound = QStringLiteral("device not found");
const QString kErrDeviceRejected = QStringLiteral("device rejected");
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

QStringList CoreContext::methodGroups() const
{
    return {QStringLiteral("rpc.*"), QStringLiteral("sys.*"), QStringLiteral("can.*"),
            QStringLiteral("relay.*"), QStringLiteral("group.*"),
            QStringLiteral("control.*"), QStringLiteral("auto.*")};
}

}  // namespace core
}  // namespace fanzhou
