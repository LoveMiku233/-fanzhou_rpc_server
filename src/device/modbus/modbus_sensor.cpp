/**
 * @file modbus_sensor.cpp
 * @brief Modbus传感器抽象基类实现
 */

#include "modbus_sensor.h"
#include "comm/serial_comm.h"
#include "utils/logger.h"

#include <QDateTime>

namespace fanzhou {
namespace device {

namespace {
const char* const kLogSource = "ModbusSensor";

// Modbus RTU CRC16计算
quint16 calcCrc16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (int i = 0; i < data.size(); ++i) {
        crc ^= static_cast<quint8>(data[i]);
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

}  // namespace

ModbusSensor::ModbusSensor(quint8 nodeId, const ModbusSensorConfig &config,
                           comm::SerialComm *comm, QObject *parent)
    : DeviceAdapter(parent)
    , nodeId_(nodeId)
    , config_(config)
    , comm_(comm)
{
}

bool ModbusSensor::init()
{
    if (!comm_) {
        LOG_ERROR(kLogSource, QStringLiteral("Serial comm not set for node %1").arg(nodeId_));
        return false;
    }

    connect(comm_, &comm::CommAdapter::bytesReceived,
            this, &ModbusSensor::onDataReceived);

    LOG_INFO(kLogSource, QStringLiteral("ModbusSensor initialized: node=%1, slave=%2")
             .arg(nodeId_).arg(config_.slaveAddr));
    return true;
}

void ModbusSensor::poll()
{
    if (!comm_ || !config_.enabled) {
        return;
    }

    QByteArray request = buildReadRequest();
    if (request.isEmpty()) {
        return;
    }

    comm_->writeBytes(request);
}

QString ModbusSensor::name() const
{
    return QStringLiteral("ModbusSensor_%1").arg(nodeId_);
}

QByteArray ModbusSensor::buildReadRequest()
{
    // 构建Modbus RTU读保持寄存器请求 (功能码03)
    QByteArray request;
    request.reserve(8);

    // 从机地址
    request.append(static_cast<char>(config_.slaveAddr));
    // 功能码 03 - 读保持寄存器
    request.append(static_cast<char>(0x03));
    // 起始地址 (高字节在前)
    request.append(static_cast<char>((config_.registerAddr >> 8) & 0xFF));
    request.append(static_cast<char>(config_.registerAddr & 0xFF));
    // 寄存器数量 (高字节在前)
    request.append(static_cast<char>((config_.registerCount >> 8) & 0xFF));
    request.append(static_cast<char>(config_.registerCount & 0xFF));

    // 添加CRC校验
    quint16 crc = calcCrc16(request);
    request.append(static_cast<char>(crc & 0xFF));        // CRC低字节
    request.append(static_cast<char>((crc >> 8) & 0xFF)); // CRC高字节

    return request;
}

void ModbusSensor::handleResponse(const QByteArray &data)
{
    // 验证最小长度：地址(1) + 功能码(1) + 字节数(1) + 数据(至少2) + CRC(2)
    if (data.size() < 7) {
        SensorReading reading;
        reading.valid = false;
        reading.error = QStringLiteral("Response too short");
        reading.timestampMs = QDateTime::currentMSecsSinceEpoch();
        lastReading_ = reading;
        emit sensorError(reading.error);
        return;
    }

    // 验证从机地址
    if (static_cast<quint8>(data[0]) != config_.slaveAddr) {
        return;  // 不是给我们的响应
    }

    // 验证功能码
    quint8 funcCode = static_cast<quint8>(data[1]);
    if (funcCode == 0x83) {  // 错误响应
        SensorReading reading;
        reading.valid = false;
        reading.error = QStringLiteral("Modbus error: 0x%1")
                        .arg(static_cast<quint8>(data[2]), 2, 16, QLatin1Char('0'));
        reading.timestampMs = QDateTime::currentMSecsSinceEpoch();
        lastReading_ = reading;
        emit sensorError(reading.error);
        return;
    }

    if (funcCode != 0x03) {
        return;  // 不是读保持寄存器响应
    }

    // 验证CRC
    QByteArray dataWithoutCrc = data.left(data.size() - 2);
    quint16 expectedCrc = calcCrc16(dataWithoutCrc);
    quint16 receivedCrc = static_cast<quint8>(data[data.size() - 2]) |
                          (static_cast<quint8>(data[data.size() - 1]) << 8);

    if (expectedCrc != receivedCrc) {
        SensorReading reading;
        reading.valid = false;
        reading.error = QStringLiteral("CRC error");
        reading.timestampMs = QDateTime::currentMSecsSinceEpoch();
        lastReading_ = reading;
        emit sensorError(reading.error);
        return;
    }

    // 解析数据
    SensorReading reading = parseResponse(data);
    reading.timestampMs = QDateTime::currentMSecsSinceEpoch();

    // 应用缩放和偏移
    if (reading.valid) {
        reading.value = reading.value * config_.scale + config_.offset;
        reading.unit = config_.unit;
        available_ = true;
    }

    lastReading_ = reading;
    emit readingUpdated(reading);
    emit updated();
}

void ModbusSensor::onDataReceived(const QByteArray &data)
{
    handleResponse(data);
}

}  // namespace device
}  // namespace fanzhou
