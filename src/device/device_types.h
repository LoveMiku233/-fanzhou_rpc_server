/**
 * @file device_types.h
 * @brief Device type definitions
 *
 * Defines device types and communication types for the system.
 */

#ifndef FANZHOU_DEVICE_TYPES_H
#define FANZHOU_DEVICE_TYPES_H

namespace fanzhou {
namespace device {

/**
 * @brief Device type identifiers
 *
 * Device type ranges:
 *   1-10:   CAN relay devices
 *   11-20:  Reserved
 *   21-50:  Serial sensors
 *   51-80:  CAN sensors
 */
enum class DeviceTypeId : int {
    RelayGd427 = 1,  ///< GD427 CAN relay device
    // Future device types can be added here
};

/**
 * @brief Communication type identifiers
 */
enum class CommTypeId : int {
    Serial = 1,  ///< Serial communication
    Can = 2      ///< CAN bus communication
};

}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_DEVICE_TYPES_H
