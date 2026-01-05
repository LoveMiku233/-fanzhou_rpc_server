/**
 * @file device_types.h
 * @brief 设备类型定义
 *
 * 定义系统的设备类型和通信类型。
 */

#ifndef FANZHOU_DEVICE_TYPES_H
#define FANZHOU_DEVICE_TYPES_H

namespace fanzhou {
namespace device {

/**
 * @brief 设备类型标识符
 *
 * 设备类型范围：
 *   1-10:   CAN继电器设备
 *   11-20:  保留
 *   21-50:  串口传感器
 *   51-80:  CAN传感器
 */
enum class DeviceTypeId : int {
    RelayGd427 = 1,  ///< GD427 CAN继电器设备
    // 未来可在此添加其他设备类型
};

/**
 * @brief 通信类型标识符
 */
enum class CommTypeId : int {
    Serial = 1,  ///< 串口通信
    Can = 2      ///< CAN总线通信
};

}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_DEVICE_TYPES_H
