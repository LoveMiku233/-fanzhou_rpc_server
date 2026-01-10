/**
 * @file modbus_sensor.h
 * @brief Modbus传感器抽象基类
 *
 * 提供Modbus传感器的通用功能实现。
 */

#ifndef FANZHOU_MODBUS_SENSOR_H
#define FANZHOU_MODBUS_SENSOR_H

#include <QObject>
#include <QString>
#include <QJsonObject>

#include "device/base/device_adapter.h"
#include "device/base/i_sensor.h"
#include "device/device_types.h"

namespace fanzhou {

namespace comm {
class SerialComm;
}

namespace device {

/**
 * @brief Modbus传感器配置
 */
struct ModbusSensorConfig {
    quint8 slaveAddr = 1;           ///< Modbus从机地址
    quint16 registerAddr = 0;       ///< 起始寄存器地址
    quint16 registerCount = 1;      ///< 读取寄存器数量
    int readIntervalMs = 1000;      ///< 读取间隔（毫秒）
    bool enabled = true;            ///< 是否启用
    double scale = 1.0;             ///< 数值缩放因子
    double offset = 0.0;            ///< 数值偏移量
    SensorUnit unit = SensorUnit::None;  ///< 传感器单位
};

/**
 * @brief Modbus传感器抽象基类
 *
 * 为所有Modbus传感器提供通用的实现框架。
 * 子类需要实现具体的数据解析逻辑。
 */
class ModbusSensor : public DeviceAdapter, public ISensor
{
    Q_OBJECT

public:
    /**
     * @brief 构造Modbus传感器
     * @param nodeId 设备节点ID
     * @param config Modbus配置
     * @param comm 串口通信适配器
     * @param parent 父对象
     */
    explicit ModbusSensor(quint8 nodeId, const ModbusSensorConfig &config,
                          comm::SerialComm *comm, QObject *parent = nullptr);

    virtual ~ModbusSensor() = default;

    // DeviceAdapter接口
    bool init() override;
    void poll() override;
    QString name() const override;

    // ISensor接口
    QString sensorName() const override { return name(); }
    SensorReading lastReading() const override { return lastReading_; }
    SensorUnit unit() const override { return config_.unit; }
    bool isAvailable() const override { return available_; }

    /**
     * @brief 获取节点ID
     * @return 设备节点ID
     */
    quint8 nodeId() const { return nodeId_; }

    /**
     * @brief 获取Modbus从机地址
     * @return 从机地址
     */
    quint8 slaveAddr() const { return config_.slaveAddr; }

signals:
    /**
     * @brief 传感器数据更新时发出
     * @param reading 新的读数
     */
    void readingUpdated(const fanzhou::device::SensorReading &reading);

    /**
     * @brief 传感器错误时发出
     * @param error 错误信息
     */
    void sensorError(const QString &error);

protected:
    /**
     * @brief 解析Modbus响应数据
     * @param data 原始响应数据
     * @return 解析后的传感器读数
     * 
     * 子类必须实现此方法来解析具体的传感器数据格式。
     */
    virtual SensorReading parseResponse(const QByteArray &data) = 0;

    /**
     * @brief 构建Modbus读取请求
     * @return 请求数据
     */
    virtual QByteArray buildReadRequest();

    /**
     * @brief 处理接收到的响应
     * @param data 响应数据
     */
    virtual void handleResponse(const QByteArray &data);

    quint8 nodeId_;
    ModbusSensorConfig config_;
    comm::SerialComm *comm_;
    SensorReading lastReading_;
    bool available_ = false;

private slots:
    void onDataReceived(const QByteArray &data);
};

}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_MODBUS_SENSOR_H
