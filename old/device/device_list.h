#ifndef DEVICE_LIST_H
#define DEVICE_LIST_H

/*  1 - 10   CAN relay type
 *  11 - 20  reserved
 *  21 - 50  serial sensor type
 *  51 - 80  CAN sensor type
 *  ...
 */
enum class DeviceTypeId : int {
    // CAN Relay
    RelayGD427 = 1,

    // 预留示例
    // RelayOther = 2,

    // Serial sensors (example)
    // SerialTemp = 21,

    // CAN sensors (example)
    // CanEnergyMeter = 51,
};


enum class CommTypeId : int {
    Serial = 1,
    Can = 2
};


#endif // DEVICE_LIST_H
