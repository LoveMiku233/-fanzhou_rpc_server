/**
 * @file uart_sensor.h
 * @brief UART传感器抽象基类
 *
 * 提供UART传感器的通用功能实现。
 */

#ifndef FANZHOU_UART_SENSOR_H
#define FANZHOU_UART_SENSOR_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QByteArray>

#include "device/base/device_adapter.h"
#include "device/base/i_sensor.h"
#include "device/device_types.h"

namespace fanzhou {

namespace comm {
class SerialComm;
}

namespace device {

/**
 * @brief UART传感器配置
 */
struct UartSensorConfig {
    int readIntervalMs = 1000;          ///< 读取间隔（毫秒）
    bool enabled = true;                ///< 是否启用
    QByteArray frameHeader;             ///< 数据帧头标识
    QByteArray frameFooter;             ///< 数据帧尾标识
    int frameLength = 0;                ///< 固定帧长度（0表示变长）
    double scale = 1.0;                 ///< 数值缩放因子
    double offset = 0.0;                ///< 数值偏移量
    SensorUnit unit = SensorUnit::None; ///< 传感器单位
};

/**
 * @brief UART传感器抽象基类
 *
 * 为所有UART传感器提供通用的实现框架。
 * 子类需要实现具体的数据解析逻辑。
 */
class UartSensor : public DeviceAdapter, public ISensor
{
    Q_OBJECT

public:
    /**
     * @brief 构造UART传感器
     * @param nodeId 设备节点ID
     * @param config UART配置
     * @param comm 串口通信适配器
     * @param parent 父对象
     */
    explicit UartSensor(quint8 nodeId, const UartSensorConfig &config,
                        comm::SerialComm *comm, QObject *parent = nullptr);

    virtual ~UartSensor() = default;

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
     * @brief 解析UART数据帧
     * @param frame 完整的数据帧
     * @return 解析后的传感器读数
     * 
     * 子类必须实现此方法来解析具体的传感器数据格式。
     */
    virtual SensorReading parseFrame(const QByteArray &frame) = 0;

    /**
     * @brief 验证数据帧
     * @param frame 数据帧
     * @return 有效返回true
     */
    virtual bool validateFrame(const QByteArray &frame);

    /**
     * @brief 尝试从缓冲区提取完整帧
     * @return 提取的完整帧，失败返回空
     */
    virtual QByteArray extractFrame();

    quint8 nodeId_;
    UartSensorConfig config_;
    comm::SerialComm *comm_;
    SensorReading lastReading_;
    bool available_ = false;
    QByteArray rxBuffer_;  ///< 接收缓冲区

private slots:
    void onDataReceived(const QByteArray &data);
};

}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_UART_SENSOR_H
