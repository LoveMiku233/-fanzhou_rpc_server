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

    // 串口Modbus传感器 (21-50)
    SensorModbusGeneric = 21,   ///< 通用Modbus传感器
    SensorModbusTemp = 22,      ///< 温度传感器 (Modbus)
    SensorModbusHumidity = 23,  ///< 湿度传感器 (Modbus)
    SensorModbusSoil = 24,      ///< 土壤传感器 (Modbus)
    SensorModbusCO2 = 25,       ///< CO2传感器 (Modbus)
    SensorModbusLight = 26,     ///< 光照传感器 (Modbus)
    SensorModbusPH = 27,        ///< pH传感器 (Modbus)
    SensorModbusEC = 28,        ///< EC传感器 (Modbus)

    // CAN传感器 (51-80)
    SensorCanGeneric = 51,      ///< 通用CAN传感器
};

/**
 * @brief 通信类型标识符
 */
enum class CommTypeId : int {
    Serial = 1,  ///< 串口通信
    Can = 2      ///< CAN总线通信
};

/**
 * @brief 将设备类型转换为字符串
 * @param type 设备类型
 * @return 设备类型名称
 */
inline const char* deviceTypeToString(DeviceTypeId type)
{
    switch (type) {
    case DeviceTypeId::RelayGd427: return "RelayGd427";
    case DeviceTypeId::SensorModbusGeneric: return "SensorModbusGeneric";
    case DeviceTypeId::SensorModbusTemp: return "SensorModbusTemp";
    case DeviceTypeId::SensorModbusHumidity: return "SensorModbusHumidity";
    case DeviceTypeId::SensorModbusSoil: return "SensorModbusSoil";
    case DeviceTypeId::SensorModbusCO2: return "SensorModbusCO2";
    case DeviceTypeId::SensorModbusLight: return "SensorModbusLight";
    case DeviceTypeId::SensorModbusPH: return "SensorModbusPH";
    case DeviceTypeId::SensorModbusEC: return "SensorModbusEC";
    case DeviceTypeId::SensorCanGeneric: return "SensorCanGeneric";
    default: return "Unknown";
    }
}

/**
 * @brief 将通信类型转换为字符串
 * @param type 通信类型
 * @return 通信类型名称
 */
inline const char* commTypeToString(CommTypeId type)
{
    switch (type) {
    case CommTypeId::Serial: return "Serial";
    case CommTypeId::Can: return "CAN";
    default: return "Unknown";
    }
}

/**
 * @brief 判断设备类型是否为传感器
 * @param type 设备类型
 * @return 是传感器返回true
 */
inline bool isSensorType(DeviceTypeId type)
{
    const int typeValue = static_cast<int>(type);
    return (typeValue >= 21 && typeValue <= 50) || (typeValue >= 51 && typeValue <= 80);
}

/**
 * @brief 判断设备类型是否为Modbus传感器
 * @param type 设备类型
 * @return 是Modbus传感器返回true
 */
inline bool isModbusSensorType(DeviceTypeId type)
{
    const int typeValue = static_cast<int>(type);
    return typeValue >= 21 && typeValue <= 50;
}

/**
 * @brief 设备类型信息结构
 */
struct DeviceTypeInfo {
    DeviceTypeId id;
    const char* name;
    const char* category;
};

/**
 * @brief 获取所有支持的设备类型信息
 * @param count 输出设备类型数量
 * @return 设备类型信息数组
 */
inline const DeviceTypeInfo* allDeviceTypes(int &count)
{
    static const DeviceTypeInfo types[] = {
        {DeviceTypeId::RelayGd427, "RelayGd427", "relay"},
        {DeviceTypeId::SensorModbusGeneric, "SensorModbusGeneric", "sensor"},
        {DeviceTypeId::SensorModbusTemp, "SensorModbusTemp", "sensor"},
        {DeviceTypeId::SensorModbusHumidity, "SensorModbusHumidity", "sensor"},
        {DeviceTypeId::SensorModbusSoil, "SensorModbusSoil", "sensor"},
        {DeviceTypeId::SensorModbusCO2, "SensorModbusCO2", "sensor"},
        {DeviceTypeId::SensorModbusLight, "SensorModbusLight", "sensor"},
        {DeviceTypeId::SensorModbusPH, "SensorModbusPH", "sensor"},
        {DeviceTypeId::SensorModbusEC, "SensorModbusEC", "sensor"},
        {DeviceTypeId::SensorCanGeneric, "SensorCanGeneric", "sensor"},
    };
    count = sizeof(types) / sizeof(types[0]);
    return types;
}

}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_DEVICE_TYPES_H
