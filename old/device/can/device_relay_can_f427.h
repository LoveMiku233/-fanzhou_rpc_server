#ifndef DEVICE_RELAY_CAN_F427_H
#define DEVICE_RELAY_CAN_F427_H

#include <QObject>
#include "device/base/device_adapter.h"
#include "device/can/relay_can_protocol.h"
#include "device/can/i_can_device.h"

#include <QObject>
#include <QElapsedTimer>

class CommCan;

class RelayCanDeviceGD427 : public DeviceAdapter, public ICanDevice
{
    Q_OBJECT
public:
    RelayCanDeviceGD427(quint8 nodeId, CommCan* bus, QObject* parent=nullptr);

    bool init() override;
    void poll() override;
    QString name() const override;

    quint8 nodeId() const { return nodeId_; }

    bool control(quint8 channel, RelayCanProtocol::Action action);
    bool query(quint8 channel);


    void markSeen();
    qint64 lastSeenMs() const;


    void onStatusFrame(quint32 canId, const QByteArray& payload);
    RelayCanProtocol::Status lastStatus(quint8 channel) const;

    // ---- ICanDevice ----
    QString canDeviceName() const override { return name(); }
    bool canAccept(quint32 canId, bool extended, bool rtr) const override;
    void canOnFrame(quint32 canId, const QByteArray& payload, bool extended, bool rtr) override;

signals:
    void statusUpdated(quint8 channel, RelayCanProtocol::Status st);

private:
    quint8 nodeId_;
    CommCan* bus_; // ext
    RelayCanProtocol::Status st_[4]{};
    QElapsedTimer lastRx_;

    qint64 lastSeenMs_ = 0;
};

#endif // DEVICE_RELAY_CAN_F427_H
