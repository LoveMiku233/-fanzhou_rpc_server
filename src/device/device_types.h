/**
 * @file device_types.h
 * @brief 设备类型定义
 *
 * 定义系统的设备类型、通信类型和接口类型。
 * 注意：串口协议类型定义在 serial/serial_protocol.h 中。
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
 *   11-20:  其他执行器设备
 *   21-50:  串口Modbus传感器
 *   51-80:  CAN传感器
 *   81-100: UART传感器
 */
enum class DeviceTypeId : int {
    // 继电器设备 (1-10)
    RelayGd427 = 1,  ///< GD427 CAN继电器设备

    // 其他执行器 (11-20)
    ActuatorGeneric = 11,       ///< 通用执行器

    // 串口Modbus传感器 (21-50)
    SensorModbusGeneric = 21,   ///< 通用Modbus传感器
    SensorModbusTemp = 22,      ///< 温度传感器 (Modbus)
    SensorModbusHumidity = 23,  ///< 湿度传感器 (Modbus)
    SensorModbusSoil = 24,      ///< 土壤传感器 (Modbus)
    SensorModbusCO2 = 25,       ///< CO2传感器 (Modbus)
    SensorModbusLight = 26,     ///< 光照传感器 (Modbus)
    SensorModbusPH = 27,        ///< pH传感器 (Modbus)
    SensorModbusEC = 28,        ///< EC传感器 (Modbus)
    SensorModbusPressure = 29,  ///< 气压传感器 (Modbus)
    SensorModbusWind = 30,      ///< 风速传感器 (Modbus)
    SensorModbusRain = 31,      ///< 雨量传感器 (Modbus)

    // CAN传感器 (51-80)
    SensorCanGeneric = 51,      ///< 通用CAN传感器
    SensorCanTemp = 52,         ///< 温度传感器 (CAN)
    SensorCanHumidity = 53,     ///< 湿度传感器 (CAN)

    // UART传感器 (81-100)
    SensorUartGeneric = 81,     ///< 通用UART传感器
    SensorUartGps = 82,         ///< GPS传感器 (UART)
    SensorUartPm25 = 83,        ///< PM2.5传感器 (UART)
};


/**
 * @brief 传感器数据来源
 *
 * 注意：
 * - 本地设备：有 nodeId / channel
 * - MQTT：无物理设备（虚拟传感器）
 */
enum class SensorSourceType : int {
    LocalDevice = 1,   ///< 本地设备采集（CAN / Modbus / UART）
    Mqtt = 2,          ///< MQTT 接收的虚拟传感器
};

/**
 * @brief 通信类型标识符
 */
enum class CommTypeId : int {
    Serial = 1,     ///< 串口通信（通用，支持协议选择，参见 serial/serial_protocol.h）
    Can = 2,        ///< CAN总线通信
    Modbus = 3,     ///< Modbus通信（基于串口RS485，已整合到Serial）
    Uart = 4,       ///< UART通信（异步串口，已整合到Serial）
};

/**
 * @brief 接口类型标识符
 *
 * 定义设备的物理接口类型
 */
enum class InterfaceTypeId : int {
    Rs232 = 1,      ///< RS232接口
    Rs485 = 2,      ///< RS485接口
    CanBus = 3,     ///< CAN总线接口
    Uart = 4,       ///< UART接口
    Gpio = 5,       ///< GPIO接口
    I2c = 6,        ///< I2C接口
    Spi = 7,        ///< SPI接口
};

/**
 * @brief 将设备类型转换为字符串
 * @param type 设备类型
 * @return 设备类型名称
 */
inline const char* deviceTypeToString(DeviceTypeId type)
{
    switch (type) {
    // 继电器设备
    case DeviceTypeId::RelayGd427: return "RelayGd427";
    // 执行器
    case DeviceTypeId::ActuatorGeneric: return "ActuatorGeneric";
    // Modbus传感器
    case DeviceTypeId::SensorModbusGeneric: return "SensorModbusGeneric";
    case DeviceTypeId::SensorModbusTemp: return "SensorModbusTemp";
    case DeviceTypeId::SensorModbusHumidity: return "SensorModbusHumidity";
    case DeviceTypeId::SensorModbusSoil: return "SensorModbusSoil";
    case DeviceTypeId::SensorModbusCO2: return "SensorModbusCO2";
    case DeviceTypeId::SensorModbusLight: return "SensorModbusLight";
    case DeviceTypeId::SensorModbusPH: return "SensorModbusPH";
    case DeviceTypeId::SensorModbusEC: return "SensorModbusEC";
    case DeviceTypeId::SensorModbusPressure: return "SensorModbusPressure";
    case DeviceTypeId::SensorModbusWind: return "SensorModbusWind";
    case DeviceTypeId::SensorModbusRain: return "SensorModbusRain";
    // CAN传感器
    case DeviceTypeId::SensorCanGeneric: return "SensorCanGeneric";
    case DeviceTypeId::SensorCanTemp: return "SensorCanTemp";
    case DeviceTypeId::SensorCanHumidity: return "SensorCanHumidity";
    // UART传感器
    case DeviceTypeId::SensorUartGeneric: return "SensorUartGeneric";
    case DeviceTypeId::SensorUartGps: return "SensorUartGps";
    case DeviceTypeId::SensorUartPm25: return "SensorUartPm25";
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
    case CommTypeId::Modbus: return "Modbus";
    case CommTypeId::Uart: return "UART";
    default: return "Unknown";
    }
}

/**
 * @brief 将接口类型转换为字符串
 * @param type 接口类型
 * @return 接口类型名称
 */
inline const char* interfaceTypeToString(InterfaceTypeId type)
{
    switch (type) {
    case InterfaceTypeId::Rs232: return "RS232";
    case InterfaceTypeId::Rs485: return "RS485";
    case InterfaceTypeId::CanBus: return "CAN";
    case InterfaceTypeId::Uart: return "UART";
    case InterfaceTypeId::Gpio: return "GPIO";
    case InterfaceTypeId::I2c: return "I2C";
    case InterfaceTypeId::Spi: return "SPI";
    default: return "Unknown";
    }
}

/**
 * @brief 判断设备类型是否为串口传感器
 * @param type 设备类型
 * @return 是串口传感器返回true（包括Modbus和UART传感器）
 */
inline bool isSerialSensorType(DeviceTypeId type)
{
    const int typeValue = static_cast<int>(type);
    // Modbus传感器 (21-50) || UART传感器 (81-100)
    return (typeValue >= 21 && typeValue <= 50) ||
           (typeValue >= 81 && typeValue <= 100);
}

/**
 * @brief 判断设备类型是否为传感器
 * @param type 设备类型
 * @return 是传感器返回true
 */
inline bool isSensorType(DeviceTypeId type)
{
    const int typeValue = static_cast<int>(type);
    // Modbus传感器 (21-50) || CAN传感器 (51-80) || UART传感器 (81-100)
    return (typeValue >= 21 && typeValue <= 50) ||
           (typeValue >= 51 && typeValue <= 80) ||
           (typeValue >= 81 && typeValue <= 100);
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
 * @brief 判断设备类型是否为CAN传感器
 * @param type 设备类型
 * @return 是CAN传感器返回true
 */
inline bool isCanSensorType(DeviceTypeId type)
{
    const int typeValue = static_cast<int>(type);
    return typeValue >= 51 && typeValue <= 80;
}

/**
 * @brief 判断设备类型是否为UART传感器
 * @param type 设备类型
 * @return 是UART传感器返回true
 */
inline bool isUartSensorType(DeviceTypeId type)
{
    const int typeValue = static_cast<int>(type);
    return typeValue >= 81 && typeValue <= 100;
}

/**
 * @brief 判断设备类型是否为执行器（如继电器）
 * @param type 设备类型
 * @return 是执行器返回true
 */
inline bool isActuatorType(DeviceTypeId type)
{
    const int typeValue = static_cast<int>(type);
    return (typeValue >= 1 && typeValue <= 10) || (typeValue >= 11 && typeValue <= 20);
}

/**
 * @brief 设备类型信息结构
 */
struct DeviceTypeInfo {
    DeviceTypeId id;
    const char* name;
    const char* category;
    CommTypeId defaultCommType;  ///< 默认通信类型
};


struct SensorSourceInfo {
    SensorSourceType id;
    const char* name;
    const char* description;
    const char* last_update;
};

/**
 * @brief 通信类型信息结构
 */
struct CommTypeInfo {
    CommTypeId id;
    const char* name;
    const char* description;
};

/**
 * @brief 接口类型信息结构
 */
struct InterfaceTypeInfo {
    InterfaceTypeId id;
    const char* name;
    const char* description;
};

/**
 * @brief 获取所有支持的设备类型信息
 * @param count 输出设备类型数量
 * @return 设备类型信息数组
 */
inline const DeviceTypeInfo* allDeviceTypes(int &count)
{
    static const DeviceTypeInfo types[] = {
        // 继电器设备
        {DeviceTypeId::RelayGd427, "RelayGd427", "relay", CommTypeId::Can},
        // 执行器
        {DeviceTypeId::ActuatorGeneric, "ActuatorGeneric", "actuator", CommTypeId::Can},
        // Modbus传感器
        {DeviceTypeId::SensorModbusGeneric, "SensorModbusGeneric", "sensor", CommTypeId::Modbus},
        {DeviceTypeId::SensorModbusTemp, "SensorModbusTemp", "sensor", CommTypeId::Modbus},
        {DeviceTypeId::SensorModbusHumidity, "SensorModbusHumidity", "sensor", CommTypeId::Modbus},
        {DeviceTypeId::SensorModbusSoil, "SensorModbusSoil", "sensor", CommTypeId::Modbus},
        {DeviceTypeId::SensorModbusCO2, "SensorModbusCO2", "sensor", CommTypeId::Modbus},
        {DeviceTypeId::SensorModbusLight, "SensorModbusLight", "sensor", CommTypeId::Modbus},
        {DeviceTypeId::SensorModbusPH, "SensorModbusPH", "sensor", CommTypeId::Modbus},
        {DeviceTypeId::SensorModbusEC, "SensorModbusEC", "sensor", CommTypeId::Modbus},
        {DeviceTypeId::SensorModbusPressure, "SensorModbusPressure", "sensor", CommTypeId::Modbus},
        {DeviceTypeId::SensorModbusWind, "SensorModbusWind", "sensor", CommTypeId::Modbus},
        {DeviceTypeId::SensorModbusRain, "SensorModbusRain", "sensor", CommTypeId::Modbus},
        // CAN传感器
        {DeviceTypeId::SensorCanGeneric, "SensorCanGeneric", "sensor", CommTypeId::Can},
        {DeviceTypeId::SensorCanTemp, "SensorCanTemp", "sensor", CommTypeId::Can},
        {DeviceTypeId::SensorCanHumidity, "SensorCanHumidity", "sensor", CommTypeId::Can},
        // UART传感器
        {DeviceTypeId::SensorUartGeneric, "SensorUartGeneric", "sensor", CommTypeId::Uart},
        {DeviceTypeId::SensorUartGps, "SensorUartGps", "sensor", CommTypeId::Uart},
        {DeviceTypeId::SensorUartPm25, "SensorUartPm25", "sensor", CommTypeId::Uart},
    };
    count = sizeof(types) / sizeof(types[0]);
    return types;
}

inline const SensorSourceInfo* allSensorSources(int& count)
{
    static const SensorSourceInfo sources[] = {
        {SensorSourceType::LocalDevice, "LocalDevice", "本地设备采集"},
        {SensorSourceType::Mqtt, "Mqtt", "MQTT 虚拟传感器"},
    };
    count = sizeof(sources) / sizeof(sources[0]);
    return sources;
}

/**
 * @brief 获取所有支持的通信类型信息
 * @param count 输出通信类型数量
 * @return 通信类型信息数组
 */
inline const CommTypeInfo* allCommTypes(int &count)
{
    static const CommTypeInfo types[] = {
        {CommTypeId::Serial, "Serial", "通用串口通信"},
        {CommTypeId::Can, "CAN", "CAN总线通信"},
        {CommTypeId::Modbus, "Modbus", "Modbus RTU/TCP通信"},
        {CommTypeId::Uart, "UART", "UART异步串口通信"},
    };
    count = sizeof(types) / sizeof(types[0]);
    return types;
}

/**
 * @brief 获取所有支持的接口类型信息
 * @param count 输出接口类型数量
 * @return 接口类型信息数组
 */
inline const InterfaceTypeInfo* allInterfaceTypes(int &count)
{
    static const InterfaceTypeInfo types[] = {
        {InterfaceTypeId::Rs232, "RS232", "RS232串口接口"},
        {InterfaceTypeId::Rs485, "RS485", "RS485差分串口接口"},
        {InterfaceTypeId::CanBus, "CAN", "CAN总线接口"},
        {InterfaceTypeId::Uart, "UART", "UART异步串口接口"},
        {InterfaceTypeId::Gpio, "GPIO", "通用IO接口"},
        {InterfaceTypeId::I2c, "I2C", "I2C总线接口"},
        {InterfaceTypeId::Spi, "SPI", "SPI总线接口"},
    };
    count = sizeof(types) / sizeof(types[0]);
    return types;
}

/**
 * @brief 根据设备类型获取默认通信类型
 * @param type 设备类型
 * @return 默认通信类型
 */
inline CommTypeId getDefaultCommType(DeviceTypeId type)
{
    if (isModbusSensorType(type)) {
        return CommTypeId::Modbus;
    } else if (isCanSensorType(type) || type == DeviceTypeId::RelayGd427) {
        return CommTypeId::Can;
    } else if (isUartSensorType(type)) {
        return CommTypeId::Uart;
    }
    return CommTypeId::Serial;
}

}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_DEVICE_TYPES_H
