/**
 * @file modbus_temp_sensor.h
 * @brief Modbus温度传感器
 *
 * 实现基于Modbus协议的温度传感器驱动。
 */

#ifndef FANZHOU_MODBUS_TEMP_SENSOR_H
#define FANZHOU_MODBUS_TEMP_SENSOR_H

#include "modbus_sensor.h"

namespace fanzhou {
namespace device {

/**
 * @brief Modbus温度传感器
 *
 * 读取通过Modbus RTU通信的温度传感器数据。
 * 支持常见的16位有符号/无符号温度数据格式。
 */
class ModbusTempSensor : public ModbusSensor
{
    Q_OBJECT

public:
    /**
     * @brief 构造温度传感器
     * @param nodeId 设备节点ID
     * @param config Modbus配置
     * @param comm 串口通信适配器
     * @param parent 父对象
     */
    explicit ModbusTempSensor(quint8 nodeId, const ModbusSensorConfig &config,
                              comm::SerialComm *comm, QObject *parent = nullptr);

    // ISensor接口
    QString sensorTypeName() const override { return QStringLiteral("temperature"); }
    SensorReading read() override;

protected:
    /**
     * @brief 解析温度数据
     * @param data Modbus响应数据
     * @return 传感器读数
     */
    SensorReading parseResponse(const QByteArray &data) override;
};

}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_MODBUS_TEMP_SENSOR_H
