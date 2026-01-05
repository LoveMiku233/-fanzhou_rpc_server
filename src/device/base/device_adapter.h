/**
 * @file device_adapter.h
 * @brief Abstract device adapter interface
 *
 * Defines the base interface for all device adapters.
 */

#ifndef FANZHOU_DEVICE_ADAPTER_H
#define FANZHOU_DEVICE_ADAPTER_H

#include <QObject>
#include <QString>

namespace fanzhou {
namespace device {

/**
 * @brief Abstract base class for device adapters
 *
 * Provides a common interface for different device types.
 * Subclasses implement specific device protocols.
 */
class DeviceAdapter : public QObject
{
    Q_OBJECT

public:
    explicit DeviceAdapter(QObject *parent = nullptr)
        : QObject(parent) {}

    virtual ~DeviceAdapter() = default;

    /**
     * @brief Initialize the device
     * @return True if successful
     */
    virtual bool init() = 0;

    /**
     * @brief Poll the device for status updates
     */
    virtual void poll() = 0;

    /**
     * @brief Get device name
     * @return Device name string
     */
    virtual QString name() const = 0;

signals:
    /**
     * @brief Emitted when device status is updated
     */
    void updated();
};

}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_DEVICE_ADAPTER_H
