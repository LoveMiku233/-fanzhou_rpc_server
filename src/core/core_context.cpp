/**
 * @file core_context.cpp
 * @brief 核心系统上下文实现
 */

#include "core_context.h"
#include "core_config.h"
#include "comm/can/can_comm.h"
#include "utils/system_settings.h"
#include "utils/system_monitor.h"
#include "cloud/mqtt/mqtt_channel_manager.h"
#include "cloud/fanzhoucloud/uploader.h"
#include "cloud/fanzhoucloud/message_handler.h"
#include "cloud/fanzhoucloud/setting_service.h"
#include "cloud/fanzhoucloud/parser.h"
#include "device/can/can_device_manager.h"
#include "device/can/relay_gd427.h"
#include "rpc/device_tcp_server.h"
#include "utils/logger.h"

#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QDir>
#include <QSet>

#include <algorithm>

namespace fanzhou {
namespace core {

namespace {
const char *const kLogSource = "CoreContext";
const QString kErrUnknownNode = QStringLiteral("unknown node");
const QString kErrDeviceNotFound = QStringLiteral("device not found");
const QString kErrDeviceRejected = QStringLiteral("device rejected");
constexpr int kMaxChannelId = 3;  ///< 最大通道ID（0-3表示4个通道）
constexpr double kFloatCompareEpsilon = 0.1;  ///< 浮点数比较精度
constexpr int kChannelKeyMultiplier = 256;  ///< 通道键编码乘数：channelKey = nodeId * 256 + channel
constexpr int kMinChannelsForMultiControl = 2;  ///< 触发controlMulti合并的最小通道数
constexpr qint64 kStrategyTriggerMinIntervalMs = 10000;  ///< 同一策略最小触发间隔
constexpr int kStrategyQueueBackpressureThreshold = 200; ///< 队列积压阈值（超过后暂停策略触发）
constexpr qint64 kBackpressureLogIntervalMs = 10000;     ///< 积压告警最小间隔

struct EffectiveTimeRange {
    bool valid = false;
    QTime begin;
    QTime end;
};

int makeChannelKey(quint8 node, quint8 channel)
{
    return static_cast<int>(node) * kChannelKeyMultiplier + static_cast<int>(channel);
}

quint8 channelKeyToNode(int key)
{
    return static_cast<quint8>(key / kChannelKeyMultiplier);
}

quint8 channelKeyToChannel(int key)
{
    return static_cast<quint8>(key % kChannelKeyMultiplier);
}

bool isChannelKeyForNode(int key, quint8 nodeId)
{
    const int baseKey = static_cast<int>(nodeId) * kChannelKeyMultiplier;
    return key >= baseKey && key < baseKey + kChannelKeyMultiplier;
}

bool isDebugLogEnabled()
{
    return Logger::instance().minLevel() <= LogLevel::Debug;
}

EffectiveTimeRange parseEffectiveTimeRangeCached(const QString &beginStr, const QString &endStr)
{
    static QHash<QString, EffectiveTimeRange> cache;
    const QString key = beginStr + QLatin1Char('|') + endStr;
    auto it = cache.find(key);
    if (it != cache.end()) {
        return it.value();
    }

    EffectiveTimeRange range;
    range.begin = QTime::fromString(beginStr, "HH:mm");
    range.end = QTime::fromString(endStr, "HH:mm");
    range.valid = range.begin.isValid() && range.end.isValid();
    cache.insert(key, range);
    return range;
}

int findStrategyIndexById(const QList<AutoStrategy> &strategies, int strategyId)
{
    for (int i = 0; i < strategies.size(); ++i) {
        if (strategies[i].strategyId == strategyId) {
            return i;
        }
    }
    return -1;
}

AutoStrategy *findMutableStrategyById(QList<AutoStrategy> &strategies, int strategyId)
{
    const int index = findStrategyIndexById(strategies, strategyId);
    if (index < 0) {
        return nullptr;
    }
    return &strategies[index];
}

void syncStrategyUpsertToCloud(cloud::fanzhoucloud::CloudMessageHandler *handler,
                               const AutoStrategy &strategy,
                               bool isUpdate)
{
    if (!handler) {
        return;
    }
    QJsonObject msg;
    msg.insert(QStringLiteral("method"), QStringLiteral("set"));
    if (handler->sendStrategyCommand(strategy, msg)) {
        return;
    }

    if (isUpdate) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Failed to sync updated strategy %1 (v%2) to cloud")
                        .arg(strategy.strategyId)
                        .arg(strategy.version));
        return;
    }

    LOG_WARNING(kLogSource,
                QStringLiteral("Failed to sync created strategy %1 to cloud")
                    .arg(strategy.strategyId));
}

void syncStrategyDeleteToCloud(cloud::fanzhoucloud::CloudMessageHandler *handler,
                               int strategyId,
                               const QString &strategyType,
                               qint64 nowMs)
{
    if (!handler) {
        return;
    }

    const int channelId = handler->getChannelId();
    if (channelId < 0) {
        return;
    }

    QJsonObject cloudMsg;
    cloudMsg.insert(QStringLiteral("data"), strategyId);
    cloudMsg.insert(QStringLiteral("type"), strategyType);
    cloudMsg.insert(QStringLiteral("requestId"),
                    QStringLiteral("local_del_%1_%2").arg(strategyId).arg(nowMs));
    cloudMsg.insert(QStringLiteral("timestamp"), nowMs);

    if (!handler->sendDeleteCommand(channelId, cloudMsg)) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Failed to sync delete to cloud for strategy %1")
                        .arg(strategyId));
        return;
    }

    LOG_DEBUG(kLogSource,
              QStringLiteral("Synced delete to cloud: strategy=%1, channel=%2")
                  .arg(strategyId)
                  .arg(channelId));
}

qint32 allocateAutoStrategyGroupId(const QHash<int, QList<quint8>> &deviceGroups)
{
    if (deviceGroups.isEmpty()) {
        return 1;
    }
    return static_cast<qint32>(deviceGroups.keys().last() + 1);
}

void setErrorIfPresent(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
}

void setBoolIfPresent(bool *flag, bool value)
{
    if (flag) {
        *flag = value;
    }
}

bool resolveTargetConfigPath(const QString &inputPath,
                             const QString &defaultPath,
                             const QString &emptyPathError,
                             QString *resolvedPath,
                             QString *error)
{
    const QString targetPath = inputPath.isEmpty() ? defaultPath : inputPath;
    if (targetPath.isEmpty()) {
        setErrorIfPresent(error, emptyPathError);
        return false;
    }
    if (resolvedPath) {
        *resolvedPath = targetPath;
    }
    return true;
}

bool ensureGroupExists(const QHash<int, QList<quint8>> &deviceGroups, int groupId, QString *error)
{
    if (deviceGroups.contains(groupId)) {
        return true;
    }
    setErrorIfPresent(error, QStringLiteral("group not found"));
    return false;
}

bool ensureRelayExists(const QHash<quint8, device::RelayGd427 *> &relays, quint8 node, QString *error)
{
    if (relays.contains(node)) {
        return true;
    }
    setErrorIfPresent(error, QStringLiteral("device not found"));
    return false;
}

bool ensureChannelInRange(int channel, QString *error)
{
    if (channel >= 0 && channel <= kMaxChannelId) {
        return true;
    }
    setErrorIfPresent(error, QStringLiteral("invalid channel (0-%1)").arg(kMaxChannelId));
    return false;
}

bool ensureNodeIdInRange(int nodeId, QString *error)
{
    if (nodeId >= 1 && nodeId <= 255) {
        return true;
    }
    setErrorIfPresent(error, QStringLiteral("invalid nodeId (1-255)"));
    return false;
}

bool isNodeIdInRange(int nodeId)
{
    return nodeId >= 1 && nodeId <= 255;
}

bool isValidCommType(device::CommTypeId commType)
{
    const int raw = static_cast<int>(commType);
    return raw >= static_cast<int>(device::CommTypeId::Serial) &&
           raw <= static_cast<int>(device::CommTypeId::TcpClient);
}

bool validateDeviceSchemaForSave(const QList<DeviceConfig> &devices,
                                 const QString &defaultCanBus,
                                 QString *error)
{
    QSet<int> nodeIds;
    for (const auto &dev : devices) {
        if (!isNodeIdInRange(dev.nodeId)) {
            setErrorIfPresent(
                error,
                QStringLiteral("invalid nodeId=%1 in devices (expected 1..255)").arg(dev.nodeId));
            return false;
        }
        if (nodeIds.contains(dev.nodeId)) {
            setErrorIfPresent(error,
                              QStringLiteral("duplicate nodeId=%1 in devices").arg(dev.nodeId));
            return false;
        }
        nodeIds.insert(dev.nodeId);

        if (!isValidCommType(dev.commType)) {
            setErrorIfPresent(
                error,
                QStringLiteral("invalid commType=%1 for nodeId=%2")
                    .arg(static_cast<int>(dev.commType))
                    .arg(dev.nodeId));
            return false;
        }

        const QString normalizedBus = dev.bus.trimmed();
        if (normalizedBus.isEmpty()) {
            setErrorIfPresent(error,
                              QStringLiteral("invalid bus for nodeId=%1: bus is empty")
                                  .arg(dev.nodeId));
            return false;
        }

        if (dev.commType == device::CommTypeId::Can &&
            defaultCanBus.trimmed().isEmpty()) {
            setErrorIfPresent(
                error,
                QStringLiteral("CAN device nodeId=%1 requires non-empty can.interface")
                    .arg(dev.nodeId));
            return false;
        }
    }
    return true;
}

bool isRelayGd427CommSupported(device::CommTypeId commType)
{
    return commType == device::CommTypeId::Can || commType == device::CommTypeId::TcpClient;
}

device::RelayGd427::TransportType relayTransportFromCommType(device::CommTypeId commType)
{
    return (commType == device::CommTypeId::TcpClient)
               ? device::RelayGd427::TransportType::TcpClient
               : device::RelayGd427::TransportType::Can;
}

device::RelayGd427 *createRelayGd427Device(quint8 node,
                                           device::CommTypeId commType,
                                           comm::CanComm *canBus,
                                           device::CanDeviceManager *canManager,
                                           rpc::DeviceTcpServer *deviceTcpServer,
                                           QObject *parent)
{
    const auto transport = relayTransportFromCommType(commType);
    auto *dev = new device::RelayGd427(node, canBus, transport, deviceTcpServer, parent);
    dev->init();
    if (transport == device::RelayGd427::TransportType::Can && canManager) {
        canManager->addDevice(dev);
    }
    return dev;
}

void registerRelayDevice(QHash<quint8, device::RelayGd427 *> &relays,
                         QHash<quint8, DeviceConfig> &deviceConfigs,
                         quint8 node,
                         device::RelayGd427 *dev,
                         const DeviceConfig &config)
{
    relays.insert(node, dev);
    deviceConfigs.insert(node, config);
}

void appendUniqueNodeToGroup(QHash<int, QList<quint8>> &deviceGroups, int groupId, quint8 node)
{
    QList<quint8> &devices = deviceGroups[groupId];
    if (!devices.contains(node)) {
        devices.append(node);
    }
}

void appendUniqueChannelKey(QHash<int, QList<int>> &groupChannels, int groupId, int channelKey)
{
    if (!groupChannels.contains(groupId)) {
        groupChannels.insert(groupId, {});
    }
    QList<int> &channels = groupChannels[groupId];
    if (!channels.contains(channelKey)) {
        channels.append(channelKey);
    }
}

void removeNodeFromAllGroups(QHash<int, QList<quint8>> &deviceGroups, quint8 nodeId)
{
    for (auto it = deviceGroups.begin(); it != deviceGroups.end(); ++it) {
        it.value().removeAll(nodeId);
    }
}

void removeNodeChannelsFromAllGroups(QHash<int, QList<int>> &groupChannels, quint8 nodeId)
{
    for (auto it = groupChannels.begin(); it != groupChannels.end(); ++it) {
        QList<int> &channels = it.value();
        channels.erase(
            std::remove_if(channels.begin(), channels.end(),
                           [nodeId](int key) {
                               return isChannelKeyForNode(key, nodeId);
                           }),
            channels.end());
    }
}

void detachAndDeleteRelayIfPresent(QHash<quint8, device::RelayGd427 *> &relays,
                                   const QHash<quint8, DeviceConfig> &deviceConfigs,
                                   device::CanDeviceManager *canManager,
                                   quint8 nodeId)
{
    if (!relays.contains(nodeId)) {
        return;
    }

    auto *dev = relays.take(nodeId);
    const auto cfg = deviceConfigs.value(nodeId);
    if (cfg.commType == device::CommTypeId::Can && canManager) {
        canManager->removeDevice(dev);
    }
    dev->deleteLater();
}

void loadSensorConfigsFromList(const QList<SensorNodeConfig> &sensors,
                               QHash<QString, SensorNodeConfig> &sensorConfigs)
{
    sensorConfigs.clear();
    for (const auto &cfg : sensors) {
        if (cfg.sensorId.isEmpty()) {
            LOG_WARNING(kLogSource, "skip sensor: empty sensorId");
            continue;
        }

        if (cfg.source == SensorSource::Mqtt) {
            if (cfg.mqttChannelId < 0 || cfg.jsonPath.isEmpty()) {
                LOG_WARNING(kLogSource,
                            QStringLiteral("skip mqtt sensor %1: invalid channel or jsonPath")
                                .arg(cfg.sensorId));
                continue;
            }
        }

        sensorConfigs.insert(cfg.sensorId, cfg);
        LOG_INFO(kLogSource,
                 QStringLiteral("load sensor ok: id=%1 source=%2 ch=%3 path=%4")
                     .arg(cfg.sensorId)
                     .arg(int(cfg.source))
                     .arg(cfg.mqttChannelId)
                     .arg(cfg.jsonPath));
    }
}

void loadRelayDevicesFromList(const QList<DeviceConfig> &deviceConfigsFromCoreConfig,
                              QHash<quint8, device::RelayGd427 *> &relays,
                              QHash<quint8, DeviceConfig> &deviceConfigs,
                              comm::CanComm *canBus,
                              device::CanDeviceManager *canManager,
                              rpc::DeviceTcpServer *deviceTcpServer,
                              QObject *parent)
{
    LOG_INFO(kLogSource,
             QStringLiteral("Found %1 devices in config")
                 .arg(deviceConfigsFromCoreConfig.size()));
    int canRelayCount = 0;
    int tcpRelayCount = 0;

    for (const auto &devConfig : deviceConfigsFromCoreConfig) {
        const bool enabled = devConfig.params.value(QStringLiteral("enabled")).toBool(true);
        if (!enabled) {
            LOG_DEBUG(kLogSource,
                      QStringLiteral("Device '%1' disabled, skipping").arg(devConfig.name));
            continue;
        }

        if (devConfig.deviceType != device::DeviceTypeId::RelayGd427 ||
            !isRelayGd427CommSupported(devConfig.commType)) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("Unsupported device type/comm: %1/%2, name=%3")
                            .arg(static_cast<int>(devConfig.deviceType))
                            .arg(static_cast<int>(devConfig.commType))
                            .arg(devConfig.name));
            continue;
        }

        if (!isNodeIdInRange(devConfig.nodeId)) {
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

        auto *dev = createRelayGd427Device(node, devConfig.commType, canBus, canManager,
                                           deviceTcpServer, parent);
        registerRelayDevice(relays, deviceConfigs, node, dev, devConfig);
        if (devConfig.commType == device::CommTypeId::Can) {
            ++canRelayCount;
        } else if (devConfig.commType == device::CommTypeId::TcpClient) {
            ++tcpRelayCount;
        }

        LOG_INFO(kLogSource,
                 QStringLiteral("RelayGd427 added: node=0x%1, name=%2, comm=%3")
                     .arg(node, 2, 16, QChar('0'))
                     .arg(devConfig.name)
                     .arg(device::commTypeToString(devConfig.commType)));
    }

    LOG_INFO(kLogSource,
             QStringLiteral("Relay topology loaded: CAN=%1, TCP=%2, total=%3")
                 .arg(canRelayCount)
                 .arg(tcpRelayCount)
                 .arg(canRelayCount + tcpRelayCount));
}

void resetGroupMappings(QHash<int, QList<quint8>> &deviceGroups,
                        QHash<int, QString> &groupNames,
                        QHash<int, QString> &groupSpecialIds,
                        QHash<int, bool> &groupCanOptimizeFrame,
                        QHash<int, QList<int>> &groupChannels,
                        int reserveSize)
{
    deviceGroups.clear();
    groupNames.clear();
    groupSpecialIds.clear();
    groupCanOptimizeFrame.clear();
    groupChannels.clear();
    deviceGroups.reserve(reserveSize);
    groupNames.reserve(reserveSize);
    groupSpecialIds.reserve(reserveSize);
    groupCanOptimizeFrame.reserve(reserveSize);
    groupChannels.reserve(reserveSize);
}

void loadGroupConfigsFromList(const QList<DeviceGroupConfig> &groups,
                              QHash<int, QList<quint8>> &deviceGroups,
                              QHash<int, QString> &groupNames,
                              QHash<int, QString> &groupSpecialIds,
                              QHash<int, bool> &groupCanOptimizeFrame,
                              QHash<int, QList<int>> &groupChannels)
{
    resetGroupMappings(deviceGroups, groupNames, groupSpecialIds, groupCanOptimizeFrame, groupChannels, groups.size());

    LOG_INFO(kLogSource,
             QStringLiteral("Loading %1 device groups...").arg(groups.size()));

    for (const auto &grpConfig : groups) {
        if (!grpConfig.enabled) {
            LOG_DEBUG(kLogSource,
                      QStringLiteral("Device group '%1' disabled, skipping")
                          .arg(grpConfig.name));
            continue;
        }

        QList<quint8> nodes;
        nodes.reserve(grpConfig.deviceNodes.size());
        for (int nodeId : grpConfig.deviceNodes) {
            if (isNodeIdInRange(nodeId)) {
                nodes.append(static_cast<quint8>(nodeId));
            }
        }

        deviceGroups.insert(grpConfig.groupId, nodes);
        groupNames.insert(grpConfig.groupId, grpConfig.name);
        if (!grpConfig.specialId.isEmpty()) {
            groupSpecialIds.insert(grpConfig.groupId, grpConfig.specialId);
        }
        groupCanOptimizeFrame.insert(grpConfig.groupId, grpConfig.canOptimizeFrame);
        if (!grpConfig.channels.isEmpty()) {
            groupChannels.insert(grpConfig.groupId, grpConfig.channels);
        }

        LOG_INFO(kLogSource,
                 QStringLiteral("Device group added: id=%1, name=%2, devices=%3, channels=%4")
                     .arg(grpConfig.groupId)
                     .arg(grpConfig.name)
                     .arg(nodes.size())
                     .arg(grpConfig.channels.size()));
    }
}

bool isPublicMethodPatternMatch(const QString &publicMethodPattern, const QString &method)
{
    if (publicMethodPattern == method) {
        return true;
    }
    if (!publicMethodPattern.endsWith(QStringLiteral(".*"))) {
        return false;
    }

    const QString prefix = publicMethodPattern.left(publicMethodPattern.length() - 1);
    return method.startsWith(prefix);
}

bool isLoopbackWhitelistMatch(const QString &whitelistedIp, const QString &ip)
{
    return whitelistedIp == QStringLiteral("localhost") &&
           (ip == QStringLiteral("127.0.0.1") || ip == QStringLiteral("::1"));
}

QList<DeviceGroupConfig> buildGroupConfigsFromRuntime(
    const QHash<int, QList<quint8>> &deviceGroups,
    const QHash<int, QString> &groupNames,
    const QHash<int, QString> &groupSpecialIds,
    const QHash<int, bool> &groupCanOptimizeFrame,
    const QHash<int, QList<int>> &groupChannels)
{
    QList<DeviceGroupConfig> groups;
    groups.reserve(deviceGroups.size());

    QList<int> groupIds = deviceGroups.keys();
    std::sort(groupIds.begin(), groupIds.end());
    for (int groupId : groupIds) {
        DeviceGroupConfig grp;
        grp.groupId = groupId;
        grp.name = groupNames.value(groupId, QString());
        grp.specialId = groupSpecialIds.value(groupId, QString());
        grp.canOptimizeFrame = groupCanOptimizeFrame.value(groupId, true);
        grp.enabled = true;
        const QList<quint8> nodes = deviceGroups.value(groupId);
        for (quint8 node : nodes) {
            grp.deviceNodes.append(static_cast<int>(node));
        }
        grp.channels = groupChannels.value(groupId, {});
        groups.append(grp);
    }
    return groups;
}

QJsonArray toJsonIntArray(const QList<int> &values)
{
    QJsonArray arr;
    for (int value : values) {
        arr.append(value);
    }
    return arr;
}

QJsonArray buildExportGroupArray(const QHash<int, QList<quint8>> &deviceGroups,
                                 const QHash<int, QString> &groupNames,
                                 const QHash<int, QString> &groupSpecialIds,
                                 const QHash<int, bool> &groupCanOptimizeFrame,
                                 const QHash<int, QList<int>> &groupChannels)
{
    QJsonArray groupArr;
    QList<int> groupIds = deviceGroups.keys();
    std::sort(groupIds.begin(), groupIds.end());
    for (int groupId : groupIds) {
        QJsonObject obj;
        obj[QStringLiteral("groupId")] = groupId;
        obj[QStringLiteral("name")] = groupNames.value(groupId, QString());
        const QString specialId = groupSpecialIds.value(groupId, QString());
        if (!specialId.isEmpty()) {
            obj[QStringLiteral("specialId")] = specialId;
        }
        obj[QStringLiteral("canOptimizeFrame")] = groupCanOptimizeFrame.value(groupId, true);

        QJsonArray devNodes;
        const QList<quint8> nodes = deviceGroups.value(groupId);
        for (quint8 node : nodes) {
            devNodes.append(static_cast<int>(node));
        }
        obj[QStringLiteral("devices")] = devNodes;
        obj[QStringLiteral("deviceCount")] = nodes.size();

        const QList<int> channels = groupChannels.value(groupId, {});
        if (!channels.isEmpty()) {
            obj[QStringLiteral("channels")] = toJsonIntArray(channels);
        }

        groupArr.append(obj);
    }
    return groupArr;
}

QJsonObject buildAuthExportObject(const AuthConfig &authConfig)
{
    QJsonObject authObj;
    authObj[QStringLiteral("enabled")] = authConfig.enabled;
    authObj[QStringLiteral("tokenExpireSec")] = authConfig.tokenExpireSec;
    authObj[QStringLiteral("whitelistCount")] = authConfig.whitelist.size();
    authObj[QStringLiteral("publicMethodsCount")] = authConfig.publicMethods.size();
    authObj[QStringLiteral("allowedTokensCount")] = authConfig.allowedTokens.size();
    return authObj;
}

QJsonObject buildMainExportObject(const MainConfig &mainConfig, const AuthConfig &authConfig)
{
    QJsonObject mainObj;
    mainObj[QStringLiteral("rpcPort")] = static_cast<int>(mainConfig.rpcPort);
    mainObj[QStringLiteral("auth")] = buildAuthExportObject(authConfig);
    return mainObj;
}

QJsonObject buildCanExportObject(const CanConfig &canConfig, const comm::CanComm *canBus)
{
    QJsonObject canObj;
    canObj[QStringLiteral("interface")] = canConfig.interface;
    canObj[QStringLiteral("bitrate")] = canConfig.bitrate;
    canObj[QStringLiteral("tripleSampling")] = canConfig.tripleSampling;
    canObj[QStringLiteral("restartMs")] = canConfig.restartMs;
    if (canBus) {
        canObj[QStringLiteral("opened")] = canBus->isOpened();
        canObj[QStringLiteral("txQueueSize")] = canBus->txQueueSize();
    }
    return canObj;
}

QJsonArray buildDeviceExportArray(const QHash<quint8, DeviceConfig> &deviceConfigs)
{
    QJsonArray devArr;
    QList<quint8> nodeIds = deviceConfigs.keys();
    std::sort(nodeIds.begin(), nodeIds.end());
    for (quint8 nodeId : nodeIds) {
        const DeviceConfig &dev = deviceConfigs.value(nodeId);
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
    return devArr;
}

QJsonObject buildScreenExportObject(const ScreenConfig &screenConfig)
{
    QJsonObject screenObj;
    screenObj[QStringLiteral("brightness")] = screenConfig.brightness;
    screenObj[QStringLiteral("contrast")] = screenConfig.contrast;
    screenObj[QStringLiteral("enabled")] = screenConfig.enabled;
    screenObj[QStringLiteral("sleepTimeoutSec")] = screenConfig.sleepTimeoutSec;
    screenObj[QStringLiteral("orientation")] = screenConfig.orientation;
    return screenObj;
}

QList<SensorNodeConfig> buildSensorConfigListFromRuntime(
    const QHash<QString, SensorNodeConfig> &sensorConfigs)
{
    QList<QString> sensorIds = sensorConfigs.keys();
    std::sort(sensorIds.begin(), sensorIds.end());

    QList<SensorNodeConfig> sensors;
    sensors.reserve(sensorIds.size());
    for (const QString &sensorId : sensorIds) {
        sensors.append(sensorConfigs.value(sensorId));
    }
    return sensors;
}

QList<DeviceConfig> buildDeviceConfigListFromRuntime(const QHash<quint8, DeviceConfig> &deviceConfigs)
{
    QList<quint8> nodeIds = deviceConfigs.keys();
    std::sort(nodeIds.begin(), nodeIds.end());

    QList<DeviceConfig> devices;
    devices.reserve(nodeIds.size());
    for (quint8 nodeId : nodeIds) {
        devices.append(deviceConfigs.value(nodeId));
    }
    return devices;
}

QJsonObject mergeDeviceParams(const QJsonObject &baseParams, const QJsonObject &patchParams)
{
    QJsonObject merged = baseParams;
    for (auto it = patchParams.begin(); it != patchParams.end(); ++it) {
        if (it.value().isNull()) {
            merged.remove(it.key());
        } else {
            merged.insert(it.key(), it.value());
        }
    }
    return merged;
}

QList<MqttChannelConfig> getMqttChannelsForSave(
    cloud::MqttChannelManager *mqttManager,
    const QList<MqttChannelConfig> &fallbackChannels)
{
    if (mqttManager) {
        return mqttManager->allChannelConfigs();
    }
    LOG_WARNING(kLogSource,
                QStringLiteral("saveConfig: mqttManager is null, keep existing mqttChannels"));
    return fallbackChannels;
}

bool syncMqttChannelsToManager(cloud::MqttChannelManager *mqttManager,
                               const QList<MqttChannelConfig> &targetChannels)
{
    if (!mqttManager) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("reloadConfig: mqttManager is null, skip runtime MQTT sync"));
        return false;
    }

    bool allOk = true;
    QSet<int> targetIds;
    for (const auto &cfg : targetChannels) {
        targetIds.insert(cfg.channelId);
    }

    const QList<MqttChannelConfig> currentChannels = mqttManager->allChannelConfigs();
    for (const auto &current : currentChannels) {
        if (targetIds.contains(current.channelId)) {
            continue;
        }
        QString error;
        if (!mqttManager->removeChannel(current.channelId, &error)) {
            allOk = false;
            LOG_WARNING(kLogSource,
                        QStringLiteral("Failed to remove MQTT channel %1 on reload: %2")
                            .arg(current.channelId)
                            .arg(error));
        }
    }

    for (const auto &cfg : targetChannels) {
        QString error;
        bool ok = false;
        if (mqttManager->hasChannel(cfg.channelId)) {
            ok = mqttManager->updateChannel(cfg, &error);
        } else {
            ok = mqttManager->addChannel(cfg, &error);
        }
        if (!ok) {
            allOk = false;
            LOG_WARNING(kLogSource,
                        QStringLiteral("Failed to upsert MQTT channel %1 on reload: %2")
                            .arg(cfg.channelId)
                            .arg(error));
        }
    }

    mqttManager->connectAll();
    return allOk;
}

void accumulateGroupControlEnqueueStats(GroupControlStats &stats, const EnqueueResult &result)
{
    if (!result.accepted) {
        stats.missing++;
        return;
    }
    stats.accepted++;
    stats.jobIds.append(result.jobId);
}

QHash<quint8, QSet<quint8>> buildNodeChannelsForGroupControl(
    const QHash<int, QList<quint8>> &deviceGroups,
    const QHash<int, QList<int>> &groupChannels,
    const QHash<quint8, device::RelayGd427 *> &relays,
    int groupId,
    int channel)
{
    QHash<quint8, QSet<quint8>> nodeChannels;

    if (channel >= 0 && channel <= kMaxChannelId) {
        const QList<quint8> nodes = deviceGroups.value(groupId);
        for (quint8 node : nodes) {
            if (!relays.contains(node)) {
                continue;
            }
            nodeChannels[node].insert(static_cast<quint8>(channel));
        }
        return nodeChannels;
    }

    const QList<int> channelKeys = groupChannels.value(groupId, {});
    if (channelKeys.isEmpty()) {
        const QList<quint8> nodes = deviceGroups.value(groupId);
        for (quint8 node : nodes) {
            if (!relays.contains(node)) {
                continue;
            }
            for (quint8 ch = 0; ch <= kMaxChannelId; ++ch) {
                nodeChannels[node].insert(ch);
            }
        }
        return nodeChannels;
    }

    for (int key : channelKeys) {
        const quint8 node = channelKeyToNode(key);
        const quint8 ch = channelKeyToChannel(key);
        if (!relays.contains(node)) {
            continue;
        }
        nodeChannels[node].insert(ch);
    }
    return nodeChannels;
}

int countTotalTargetChannels(const QHash<quint8, QSet<quint8>> &nodeChannels)
{
    int total = 0;
    for (auto it = nodeChannels.begin(); it != nodeChannels.end(); ++it) {
        total += it.value().size();
    }
    return total;
}

QHash<quint8, QHash<quint8, device::RelayProtocol::Action>>
buildNodeChannelActions(const QList<BatchControlItem> &items)
{
    QHash<quint8, QHash<quint8, device::RelayProtocol::Action>> nodeChannelActions;
    for (const auto &item : items) {
        if (item.channel > kMaxChannelId) {
            continue;
        }
        nodeChannelActions[item.node][item.channel] = item.action;
    }
    return nodeChannelActions;
}

void accumulateBatchEnqueueResult(BatchControlResult &result, const EnqueueResult &enqueueResult)
{
    if (enqueueResult.accepted) {
        result.accepted++;
        result.jobIds.append(enqueueResult.jobId);
        return;
    }
    result.failed++;
}

device::RelayProtocol::Action actionFromStatusByte(quint8 statusByte)
{
    const quint8 mode = device::RelayProtocol::modeBits(statusByte);
    if (mode == 1) {
        return device::RelayProtocol::Action::Forward;
    }
    if (mode == 2) {
        return device::RelayProtocol::Action::Reverse;
    }
    return device::RelayProtocol::Action::Stop;
}

void fillActionsFromDeviceState(device::RelayGd427 *device,
                                device::RelayProtocol::Action actions[4])
{
    for (quint8 ch = 0; ch <= kMaxChannelId; ++ch) {
        const auto status = device->lastStatus(ch);
        actions[ch] = actionFromStatusByte(status.statusByte);
    }
}

bool controlMultiMergedByChannels(device::RelayGd427 *device,
                                  const QSet<quint8> &channels,
                                  device::RelayProtocol::Action action)
{
    device::RelayProtocol::Action actions[4];
    fillActionsFromDeviceState(device, actions);
    bool hasChange = false;
    for (quint8 ch : channels) {
        if (ch > kMaxChannelId) {
            continue;
        }
        if (actions[ch] != action) {
            hasChange = true;
        }
        actions[ch] = action;
    }
    if (!hasChange) {
        if (action == device::RelayProtocol::Action::Stop) {
            return device->controlMulti(actions);
        }
        return true;
    }
    return device->controlMulti(actions);
}

bool controlMultiMergedByActions(
    device::RelayGd427 *device,
    const QHash<quint8, device::RelayProtocol::Action> &channelActions)
{
    device::RelayProtocol::Action actions[4];
    fillActionsFromDeviceState(device, actions);
    bool hasChange = false;
    bool allRequestedStop = true;
    for (auto it = channelActions.begin(); it != channelActions.end(); ++it) {
        if (it.key() > kMaxChannelId) {
            continue;
        }
        if (actions[it.key()] != it.value()) {
            hasChange = true;
        }
        if (it.value() != device::RelayProtocol::Action::Stop) {
            allRequestedStop = false;
        }
        actions[it.key()] = it.value();
    }
    if (!hasChange) {
        if (allRequestedStop) {
            return device->controlMulti(actions);
        }
        return true;
    }
    return device->controlMulti(actions);
}

void recordMultiControlResult(GroupControlStats &stats,
                              bool ok,
                              int affectedChannels,
                              quint64 &nextJobId)
{
    stats.optimizedFrameCount++;
    if (ok) {
        stats.accepted += affectedChannels;
        stats.jobIds.append(nextJobId++);
        return;
    }
    stats.missing += affectedChannels;
}

void recordMultiControlResult(BatchControlResult &result,
                              bool ok,
                              int affectedChannels,
                              quint64 &nextJobId)
{
    result.optimizedFrames++;
    if (ok) {
        result.accepted += affectedChannels;
        result.jobIds.append(nextJobId++);
        return;
    }
    result.failed += affectedChannels;
}
}  // namespace

CoreContext::CoreContext(QObject *parent)
    : QObject(parent)
{
}

bool CoreContext::init()
{
    coreConfig = CoreConfig::makeDefault();
    greenhouseState = coreConfig.greenhouseState;
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

    initMqtt();
    initQueue();
    LOG_INFO(kLogSource, QStringLiteral("Core context initialization complete"));
    return true;
}

bool CoreContext::init(const CoreConfig &config)
{

    coreConfig = config;
    greenhouseState = config.greenhouseState;
    LOG_INFO(kLogSource, QStringLiteral("Initializing core context with config..."));
    LOG_DEBUG(kLogSource,
              QStringLiteral("RPC port: %1, CAN interface: %2, bitrate: %3")
                  .arg(config.main.rpcPort)
                  .arg(config.can.interface)
                  .arg(config.can.bitrate));

    if (!initSystemSettings()) {
        LOG_ERROR(kLogSource, QStringLiteral("Failed to initialize system settings"));
        return false;
    }

    // 初始化系统资源监控器
    systemMonitor = new config::SystemMonitor(this);
    systemMonitor->start(1000);  // 每秒采样一次
    LOG_INFO(kLogSource, QStringLiteral("System monitor started"));



    if (!initCan()) {
        LOG_ERROR(kLogSource, QStringLiteral("Failed to initialize CAN bus"));
        return false;
    }
    if (!initDevices()) {
        LOG_ERROR(kLogSource, QStringLiteral("Failed to initialize devices from config"));
        return false;
    }

    cloudUploadConfig = config.cloudUpload;

    initMqtt();
    initQueue();
    initStrategy();

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
                 .arg(coreConfig.can.interface)
                 .arg(coreConfig.can.bitrate)
                 .arg(coreConfig.can.tripleSampling));
    systemSettings->setCanBitrate(coreConfig.can.interface, coreConfig.can.bitrate, coreConfig.can.tripleSampling);
    return true;
}

bool CoreContext::initCan()
{
    LOG_DEBUG(kLogSource, QStringLiteral("Initializing CAN bus..."));

    comm::CanConfig canConfig;
    canConfig.interface = coreConfig.can.interface;
    canConfig.bitrate = coreConfig.can.bitrate;
    canConfig.tripleSampling = coreConfig.can.tripleSampling;
    canConfig.canFd = false;
    canConfig.restartMs = coreConfig.can.restartMs;
    canConfig.periodicRestartMin = coreConfig.can.periodicRestartMin;
    canConfig.is_fake = coreConfig.can.is_fake;
    canConfig.fake_can = coreConfig.can.fake_can;
    canConfig.fake_can_baudrate = coreConfig.can.fake_can_baudrate;

    canBus = new comm::CanComm(canConfig, this);
    connect(canBus, &comm::CanComm::errorOccurred, this, [](const QString &error) {
        LOG_ERROR("CAN", QStringLiteral("Error: %1").arg(error));
    });

    // 连接空闲探测信号：总线空闲时查询所有设备以保持通信活跃
    connect(canBus, &comm::CanComm::idleProbeNeeded, this, [this]() {
        for (auto *relay : relays) {
            if (relay) {
                relay->queryAll();
            }
        }
    });

    if (!canBus->open()) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("CAN open failed, RPC service will start but CAN methods will not work"));
    } else {
        LOG_INFO(kLogSource, QStringLiteral("CAN bus opened: %1").arg(coreConfig.can.interface));
    }

    canManager = new device::CanDeviceManager(canBus, this);
    LOG_DEBUG(kLogSource, QStringLiteral("CAN device manager created"));
    return true;
}


bool CoreContext::initDevices()
{
    LOG_DEBUG(kLogSource, QStringLiteral("Initializing devices from config..."));
    relays.clear();
    deviceConfigs.clear();
    resetGroupMappings(deviceGroups, groupNames, groupSpecialIds, groupCanOptimizeFrame, groupChannels, 0);
    loadSensorConfigsFromList(coreConfig.sensors, sensorConfigs);

    if (!coreConfig.devices.isEmpty()) {
        loadRelayDevicesFromList(coreConfig.devices, relays, deviceConfigs, canBus, canManager,
                                 deviceTcpServer_, this);
        loadGroupConfigsFromList(coreConfig.groups, deviceGroups, groupNames, groupSpecialIds, groupCanOptimizeFrame, groupChannels);

        return true;
    }

    LOG_WARNING(kLogSource, QStringLiteral("No devices configured (devices list empty)"));
    return true;
}


bool CoreContext::initStrategy()
{
    strategies_ = coreConfig.strategies;
    deletedStrategies_.clear();

    autoStrategyScheduler_ = new QTimer(this);
    autoStrategyScheduler_->setInterval(1000); // 每秒扫描一次
    connect(autoStrategyScheduler_, &QTimer::timeout,
            this, &CoreContext::evaluateAllStrategies);
    autoStrategyScheduler_->start();

    LOG_INFO(kLogSource, QStringLiteral("Strategy scheduler initialized with %1 strategies")
                 .arg(strategies_.size()));
    return true;
}

bool CoreContext::initMqtt()
{
    // 初始化MQTT多通道管理器
    mqttManager = new cloud::MqttChannelManager(this);
    // 加载MQTT通道配置
    for (const auto &mqttConfig : coreConfig.mqttChannels) {
        MqttChannelConfig cloudConfig;
        cloudConfig.type = mqttConfig.type;
        cloudConfig.channelId = mqttConfig.channelId;
        cloudConfig.name = mqttConfig.name;
        cloudConfig.enabled = mqttConfig.enabled;
        cloudConfig.broker = mqttConfig.broker;
        cloudConfig.port = mqttConfig.port;
        cloudConfig.clientId = mqttConfig.clientId;
        cloudConfig.username = mqttConfig.username;
        cloudConfig.password = mqttConfig.password;
        cloudConfig.topicPrefix = mqttConfig.topicPrefix;
        cloudConfig.keepAliveSec = mqttConfig.keepAliveSec;
        cloudConfig.autoReconnect = mqttConfig.autoReconnect;
        cloudConfig.reconnectIntervalSec = mqttConfig.reconnectIntervalSec;
        cloudConfig.qos = mqttConfig.qos;
        cloudConfig.topicControlSub  = mqttConfig.topicControlSub;
        cloudConfig.topicStrategySub = mqttConfig.topicStrategySub;
        cloudConfig.topicStatusPub   = mqttConfig.topicStatusPub;
        cloudConfig.topicEventPub    = mqttConfig.topicEventPub;
        cloudConfig.topicSettingSub  = mqttConfig.topicSettingSub;
        cloudConfig.topicSettingPub  = mqttConfig.topicSettingPub;


        QString error;
        if (!mqttManager->addChannel(cloudConfig, &error)) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("Failed to add MQTT channel %1: %2")
                            .arg(mqttConfig.channelId)
                            .arg(error));
        }
    }
    LOG_INFO(kLogSource,
             QStringLiteral("MQTT manager initialized with %1 channels")
                 .arg(mqttManager->channelCount()));

    LOG_INFO(kLogSource, QStringLiteral("Cloud message handler initialized"));

    cloudUploader = new cloud::fanzhoucloud::CloudUploader(this, this);
    cloudUploader->applyConfig(cloudUploadConfig);

    cloudMessageHandler = new cloud::fanzhoucloud::CloudMessageHandler(this, this);
    cloudMessageHandler->setChannelId(1);

    cloudSettingService = new cloud::fanzhoucloud::SettingService();

    connect(mqttManager, &cloud::MqttChannelManager::messageReceived,
            cloudMessageHandler, &cloud::fanzhoucloud::CloudMessageHandler::onMqttMessage);


    for (auto it = relays.begin(); it != relays.end(); ++it) {
        auto *relay = it.value();
        quint8 nodeId = it.key();

        connect(relay, &device::RelayGd427::statusUpdated,
                this,
                [this, nodeId](quint8 ch,
                               fanzhou::device::RelayProtocol::Status status) {
                    // 更新传感器值（用于策略调用）
                    updateRelaySensorValue(nodeId, ch, status);
                    
                    // 云端上报
                    if (cloudUploader)
                        cloudUploader->onChannelValueChanged(nodeId, ch);
                });

        connect(relay, &device::RelayGd427::autoStatusReceived,
                this,
                [this, nodeId](
                    fanzhou::device::RelayProtocol::AutoStatusReport report) {
                    // 更新所有通道的传感器值
                    for (int ch = 0; ch < 4; ++ch) {
                        device::RelayProtocol::Status status;
                        status.channel = static_cast<quint8>(ch);
                        status.statusByte = static_cast<quint8>(report.status[ch]);
                        status.currentA = report.currentA[ch];
                        updateRelaySensorValue(nodeId, static_cast<quint8>(ch), status);
                    }
                    
                    if (cloudUploader)
                        cloudUploader->onDeviceStatusChanged(nodeId);
                });

    }

    LOG_INFO(kLogSource, QStringLiteral("MQTT initialization complete"));
    return true;
}

void CoreContext::initQueue()
{
    if (controlTimer_) return;
    controlTimer_ = new QTimer(this);
    controlTimer_->setInterval(kQueueTickMs);
    connect(controlTimer_, &QTimer::timeout, this, &CoreContext::processNextJob);
}


bool CoreContext::isInEffectiveTime(const AutoStrategy &s, const QTime &now) const
{
    if (s.effectiveBeginTime.isEmpty() || s.effectiveEndTime.isEmpty()) {
        return true;
    }

    const EffectiveTimeRange range =
        parseEffectiveTimeRangeCached(s.effectiveBeginTime, s.effectiveEndTime);
    if (!range.valid) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("strategy[%1] invalid effective time: %2 ~ %3")
                        .arg(s.strategyId)
                        .arg(s.effectiveBeginTime)
                        .arg(s.effectiveEndTime));
        return true;
    }

    const QTime &begin = range.begin;
    const QTime &end = range.end;
    bool inRange = false;
    if (begin <= end) {
        inRange = (now >= begin && now <= end);
    } else {
        inRange = (now >= begin || now <= end);
    }

    if (isDebugLogEnabled()) {
        LOG_DEBUG(kLogSource,
                  QStringLiteral("strategy[%1] time check: now=%2 range=%3~%4 result=%5")
                      .arg(s.strategyId)
                      .arg(now.toString("HH:mm"))
                      .arg(begin.toString("HH:mm"))
                      .arg(end.toString("HH:mm"))
                      .arg(inRange));
    }

    return inRange;
}


void CoreContext::executeActions(const QList<StrategyAction> &actions)
{
    int cnt = 0;
    for (const auto &a : actions) {
        cnt++;
        const QString source = QStringLiteral("strategy_action:%1").arg(cnt);
        enqueueControl(a.node, a.channel,
                       static_cast<device::RelayProtocol::Action>(a.identifierValue),
                       source, true);
    }
}

bool CoreContext::evaluateConditions(const QList<StrategyCondition> &conditions,
                                     qint8 matchType) const
{
    if (conditions.isEmpty()) {
        return true;
    }

    bool hasValidCondition = false;

    for (const auto &c : conditions) {

        // 1. 传感器是否存在
        if (!sensorValues.contains(c.identifier)) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("condition sensor not found, skip: %1")
                            .arg(c.identifier));
            continue;   // ❗ OR / AND 都应该跳过
        }

        bool ok1 = false;
        const double value = sensorValues.value(c.identifier).toDouble(&ok1);
        if (!ok1) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("invalid sensor value, skip: %1")
                            .arg(c.identifier));
            continue;
        }

        hasValidCondition = true;

        const bool ok = evaluateSensorCondition(c.op, value, c.identifierValue);

        if (matchType == 0) { // AND
            if (!ok) {
                return false;
            }
        } else { // OR
            if (ok) {
                return true;
            }
        }
    }

    // 如果没有任何“有效条件”
    if (!hasValidCondition) {
        LOG_WARNING(kLogSource, "no valid conditions evaluated");
        return false;
    }

    // AND：所有有效条件都通过
    // OR ：没有任何条件命中
    return (matchType == 0);
}



void CoreContext::evaluateAllStrategies()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (shouldPauseStrategyEvaluation(nowMs)) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTime();

    for (AutoStrategy &strategy : strategies_) {
        if (!shouldEvaluateStrategyNow(strategy, now)) {
            continue;
        }
        triggerStrategyActions(strategy, now);
    }
}

bool CoreContext::shouldPauseStrategyEvaluation(qint64 nowMs)
{
    if (controlQueue_.size() < kStrategyQueueBackpressureThreshold) {
        return false;
    }
    if (nowMs - lastStrategyOverloadLogMs_ >= kBackpressureLogIntervalMs) {
        lastStrategyOverloadLogMs_ = nowMs;
        LOG_WARNING(kLogSource,
                    QStringLiteral("strategy scheduler paused: control queue backlog=%1")
                        .arg(controlQueue_.size()));
    }
    return true;
}

bool CoreContext::shouldEvaluateStrategyNow(const AutoStrategy &strategy, const QDateTime &now) const
{
    if (deletedStrategies_.contains(strategy.strategyId)) {
        return false;
    }
    if (!strategy.enabled) {
        return false;
    }
    if (strategy.strategyType == QStringLiteral("manual")) {
        return false;
    }
    if (strategy.actions.isEmpty()) {
        return false;
    }
    if (!isInEffectiveTime(strategy, now.time())) {
        return false;
    }
    if (strategy.lastTriggered.isValid() &&
        strategy.lastTriggered.msecsTo(now) < kStrategyTriggerMinIntervalMs) {
        return false;
    }
    if (!evaluateConditions(strategy.conditions, strategy.matchType)) {
        return false;
    }
    return true;
}

void CoreContext::triggerStrategyActions(AutoStrategy &strategy, const QDateTime &now)
{
    strategy.lastTriggered = now;
    LOG_INFO(kLogSource, QStringLiteral("Strategy %1 [%2] triggered, executing %3 actions sequentially")
                 .arg(strategy.strategyId)
                 .arg(strategy.strategyName)
                 .arg(strategy.actions.size()));
    executeActions(strategy.actions);
}

int CoreContext::strategyIntervalMs(const AutoStrategy &config) const
{
    Q_UNUSED(config);
    return 1000; // 1000ms
}


QList<AutoStrategyState> CoreContext::strategyStates() const
{
    QList<AutoStrategyState> states;
    for (const auto &s: strategies_) {
        if (deletedStrategies_.contains(s.strategyId)) continue;
        AutoStrategyState st;
        st.config = s;
        st.attached = (s.groupId > 0);       ///< 是否已绑定分组
        st.running = false;                  ///< 是否运行
        states.append(st);
    }
    return states;
}

bool CoreContext::setStrategyEnabled(int strategyId, bool enabled)
{
    AutoStrategy *strategy = findMutableStrategyById(strategies_, strategyId);
    if (!strategy) {
        LOG_WARNING(kLogSource, QStringLiteral("Strategy %1 not found").arg(strategyId));
        return false;
    }

    strategy->enabled = enabled;
    LOG_INFO(kLogSource, QStringLiteral("Strategy %1 set enabled=%2")
                 .arg(strategyId)
                 .arg(enabled));

    if (!enabled && !strategy->actions.isEmpty()) {
        stopStrategyActionsSequentially(*strategy);
    }
    return true;
}

void CoreContext::stopStrategyActionsSequentially(const AutoStrategy &strategy)
{
    LOG_INFO(kLogSource, QStringLiteral("Strategy %1 disabled, stopping %2 actions sequentially")
                 .arg(strategy.strategyId)
                 .arg(strategy.actions.size()));
    int actionIndex = 0;
    for (const auto &action : strategy.actions) {
        actionIndex++;
        const QString source = QStringLiteral("strategy_disable:%1 action:%2")
                                   .arg(strategy.strategyName)
                                   .arg(actionIndex);
        enqueueControl(action.node,
                       action.channel,
                       device::RelayProtocol::Action::Stop,
                       source,
                       true);
    }
}

bool CoreContext::triggerStrategy(int strategyId)
{
    AutoStrategy *strategy = findMutableStrategyById(strategies_, strategyId);
    if (!strategy) {
        LOG_WARNING(kLogSource, QStringLiteral("Strategy %1 not found").arg(strategyId));
        return false;
    }
    if (!strategy->enabled) {
        LOG_WARNING(kLogSource, QStringLiteral("Strategy %1 is disabled").arg(strategyId));
        return false;
    }

    LOG_INFO(kLogSource, QStringLiteral("Triggering strategy %1, executing %2 actions sequentially")
                 .arg(strategyId)
                 .arg(strategy->actions.size()));
    executeActions(strategy->actions);
    return true;
}

bool CoreContext::createStrategy(const AutoStrategy &config, bool *isUpdate, QString *error, bool syncToCloud)
{
    Q_UNUSED(error);

    const int existingIndex = findStrategyIndexById(strategies_, config.strategyId);
    if (existingIndex >= 0) {
        return updateExistingStrategy(config, existingIndex, isUpdate, syncToCloud);
    }
    return appendNewStrategy(config, isUpdate, syncToCloud);
}

bool CoreContext::updateExistingStrategy(const AutoStrategy &config,
                                         int existingIndex,
                                         bool *isUpdate,
                                         bool syncToCloud)
{
    AutoStrategy &strategy = strategies_[existingIndex];
    const AutoStrategy previous = strategy;

    strategy = config;
    strategy.version = previous.version + 1;
    strategy.lastTriggered = previous.lastTriggered;
    strategy.cloudChannelId = previous.cloudChannelId;

    if (isUpdate) {
        *isUpdate = true;
    }
    LOG_INFO(kLogSource,
             QStringLiteral("Updated strategy %1: version %2 -> %3 (auto increment)")
                 .arg(config.strategyId)
                 .arg(previous.version)
                 .arg(strategy.version));

    if (syncToCloud) {
        syncStrategyUpsertToCloud(cloudMessageHandler, strategy, true);
    }
    return true;
}

bool CoreContext::appendNewStrategy(const AutoStrategy &config, bool *isUpdate, bool syncToCloud)
{
    strategies_.append(config);
    AutoStrategy &created = strategies_.last();
    if (created.version <= 0) {
        created.version = 1;
    }
    if (isUpdate) {
        *isUpdate = false;
    }
    LOG_INFO(kLogSource, QStringLiteral("Created strategy %1, version=%2")
                 .arg(config.strategyId)
                 .arg(created.version));

    if (syncToCloud) {
        syncStrategyUpsertToCloud(cloudMessageHandler, created, false);
    }
    return true;
}


bool CoreContext::deleteStrategy(int strategyId, QString *error, bool *alreadyDeleted, bool syncToCloud)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (alreadyDeleted) {
        *alreadyDeleted = false;
    }

    const int index = findStrategyIndexById(strategies_, strategyId);
    if (index >= 0) {
        return deleteExistingStrategyByIndex(index, strategyId, nowMs, syncToCloud);
    }

    if (deletedStrategies_.contains(strategyId)) {
        return markAlreadyDeletedStrategy(strategyId, nowMs, error, alreadyDeleted);
    }

    return markMissingStrategyDelete(strategyId, nowMs, error);
}

bool CoreContext::deleteExistingStrategyByIndex(int index,
                                                int strategyId,
                                                qint64 nowMs,
                                                bool syncToCloud)
{
    const AutoStrategy removed = strategies_[index];
    const QString strategyType =
        removed.type.isEmpty() ? QStringLiteral("scene") : removed.type;

    deletedStrategies_.insert(strategyId, {removed.version, nowMs});
    strategies_.removeAt(index);

    LOG_INFO(kLogSource, QStringLiteral("Deleted strategy %1").arg(strategyId));

    if (syncToCloud) {
        // 本地删除成功时不阻断返回，云端同步失败仅告警
        syncStrategyDeleteToCloud(cloudMessageHandler, strategyId, strategyType, nowMs);
    }
    return true;
}

bool CoreContext::markAlreadyDeletedStrategy(int strategyId,
                                             qint64 nowMs,
                                             QString *error,
                                             bool *alreadyDeleted)
{
    auto it = deletedStrategies_.find(strategyId);
    if (it == deletedStrategies_.end()) {
        return false;
    }

    it->deleteMs = nowMs;
    if (alreadyDeleted) {
        *alreadyDeleted = true;
    }
    setErrorIfPresent(error, QStringLiteral("Strategy %1 already deleted").arg(strategyId));
    return false;
}

bool CoreContext::markMissingStrategyDelete(int strategyId, qint64 nowMs, QString *error)
{
    deletedStrategies_.insert(strategyId, {0, nowMs});
    setErrorIfPresent(error, QStringLiteral("StrategyId %1 not found").arg(strategyId));
    return false;
}

bool CoreContext::setStrategyId(int oldId, int newId)
{
    if (!validateStrategyIdMapping(oldId, newId)) {
        return false;
    }
    if (hasStrategyIdConflict(oldId, newId)) {
        return false;
    }
    return applyStrategyIdRemap(oldId, newId);
}

bool CoreContext::validateStrategyIdMapping(int oldId, int newId) const
{
    if (oldId == -1 || newId <= 0 || oldId == newId) {
        LOG_ERROR(kLogSource,
                  QStringLiteral("invalid strategy id mapping: old=%1 new=%2")
                      .arg(oldId)
                      .arg(newId));
        return false;
    }
    return true;
}

bool CoreContext::hasStrategyIdConflict(int oldId, int newId) const
{
    for (const auto &strategy : strategies_) {
        if (strategy.strategyId != newId) {
            continue;
        }
        LOG_ERROR(kLogSource,
                  QStringLiteral("strategyId %1 already exists, cannot replace old %2")
                      .arg(newId)
                      .arg(oldId));
        return true;
    }
    return false;
}

bool CoreContext::applyStrategyIdRemap(int oldId, int newId)
{
    AutoStrategy *strategy = findMutableStrategyById(strategies_, oldId);
    if (!strategy) {
        LOG_ERROR(kLogSource,
                  QStringLiteral("old strategyId %1 not found when setting newId %2")
                      .arg(oldId)
                      .arg(newId));
        return false;
    }

    strategy->strategyId = newId;

    // 云端刚创建时，版本默认至少为 1
    if (strategy->version <= 0) {
        strategy->version = 1;
    }
    strategy->updateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    LOG_INFO(kLogSource,
             QStringLiteral("strategyId updated: %1 -> %2")
                 .arg(oldId)
                 .arg(newId));

    deletedStrategies_.remove(oldId);
    deletedStrategies_.remove(newId);
    return true;
}


bool CoreContext::evaluateSensorCondition(const QString &op,
                                          double value,
                                          double threshold) const
{
    if (op == "gt")   return value > threshold;
    if (op == "lt")   return value < threshold;
    if (op == "eq")   return qAbs(value - threshold) < kFloatCompareEpsilon;
    if (op == "ne" || op == "neq")   return qAbs(value - threshold) >= kFloatCompareEpsilon;
    if (op == "egt" || op == "ge")  return value >= threshold;
    if (op == "elt" || op == "le")  return value <= threshold;

    LOG_WARNING(kLogSource,
                QStringLiteral("unknown condition op: %1").arg(op));
    return false;
}




bool CoreContext::ensureGroupForStrategy(AutoStrategy &s, QString *error)
{
    if (!ensureStrategyGroupExists(s, error)) {
        return false;
    }

    const qint32 groupId = s.groupId;
    for (const auto &action : s.actions) {
        if (!ensureStrategyActionDeviceInGroup(groupId, action.node, error)) {
            return false;
        }
        if (!ensureStrategyActionChannelInGroup(groupId, action.node, action.channel, error)) {
            return false;
        }
    }
    return true;
}

bool CoreContext::ensureStrategyGroupExists(AutoStrategy &strategy, QString *error)
{
    if (strategy.groupId > 0) {
        return true;
    }

    const qint32 newGroupId = allocateAutoStrategyGroupId(deviceGroups);
    const QString name = QStringLiteral("auto_strategy_%1").arg(strategy.strategyId);
    if (!createGroup(newGroupId, name, error)) {
        return false;
    }
    strategy.groupId = newGroupId;

    LOG_INFO(kLogSource, QStringLiteral("Auto create group for Strategy: strategyId=%1, groupId=%2")
                 .arg(strategy.strategyId)
                 .arg(strategy.groupId));
    return true;
}

bool CoreContext::ensureStrategyActionDeviceInGroup(qint32 groupId, quint8 node, QString *error)
{
    const bool nodeMissing =
        !deviceGroups.contains(groupId) || !deviceGroups[groupId].contains(node);
    if (!nodeMissing) {
        return true;
    }

    if (!addDeviceToGroup(groupId, node, error)) {
        if (error && error->isEmpty()) {
            setErrorIfPresent(error,
                              QStringLiteral("addDeviceToGroup failed: group=%1 node=%2")
                                  .arg(groupId)
                                  .arg(node));
        }
        return false;
    }
    return true;
}

bool CoreContext::ensureStrategyActionChannelInGroup(qint32 groupId,
                                                     quint8 node,
                                                     quint32 channel,
                                                     QString *error)
{
    const QList<int> &channels = groupChannels.value(groupId);
    if (channels.contains(static_cast<int>(channel))) {
        return true;
    }

    if (!addChannelToGroup(groupId, node, channel, error)) {
        if (error && error->isEmpty()) {
            setErrorIfPresent(error,
                              QStringLiteral("addChannelToGroup failed: group=%1 node=%2 ch=%3")
                                  .arg(groupId)
                                  .arg(node)
                                  .arg(channel));
        }
        return false;
    }
    return true;
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
        LOG_WARNING(kLogSource, QStringLiteral("executeJob: device not found, node=0x%1, ch=%2, source=%3")
                        .arg(job.node, 2, 16, QChar('0'))
                        .arg(job.channel)
                        .arg(job.source));
        return result;
    }

    const qint64 jobAgeMs = result.finishedMs - job.enqueuedMs;
    if (jobAgeMs >= 0 && jobAgeMs > kMaxQueuedJobAgeMs) {
        result.ok = false;
        result.message = QStringLiteral("stale_dropped");
        jobResults_.insert(job.id, result);
        lastJobId_ = job.id;
        LOG_WARNING(kLogSource, QStringLiteral("executeJob: stale control dropped, node=0x%1, ch=%2, action=%3, ageMs=%4, source=%5")
                        .arg(job.node, 2, 16, QChar('0'))
                        .arg(job.channel)
                        .arg(static_cast<int>(job.action))
                        .arg(jobAgeMs)
                        .arg(job.source));
        return result;
    }

    // 仅在目标状态发生变化时才真正下发控制命令，避免重复控制导致误动作。
    const auto current = dev->lastStatus(job.channel);
    const auto currentAction = actionFromStatusByte(current.statusByte);
    if (currentAction == job.action &&
        job.action != device::RelayProtocol::Action::Stop) {
        result.ok = true;
        result.message = QStringLiteral("no_change");
        jobResults_.insert(job.id, result);
        lastJobId_ = job.id;
        LOG_DEBUG(kLogSource, QStringLiteral("executeJob: skip no-change, node=0x%1, ch=%2, action=%3, source=%4")
                              .arg(job.node, 2, 16, QChar('0'))
                              .arg(job.channel)
                              .arg(static_cast<int>(job.action))
                              .arg(job.source));
        return result;
    }

    const bool ok = dev->control(job.channel, job.action);
    result.ok = ok;
    result.message = ok ? QStringLiteral("ok") : kErrDeviceRejected;
    if (!ok) {
        LOG_WARNING(kLogSource, QStringLiteral("executeJob: control rejected, node=0x%1, ch=%2, action=%3, source=%4")
                        .arg(job.node, 2, 16, QChar('0'))
                        .arg(job.channel)
                        .arg(static_cast<int>(job.action))
                        .arg(job.source));
    }
    jobResults_.insert(job.id, result);
    lastJobId_ = job.id;
    return result;
}

void CoreContext::processNextJob()
{
    if (processingQueue_) return;
    if (controlQueue_.isEmpty()) {
        if (controlTimer_) controlTimer_->stop();
        // 队列空闲时清理过期数据
        trimJobResults();
        trimDeletedStrategies();
        return;
    }

    processingQueue_ = true;
    int processed = 0;
    while (!controlQueue_.isEmpty() && processed < kMaxJobsPerQueueTick) {
        const auto job = controlQueue_.dequeue();
        executeJob(job);
        ++processed;
    }
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
        LOG_WARNING(kLogSource, QStringLiteral("enqueueControl: unknown node=0x%1, ch=%2, source=%3")
                        .arg(node, 2, 16, QChar('0'))
                        .arg(channel)
                        .arg(source));
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

    int mergedCount = 0;
    for (int i = controlQueue_.size() - 1; i >= 0; --i) {
        const auto &queued = controlQueue_.at(i);
        if (queued.node == node && queued.channel == channel) {
            controlQueue_.removeAt(i);
            ++mergedCount;
        }
    }
    if (mergedCount > 0) {
        LOG_DEBUG(kLogSource, QStringLiteral("enqueueControl: merged %1 pending jobs for node=0x%2 ch=%3")
                              .arg(mergedCount)
                              .arg(node, 2, 16, QChar('0'))
                              .arg(channel));
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
        accumulateGroupControlEnqueueStats(stats, result);
    }
    return stats;
}

GroupControlStats CoreContext::queueGroupBoundChannelsControl(int groupId,
                                                               device::RelayProtocol::Action action,
                                                               const QString &source)
{
    GroupControlStats stats;
    
    // 获取分组绑定的通道列表
    const QList<int> channelKeys = groupChannels.value(groupId, {});
    
    if (channelKeys.isEmpty()) {
        // 回退到控制分组中所有设备的所有通道（向后兼容）
        // 触发条件：分组没有通过 group.addChannel 绑定任何特定通道
        // 此时策略会控制分组中所有设备的所有通道（0-kMaxChannelId）
        const QList<quint8> nodes = deviceGroups.value(groupId);
        stats.total = nodes.size() * (kMaxChannelId + 1);
        for (quint8 node : nodes) {
            for (quint8 ch = 0; ch <= kMaxChannelId; ++ch) {
                const auto result = enqueueControl(node, ch, action, source, true);
                accumulateGroupControlEnqueueStats(stats, result);
            }
        }
    } else {
        // 只控制已绑定的特定通道
        // channelKey = nodeId * kChannelKeyMultiplier + channel
        stats.total = channelKeys.size();
        for (int key : channelKeys) {
            const quint8 node = channelKeyToNode(key);
            const quint8 ch = channelKeyToChannel(key);
            const auto result = enqueueControl(node, ch, action, source, true);
            accumulateGroupControlEnqueueStats(stats, result);
        }
    }
    
    return stats;
}

/**
 * @brief 分组控制优化版 - 合并同一节点的多通道控制为单条CAN帧
 * 
 * 优化逻辑：
 * 1. 收集所有需要控制的(节点, 通道)对
 * 2. 按节点分组，统计每个节点需要控制的通道
 * 3. 如果一个节点需要控制多个通道，使用controlMulti发送单条CAN帧
 * 4. 如果只控制单个通道，使用普通control发送
 * 
 * 例如：控制节点1的通道0,1,2,3，原本需要4条CAN帧，优化后只需1条。
 */
GroupControlStats CoreContext::queueGroupControlOptimized(int groupId, int channel,
                                                           device::RelayProtocol::Action action,
                                                           const QString &source)
{
    const bool canOptimize = groupCanOptimizeFrame.value(groupId, true);
    if (!canOptimize) {
        GroupControlStats stats;
        if (channel >= 0 && channel <= kMaxChannelId) {
            stats = queueGroupControl(groupId, static_cast<quint8>(channel), action, source);
        } else {
            stats = queueGroupBoundChannelsControl(groupId, action, source);
        }
        stats.originalFrameCount = stats.total;
        stats.optimizedFrameCount = stats.total;
        LOG_INFO(kLogSource,
                 QStringLiteral("[优化] 分组%1禁用帧优化，按逐通道发送: frames=%2")
                     .arg(groupId)
                     .arg(stats.total));
        return stats;
    }

    GroupControlStats stats;

    const QHash<quint8, QSet<quint8>> nodeChannels =
        buildNodeChannelsForGroupControl(deviceGroups, groupChannels, relays, groupId, channel);

    stats.originalFrameCount = countTotalTargetChannels(nodeChannels);
    stats.total = stats.originalFrameCount;
    
    // 优化发送：对每个节点使用最优方式发送
    stats.optimizedFrameCount = 0;
    for (auto it = nodeChannels.begin(); it != nodeChannels.end(); ++it) {
        const quint8 node = it.key();
        const QSet<quint8> &channels = it.value();
        
        auto *dev = relays.value(node, nullptr);
        if (!dev) {
            stats.missing += channels.size();
            continue;
        }
        
        // 判断是否需要合并：控制多个通道时使用controlMulti以节省CAN帧
        if (channels.size() >= kMinChannelsForMultiControl) {
            const bool ok = controlMultiMergedByChannels(dev, channels, action);
            recordMultiControlResult(stats, ok, channels.size(), nextJobId_);
            
            LOG_DEBUG(kLogSource, QStringLiteral("[优化] 节点0x%1: 合并%2通道为1帧CAN (来源: %3)")
                .arg(node, 2, 16, QChar('0'))
                .arg(channels.size())
                .arg(source));
        } else {
            // 只有1个通道，使用普通control
            for (quint8 ch : channels) {
                const auto result = enqueueControl(node, ch, action, source, true);
                stats.optimizedFrameCount++;
                accumulateGroupControlEnqueueStats(stats, result);
            }
        }
    }
    
    LOG_INFO(kLogSource, QStringLiteral("[优化] 分组%1控制: 原%2帧 -> 优化后%3帧 (节省%4帧)")
        .arg(groupId)
        .arg(stats.originalFrameCount)
        .arg(stats.optimizedFrameCount)
        .arg(stats.originalFrameCount - stats.optimizedFrameCount));
    
    return stats;
}

/**
 * @brief 批量控制 - 支持一次调用控制多个节点/通道
 * 
 * 优化策略：
 * 1. 按节点分组所有控制请求
 * 2. 对于同一节点的多个通道控制，合并为单条controlMulti CAN帧
 * 3. 返回优化统计，显示节省的帧数
 */
BatchControlResult CoreContext::batchControl(const QList<BatchControlItem> &items,
                                              const QString &source)
{
    BatchControlResult result;
    result.total = items.size();
    result.originalFrames = items.size();
    
    if (items.isEmpty()) {
        return result;
    }
    
    const QHash<quint8, QHash<quint8, device::RelayProtocol::Action>> nodeChannelActions =
        buildNodeChannelActions(items);
    
    // 优化发送
    for (auto nodeIt = nodeChannelActions.begin(); nodeIt != nodeChannelActions.end(); ++nodeIt) {
        const quint8 node = nodeIt.key();
        const QHash<quint8, device::RelayProtocol::Action> &channelActions = nodeIt.value();
        
        auto *dev = relays.value(node, nullptr);
        if (!dev) {
            result.failed += channelActions.size();
            continue;
        }
        
        if (channelActions.size() >= kMinChannelsForMultiControl) {
            const bool ok = controlMultiMergedByActions(dev, channelActions);
            recordMultiControlResult(result, ok, channelActions.size(), nextJobId_);
            
            LOG_DEBUG(kLogSource, QStringLiteral("[批量] 节点0x%1: 合并%2通道为1帧")
                .arg(node, 2, 16, QChar('0'))
                .arg(channelActions.size()));
        } else {
            // 单个通道
            for (auto chIt = channelActions.begin(); chIt != channelActions.end(); ++chIt) {
                const auto enqResult = enqueueControl(node, chIt.key(), chIt.value(), source, true);
                result.optimizedFrames++;
                accumulateBatchEnqueueResult(result, enqResult);
            }
        }
    }
    
    result.ok = (result.failed == 0);
    
    LOG_INFO(kLogSource, QStringLiteral("[批量] 控制完成: 总%1项, 成功%2, 失败%3, 原%4帧->优化后%5帧")
        .arg(result.total)
        .arg(result.accepted)
        .arg(result.failed)
        .arg(result.originalFrames)
        .arg(result.optimizedFrames));
    
    return result;
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
        setErrorIfPresent(error, QStringLiteral("groupId must be positive"));
        return false;
    }
    if (deviceGroups.contains(groupId)) {
        setErrorIfPresent(error, QStringLiteral("group exists"));
        return false;
    }
    deviceGroups.insert(groupId, {});
    groupNames.insert(groupId, name);
    groupSpecialIds.remove(groupId);
    groupCanOptimizeFrame.insert(groupId, true);
    return true;
}

bool CoreContext::deleteGroup(int groupId, QString *error)
{
    if (!ensureGroupExists(deviceGroups, groupId, error)) {
        return false;
    }
    deviceGroups.remove(groupId);
    groupNames.remove(groupId);
    groupSpecialIds.remove(groupId);
    groupCanOptimizeFrame.remove(groupId);
    groupChannels.remove(groupId);
    return true;
}

bool CoreContext::addDeviceToGroup(int groupId, quint8 node, QString *error)
{
    if (!ensureGroupExists(deviceGroups, groupId, error)) {
        return false;
    }
    if (!ensureRelayExists(relays, node, error)) {
        return false;
    }
    appendUniqueNodeToGroup(deviceGroups, groupId, node);
    return true;
}

bool CoreContext::removeDeviceFromGroup(int groupId, quint8 node, QString *error)
{
    if (!ensureGroupExists(deviceGroups, groupId, error)) {
        return false;
    }
    deviceGroups[groupId].removeAll(node);
    return true;
}

device::RelayProtocol::Action CoreContext::parseAction(const QString &str, bool *ok) const
{
    const QString a = str.trimmed().toLower();
    setBoolIfPresent(ok, true);

    if (a == QStringLiteral("stop") || a == QStringLiteral("0"))
        return device::RelayProtocol::Action::Stop;
    if (a == QStringLiteral("fwd") || a == QStringLiteral("forward") || a == QStringLiteral("1"))
        return device::RelayProtocol::Action::Forward;
    if (a == QStringLiteral("rev") || a == QStringLiteral("reverse") || a == QStringLiteral("2"))
        return device::RelayProtocol::Action::Reverse;

    setBoolIfPresent(ok, false);
    return device::RelayProtocol::Action::Stop;
}



QStringList CoreContext::methodGroups() const
{
    return {QStringLiteral("rpc.*"), QStringLiteral("sys.*"), QStringLiteral("can.*"),
            QStringLiteral("relay.*"), QStringLiteral("group.*"),
            QStringLiteral("control.*"), QStringLiteral("auto.*"),
            QStringLiteral("device.*"), QStringLiteral("screen.*"),
            QStringLiteral("greenhouse.*")};
}

bool CoreContext::addChannelToGroup(int groupId, quint8 node, int channel, QString *error)
{
    if (!ensureGroupExists(deviceGroups, groupId, error)) {
        return false;
    }
    if (!ensureRelayExists(relays, node, error)) {
        return false;
    }
    // 注意：此函数用于添加特定通道到分组，channel=-1 不适用于此场景
    // 如需添加所有通道，请多次调用此函数或使用 addDeviceToGroup
    if (!ensureChannelInRange(channel, error)) {
        return false;
    }

    appendUniqueNodeToGroup(deviceGroups, groupId, node);

    // Encode node+channel as unique key: node * kChannelKeyMultiplier + channel
    const int channelKey = makeChannelKey(node, static_cast<quint8>(channel));
    appendUniqueChannelKey(groupChannels, groupId, channelKey);
    return true;
}

bool CoreContext::removeChannelFromGroup(int groupId, quint8 node, int channel, QString *error)
{
    if (!ensureGroupExists(deviceGroups, groupId, error)) {
        return false;
    }

    const int channelKey = makeChannelKey(node, static_cast<quint8>(channel));
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
    if (!ensureNodeIdInRange(config.nodeId, error)) {
        return false;
    }

    const quint8 node = static_cast<quint8>(config.nodeId);
    if (relays.contains(node)) {
        setErrorIfPresent(error, QStringLiteral("device already exists"));
        return false;
    }

    // Currently only support RelayGd427 device type
    if (config.deviceType == device::DeviceTypeId::RelayGd427 &&
        isRelayGd427CommSupported(config.commType)) {
        auto *dev = createRelayGd427Device(node, config.commType, canBus, canManager,
                                           deviceTcpServer_, this);
        registerRelayDevice(relays, deviceConfigs, node, dev, config);

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

    setErrorIfPresent(error, QStringLiteral("unsupported device type"));
    return false;
}

bool CoreContext::removeDevice(quint8 nodeId, QString *error)
{
    if (!relays.contains(nodeId) && !deviceConfigs.contains(nodeId)) {
        setErrorIfPresent(error, QStringLiteral("device not found"));
        return false;
    }

    removeNodeFromAllGroups(deviceGroups, nodeId);
    removeNodeChannelsFromAllGroups(groupChannels, nodeId);
    detachAndDeleteRelayIfPresent(relays, deviceConfigs, canManager, nodeId);

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

QJsonObject CoreContext::getDeviceParams(quint8 nodeId) const
{
    return getDeviceConfig(nodeId).params;
}

bool CoreContext::setDeviceParams(quint8 nodeId, const QJsonObject &params, bool merge, QString *error)
{
    auto it = deviceConfigs.find(nodeId);
    if (it == deviceConfigs.end()) {
        setErrorIfPresent(error, QStringLiteral("device not found"));
        return false;
    }

    if (merge) {
        it->params = mergeDeviceParams(it->params, params);
    } else {
        it->params = params;
    }
    return true;
}

bool CoreContext::checkActionValid(const AutoStrategy &arr, QString *errMsg)
{

    for (auto a : arr.actions) {
        int nodeId = 0;
        int channel = 0;

        if (!cloud::fanzhoucloud::parseNodeChannelKey(a.identifier, nodeId, channel)) {
            setErrorIfPresent(errMsg, QString("invalid identifier format: %1").arg(a.identifier));
            return false;
        }

        // 1. 校验 node 是否存在
        auto devIt = relays.find(nodeId);
        if (devIt == relays.end()) {
            setErrorIfPresent(errMsg, QString("device node not exist: %1").arg(nodeId));
            return false;
        }

        // 2. 校验通道范围
        int maxChannels = kMaxChannelId;
        if (channel < 0 || channel > maxChannels) {
            if (errMsg) {
                *errMsg = QString("invalid channel index: node_%1_sw%2")
                        .arg(nodeId)
                        .arg(channel + 1);
            }
            return false;
        }

        // 3. 校验值范围（继电器一般只能 0/1）
        if (a.identifierValue > 2) {
            setErrorIfPresent(errMsg, QString("invalid value for %1").arg(a.identifier));
            return false;
        }
    }

    return true;
}


void CoreContext::onLocalSensorReport(int nodeId, int channel, double rawValue)
{
    for (auto it = sensorConfigs.begin(); it != sensorConfigs.end(); ++it) {
        const auto &cfg = it.value();

        if (cfg.source != SensorSource::Local)
            continue;

        if (cfg.nodeId == nodeId && cfg.channel == channel) {
            const double value = rawValue * cfg.scale + cfg.offset;

            sensorValues[cfg.sensorId] = value;
            sensorUpdateTime[cfg.sensorId] = QDateTime::currentDateTime();

            LOG_DEBUG(kLogSource,
                      QStringLiteral("local sensor update: %1 node=%2 ch=%3 raw=%4 value=%5")
                          .arg(cfg.sensorId)
                          .arg(nodeId)
                          .arg(channel)
                          .arg(rawValue)
                          .arg(value));
        }
    }
}

void CoreContext::onMqttSensorMessage(const int channelId,
                                      const QString &topic,
                                      const QJsonObject &payload)
{
    const QDateTime now = QDateTime::currentDateTime();

    LOG_DEBUG(kLogSource,
              QStringLiteral("mqtt sensor message: ch=%1 topic=%2 payload=%3")
                  .arg(channelId)
                  .arg(topic)
                  .arg(QString(QJsonDocument(payload).toJson(QJsonDocument::Compact))));


    for (auto it = sensorConfigs.begin(); it != sensorConfigs.end(); ++it) {
        const SensorNodeConfig &cfg = it.value();
        LOG_DEBUG(kLogSource,
            QStringLiteral("check sensor cfg: id=%1 source=%2 ch=%3 jsonPath=%4")
                .arg(cfg.sensorId)
                .arg(int(cfg.source))
                .arg(cfg.mqttChannelId)
                .arg(cfg.jsonPath));

        if (cfg.source != SensorSource::Mqtt)
            continue;

        if (cfg.mqttChannelId != channelId)
            continue;

        QJsonValue v(payload);
        const QStringList keys = cfg.jsonPath.split('.');

        for (const QString &key : keys) {
            if (key.isEmpty())
                continue;

            if (!v.isObject()) {
                v = QJsonValue();
                break;
            }
            v = v.toObject().value(key);
        }

        if (v.isUndefined() || v.isNull()) {
            LOG_DEBUG(kLogSource,
                      QStringLiteral("mqtt sensor [%1] jsonPath not found: %2")
                          .arg(cfg.sensorId)
                          .arg(cfg.jsonPath));
            continue;
        }

        sensorValues[cfg.sensorId] = v.toVariant();
        sensorUpdateTime[cfg.sensorId] = now;

        LOG_DEBUG(kLogSource,
                  QStringLiteral("mqtt sensor update: %1 value=%2")
                      .arg(cfg.sensorId)
                      .arg(v.toVariant().toString()));
    }
}

void CoreContext::updateRelaySensorValue(quint8 nodeId, quint8 channel,
                                         const device::RelayProtocol::Status &status)
{
    // 为继电器状态自动生成传感器ID格式: node_{nodeId}_sw{channel+1}_status
    // 以及电流值: node_{nodeId}_sw{channel+1}_current
    const QString statusSensorId = QStringLiteral("node_%1_sw%2_status")
                                       .arg(nodeId)
                                       .arg(channel + 1);
    const QString currentSensorId = QStringLiteral("node_%1_sw%2_current")
                                        .arg(nodeId)
                                        .arg(channel + 1);

    const QDateTime now = QDateTime::currentDateTime();

    // 更新状态值（0=停止，1=正转，2=反转）
    const int modeValue = device::RelayProtocol::modeBits(status.statusByte);
    sensorValues[statusSensorId] = modeValue;
    sensorUpdateTime[statusSensorId] = now;

    // 更新电流值
    sensorValues[currentSensorId] = status.currentA;
    sensorUpdateTime[currentSensorId] = now;

    // 如果传感器配置不存在，自动创建（用于策略调用）
    if (!sensorConfigs.contains(statusSensorId)) {
        SensorNodeConfig cfg;
        cfg.sensorId = statusSensorId;
        cfg.name = QStringLiteral("Node%1 Ch%2 Status").arg(nodeId).arg(channel);
        cfg.source = SensorSource::Local;
        cfg.valueType = SensorValueType::Int;
        cfg.nodeId = nodeId;
        cfg.channel = channel;
        cfg.unit = QStringLiteral("");
        cfg.enabled = true;
        sensorConfigs.insert(statusSensorId, cfg);
    }

    if (!sensorConfigs.contains(currentSensorId)) {
        SensorNodeConfig cfg;
        cfg.sensorId = currentSensorId;
        cfg.name = QStringLiteral("Node%1 Ch%2 Current").arg(nodeId).arg(channel);
        cfg.source = SensorSource::Local;
        cfg.valueType = SensorValueType::Double;
        cfg.nodeId = nodeId;
        cfg.channel = channel;
        cfg.unit = QStringLiteral("A");
        cfg.enabled = true;
        sensorConfigs.insert(currentSensorId, cfg);
    }
}


ScreenConfig CoreContext::getScreenConfig() const
{
    return screenConfig;
}

bool CoreContext::setScreenConfig(const ScreenConfig &config, QString *error)
{
    if (config.brightness < 0 || config.brightness > 100) {
        setErrorIfPresent(error, QStringLiteral("brightness must be 0-100"));
        return false;
    }
    if (config.contrast < 0 || config.contrast > 100) {
        setErrorIfPresent(error, QStringLiteral("contrast must be 0-100"));
        return false;
    }
    if (config.sleepTimeoutSec < 0) {
        setErrorIfPresent(error, QStringLiteral("sleepTimeoutSec must be >= 0"));
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
 * @param path 配置文件路径，为空则使用configFilePath
 * @param error 错误信息输出
 * @return 成功返回true
 */
bool CoreContext::saveConfig(const QString &path, QString *error)
{
    QString targetPath;
    if (!resolveTargetConfigPath(path, configFilePath,
                                 QStringLiteral("配置文件路径未设置，请先指定configFilePath或提供path参数"),
                                 &targetPath, error)) {
        LOG_WARNING(kLogSource, QStringLiteral("saveConfig failed: config file path not set"));
        return false;
    }


    // 屏幕配置
    coreConfig.screen = screenConfig;
    
    // 设备列表
    coreConfig.devices = buildDeviceConfigListFromRuntime(deviceConfigs);

    // 配置结构校验（防止写入无效配置）
    QString schemaError;
    if (!validateDeviceSchemaForSave(coreConfig.devices, coreConfig.can.interface, &schemaError)) {
        setErrorIfPresent(error, QStringLiteral("保存配置失败: %1").arg(schemaError));
        LOG_WARNING(kLogSource,
                    QStringLiteral("saveConfig rejected by schema validation: %1").arg(schemaError));
        return false;
    }
    
    // 设备分组
    coreConfig.groups = buildGroupConfigsFromRuntime(deviceGroups, groupNames, groupSpecialIds, groupCanOptimizeFrame, groupChannels);
    coreConfig.greenhouseState = greenhouseState;
    
    // 定时策略
    coreConfig.strategies = strategies_;

    // ===== 传感器配置 =====
    coreConfig.sensors = buildSensorConfigListFromRuntime(sensorConfigs);

    coreConfig.mqttChannels = getMqttChannelsForSave(mqttManager, coreConfig.mqttChannels);
    // 保存到文件
    QString saveError;
    if (!coreConfig.saveToFile(targetPath, &saveError)) {
        setErrorIfPresent(error, QStringLiteral("保存配置失败: %1").arg(saveError));
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
    QString targetPath;
    if (!resolveTargetConfigPath(path, configFilePath,
                                 QStringLiteral("配置文件路径未设置"),
                                 &targetPath, error)) {
        return false;
    }

    QString loadError;
    if (!coreConfig.loadFromFile(targetPath, &loadError)) {
        setErrorIfPresent(error, QStringLiteral("加载配置失败: %1").arg(loadError));
        LOG_ERROR(kLogSource, QStringLiteral("reloadConfig failed: %1").arg(loadError));
        return false;
    }
    
    // 注意：这里只更新部分配置，不重新初始化整个系统
    // 完整的重新初始化需要重启服务
    
    // 更新分组配置（复用与启动时一致的加载逻辑）
    loadGroupConfigsFromList(coreConfig.groups, deviceGroups, groupNames, groupSpecialIds, groupCanOptimizeFrame, groupChannels);
    
    // 策略运行态（如 lastTriggered）保持不变，避免 reload 时丢失节流状态。
    
    // 更新屏幕配置
    screenConfig = coreConfig.screen;
    
    // 更新云数据上传配置
    cloudUploadConfig = coreConfig.cloudUpload;
    greenhouseState = coreConfig.greenhouseState;

    if (!syncMqttChannelsToManager(mqttManager, coreConfig.mqttChannels)) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("reloadConfig: some MQTT channels failed to sync"));
    }
    
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
    root[QStringLiteral("main")] = buildMainExportObject(coreConfig.main, authConfig);
    root[QStringLiteral("can")] = buildCanExportObject(coreConfig.can, canBus);
    root[QStringLiteral("devices")] = buildDeviceExportArray(deviceConfigs);
    
    // 设备分组
    root[QStringLiteral("groups")] =
        buildExportGroupArray(deviceGroups, groupNames, groupSpecialIds, groupCanOptimizeFrame, groupChannels);
    
    // 策略数量统计
    root[QStringLiteral("strategyCount")] = strategies_.size();
    
    // 屏幕配置
    root[QStringLiteral("screen")] = buildScreenExportObject(screenConfig);
    
    // 配置文件路径
    root[QStringLiteral("configFilePath")] = configFilePath;
    root[QStringLiteral("greenhouse")] = QJsonObject{
        {QStringLiteral("state"), greenhouseState}
    };
    
    return root;
}

// ===================== 认证管理实现 =====================

bool CoreContext::verifyToken(const QString &token) const
{
    // 如果认证未启用，所有token都视为有效
    if (!authConfig.enabled) {
        return true;
    }
    
    // 空token无效
    if (token.isEmpty()) {
        return false;
    }
    
    // 检查预设的静态token列表
    if (authConfig.allowedTokens.contains(token)) {
        return true;
    }
    
    // 检查动态生成的token
    if (validTokens.contains(token)) {
        const qint64 expireMs = validTokens.value(token);
        // 0表示永不过期
        if (expireMs == 0) {
            return true;
        }
        // 检查是否过期
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        return now < expireMs;
    }
    
    return false;
}

bool CoreContext::generateToken(const QString &username, const QString &password,
                                 QString *outToken, QString *error)
{
    // 如果认证未启用，不需要生成token
    if (!authConfig.enabled) {
        setErrorIfPresent(error, QStringLiteral("authentication not enabled"));
        return false;
    }
    
    // 验证密码（简单实现：使用secret作为密码）
    // 当前实现适用于内网/受信环境的基本防护
    if (password != authConfig.secret) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Authentication failed for user: %1").arg(username));
        setErrorIfPresent(error, QStringLiteral("invalid credentials"));
        return false;
    }
    
    // 生成token
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const quint32 random1 = QRandomGenerator::global()->generate();
    const quint32 random2 = QRandomGenerator::global()->generate();
    const QString token = QString::number(now, 16) +
                          QStringLiteral("-") +
                          QString::number(random1, 16) +
                          QString::number(random2, 16);
    
    // 计算过期时间
    qint64 expireMs = 0;  // 0表示永不过期
    if (authConfig.tokenExpireSec > 0) {
        expireMs = now + authConfig.tokenExpireSec * 1000LL;
    }
    
    // 保存token（非const方法，可以直接修改成员变量）
    validTokens.insert(token, expireMs);
    
    LOG_INFO(kLogSource,
             QStringLiteral("Token generated for user: %1, expires: %2")
                 .arg(username)
                 .arg(expireMs > 0 ? QDateTime::fromMSecsSinceEpoch(expireMs).toString() : QStringLiteral("never")));
    
    if (outToken) *outToken = token;
    return true;
}

bool CoreContext::methodRequiresAuth(const QString &method) const
{
    // 如果认证未启用，所有方法都不需要认证
    if (!authConfig.enabled) {
        return false;
    }
    
    // 检查是否是公共方法
    for (const auto &publicMethod : authConfig.publicMethods) {
        if (isPublicMethodPatternMatch(publicMethod, method)) {
            return false;
        }
    }
    
    return true;
}

bool CoreContext::isIpWhitelisted(const QString &ip) const
{
    // 如果认证未启用，不需要检查白名单
    if (!authConfig.enabled) {
        return true;
    }
    
    // 检查是否在白名单中
    for (const auto &whitelistedIp : authConfig.whitelist) {
        if (whitelistedIp == ip || isLoopbackWhitelistMatch(whitelistedIp, ip)) {
            return true;
        }
    }
    
    return false;
}

void CoreContext::trimJobResults()
{
    if (jobResults_.size() <= kMaxJobResults) return;

    // 找到最小的jobId阈值，只保留最近的kMaxJobResults条
    QList<quint64> ids = jobResults_.keys();
    std::sort(ids.begin(), ids.end());
    const int toRemove = ids.size() - kMaxJobResults;
    for (int i = 0; i < toRemove; ++i) {
        jobResults_.remove(ids[i]);
    }
}

void CoreContext::trimDeletedStrategies()
{
    if (deletedStrategies_.isEmpty()) return;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QList<int> expired;
    for (auto it = deletedStrategies_.begin(); it != deletedStrategies_.end(); ++it) {
        if (nowMs - it->deleteMs > kDeletedStrategyTtlMs) {
            expired.append(it.key());
        }
    }
    for (int key : expired) {
        deletedStrategies_.remove(key);
    }
}

}  // namespace core
}  // namespace fanzhou
