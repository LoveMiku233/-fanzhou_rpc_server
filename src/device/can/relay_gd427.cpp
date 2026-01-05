/**
 * @file relay_gd427.cpp
 * @brief GD427 CAN relay device implementation
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
    // Query all channels on initialization
    for (quint8 ch = 0; ch <= kMaxChannel; ++ch) {
        query(ch);
    }
    return true;
}

void RelayGd427::poll()
{
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

bool RelayGd427::control(quint8 channel, RelayProtocol::Action action)
{
    if (!bus_ || channel > kMaxChannel) {
        return false;
    }

    RelayProtocol::CtrlCmd cmd;
    cmd.type = RelayProtocol::CmdType::ControlRelay;
    cmd.channel = channel;
    cmd.action = action;

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

    RelayProtocol::Status status;
    if (!RelayProtocol::decodeStatus(payload, status)) {
        return;
    }

    if (status.channel > kMaxChannel) {
        return;
    }

    status_[status.channel] = status;
    markSeen();
    lastRxTimer_.restart();

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
