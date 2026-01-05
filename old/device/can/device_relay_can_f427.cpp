#include "device_relay_can_f427.h"
#include "comm/comm_can.h"

#include <QtGlobal>
#include <QDateTime>


RelayCanDeviceGD427::RelayCanDeviceGD427(quint8 nodeId, CommCan* bus, QObject* parent)
    : DeviceAdapter(parent), nodeId_(nodeId), bus_(bus)
{
    lastRx_.start();
}


bool RelayCanDeviceGD427::init()
{
    // 这里可以做：上电查询一次
    for (quint8 ch = 0; ch < 4; ++ch) query(ch);
    return true;
}


void RelayCanDeviceGD427::poll()
{
    // 简单策略：轮询所有通道状态
    static quint8 ch = 0;
    query(ch);
    ch = (ch + 1) % 4;
}


QString RelayCanDeviceGD427::name() const
{
    return QString("RelayCanDevice(nodeId=0x%1)").arg(nodeId_, 2, 16, QLatin1Char('0')).toUpper();
}

void RelayCanDeviceGD427::markSeen()
{
    lastSeenMs_ = QDateTime::currentMSecsSinceEpoch();
}

qint64 RelayCanDeviceGD427::lastSeenMs() const
{
    return lastSeenMs_;
}

bool RelayCanDeviceGD427::control(quint8 channel, RelayCanProtocol::Action action)
{
    if (!bus_) return false;
    if (channel > 3) return false;

    RelayCanProtocol::CtrlCmd cmd;
    cmd.type = RelayCanProtocol::CmdType::ControlRelay;
    cmd.channel = channel;
    cmd.action = action;

    const quint32 canId = RelayCanProtocol::kCtrlBaseId + nodeId_;
    return bus_->sendFrame(canId, RelayCanProtocol::encodeCtrl(cmd), /*extended=*/false, /*rtr=*/false);
}

bool RelayCanDeviceGD427::query(quint8 channel)
{
    if (!bus_) return false;
    if (channel > 3) return false;

    RelayCanProtocol::CtrlCmd cmd;
    cmd.type = RelayCanProtocol::CmdType::QueryStatus;
    cmd.channel = channel;
    cmd.action = RelayCanProtocol::Action::Stop;

    const quint32 canId = RelayCanProtocol::kCtrlBaseId + nodeId_;
    return bus_->sendFrame(canId, RelayCanProtocol::encodeCtrl(cmd), false, false);
}

void RelayCanDeviceGD427::onStatusFrame(quint32 canId, const QByteArray& payload)
{
    Q_UNUSED(canId);

    RelayCanProtocol::Status st;
    if (!RelayCanProtocol::decodeStatus(payload, st)) return;
    if (st.channel > 3) return;

    st_[st.channel] = st;

    markSeen();

    lastRx_.restart();

    emit statusUpdated(st.channel, st);
    emit updated();
}

RelayCanProtocol::Status RelayCanDeviceGD427::lastStatus(quint8 channel) const
{
    if (channel > 3) return RelayCanProtocol::Status{};
    return st_[channel];
}

bool RelayCanDeviceGD427::canAccept(quint32 canId, bool extended, bool rtr) const
{
    if (extended || rtr) return false;
    const quint32 expect = RelayCanProtocol::kStatusBaseId + nodeId_;
    return canId == expect;
}

void RelayCanDeviceGD427::canOnFrame(quint32 canId, const QByteArray& payload, bool extended, bool rtr)
{
    Q_UNUSED(extended);
    Q_UNUSED(rtr);
    onStatusFrame(canId, payload);
}
