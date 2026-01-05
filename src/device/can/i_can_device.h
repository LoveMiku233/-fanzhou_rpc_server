/**
 * @file i_can_device.h
 * @brief CAN设备接口
 *
 * 定义通过CAN总线通信的设备接口。
 */

#ifndef FANZHOU_I_CAN_DEVICE_H
#define FANZHOU_I_CAN_DEVICE_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

namespace fanzhou {
namespace device {

/**
 * @brief CAN总线设备接口
 *
 * 提供接收和过滤CAN帧的方法。
 */
class ICanDevice
{
public:
    virtual ~ICanDevice() = default;

    /**
     * @brief 获取CAN设备名称
     * @return 用于识别的设备名称
     */
    virtual QString canDeviceName() const = 0;

    /**
     * @brief 检查设备是否接受特定CAN帧
     * @param canId CAN标识符
     * @param extended 是否为扩展标识符
     * @param rtr 是否为远程传输请求
     * @return 如果设备处理此帧返回true
     */
    virtual bool canAccept(quint32 canId, bool extended, bool rtr) const = 0;

    /**
     * @brief 处理接收到的CAN帧
     * @param canId CAN标识符
     * @param payload 帧数据
     * @param extended 是否为扩展标识符
     * @param rtr 是否为远程传输请求
     */
    virtual void canOnFrame(quint32 canId, const QByteArray &payload,
                            bool extended, bool rtr) = 0;
};

}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_I_CAN_DEVICE_H
