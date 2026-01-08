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
    // Query all channels on initialization
    for (quint8 ch = 0; ch <= kMaxChannel; ++ch) {
        query(ch);
    }
    return true;
}

void RelayGd427::poll()
{
    // 简单轮询：每次轮询查询一个通道
    // Simple polling: query one channel per poll cycle
    static quint8 currentChannel = 0;
    query(currentChannel);
    currentChannel = (currentChannel + 1) % (kMaxChannel + 1);
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
 * @brief 控制继电器通道
 * 
 * 发送控制命令到CAN总线。如果命令发送失败，可能的原因：
 * 1. bus_ 为空 - CAN总线未初始化
 * 2. 通道号超出范围 - 有效范围 0-3
 * 3. CAN发送队列满 - 检查 CanComm::sendFrame() 返回值
 * 
 * @param channel 通道号 (0-3)
 * @param action 动作 (Stop/Forward/Reverse)
 * @return 命令是否成功入队发送
 */
bool RelayGd427::control(quint8 channel, RelayProtocol::Action action)
{
    // 检查参数有效性
    if (!bus_ || channel > kMaxChannel) {
        return false;
    }

    // 构建控制命令
    RelayProtocol::CtrlCmd cmd;
    cmd.type = RelayProtocol::CmdType::ControlRelay;
    cmd.channel = channel;
    cmd.action = action;

    // 计算CAN ID并发送帧
    // CAN ID = 控制基地址 + 节点ID
    const quint32 canId = RelayProtocol::kCtrlBaseId + nodeId_;
    return bus_->sendFrame(canId, RelayProtocol::encodeCtrl(cmd), false, false);
}

bool RelayGd427::query(quint8 channel)
{
    if (!bus_ || channel > kMaxChannel) {
        return false;
    }

    RelayProtocol::CtrlCmd cmd;
    cmd.type = RelayProtocol::CmdType::QueryStatus;
    cmd.channel = channel;
    cmd.action = RelayProtocol::Action::Stop;

    const quint32 canId = RelayProtocol::kCtrlBaseId + nodeId_;
    return bus_->sendFrame(canId, RelayProtocol::encodeCtrl(cmd), false, false);
}

void RelayGd427::onStatusFrame(quint32 canId, const QByteArray &payload)
{
    Q_UNUSED(canId);

    // 收到任何来自该节点的CAN帧都更新lastSeenMs，表示节点在线
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
    const quint32 expectedId = RelayProtocol::kStatusBaseId + nodeId_;
    return canId == expectedId;
}

void RelayGd427::canOnFrame(quint32 canId, const QByteArray &payload,
                             bool extended, bool rtr)
{
    Q_UNUSED(extended);
    Q_UNUSED(rtr);
    onStatusFrame(canId, payload);
}

}  // namespace device
}  // namespace fanzhou
