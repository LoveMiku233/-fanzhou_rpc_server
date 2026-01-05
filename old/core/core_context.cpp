#include "core_context.h"
#include "core_config.h"
#include "config/system_settings.h"
#include "comm/comm_can.h"
#include "device/can/can_device_manager.h"
#include "device/can/device_relay_can_f427.h"
#include "utils/logger.h"

#include <QDebug>
#include <QDateTime>

static const char* LOG_SOURCE = "核心上下文";
static constexpr int kQueueTickMs = 10;
static constexpr int kMsPerSec = 1000;
static const QString kErrUnknownNode = QStringLiteral("unknown node");
static const QString kErrDeviceNotFound = QStringLiteral("device not found");
static const QString kErrDeviceRejected = QStringLiteral("device rejected");

CoreContext::CoreContext(QObject* parent) : QObject(parent) {}

/**
 * @brief 使用默认配置初始化核心上下文
 */
bool CoreContext::init()
{
    LOG_INFO(LOG_SOURCE, "正在初始化核心上下文(默认配置)...");
    if (!initSystemSettings()) {
        LOG_ERROR(LOG_SOURCE, "初始化系统设置失败");
        return false;
    }
    if (!initCan()) {
        LOG_ERROR(LOG_SOURCE, "初始化CAN总线失败");
        return false;
    }
    if (!initDevices()) {
        LOG_ERROR(LOG_SOURCE, "初始化设备失败");
        return false;
    }
    initQueue();
    bindStrategies({});
    LOG_INFO(LOG_SOURCE, "核心上下文初始化完成");
    return true;
}

/**
 * @brief 使用配置文件初始化核心上下文
 * @param cfg 核心配置对象
 */
bool CoreContext::init(const CoreConfig& cfg)
{
    LOG_INFO(LOG_SOURCE, "正在使用配置初始化核心上下文...");
    LOG_DEBUG(LOG_SOURCE, QString("RPC端口: %1, CAN接口: %2, 波特率: %3")
              .arg(cfg.core_.rpcPort).arg(cfg.can_.canIfname).arg(cfg.can_.canBitrate));
    
    rpcPort = cfg.core_.rpcPort;
    canIfname = cfg.can_.canIfname;
    canBitrate = cfg.can_.canBitrate;
    tripleSampling = cfg.can_.canTripleSampling;

    if (!initSystemSettings()) {
        LOG_ERROR(LOG_SOURCE, "初始化系统设置失败");
        return false;
    }
    if (!initCan()) {
        LOG_ERROR(LOG_SOURCE, "初始化CAN总线失败");
        return false;
    }

    // 从配置初始化设备
    if (!initDevices(cfg)) {
        LOG_ERROR(LOG_SOURCE, "从配置初始化设备失败");
        return false;
    }

    initQueue();
    bindStrategies(cfg.strategies_);
    
    LOG_INFO(LOG_SOURCE, "核心上下文初始化完成");
    return true;
}

/**
 * @brief 初始化系统设置
 */
bool CoreContext::initSystemSettings()
{
    LOG_DEBUG(LOG_SOURCE, "正在初始化系统设置...");
    sys = new SystemSettings(this);

    // 连接信号
    connect(sys, &SystemSettings::commandOutput, this, [](const QString& s){
        LOG_DEBUG("系统设置", QString("[输出] %1").arg(s));
    });

    connect(sys, &SystemSettings::errorOccurred, this, [](const QString& s){
        LOG_WARNING("系统设置", QString("[错误] %1").arg(s));
    });

    connect(sys, &SystemSettings::candumpLine, this, [](const QString& line){
        LOG_DEBUG("CAN抓包", line);
    });

    LOG_INFO(LOG_SOURCE, QString("设置CAN波特率: 接口=%1, 波特率=%2, 三重采样=%3")
             .arg(canIfname).arg(canBitrate).arg(tripleSampling));
    sys->setCanBitrate(canIfname, canBitrate, tripleSampling);
    return true;
}

/**
 * @brief 初始化CAN总线通讯
 */
bool CoreContext::initCan()
{
    LOG_DEBUG(LOG_SOURCE, "正在初始化CAN总线...");
    CanConfig cfg;
    cfg.ifname = canIfname;
    cfg.canFd = false;

    canBus = new CommCan(cfg, this);
    connect(canBus, &CommCan::errorOccurred, this, [](const QString& e){
        LOG_ERROR("CAN", QString("错误: %1").arg(e));
    });

    if (!canBus->open()) {
        LOG_WARNING(LOG_SOURCE, "CAN打开失败，RPC服务仍将启动，但CAN方法将无法使用!");
        // 不返回false，允许RPC服务正常启动
    } else {
        LOG_INFO(LOG_SOURCE, QString("CAN总线打开成功: %1").arg(canIfname));
    }

    canMgr = new CanDeviceManager(canBus, this);
    LOG_DEBUG(LOG_SOURCE, "CAN设备管理器已创建");
    return true;
}

/**
 * @brief 使用默认配置初始化设备
 */
bool CoreContext::initDevices()
{
    LOG_DEBUG(LOG_SOURCE, "正在初始化设备(默认模式)...");
    const QList<quint8> nodes = {0x01};
    for (auto node : nodes) {
        auto* d = new RelayCanDeviceGD427(node, canBus, this);
        d->init();
        canMgr->addDevice(d);
        relays.insert(node, d);
        LOG_INFO(LOG_SOURCE, QString("继电器设备已添加: 节点=0x%1").arg(node, 2, 16, QChar('0')));
    }
    return true;
}

/**
 * @brief 使用配置文件初始化设备
 * @param cfg 核心配置对象
 */
bool CoreContext::initDevices(const CoreConfig& cfg)
{
    LOG_DEBUG(LOG_SOURCE, "正在从配置初始化设备...");
    // 清除旧设备
    relays.clear();
    if (!cfg.devices_.isEmpty()) {
        LOG_INFO(LOG_SOURCE, QString("在配置中发现 %1 个设备").arg(cfg.devices_.size()));
        
        for (const auto& dcfg : cfg.devices_) {

            // 检查设备是否启用
            const bool enabled = dcfg.params.value("enabled").toBool(true);
            if (!enabled) {
                LOG_DEBUG(LOG_SOURCE, QString("设备 '%1' 已禁用，跳过").arg(dcfg.name));
                continue;
            }

            // RelayGD427 + CAN 设备类型
            if (dcfg.deviceType == DeviceTypeId::RelayGD427 &&
                dcfg.commType == CommTypeId::Can) {

                if (dcfg.node_id < 1 || dcfg.node_id > 255) {
                    LOG_WARNING(LOG_SOURCE, QString("配置中节点ID无效: %1, 名称=%2")
                                .arg(dcfg.node_id).arg(dcfg.name));
                    continue;
                }

                const quint8 node = quint8(dcfg.node_id);

                // 防止重复添加
                if (relays.contains(node)) {
                    LOG_WARNING(LOG_SOURCE, QString("配置中继电器节点重复: %1, 跳过").arg(int(node)));
                    continue;
                }

                auto* dev = new RelayCanDeviceGD427(node, canBus, this);
                dev->init();
                canMgr->addDevice(dev);
                relays.insert(node, dev);

                LOG_INFO(LOG_SOURCE, QString("RelayGD427已添加: 节点=0x%1, 名称=%2")
                         .arg(node, 2, 16, QChar('0')).arg(dcfg.name));
            } else {
                LOG_WARNING(LOG_SOURCE, QString("不支持的设备类型/通讯类型: %1/%2, 名称=%3")
                            .arg(int(dcfg.deviceType)).arg(int(dcfg.commType)).arg(dcfg.name));
            }
        }

        // 加载设备组
        deviceGroups.clear();
        groupNames.clear();
        deviceGroups.reserve(cfg.groups_.size());
        groupNames.reserve(cfg.groups_.size());
        
        LOG_INFO(LOG_SOURCE, QString("正在加载 %1 个设备组...").arg(cfg.groups_.size()));
        
        for (const auto& gcfg : cfg.groups_) {
            if (!gcfg.enabled) {
                LOG_DEBUG(LOG_SOURCE, QString("设备组 '%1' 已禁用，跳过").arg(gcfg.name));
                continue;
            }
            
            QList<quint8> nodes;
            nodes.reserve(gcfg.deviceNodes.size());
            for (int nodeId : gcfg.deviceNodes) {
                if (nodeId >= 1 && nodeId <= 255) {
                    nodes.append(quint8(nodeId));
                }
            }
            
            deviceGroups.insert(gcfg.groupId, nodes);
            groupNames.insert(gcfg.groupId, gcfg.name);
            
            LOG_INFO(LOG_SOURCE, QString("设备组已添加: ID=%1, 名称=%2, 设备数=%3")
                     .arg(gcfg.groupId).arg(gcfg.name).arg(nodes.size()));
        }

        return true;
    }
    // 没有配置设备时的警告
    LOG_WARNING(LOG_SOURCE, "未配置任何设备(devices_为空)");
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

ControlJobResult CoreContext::executeJob(const ControlJob& job)
{
    ControlJobResult res;
    res.finishedMs = QDateTime::currentMSecsSinceEpoch();

    auto* dev = relays.value(job.node, nullptr);
    if (!dev) {
        res.message = kErrDeviceNotFound;
        return res;
    }

    const bool ok = dev->control(job.channel, job.action);
    res.ok = ok;
    res.message = ok ? "ok" : kErrDeviceRejected;
    jobResults_.insert(job.id, res);
    lastJobId_ = job.id;
    return res;
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

EnqueueResult CoreContext::enqueueControl(quint8 node, quint8 ch, RelayCanProtocol::Action action, const QString& source, bool forceQueue)
{
    EnqueueResult r;
    if (!controlTimer_) initQueue();
    if (!relays.contains(node)) {
        r.error = kErrUnknownNode;
        return r;
    }

    ControlJob job;
    job.id = nextJobId_++;
    job.node = node;
    job.channel = ch;
    job.action = action;
    job.source = source;
    job.enqueuedMs = QDateTime::currentMSecsSinceEpoch();

    const bool immediate = controlQueue_.isEmpty() && !processingQueue_ && !forceQueue;
    r.accepted = true;
    r.jobId = job.id;

    if (immediate) {
        const auto res = executeJob(job);
        r.executedImmediately = true;
        r.success = res.ok;
        return r;
    }

    controlQueue_.enqueue(job);
    startQueueProcessor();
    return r;
}

GroupControlStats CoreContext::queueGroupControl(int groupId, quint8 ch, RelayCanProtocol::Action action, const QString& source)
{
    GroupControlStats st;
    const QList<quint8> nodes = deviceGroups.value(groupId);
    st.total = nodes.size();

    for (quint8 node : nodes) {
        const auto res = enqueueControl(node, ch, action, source, /*forceQueue=*/true);
        if (!res.accepted) {
            st.missing++;
            continue;
        }
        st.accepted++;
        st.jobIds.append(res.jobId);
    }
    return st;
}

QueueSnapshot CoreContext::queueSnapshot() const
{
    QueueSnapshot s;
    s.pending = controlQueue_.size();
    s.active = controlTimer_ && controlTimer_->isActive();
    s.lastJobId = lastJobId_;
    return s;
}

ControlJobResult CoreContext::jobResult(quint64 jobId) const
{
    return jobResults_.value(jobId, ControlJobResult{});
}

bool CoreContext::createGroup(int groupId, const QString& name, QString* err)
{
    if (groupId < 1) {
        if (err) *err = "groupId must be positive";
        return false;
    }
    if (deviceGroups.contains(groupId)) {
        if (err) *err = "group exists";
        return false;
    }
    deviceGroups.insert(groupId, {});
    groupNames.insert(groupId, name);
    attachStrategiesForGroup(groupId);
    return true;
}

bool CoreContext::deleteGroup(int groupId, QString* err)
{
    if (!deviceGroups.contains(groupId)) {
        if (err) *err = "group not found";
        return false;
    }
    detachStrategiesForGroup(groupId);
    deviceGroups.remove(groupId);
    groupNames.remove(groupId);
    return true;
}

bool CoreContext::addDeviceToGroup(int groupId, quint8 node, QString* err)
{
    if (!deviceGroups.contains(groupId)) {
        if (err) *err = "group not found";
        return false;
    }
    if (!relays.contains(node)) {
        if (err) *err = "device not found";
        return false;
    }
    QList<quint8>& devices = deviceGroups[groupId];
    if (!devices.contains(node)) {
        devices.append(node);
    }
    attachStrategiesForGroup(groupId);
    return true;
}

bool CoreContext::removeDeviceFromGroup(int groupId, quint8 node, QString* err)
{
    if (!deviceGroups.contains(groupId)) {
        if (err) *err = "group not found";
        return false;
    }
    deviceGroups[groupId].removeAll(node);
    return true;
}

RelayCanProtocol::Action CoreContext::parseAction(const QString& s, bool* ok) const
{
    const QString a = s.trimmed().toLower();
    if (ok) *ok = true;
    if (a == "stop" || a == "0") return RelayCanProtocol::Action::Stop;
    if (a == "fwd"  || a == "forward" || a == "1") return RelayCanProtocol::Action::Forward;
    if (a == "rev"  || a == "reverse" || a == "2") return RelayCanProtocol::Action::Reverse;
    if (ok) *ok = false;
    return RelayCanProtocol::Action::Stop;
}

int CoreContext::strategyIntervalMs(const AutoStrategyConfig& cfg) const
{
    return qMax(1, cfg.intervalSec) * kMsPerSec;
}

void CoreContext::bindStrategies(const QList<AutoStrategyConfig>& strategies)
{
    strategyConfigs_ = strategies;
    for (auto* t : strategyTimers_) {
        if (!t) continue;
        t->stop();
        t->deleteLater();
    }
    strategyTimers_.clear();

    for (auto it = deviceGroups.begin(); it != deviceGroups.end(); ++it) {
        attachStrategiesForGroup(it.key());
    }
}

void CoreContext::attachStrategiesForGroup(int groupId)
{
    for (const auto& cfg : strategyConfigs_) {
        if (!cfg.enabled || cfg.groupId != groupId) continue;
        bool okAction = false;
        const auto action = parseAction(cfg.action, &okAction);
        if (!okAction) continue;
        if (!deviceGroups.contains(cfg.groupId)) continue;

        QTimer* timer = strategyTimers_.value(cfg.strategyId, nullptr);
        if (!timer) {
            timer = new QTimer(this);
            timer->setInterval(strategyIntervalMs(cfg));
            const int strategyId = cfg.strategyId;
            const int groupId = cfg.groupId;
            const quint8 channel = cfg.channel;
            const QString strategyName = cfg.name;
            connect(timer, &QTimer::timeout, this, [this, groupId, channel, strategyId, strategyName, action](){
                const QString reason = QString("auto:%1")
                                           .arg(strategyName.isEmpty() ? QString::number(strategyId) : strategyName);
                queueGroupControl(groupId, channel, action, reason);
            });
            strategyTimers_.insert(cfg.strategyId, timer);
        } else {
            timer->setInterval(strategyIntervalMs(cfg));
        }

        if (!cfg.autoStart && timer->isActive()) {
            timer->stop();
        }
        if (cfg.autoStart && !timer->isActive()) {
            timer->start();
        }
    }
}

void CoreContext::detachStrategiesForGroup(int groupId)
{
    for (const auto& cfg : strategyConfigs_) {
        if (cfg.groupId != groupId) continue;
        if (auto* timer = strategyTimers_.value(cfg.strategyId, nullptr)) {
            timer->stop();
        }
    }
}

QList<AutoStrategyState> CoreContext::strategyStates() const
{
    QList<AutoStrategyState> list;
    for (const auto& cfg : strategyConfigs_) {
        AutoStrategyState st;
        st.cfg = cfg;
        auto* timer = strategyTimers_.value(cfg.strategyId, nullptr);
        st.attached = timer != nullptr && deviceGroups.contains(cfg.groupId);
        st.running = timer && timer->isActive();
        list.append(st);
    }
    return list;
}

bool CoreContext::setStrategyEnabled(int strategyId, bool enabled)
{
    bool found = false;
    int groupId = 0;
    for (auto& cfg : strategyConfigs_) {
        if (cfg.strategyId == strategyId) {
            cfg.enabled = enabled;
            found = true;
            groupId = cfg.groupId;
            break;
        }
    }
    if (!found) return false;

    if (enabled) {
        attachStrategiesForGroup(groupId);
    } else {
        // stop timer if exists
        if (auto* timer = strategyTimers_.value(strategyId, nullptr)) {
            timer->stop();
        }
    }
    return true;
}

bool CoreContext::triggerStrategy(int strategyId)
{
    for (const auto& cfg : strategyConfigs_) {
        if (cfg.strategyId != strategyId) continue;
        bool okAction = false;
        const auto action = parseAction(cfg.action, &okAction);
        if (!okAction) return false;
        if (!deviceGroups.contains(cfg.groupId)) return false;
        const auto stats = queueGroupControl(cfg.groupId, cfg.channel, action,
                          QString("manual-strategy:%1").arg(cfg.name.isEmpty() ? QString::number(cfg.strategyId) : cfg.name));
        return stats.accepted > 0;
    }
    return false;
}

QStringList CoreContext::methodGroups() const
{
    return {"rpc.*", "sys.*", "can.*", "relay.*", "group.*", "control.*", "auto.*"};
}
