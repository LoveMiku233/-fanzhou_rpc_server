/**
 * @file i_can_device.h
 * @brief CAN device interface
 *
 * Defines the interface for devices that communicate over CAN bus.
 */

#ifndef FANZHOU_I_CAN_DEVICE_H
#define FANZHOU_I_CAN_DEVICE_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

namespace fanzhou {
namespace device {

/**
 * @brief Interface for CAN bus devices
 *
 * Provides methods for receiving and filtering CAN frames.
 */
class ICanDevice
{
public:
    virtual ~ICanDevice() = default;

    /**
     * @brief Get CAN device name
     * @return Device name for identification
     */
    virtual QString canDeviceName() const = 0;

    /**
     * @brief Check if device accepts a specific CAN frame
     * @param canId CAN identifier
     * @param extended True if extended identifier
     * @param rtr True if remote transmission request
     * @return True if device handles this frame
     */
    virtual bool canAccept(quint32 canId, bool extended, bool rtr) const = 0;

    /**
     * @brief Process received CAN frame
     * @param canId CAN identifier
     * @param payload Frame data
     * @param extended True if extended identifier
     * @param rtr True if remote transmission request
     */
    virtual void canOnFrame(quint32 canId, const QByteArray &payload,
                            bool extended, bool rtr) = 0;
};

}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_I_CAN_DEVICE_H
