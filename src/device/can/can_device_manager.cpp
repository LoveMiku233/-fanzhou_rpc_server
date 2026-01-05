/**
 * @file can_device_manager.cpp
 * @brief CAN设备管理器实现
 */

#include "can_device_manager.h"
#include "comm/can_comm.h"
#include "i_can_device.h"

namespace fanzhou {
namespace device {

CanDeviceManager::CanDeviceManager(comm::CanComm *bus, QObject *parent)
    : QObject(parent)
    , bus_(bus)
{
    QObject::connect(bus_, &comm::CanComm::canFrameReceived,
                     this, [this](quint32 canId, const QByteArray &payload,
                                  bool extended, bool rtr) {
        onCanFrame(canId, payload, extended, rtr);
    });
}

void CanDeviceManager::addDevice(ICanDevice *device)
{
    if (!device || devices_.contains(device)) {
        return;
    }
    devices_.push_back(device);
}

void CanDeviceManager::removeDevice(ICanDevice *device)
{
    devices_.removeAll(device);
}

void CanDeviceManager::onCanFrame(quint32 canId, const QByteArray &payload,
                                   bool extended, bool rtr)
{
    for (auto *device : devices_) {
        if (device && device->canAccept(canId, extended, rtr)) {
            device->canOnFrame(canId, payload, extended, rtr);
        }
    }
}

void CanDeviceManager::pollAll()
{
    // Polling implementation for devices that need periodic updates
    // 需要定期更新的设备轮询实现
}

}  // namespace device
}  // namespace fanzhou
