/**
 * @file serial_temp_sensor.h
 * @brief 串口温度传感器
 *
 * 实现基于串口通信的温度传感器驱动。
 * 支持Modbus协议和自定义协议。
 */

#ifndef FANZHOU_SERIAL_TEMP_SENSOR_H
#define FANZHOU_SERIAL_TEMP_SENSOR_H

#include "serial_sensor.h"

namespace fanzhou {
namespace device {

/**
 * @brief 串口温度传感器
 *
 * 读取通过串口通信的温度传感器数据。
 * 支持Modbus RTU协议和自定义帧协议，可通过配置切换。
 */
class SerialTempSensor : public SerialSensor
{
    Q_OBJECT

public:
    /**
     * @brief 构造温度传感器
     * @param nodeId 设备节点ID
     * @param config 传感器配置
     * @param comm 串口通信适配器
     * @param parent 父对象
     */
    explicit SerialTempSensor(quint8 nodeId, const SerialSensorConfig &config,
                              comm::SerialComm *comm, QObject *parent = nullptr);

    // ISensor接口
    QString sensorTypeName() const override { return QStringLiteral("temperature"); }
    SensorReading read() override;

protected:
    /**
     * @brief 解析Modbus温度数据
     * @param data Modbus响应数据
     * @return 传感器读数
     */
    SensorReading parseModbusResponse(const QByteArray &data) override;

    /**
     * @brief 解析自定义协议温度数据
     * @param frame 数据帧
     * @return 传感器读数
     */
    SensorReading parseCustomFrame(const QByteArray &frame) override;
};

}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_SERIAL_TEMP_SENSOR_H
