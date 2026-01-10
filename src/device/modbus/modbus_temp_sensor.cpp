/**
 * @file modbus_temp_sensor.cpp
 * @brief Modbus温度传感器实现
 */

#include "modbus_temp_sensor.h"
#include "utils/logger.h"

#include <QDateTime>

namespace fanzhou {
namespace device {

namespace {
const char* const kLogSource = "ModbusTempSensor";
}  // namespace

ModbusTempSensor::ModbusTempSensor(quint8 nodeId, const ModbusSensorConfig &config,
                                   comm::SerialComm *comm, QObject *parent)
    : ModbusSensor(nodeId, config, comm, parent)
{
    // 设置默认单位为摄氏度
    if (config_.unit == SensorUnit::None) {
        config_.unit = SensorUnit::Celsius;
    }
    // 常见温度传感器的缩放因子（除以10得到实际温度）
    if (config_.scale == 1.0) {
        config_.scale = 0.1;
    }
}

SensorReading ModbusTempSensor::read()
{
    poll();  // 发送读取请求
    return lastReading_;  // 返回上次的读数（异步更新）
}

SensorReading ModbusTempSensor::parseResponse(const QByteArray &data)
{
    SensorReading reading;
    reading.unit = config_.unit;

    // 验证响应格式
    // 格式: 地址(1) + 功能码(1) + 字节数(1) + 数据(N) + CRC(2)
    if (data.size() < 7) {
        reading.valid = false;
        reading.error = QStringLiteral("Response too short");
        return reading;
    }

    quint8 byteCount = static_cast<quint8>(data[2]);
    if (byteCount < 2) {
        reading.valid = false;
        reading.error = QStringLiteral("Invalid byte count");
        return reading;
    }

    // 解析16位有符号温度值（大端序）
    qint16 rawValue = static_cast<qint16>(
        (static_cast<quint8>(data[3]) << 8) |
        static_cast<quint8>(data[4])
    );

    reading.value = static_cast<double>(rawValue);
    reading.valid = true;

    LOG_DEBUG(kLogSource, QStringLiteral("Temperature: raw=%1, value=%2")
              .arg(rawValue).arg(reading.value * config_.scale + config_.offset));

    return reading;
}

}  // namespace device
}  // namespace fanzhou
