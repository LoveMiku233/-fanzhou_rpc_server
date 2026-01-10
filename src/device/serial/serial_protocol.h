/**
 * @file serial_protocol.h
 * @brief 串口通信协议定义
 *
 * 定义串口传感器支持的通信协议类型。
 */

#ifndef FANZHOU_SERIAL_PROTOCOL_H
#define FANZHOU_SERIAL_PROTOCOL_H

namespace fanzhou {
namespace device {

/**
 * @brief 串口通信协议类型
 *
 * 定义串口传感器可使用的通信协议：
 * - Modbus RTU: 标准Modbus RTU协议
 * - Custom: 自定义帧格式协议
 * - Raw: 原始数据流（无帧格式）
 */
enum class SerialProtocol : int {
    Modbus = 0,     ///< Modbus RTU协议
    Custom = 1,     ///< 自定义帧协议
    Raw = 2,        ///< 原始数据流
};

/**
 * @brief 将串口协议类型转换为字符串
 * @param protocol 协议类型
 * @return 协议名称
 */
inline const char* serialProtocolToString(SerialProtocol protocol)
{
    switch (protocol) {
    case SerialProtocol::Modbus: return "Modbus";
    case SerialProtocol::Custom: return "Custom";
    case SerialProtocol::Raw: return "Raw";
    default: return "Unknown";
    }
}

/**
 * @brief 从字符串解析串口协议类型
 * @param str 协议名称字符串
 * @return 协议类型，无效返回Modbus
 */
inline SerialProtocol serialProtocolFromString(const char* str)
{
    if (!str) {
        return SerialProtocol::Modbus;
    }
    if (strcmp(str, "Modbus") == 0 || strcmp(str, "modbus") == 0) {
        return SerialProtocol::Modbus;
    }
    if (strcmp(str, "Custom") == 0 || strcmp(str, "custom") == 0) {
        return SerialProtocol::Custom;
    }
    if (strcmp(str, "Raw") == 0 || strcmp(str, "raw") == 0) {
        return SerialProtocol::Raw;
    }
    return SerialProtocol::Modbus;
}

}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_SERIAL_PROTOCOL_H
