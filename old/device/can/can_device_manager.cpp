#include "can_device_manager.h"
#include "i_can_device.h"
#include "comm/comm_can.h"
#include "device/base/device_adapter.h"


CanDeviceManager::CanDeviceManager(CommCan* bus, QObject* parent)
    : QObject(parent), bus_(bus)
{
    QObject::connect(bus_, &CommCan::canFrameReceived,
                     this, [this](quint32 canId, const QByteArray& payload, bool extended, bool rtr){
        onCanFrame(canId, payload, extended, rtr);
    });
}


void CanDeviceManager::addDevice(ICanDevice* dev)
{
    if (!dev) return;
    if (devices_.contains(dev)) return;
    devices_.push_back(dev);
}

void CanDeviceManager::removeDevice(ICanDevice* dev)
{
    devices_.removeAll(dev);
}

void CanDeviceManager::onCanFrame(quint32 canId, const QByteArray& payload, bool extended, bool rtr)
{
    for (auto* d : devices_) {
        if (d && d->canAccept(canId, extended, rtr)) {
            d->canOnFrame(canId, payload, extended, rtr);
        }
    }
}

void CanDeviceManager::pollAll()
{

}
