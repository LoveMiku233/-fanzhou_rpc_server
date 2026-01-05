#ifndef I_CAN_DEVICE_H
#define I_CAN_DEVICE_H

#include <QtGlobal>
#include <QByteArray>
#include <QString>

class ICanDevice
{
public:
    virtual ~ICanDevice() = default;

    // 设备名称
    virtual QString canDeviceName() const = 0;

    // 是否处理这个 CAN 帧
    virtual bool canAccept(quint32 canId, bool extended, bool rtr) const = 0;

    // 处理帧
    virtual void canOnFrame(quint32 canId, const QByteArray& payload, bool extended, bool rtr) = 0;
};

#endif // I_CAN_DEVICE_H
