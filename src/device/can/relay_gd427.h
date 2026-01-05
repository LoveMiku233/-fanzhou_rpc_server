/**
 * @file relay_gd427.h
 * @brief GD427 CAN relay device
 *
 * Implements control and monitoring for GD427 CAN relay modules.
 */

#ifndef FANZHOU_RELAY_GD427_H
#define FANZHOU_RELAY_GD427_H

#include <QElapsedTimer>
#include <QObject>

#include "device/base/device_adapter.h"
#include "i_can_device.h"
#include "relay_protocol.h"

namespace fanzhou {

namespace comm {
class CanComm;
}

namespace device {

/**
 * @brief GD427 CAN relay device controller
 *
 * Provides control and status monitoring for 4-channel relay modules
 * connected via CAN bus.
 */
class RelayGd427 : public DeviceAdapter, public ICanDevice
{
    Q_OBJECT

public:
    /**
     * @brief Construct a relay device
     * @param nodeId CAN node identifier
     * @param bus CAN communication bus
     * @param parent Parent object
     */
    RelayGd427(quint8 nodeId, comm::CanComm *bus, QObject *parent = nullptr);

    // DeviceAdapter interface
    bool init() override;
    void poll() override;
    QString name() const override;

    /**
     * @brief Get node ID
     * @return CAN node identifier
     */
    quint8 nodeId() const { return nodeId_; }

    /**
     * @brief Control a relay channel
     * @param channel Channel number (0-3)
     * @param action Relay action
     * @return True if command sent successfully
     */
    bool control(quint8 channel, RelayProtocol::Action action);

    /**
     * @brief Query a channel's status
     * @param channel Channel number (0-3)
     * @return True if query sent successfully
     */
    bool query(quint8 channel);

    /**
     * @brief Get last status for a channel
     * @param channel Channel number (0-3)
     * @return Channel status
     */
    RelayProtocol::Status lastStatus(quint8 channel) const;

    /**
     * @brief Get timestamp of last received status
     * @return Milliseconds since epoch, or 0 if never received
     */
    qint64 lastSeenMs() const;

    // ICanDevice interface
    QString canDeviceName() const override { return name(); }
    bool canAccept(quint32 canId, bool extended, bool rtr) const override;
    void canOnFrame(quint32 canId, const QByteArray &payload,
                    bool extended, bool rtr) override;

signals:
    /**
     * @brief Emitted when channel status is updated
     * @param channel Channel number
     * @param status New status
     */
    void statusUpdated(quint8 channel, fanzhou::device::RelayProtocol::Status status);

private:
    void markSeen();
    void onStatusFrame(quint32 canId, const QByteArray &payload);

    quint8 nodeId_;
    comm::CanComm *bus_;
    RelayProtocol::Status status_[4] {};
    QElapsedTimer lastRxTimer_;
    qint64 lastSeenMs_ = 0;

    static constexpr int kMaxChannel = 3;
};

}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_RELAY_GD427_H
