/**
 * @file serial_sensor.h
 * @brief 串口传感器抽象基类
 *
 * 提供统一的串口传感器框架，支持Modbus和自定义协议切换。
 */

#ifndef FANZHOU_SERIAL_SENSOR_H
#define FANZHOU_SERIAL_SENSOR_H

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include "device/base/device_adapter.h"
#include "device/base/i_sensor.h"
#include "device/device_types.h"
#include "serial_protocol.h"

namespace fanzhou {

namespace comm {
class SerialComm;
}

namespace device {

/**
 * @brief 串口传感器配置
 *
 * 统一的配置结构，支持Modbus协议和自定义协议。
 */
struct SerialSensorConfig {
    // 通用配置
    SerialProtocol protocol = SerialProtocol::Modbus;  ///< 通信协议类型
    int readIntervalMs = 1000;          ///< 读取间隔（毫秒）
    bool enabled = true;                ///< 是否启用
    double scale = 1.0;                 ///< 数值缩放因子
    double offset = 0.0;                ///< 数值偏移量
    SensorUnit unit = SensorUnit::None; ///< 传感器单位

    // Modbus协议配置
    quint8 slaveAddr = 1;               ///< Modbus从机地址
    quint16 registerAddr = 0;           ///< 起始寄存器地址
    quint16 registerCount = 1;          ///< 读取寄存器数量

    // 自定义协议配置
    QByteArray frameHeader;             ///< 数据帧头标识
    QByteArray frameFooter;             ///< 数据帧尾标识
    int frameLength = 0;                ///< 固定帧长度（0表示变长）
};

/**
 * @brief 串口传感器抽象基类
 *
 * 为所有串口传感器提供通用的实现框架。
 * 支持Modbus RTU协议和自定义帧协议，可通过配置切换。
 * 子类需要实现具体的数据解析逻辑。
 */
class SerialSensor : public DeviceAdapter, public ISensor
{
    Q_OBJECT

public:
    /**
     * @brief 构造串口传感器
     * @param nodeId 设备节点ID
     * @param config 传感器配置
     * @param comm 串口通信适配器
     * @param parent 父对象
     */
    explicit SerialSensor(quint8 nodeId, const SerialSensorConfig &config,
                          comm::SerialComm *comm, QObject *parent = nullptr);

    virtual ~SerialSensor() = default;

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
     * @brief 获取当前使用的协议类型
     * @return 协议类型
     */
    SerialProtocol protocol() const { return config_.protocol; }

    /**
     * @brief 获取Modbus从机地址
     * @return 从机地址（仅Modbus协议有效）
     */
    quint8 slaveAddr() const { return config_.slaveAddr; }

    /**
     * @brief 设置协议类型
     * @param protocol 新的协议类型
     */
    void setProtocol(SerialProtocol protocol);

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
     * 使用Modbus协议时由子类实现。
     */
    virtual SensorReading parseModbusResponse(const QByteArray &data);

    /**
     * @brief 解析自定义协议数据帧
     * @param frame 完整的数据帧
     * @return 解析后的传感器读数
     *
     * 使用自定义协议时由子类实现。
     */
    virtual SensorReading parseCustomFrame(const QByteArray &frame);

    /**
     * @brief 构建Modbus读取请求
     * @return 请求数据
     */
    virtual QByteArray buildModbusRequest();

    /**
     * @brief 构建自定义协议请求
     * @return 请求数据（如果需要主动请求）
     */
    virtual QByteArray buildCustomRequest();

    /**
     * @brief 验证自定义协议数据帧
     * @param frame 数据帧
     * @return 有效返回true
     */
    virtual bool validateCustomFrame(const QByteArray &frame);

    /**
     * @brief 尝试从缓冲区提取完整帧（自定义协议）
     * @return 提取的完整帧，失败返回空
     */
    virtual QByteArray extractCustomFrame();

    quint8 nodeId_;
    SerialSensorConfig config_;
    comm::SerialComm *comm_;
    SensorReading lastReading_;
    bool available_ = false;
    QByteArray rxBuffer_;  ///< 接收缓冲区

private slots:
    void onDataReceived(const QByteArray &data);

private:
    void handleModbusResponse(const QByteArray &data);
    void handleCustomData(const QByteArray &data);

    static quint16 calcModbusCrc16(const QByteArray &data);
};

}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_SERIAL_SENSOR_H
