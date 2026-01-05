/**
 * @file can_device_manager.h
 * @brief CAN device manager
 *
 * Manages multiple CAN devices and dispatches received frames.
 */

#ifndef FANZHOU_CAN_DEVICE_MANAGER_H
#define FANZHOU_CAN_DEVICE_MANAGER_H

#include <QObject>
#include <QVector>

namespace fanzhou {

namespace comm {
class CanComm;
}

namespace device {

class ICanDevice;

/**
 * @brief Manager for CAN bus devices
 *
 * Routes received CAN frames to registered devices based on their
 * acceptance filters.
 */
class CanDeviceManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct a CAN device manager
     * @param bus CAN communication bus
     * @param parent Parent object
     */
    explicit CanDeviceManager(comm::CanComm *bus, QObject *parent = nullptr);

    /**
     * @brief Add a device to the manager
     * @param device Device to add (lifetime managed externally)
     */
    void addDevice(ICanDevice *device);

    /**
     * @brief Remove a device from the manager
     * @param device Device to remove
     */
    void removeDevice(ICanDevice *device);

    /**
     * @brief Poll all registered devices
     */
    void pollAll();

private:
    void onCanFrame(quint32 canId, const QByteArray &payload, bool extended, bool rtr);

    comm::CanComm *bus_ = nullptr;
    QVector<ICanDevice *> devices_;
};

}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_CAN_DEVICE_MANAGER_H
