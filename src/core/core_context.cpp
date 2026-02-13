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
#include "utils/logger.h"

#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QDir>

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
}  // namespace

CoreContext::CoreContext(QObject *parent)
    : QObject(parent)
{
}

bool CoreContext::init()
{
    coreConfig = CoreConfig::makeDefault();
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
    canConfig.canFd = false;

    canBus = new comm::CanComm(canConfig, this);
    connect(canBus, &comm::CanComm::errorOccurred, this, [](const QString &error) {
        LOG_ERROR("CAN", QStringLiteral("Error: %1").arg(error));
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

    sensorConfigs.clear();
    for (const auto &cfg : coreConfig.sensors) {

        // 防御性校验（非常重要）
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

    if (!coreConfig.devices.isEmpty()) {
        LOG_INFO(kLogSource,
                 QStringLiteral("Found %1 devices in config").arg(coreConfig.devices.size()));

        for (const auto &devConfig : coreConfig.devices) {
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
        groupChannels.clear();
        deviceGroups.reserve(coreConfig.groups.size());
        groupNames.reserve(coreConfig.groups.size());
        groupChannels.reserve(coreConfig.groups.size());

        LOG_INFO(kLogSource,
                 QStringLiteral("Loading %1 device groups...").arg(coreConfig.groups.size()));

        for (const auto &grpConfig : coreConfig.groups) {
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
            
            // Load group channels
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

        return true;
    }

    LOG_WARNING(kLogSource, QStringLiteral("No devices configured (devices list empty)"));
    return true;
}


bool CoreContext::initStrategy()
{
    strategys_ = coreConfig.strategies;
    deletedStrategies_.clear();

    autoStrategyScheduler_ = new QTimer(this);
    autoStrategyScheduler_->setInterval(1000); // 每秒扫描一次
    connect(autoStrategyScheduler_, &QTimer::timeout,
            this, &CoreContext::evaluateAllStrategies);
    autoStrategyScheduler_->start();

    LOG_INFO(kLogSource, QStringLiteral("Strategy scheduler initialized with %1 strategies")
                 .arg(strategys_.size()));
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
        LOG_DEBUG(kLogSource,
                  QStringLiteral("strategy[%1] no effective time limit")
                      .arg(s.strategyId));
        return true;
    }

    QTime begin = QTime::fromString(s.effectiveBeginTime, "HH:mm");
    QTime end   = QTime::fromString(s.effectiveEndTime,   "HH:mm");

    if (!begin.isValid() || !end.isValid()) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("strategy[%1] invalid effective time: %2 ~ %3")
                        .arg(s.strategyId)
                        .arg(s.effectiveBeginTime)
                        .arg(s.effectiveEndTime));
        return true;
    }

    bool inRange = false;
    if (begin <= end) {
        inRange = (now >= begin && now <= end);
    } else {
        inRange = (now >= begin || now <= end);
    }

    LOG_DEBUG(kLogSource,
              QStringLiteral("strategy[%1] time check: now=%2 range=%3~%4 result=%5")
                  .arg(s.strategyId)
                  .arg(now.toString("HH:mm"))
                  .arg(begin.toString("HH:mm"))
                  .arg(end.toString("HH:mm"))
                  .arg(inRange));

    return inRange;
}


void CoreContext::executeActions(const QList<StrategyAction> &actions)
{
//    for (auto a : actions) {
//
//    }
}

bool CoreContext::evaluateConditions(const QList<StrategyCondition> &conditions,
                                     qint8 matchType)
{
    if (conditions.isEmpty()) {
        LOG_DEBUG(kLogSource, "evaluateConditions: no conditions, auto pass");
        return true;
    }

    bool hasValidCondition = false;

    LOG_DEBUG(kLogSource,
              QStringLiteral("evaluateConditions: matchType=%1, condCount=%2")
                  .arg(matchType)
                  .arg(conditions.size()));

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
                LOG_DEBUG(kLogSource, "AND mode -> one condition failed");
                return false;
            }
        } else { // OR
            if (ok) {
                LOG_DEBUG(kLogSource, "OR mode -> one condition matched");
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
    const QDateTime now = QDateTime::currentDateTime();

    for (AutoStrategy &s : strategys_) {
        if (deletedStrategies_.contains(s.strategyId)) continue;
        if (s.enabled == false) continue;                  // not enabled, skip

        if (!isInEffectiveTime(s, now.time()))   // not within the effective time
            continue;
        if (s.lastTriggered.isValid()) {
            if (s.lastTriggered.msecsTo(now) < 10000)
                continue;
        }

        if (!evaluateConditions(s.conditions, s.matchType))
            continue;

        s.lastTriggered = now;
        int cnt = 0;
        for (auto &a: s.actions) {
            cnt++;
            const QString source = QStringLiteral("auto:%1 count:%2").arg(s.strategyName).arg(cnt);
            enqueueControl(a.node, a.channel, static_cast<device::RelayProtocol::Action>(a.identifierValue), source, true);
            LOG_DEBUG(kLogSource, source);
        }


//          ;2
//          queueGroupControlOptimized(
//                    s.groupId,
//                    -1,
//                    static_cast<device::RelayProtocol::Action>(a.identifierValue),
//                    reason
//                    );

    }
}

int CoreContext::strategyIntervalMs(const AutoStrategy &config) const
{
    Q_UNUSED(config);
    return 1000; // 1000ms
}


QList<AutoStrategyState> CoreContext::strategyStates() const
{
    QList<AutoStrategyState> states;
    for (const auto &s: strategys_) {
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
    for (auto &s : strategys_) {
        if (s.strategyId == strategyId) {
            s.enabled = enabled;
            LOG_INFO(kLogSource, QStringLiteral("Strategy %1 set enabled=%2")
                     .arg(strategyId)
                     .arg(enabled));
            return true;
        }
    }
    LOG_WARNING(kLogSource, QStringLiteral("Strategy %1 not found").arg(strategyId));
    return false;
}

bool CoreContext::triggerStrategy(int strategyId)
{
    for (auto &s : strategys_) {
        if (s.strategyId == strategyId) {
            if (!s.enabled) {
                LOG_WARNING(kLogSource, QStringLiteral("Strategy %1 is disabled").arg(strategyId));
                return false;
            }

            LOG_INFO(kLogSource, QStringLiteral("Triggering strategy %1").arg(strategyId));
            // 执行策略动作
            int cnt = 0;
            for (auto &a: s.actions) {
                cnt++;
                const QString source = QStringLiteral("auto:%1 count:%2").arg(s.strategyName).arg(cnt);
                enqueueControl(a.node, a.channel, static_cast<device::RelayProtocol::Action>(a.identifierValue), source, true);
            }
            return true;
        }
    }
    LOG_WARNING(kLogSource, QStringLiteral("Strategy %1 not found").arg(strategyId));
    return false;
}

// @
bool CoreContext::createStrategy(const AutoStrategy &config, bool *isUpdate, QString *error)
{
    // 查找现有策略（无视版本号，找到就更新）
    for (int i = 0; i < strategys_.size(); ++i) {
        auto &s = strategys_[i];
        if (s.strategyId != config.strategyId) continue;

        // 找到相同 ID，执行更新
        AutoStrategy old = s;

        // 先复制新配置
        s = config;

        // ===== 无视传入的 config.version =====
        s.version = old.version + 1;
        // =====================================================================

        // 保留运行时状态（不覆盖）
        s.lastTriggered = old.lastTriggered;
        s.cloudChannelId = old.cloudChannelId;
        // s.enabled = old.enabled;  // 如需接受云端的 enabled 状态，删除这行

        if (isUpdate) *isUpdate = true;

        LOG_INFO(kLogSource,
                 QStringLiteral("Updated strategy %1: version %2 -> %3 (auto increment)")
                 .arg(config.strategyId)
                 .arg(old.version)
                 .arg(s.version));

        // ========== 同步到云端（携带 +1 后的版本号）==========

        if (cloudMessageHandler) {
            QJsonObject msg;
            msg.insert("method", QStringLiteral("set"));
            if (!cloudMessageHandler->sendStrategyCommand(s, msg)) {
                LOG_WARNING(kLogSource,
                            QStringLiteral("Failed to sync updated strategy %1 (v%2) to cloud")
                            .arg(config.strategyId)
                            .arg(s.version));
            }
        }
        // ==================================================

        return true;
    }

    // 不存在：新建
    strategys_.append(config);

    // 如果是新建且版本号无效，可设为 1
    if (strategys_.last().version <= 0) {
        strategys_.last().version = 1;
    }

    if (isUpdate) *isUpdate = false;

    LOG_INFO(kLogSource, QStringLiteral("Created strategy %1, version=%2")
             .arg(config.strategyId)
             .arg(strategys_.last().version));

    // ========== 同步新建到云端 ==========
    // 删除sceneId
    if (cloudMessageHandler) {
        QJsonObject msg;
        msg.insert("method", QStringLiteral("set"));
        if (!cloudMessageHandler->sendStrategyCommand(strategys_.last(), msg)) {
            LOG_WARNING(kLogSource,
                       QStringLiteral("Failed to sync created strategy %1 to cloud")
                       .arg(config.strategyId));
        }
    }
    // ====================================

    return true;
}


bool CoreContext::deleteStrategy(int strategyId, QString *error, bool *alreadyDeleted)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (alreadyDeleted) *alreadyDeleted = false;

    for (int i = 0; i < strategys_.size(); ++i) {
        if (strategys_[i].strategyId == strategyId) {
            // 保存策略信息用于云端同步（在移除前获取）
            const int deletedVersion = strategys_[i].version;
            const QString strategyType = strategys_[i].type.isEmpty()
                ? QStringLiteral("scene")
                : strategys_[i].type;

            // 本地删除逻辑
            deletedStrategies_.insert(strategyId,
                                      { deletedVersion, nowMs });
            strategys_.removeAt(i);

            LOG_INFO(kLogSource, QStringLiteral("Deleted strategy %1").arg(strategyId));

            // ========== 同步删除到云端 ==========
            int channelId = cloudMessageHandler->getChannelId();
            if (cloudMessageHandler && channelId >= 0) {
                QJsonObject cloudMsg;
                cloudMsg.insert("data", strategyId);  // 单个ID直接传整数
                cloudMsg.insert("type", strategyType);
                cloudMsg.insert("requestId", QStringLiteral("local_del_%1_%2")
                    .arg(strategyId)
                    .arg(nowMs));
                cloudMsg.insert("timestamp", nowMs);

                if (!cloudMessageHandler->sendDeleteCommand(channelId, cloudMsg)) {
                    LOG_WARNING(kLogSource,
                        QStringLiteral("Failed to sync delete to cloud for strategy %1")
                        .arg(strategyId));
                    // 继续返回 true，本地删除已成功，云端同步失败可重试或记录
                } else {
                    LOG_DEBUG(kLogSource,
                        QStringLiteral("Synced delete to cloud: strategy=%1, channel=%2")
                        .arg(strategyId)
                        .arg(channelId));
                }
            }
            // ====================================

            return true;
        }
    }

    // 已删除的情况（更新标记但不重复通知云端）
    auto it = deletedStrategies_.find(strategyId);
    if (it != deletedStrategies_.end()) {
        it->deleteMs = nowMs;
        if (alreadyDeleted) *alreadyDeleted = true;
        if (error) *error = QStringLiteral("Strategy %1 already deleted").arg(strategyId);
        return false;
    }

    // 不存在的情况
    deletedStrategies_.insert(strategyId, { 0, nowMs });
    if (error) *error = QStringLiteral("StrategyId %1 not found").arg(strategyId);
    return false;
}

bool CoreContext::setStrategyId(int old_id, int new_id)
{
    if (old_id == -1 || new_id <= 0 || old_id == new_id) {
        LOG_ERROR(kLogSource,
                  QStringLiteral("invalid strategy id mapping: old=%1 new=%2")
                  .arg(old_id)
                  .arg(new_id));
        return false;
    }

    // new_id 是否已经存在
    for (const auto &s : strategys_) {
        if (s.strategyId == new_id) {
            LOG_ERROR(kLogSource,
                      QStringLiteral("strategyId %1 already exists, cannot replace old %2")
                      .arg(new_id)
                      .arg(old_id));
            return false;
        }
    }

    for (AutoStrategy &strategy : strategys_) {
        if (strategy.strategyId == old_id) {

            strategy.strategyId = new_id;

            // 云端刚创建，版本一般以云端为准（默认 1）
            if (strategy.version <= 0) {
                strategy.version = 1;
            }

            strategy.updateTime = QDateTime::currentDateTime()
                    .toString("yyyy-MM-dd HH:mm:ss");

            LOG_INFO(kLogSource,
                     QStringLiteral("strategyId updated: %1 -> %2")
                     .arg(old_id)
                     .arg(new_id));

            deletedStrategies_.remove(old_id);
            deletedStrategies_.remove(new_id);

            return true;
        }
    }

    LOG_ERROR(kLogSource,
              QStringLiteral("old strategyId %1 not found when setting newId %2")
              .arg(old_id)
              .arg(new_id));

    return false;
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
    auto fail = [&](const QString &msg) {
        if (error) *error = msg;
        return false;
    };

    // new
    if (s.groupId <= 0) {
        qint32 newGroupId = 1;
        if (!deviceGroups.isEmpty()) {
            newGroupId = deviceGroups.keys().last() + 1;
        }

        QString name = QStringLiteral("auto_strategy_%1").arg(s.strategyId);

        if (!createGroup(newGroupId, name, error)) {
            return false;
        }

        s.groupId = newGroupId;

        LOG_INFO(kLogSource, QStringLiteral("Auto create group for Strategy: strategyId=%1, groupId=%2")
                 .arg(s.strategyId)
                 .arg(s.groupId));

    }

     // updata
    const qint32 groupId = s.groupId;
    for (const auto &a : s.actions) {
        const quint8 node = a.node;
        const quint32 ch = a.channel;

        if (!deviceGroups.contains(groupId) ||
                !deviceGroups[groupId].contains(node)) {

            if (!addDeviceToGroup(groupId, node, error)) {
                return fail(QStringLiteral("addDeviceToGroup failed: group=%1 node=%2")
                            .arg(groupId).arg(node));
            }
        }

        if (ch >= 0) {
            // groupChannels: groupId -> QList<int>
            const QList<int> &chs = groupChannels.value(groupId);

            if (!chs.contains(ch)) {
                if (!addChannelToGroup(groupId, node, ch, error)) {
                    return fail(QStringLiteral("addChannelToGroup failed: group=%1 node=%2 ch=%3")
                                .arg(groupId).arg(node).arg(ch));
                }
            }
        }
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
        // 队列空闲时清理过期数据
        trimJobResults();
        trimDeletedStrategies();
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
                if (!result.accepted) {
                    stats.missing++;
                    continue;
                }
                stats.accepted++;
                stats.jobIds.append(result.jobId);
            }
        }
    } else {
        // 只控制已绑定的特定通道
        // channelKey = nodeId * kChannelKeyMultiplier + channel
        stats.total = channelKeys.size();
        for (int key : channelKeys) {
            const quint8 node = static_cast<quint8>(key / kChannelKeyMultiplier);
            const quint8 ch = static_cast<quint8>(key % kChannelKeyMultiplier);
            const auto result = enqueueControl(node, ch, action, source, true);
            if (!result.accepted) {
                stats.missing++;
                continue;
            }
            stats.accepted++;
            stats.jobIds.append(result.jobId);
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
    GroupControlStats stats;
    
    // 收集需要控制的(节点, 通道)对
    // 使用 QHash<节点ID, QSet<通道ID>> 结构
    QHash<quint8, QSet<quint8>> nodeChannels;
    
    if (channel >= 0 && channel <= kMaxChannelId) {
        // 指定通道：对分组中所有设备的指定通道发送控制
        const QList<quint8> nodes = deviceGroups.value(groupId);
        for (quint8 node : nodes) {
            if (!relays.contains(node)) continue;
            nodeChannels[node].insert(static_cast<quint8>(channel));
        }
    } else {
        // channel=-1：使用分组绑定的通道
        const QList<int> channelKeys = groupChannels.value(groupId, {});
        
        if (channelKeys.isEmpty()) {
            // 没有绑定通道，控制所有设备的所有通道
            const QList<quint8> nodes = deviceGroups.value(groupId);
            for (quint8 node : nodes) {
                if (!relays.contains(node)) continue;
                for (quint8 ch = 0; ch <= kMaxChannelId; ++ch) {
                    nodeChannels[node].insert(ch);
                }
            }
        } else {
            // 只控制绑定的通道
            for (int key : channelKeys) {
                const quint8 node = static_cast<quint8>(key / kChannelKeyMultiplier);
                const quint8 ch = static_cast<quint8>(key % kChannelKeyMultiplier);
                if (!relays.contains(node)) continue;
                nodeChannels[node].insert(ch);
            }
        }
    }
    
    // 统计原始帧数
    stats.originalFrameCount = 0;
    for (auto it = nodeChannels.begin(); it != nodeChannels.end(); ++it) {
        stats.originalFrameCount += it.value().size();
    }
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
            // 使用controlMulti合并发送
            device::RelayProtocol::Action actions[4] = {
                device::RelayProtocol::Action::Stop,
                device::RelayProtocol::Action::Stop,
                device::RelayProtocol::Action::Stop,
                device::RelayProtocol::Action::Stop
            };
            
            // controlMulti会同时设置所有4个通道的状态
            // 对于需要控制的通道，设置为请求的动作
            // 对于未指定的通道，保留其当前状态（从设备缓存读取）
            for (quint8 ch = 0; ch <= kMaxChannelId; ++ch) {
                if (channels.contains(ch)) {
                    actions[ch] = action;
                } else {
                    // 保留当前状态：从设备读取最后状态
                    const auto status = dev->lastStatus(ch);
                    quint8 mode = device::RelayProtocol::modeBits(status.statusByte);
                    if (mode == 1) {
                        actions[ch] = device::RelayProtocol::Action::Forward;
                    } else if (mode == 2) {
                        actions[ch] = device::RelayProtocol::Action::Reverse;
                    } else {
                        actions[ch] = device::RelayProtocol::Action::Stop;
                    }
                }
            }
            
            const bool ok = dev->controlMulti(actions);
            stats.optimizedFrameCount++;
            
            if (ok) {
                stats.accepted += channels.size();
                // 记录一个任务ID表示这批操作
                stats.jobIds.append(nextJobId_++);
            } else {
                stats.missing += channels.size();
            }
            
            LOG_DEBUG(kLogSource, QStringLiteral("[优化] 节点0x%1: 合并%2通道为1帧CAN (来源: %3)")
                .arg(node, 2, 16, QChar('0'))
                .arg(channels.size())
                .arg(source));
        } else {
            // 只有1个通道，使用普通control
            for (quint8 ch : channels) {
                const auto result = enqueueControl(node, ch, action, source, true);
                stats.optimizedFrameCount++;
                if (result.accepted) {
                    stats.accepted++;
                    stats.jobIds.append(result.jobId);
                } else {
                    stats.missing++;
                }
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
    
    // 按节点分组：QHash<节点ID, QHash<通道ID, Action>>
    QHash<quint8, QHash<quint8, device::RelayProtocol::Action>> nodeChannelActions;
    
    for (const auto &item : items) {
        if (item.channel > kMaxChannelId) continue;
        nodeChannelActions[item.node][item.channel] = item.action;
    }
    
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
            // 合并为controlMulti
            device::RelayProtocol::Action actions[4];
            
            // 首先获取所有通道的当前状态
            for (quint8 ch = 0; ch <= kMaxChannelId; ++ch) {
                const auto status = dev->lastStatus(ch);
                quint8 mode = device::RelayProtocol::modeBits(status.statusByte);
                if (mode == 1) {
                    actions[ch] = device::RelayProtocol::Action::Forward;
                } else if (mode == 2) {
                    actions[ch] = device::RelayProtocol::Action::Reverse;
                } else {
                    actions[ch] = device::RelayProtocol::Action::Stop;
                }
            }
            
            // 应用请求的动作
            for (auto chIt = channelActions.begin(); chIt != channelActions.end(); ++chIt) {
                actions[chIt.key()] = chIt.value();
            }
            
            const bool ok = dev->controlMulti(actions);
            result.optimizedFrames++;
            
            if (ok) {
                result.accepted += channelActions.size();
                result.jobIds.append(nextJobId_++);
            } else {
                result.failed += channelActions.size();
            }
            
            LOG_DEBUG(kLogSource, QStringLiteral("[批量] 节点0x%1: 合并%2通道为1帧")
                .arg(node, 2, 16, QChar('0'))
                .arg(channelActions.size()));
        } else {
            // 单个通道
            for (auto chIt = channelActions.begin(); chIt != channelActions.end(); ++chIt) {
                const auto enqResult = enqueueControl(node, chIt.key(), chIt.value(), source, true);
                result.optimizedFrames++;
                if (enqResult.accepted) {
                    result.accepted++;
                    result.jobIds.append(enqResult.jobId);
                } else {
                    result.failed++;
                }
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
        if (error) *error = QStringLiteral("groupId must be positive");
        return false;
    }
    if (deviceGroups.contains(groupId)) {
        if (error) *error = QStringLiteral("group exists");
        return false;
    }
    deviceGroups.insert(groupId, {});
    groupNames.insert(groupId, name);
    return true;
}

bool CoreContext::deleteGroup(int groupId, QString *error)
{
    if (!deviceGroups.contains(groupId)) {
        if (error) *error = QStringLiteral("group not found");
        return false;
    }
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
    // 注意：此函数用于添加特定通道到分组，channel=-1 不适用于此场景
    // 如需添加所有通道，请多次调用此函数或使用 addDeviceToGroup
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

    // Encode node+channel as unique key: node * kChannelKeyMultiplier + channel
    const int channelKey = static_cast<int>(node) * kChannelKeyMultiplier + channel;
    QList<int> &channels = groupChannels[groupId];
    if (!channels.contains(channelKey)) {
        channels.append(channelKey);
    }
    return true;
}

bool CoreContext::removeChannelFromGroup(int groupId, quint8 node, int channel, QString *error)
{
    if (!deviceGroups.contains(groupId)) {
        if (error) *error = QStringLiteral("group not found");
        return false;
    }

    const int channelKey = static_cast<int>(node) * kChannelKeyMultiplier + channel;
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
        const int baseKey = static_cast<int>(nodeId) * kChannelKeyMultiplier;
        channels.erase(
            std::remove_if(channels.begin(), channels.end(),
                           [baseKey](int key) {
                               return key >= baseKey && key < baseKey + kChannelKeyMultiplier;
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

bool CoreContext::checkActionValid(const AutoStrategy &arr, QString *errMsg)
{

    for (auto a : arr.actions) {
        int nodeId = 0;
        int channel = 0;

        if (!cloud::fanzhoucloud::parseNodeChannelKey(a.identifier, nodeId, channel)) {
            if (errMsg) *errMsg = QString("invalid identifier format: %1").arg(a.identifier);
            return false;
        }

        // 1. 校验 node 是否存在
        auto devIt = relays.find(nodeId);
        if (devIt == relays.end()) {
            if (errMsg) *errMsg = QString("device node not exist: %1").arg(nodeId);
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
            if (errMsg) *errMsg = QString("invalid value for %1").arg(a.identifier);
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

    LOG_DEBUG(kLogSource,
              QStringLiteral("relay sensor update: node=%1 ch=%2 status=%3 current=%4A")
                  .arg(nodeId)
                  .arg(channel)
                  .arg(modeValue)
                  .arg(status.currentA, 0, 'f', 2));
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


    // 屏幕配置
    coreConfig.screen = screenConfig;
    
    // 设备列表
    coreConfig.devices = deviceConfigs.values();
    
    // 设备分组
    coreConfig.groups.clear();
    for (auto it = deviceGroups.begin(); it != deviceGroups.end(); ++it) {
        DeviceGroupConfig grp;
        grp.groupId = it.key();
        grp.name = groupNames.value(it.key(), QString());
//        int sId = -1;
//        if (grp.name.startsWith(QStringLiteral("auto_strategy_"))) {
//
//            sId = grp.name.section('_', -1).toInt();
//        }

//        if (sId != -1) {
//            qDebug() << "Found Strategy ID:" << sId << " for Group:" << grp.groupId;
//        }
        grp.enabled = true;
        // 转换设备节点ID列表
        for (quint8 node : it.value()) {
            grp.deviceNodes.append(static_cast<int>(node));
        }
        // 保存分组通道
        grp.channels = groupChannels.value(it.key(), {});
        coreConfig.groups.append(grp);
    }
    
    // 定时策略
    coreConfig.strategies = strategys_;

    // ===== 传感器配置 =====
    coreConfig.sensors.clear();
    for (auto it = sensorConfigs.begin(); it != sensorConfigs.end(); ++it) {
        coreConfig.sensors.append(it.value());
    }

    coreConfig.mqttChannels = mqttManager->allChannelConfigs();
    // 保存到文件
    QString saveError;
    if (!coreConfig.saveToFile(targetPath, &saveError)) {
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

    QString loadError;
    if (!coreConfig.loadFromFile(targetPath, &loadError)) {
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
    
    for (const auto &grpConfig : coreConfig.groups) {
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
    // @TODO
    
    // 更新屏幕配置
    screenConfig = coreConfig.screen;
    
    // 更新云数据上传配置
    cloudUploadConfig = coreConfig.cloudUpload;
    
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
    mainObj[QStringLiteral("rpcPort")] = static_cast<int>(coreConfig.main.rpcPort);
    
    // 认证状态（不包含敏感信息）
    QJsonObject authObj;
    authObj[QStringLiteral("enabled")] = authConfig.enabled;
    authObj[QStringLiteral("tokenExpireSec")] = authConfig.tokenExpireSec;
    authObj[QStringLiteral("whitelistCount")] = authConfig.whitelist.size();
    authObj[QStringLiteral("publicMethodsCount")] = authConfig.publicMethods.size();
    authObj[QStringLiteral("allowedTokensCount")] = authConfig.allowedTokens.size();
    mainObj[QStringLiteral("auth")] = authObj;
    
    root[QStringLiteral("main")] = mainObj;
    
    // CAN配置
    QJsonObject canObj;
    canObj[QStringLiteral("interface")] = coreConfig.can.interface;
    canObj[QStringLiteral("bitrate")] = coreConfig.can.bitrate;
    canObj[QStringLiteral("tripleSampling")] = coreConfig.can.tripleSampling;
    
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
    root[QStringLiteral("strategyCount")] = strategys_.size();
    
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
        if (error) *error = QStringLiteral("authentication not enabled");
        return false;
    }
    
    // 验证密码（简单实现：使用secret作为密码）
    // 当前实现适用于内网/受信环境的基本防护
    if (password != authConfig.secret) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Authentication failed for user: %1").arg(username));
        if (error) *error = QStringLiteral("invalid credentials");
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
        if (publicMethod == method) {
            return false;
        }
        // 支持通配符匹配，如 "rpc.*"
        if (publicMethod.endsWith(QStringLiteral(".*"))) {
            const QString prefix = publicMethod.left(publicMethod.length() - 1);
            if (method.startsWith(prefix)) {
                return false;
            }
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
        if (whitelistedIp == ip) {
            return true;
        }
        // 支持CIDR格式，如 "192.168.1.0/24"（简单实现）
        // 这里只实现了精确匹配和"127.0.0.1"、"localhost"等常见情况
        if (whitelistedIp == QStringLiteral("localhost") &&
            (ip == QStringLiteral("127.0.0.1") || ip == QStringLiteral("::1"))) {
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
