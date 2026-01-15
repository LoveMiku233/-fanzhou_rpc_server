/**
 * @file relay_gd427.cpp
 * @brief GD427 CAN继电器设备实现
 * 
 * 设备控制流程说明：
 * 1. RPC调用 relay.control 到达 RpcRegistry
 * 2. RpcRegistry 调用 CoreContext::enqueueControl() 将命令入队
 * 3. CoreContext::executeJob() 执行命令，调用 RelayGd427::control()
 * 4. control() 通过 CanComm::sendFrame() 发送CAN帧
 * 
 * 协议版本：v1.2
 * - 支持单通道控制（0x10x）和多通道控制（0x12x）
 * - 支持单通道查询（0x14x）和全通道查询（0x16x）
 * - 支持单通道状态响应（0x20x）和自动状态上报（0x22x）
 * - 支持设置命令（0x30x）和设置响应（0x38x）
 * 
 * 如果设备无法控制，请检查：
 * 1. CAN总线是否打开 - 调用 can.status RPC 检查
 * 2. CAN接口是否启动 - 执行 ip link show can0
 * 3. 波特率是否匹配 - 默认 125000
 * 4. 终端电阻是否正确 - 需要 120Ω
 * 5. 接线是否正确 - CAN_H/CAN_L
 */

#include "relay_gd427.h"
#include "comm/can_comm.h"

#include <QDateTime>
#include <QtGlobal>

namespace fanzhou {
namespace device {

RelayGd427::RelayGd427(quint8 nodeId, comm::CanComm *bus, QObject *parent)
    : DeviceAdapter(parent)
    , nodeId_(nodeId)
    , bus_(bus)
{
    lastRxTimer_.start();
}

bool RelayGd427::init()
{
    // 初始化时查询所有通道状态
    return queryAll();
}

void RelayGd427::poll()
{
    // 使用全通道查询替代逐个通道查询，减少CAN总线负载
    queryAll();
}

QString RelayGd427::name() const
{
    return QStringLiteral("RelayGD427(node=0x%1)")
        .arg(nodeId_, 2, 16, QLatin1Char('0'))
        .toUpper();
}

void RelayGd427::markSeen()
{
    lastSeenMs_ = QDateTime::currentMSecsSinceEpoch();
}

qint64 RelayGd427::lastSeenMs() const
{
    return lastSeenMs_;
}

/**
 * @brief 控制单个继电器通道
 * 
 * 发送单通道控制命令到CAN总线（0x10x）。
 * 
 * @param channel 通道号 (0-3)
 * @param action 动作 (Stop/Forward/Reverse)
 * @return 命令是否成功入队发送
 */
bool RelayGd427::control(quint8 channel, RelayProtocol::Action action)
{
    if (!bus_ || channel > kMaxChannel) {
        return false;
    }

    RelayProtocol::CtrlCmd cmd;
    cmd.type = RelayProtocol::CmdType::ControlRelay;
    cmd.channel = channel;
    cmd.action = action;

    // CAN ID = 单通道控制基地址 + 节点ID
    const quint32 canId = RelayProtocol::kSingleCtrlBaseId + nodeId_;
    return bus_->sendFrame(canId, RelayProtocol::encodeCtrl(cmd), false, false);
}

/**
 * @brief 同时控制多个继电器通道
 * 
 * 发送多通道控制命令到CAN总线（0x12x）。
 * 一次性控制所有4个通道。
 * 
 * @param actions 4个通道的动作数组
 * @return 命令是否成功入队发送
 */
bool RelayGd427::controlMulti(const RelayProtocol::Action actions[4])
{
    if (!bus_) {
        return false;
    }

    RelayProtocol::MultiCtrlCmd cmd;
    cmd.type = RelayProtocol::CmdType::MultiControlRelay;
    for (int i = 0; i < 4; ++i) {
        cmd.actions[i] = actions[i];
    }

    // CAN ID = 多通道控制基地址 + 节点ID
    const quint32 canId = RelayProtocol::kMultiCtrlBaseId + nodeId_;
    return bus_->sendFrame(canId, RelayProtocol::encodeMultiCtrl(cmd), false, false);
}

/**
 * @brief 查询单个通道状态
 * 
 * 发送单通道查询命令到CAN总线（0x14x）。
 * 
 * @param channel 通道号 (0-3)
 * @return 查询是否成功入队发送
 */
bool RelayGd427::query(quint8 channel)
{
    if (!bus_ || channel > kMaxChannel) {
        return false;
    }

    // CAN ID = 单通道查询基地址 + 节点ID
    const quint32 canId = RelayProtocol::kSingleQueryBaseId + nodeId_;
    return bus_->sendFrame(canId, RelayProtocol::encodeSingleQuery(channel), false, false);
}

/**
 * @brief 查询所有通道状态
 * 
 * 发送全通道查询命令到CAN总线（0x16x）。
 * 设备会依次响应4个单通道状态帧（0x20x）。
 * 
 * @return 查询是否成功入队发送
 */
bool RelayGd427::queryAll()
{
    if (!bus_) {
        return false;
    }

    // CAN ID = 全通道查询基地址 + 节点ID
    const quint32 canId = RelayProtocol::kAllQueryBaseId + nodeId_;
    return bus_->sendFrame(canId, RelayProtocol::encodeAllQuery(), false, false);
}

/**
 * @brief 设置过流标志
 * 
 * 发送设置过流标志命令到CAN总线（0x30x，命令类型0x17）。
 * 
 * @param channel 通道号（0-3，或0xFF表示所有通道）
 * @param flag 过流标志值
 * @return 命令是否成功入队发送
 */
bool RelayGd427::setOvercurrentFlag(quint8 channel, quint8 flag)
{
    if (!bus_) {
        return false;
    }

    // CAN ID = 设置命令基地址 + 节点ID
    const quint32 canId = RelayProtocol::kSettingsCmdBaseId + nodeId_;
    return bus_->sendFrame(canId, RelayProtocol::encodeSetOvercurrentFlag(channel, flag), false, false);
}

/**
 * @brief 处理单通道状态响应帧（0x20x）
 */
void RelayGd427::onSingleStatusFrame(quint32 canId, const QByteArray &payload)
{
    Q_UNUSED(canId);

    markSeen();
    lastRxTimer_.restart();

    RelayProtocol::Status status;
    if (!RelayProtocol::decodeStatus(payload, status)) {
        return;
    }

    if (status.channel > kMaxChannel) {
        return;
    }

    status_[status.channel] = status;

    emit statusUpdated(status.channel, status);
    emit updated();
}

/**
 * @brief 处理自动状态上报帧（0x22x）
 */
void RelayGd427::onAutoStatusFrame(quint32 canId, const QByteArray &payload)
{
    Q_UNUSED(canId);

    markSeen();
    lastRxTimer_.restart();

    RelayProtocol::AutoStatusReport report;
    if (!RelayProtocol::decodeAutoStatus(payload, report)) {
        return;
    }

    // 保存自动状态上报
    autoStatus_ = report;

    // 同步更新各通道状态
    for (int i = 0; i < 4; ++i) {
        status_[i].channel = static_cast<quint8>(i);
        status_[i].statusByte = static_cast<quint8>(report.status[i]);
        status_[i].phaseLostFlag = report.phaseLost[i] ? 1 : 0;
        status_[i].currentA = report.currentA[i];
        status_[i].overcurrent = report.overcurrent[i];
    }

    emit autoStatusReceived(report);
    emit updated();
}

/**
 * @brief 处理设置响应帧（0x38x）
 */
void RelayGd427::onSettingsRespFrame(quint32 canId, const QByteArray &payload)
{
    Q_UNUSED(canId);

    markSeen();

    RelayProtocol::SettingsResp resp;
    if (!RelayProtocol::decodeSettingsResp(payload, resp)) {
        return;
    }

    emit settingsResponseReceived(static_cast<quint8>(resp.cmdType), static_cast<quint8>(resp.status));
}

RelayProtocol::Status RelayGd427::lastStatus(quint8 channel) const
{
    if (channel > kMaxChannel) {
        return RelayProtocol::Status{};
    }
    return status_[channel];
}

bool RelayGd427::canAccept(quint32 canId, bool extended, bool rtr) const
{
    if (extended || rtr) {
        return false;
    }

    // 计算期望的CAN ID
    const quint32 singleStatusId = RelayProtocol::kSingleStatusBaseId + nodeId_;
    const quint32 autoStatusId = RelayProtocol::kAutoStatusBaseId + nodeId_;
    const quint32 settingsRespId = RelayProtocol::kSettingsRespBaseId + nodeId_;

    // 接受单通道状态响应、自动状态上报和设置响应
    return canId == singleStatusId || canId == autoStatusId || canId == settingsRespId;
}

void RelayGd427::canOnFrame(quint32 canId, const QByteArray &payload,
                             bool extended, bool rtr)
{
    Q_UNUSED(extended);
    Q_UNUSED(rtr);

    const quint32 singleStatusId = RelayProtocol::kSingleStatusBaseId + nodeId_;
    const quint32 autoStatusId = RelayProtocol::kAutoStatusBaseId + nodeId_;
    const quint32 settingsRespId = RelayProtocol::kSettingsRespBaseId + nodeId_;

    if (canId == singleStatusId) {
        onSingleStatusFrame(canId, payload);
    } else if (canId == autoStatusId) {
        onAutoStatusFrame(canId, payload);
    } else if (canId == settingsRespId) {
        onSettingsRespFrame(canId, payload);
    }
}

}  // namespace device
}  // namespace fanzhou
