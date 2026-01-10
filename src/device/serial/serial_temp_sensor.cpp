/**
 * @file serial_temp_sensor.cpp
 * @brief 串口温度传感器实现
 */

#include "serial_temp_sensor.h"
#include "utils/logger.h"

namespace fanzhou {
namespace device {

namespace {
const char* const kLogSource = "SerialTempSensor";
}  // namespace

SerialTempSensor::SerialTempSensor(quint8 nodeId, const SerialSensorConfig &config,
                                   comm::SerialComm *comm, QObject *parent)
    : SerialSensor(nodeId, config, comm, parent)
{
    // 温度传感器默认单位为摄氏度
    if (config_.unit == SensorUnit::None) {
        config_.unit = SensorUnit::Celsius;
    }
}

SensorReading SerialTempSensor::read()
{
    // 触发轮询
    poll();
    return lastReading_;
}

SensorReading SerialTempSensor::parseModbusResponse(const QByteArray &data)
{
    SensorReading reading;
    reading.unit = SensorUnit::Celsius;

    // 数据格式: 地址(1) + 功能码(1) + 字节数(1) + 数据(N) + CRC(2)
    if (data.size() < 7) {
        reading.valid = false;
        reading.error = QStringLiteral("Invalid Modbus response length");
        return reading;
    }

    quint8 byteCount = static_cast<quint8>(data[2]);
    if (data.size() < 3 + byteCount + 2) {
        reading.valid = false;
        reading.error = QStringLiteral("Incomplete Modbus data");
        return reading;
    }

    // 解析温度值（假设为16位有符号整数）
    // 注意：缩放因子由配置中的scale参数控制，在基类中应用
    if (byteCount >= 2) {
        qint16 rawValue = static_cast<qint16>(
            (static_cast<quint8>(data[3]) << 8) | static_cast<quint8>(data[4]));
        reading.value = static_cast<double>(rawValue);
        reading.valid = true;
    } else {
        reading.valid = false;
        reading.error = QStringLiteral("No temperature data");
    }

    return reading;
}

SensorReading SerialTempSensor::parseCustomFrame(const QByteArray &frame)
{
    SensorReading reading;
    reading.unit = SensorUnit::Celsius;

    // 自定义协议解析示例
    // 子类可以重写此方法实现具体的协议解析
    // 这里提供一个简单的默认实现

    if (frame.isEmpty()) {
        reading.valid = false;
        reading.error = QStringLiteral("Empty frame");
        return reading;
    }

    // 移除帧头和帧尾后的数据
    QByteArray payload = frame;
    if (!config_.frameHeader.isEmpty() && payload.startsWith(config_.frameHeader)) {
        payload.remove(0, config_.frameHeader.size());
    }
    if (!config_.frameFooter.isEmpty() && payload.endsWith(config_.frameFooter)) {
        payload.chop(config_.frameFooter.size());
    }

    // 尝试解析温度值
    if (payload.size() >= 2) {
        // 假设数据为16位有符号整数，大端序
        qint16 rawValue = static_cast<qint16>(
            (static_cast<quint8>(payload[0]) << 8) | static_cast<quint8>(payload[1]));
        reading.value = static_cast<double>(rawValue);
        reading.valid = true;
    } else {
        reading.valid = false;
        reading.error = QStringLiteral("Insufficient data for temperature");
    }

    return reading;
}

}  // namespace device
}  // namespace fanzhou
