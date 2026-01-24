/**
 * @file can_device_manager.h
 * @brief CAN设备管理器
 *
 * 管理多个CAN设备并分发接收到的帧。
 */

#ifndef FANZHOU_CAN_DEVICE_MANAGER_H
#define FANZHOU_CAN_DEVICE_MANAGER_H

#include <QObject>
#include <QList>

namespace fanzhou {

namespace comm {
class CanComm;
}

namespace device {

class ICanDevice;

/**
 * @brief CAN总线设备管理器
 *
 * 根据设备的接受过滤器将接收到的CAN帧路由到已注册的设备。
 */
class CanDeviceManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造CAN设备管理器
     * @param bus CAN通信总线
     * @param parent 父对象
     */
    explicit CanDeviceManager(comm::CanComm *bus, QObject *parent = nullptr);

    /**
     * @brief 向管理器添加设备
     * @param device 要添加的设备（生命周期由外部管理）
     */
    void addDevice(ICanDevice *device);

    /**
     * @brief 从管理器移除设备
     * @param device 要移除的设备
     */
    void removeDevice(ICanDevice *device);

    /**
     * @brief 轮询所有已注册设备
     */
    void pollAll();

private:
    void onCanFrame(quint32 canId, const QByteArray &payload, bool extended, bool rtr);

    comm::CanComm *bus_ = nullptr;
    QList<ICanDevice *> devices_;
};

}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_CAN_DEVICE_MANAGER_H
