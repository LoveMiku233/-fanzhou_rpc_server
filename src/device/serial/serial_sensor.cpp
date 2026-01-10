/**
 * @file serial_sensor.cpp
 * @brief 串口传感器抽象基类实现
 */

#include "serial_sensor.h"
#include "comm/serial_comm.h"
#include "utils/logger.h"

#include <QDateTime>

namespace fanzhou {
namespace device {

namespace {
const char* const kLogSource = "SerialSensor";
constexpr int kMaxBufferSize = 4096;  ///< 最大缓冲区大小
}  // namespace

SerialSensor::SerialSensor(quint8 nodeId, const SerialSensorConfig &config,
                           comm::SerialComm *comm, QObject *parent)
    : DeviceAdapter(parent)
    , nodeId_(nodeId)
    , config_(config)
    , comm_(comm)
{
}

bool SerialSensor::init()
{
    if (!comm_) {
        LOG_ERROR(kLogSource, QStringLiteral("Serial comm not set for node %1").arg(nodeId_));
        return false;
    }

    connect(comm_, &comm::CommAdapter::bytesReceived,
            this, &SerialSensor::onDataReceived);

    LOG_INFO(kLogSource, QStringLiteral("SerialSensor initialized: node=%1, protocol=%2")
             .arg(nodeId_)
             .arg(QString::fromLatin1(serialProtocolToString(config_.protocol))));
    return true;
}

void SerialSensor::poll()
{
    if (!comm_ || !config_.enabled) {
        return;
    }

    QByteArray request;
    switch (config_.protocol) {
    case SerialProtocol::Modbus:
        request = buildModbusRequest();
        break;
    case SerialProtocol::Custom:
        request = buildCustomRequest();
        break;
    case SerialProtocol::Raw:
        // Raw协议通常不需要主动请求
        return;
    }

    if (!request.isEmpty()) {
        comm_->writeBytes(request);
    }
}

QString SerialSensor::name() const
{
    return QStringLiteral("SerialSensor_%1").arg(nodeId_);
}

void SerialSensor::setProtocol(SerialProtocol protocol)
{
    if (config_.protocol != protocol) {
        config_.protocol = protocol;
        rxBuffer_.clear();  // 切换协议时清空缓冲区
        LOG_INFO(kLogSource, QStringLiteral("Protocol changed to %1 for node %2")
                 .arg(QString::fromLatin1(serialProtocolToString(protocol)))
                 .arg(nodeId_));
    }
}

quint16 SerialSensor::calcModbusCrc16(const QByteArray &data)
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

QByteArray SerialSensor::buildModbusRequest()
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
    quint16 crc = calcModbusCrc16(request);
    request.append(static_cast<char>(crc & 0xFF));        // CRC低字节
    request.append(static_cast<char>((crc >> 8) & 0xFF)); // CRC高字节

    return request;
}

QByteArray SerialSensor::buildCustomRequest()
{
    // 默认实现：自定义协议通常是被动接收数据
    // 子类可以重写此方法实现主动请求
    return QByteArray();
}

SensorReading SerialSensor::parseModbusResponse(const QByteArray &data)
{
    Q_UNUSED(data);
    // 默认实现，子类应该重写
    SensorReading reading;
    reading.valid = false;
    reading.error = QStringLiteral("parseModbusResponse not implemented");
    return reading;
}

SensorReading SerialSensor::parseCustomFrame(const QByteArray &frame)
{
    Q_UNUSED(frame);
    // 默认实现，子类应该重写
    SensorReading reading;
    reading.valid = false;
    reading.error = QStringLiteral("parseCustomFrame not implemented");
    return reading;
}

bool SerialSensor::validateCustomFrame(const QByteArray &frame)
{
    // 默认验证：检查帧头帧尾
    if (!config_.frameHeader.isEmpty()) {
        if (!frame.startsWith(config_.frameHeader)) {
            return false;
        }
    }
    if (!config_.frameFooter.isEmpty()) {
        if (!frame.endsWith(config_.frameFooter)) {
            return false;
        }
    }
    // 检查帧长度
    if (config_.frameLength > 0 && frame.size() != config_.frameLength) {
        return false;
    }
    return true;
}

QByteArray SerialSensor::extractCustomFrame()
{
    // 如果有帧头，查找帧头位置
    if (!config_.frameHeader.isEmpty()) {
        int headerPos = rxBuffer_.indexOf(config_.frameHeader);
        if (headerPos < 0) {
            // 未找到帧头，清理缓冲区
            rxBuffer_.clear();
            return QByteArray();
        }
        // 丢弃帧头之前的数据
        if (headerPos > 0) {
            rxBuffer_.remove(0, headerPos);
        }
    }

    // 固定长度帧
    if (config_.frameLength > 0) {
        if (rxBuffer_.size() >= config_.frameLength) {
            QByteArray frame = rxBuffer_.left(config_.frameLength);
            rxBuffer_.remove(0, config_.frameLength);
            return frame;
        }
        return QByteArray();
    }

    // 变长帧，需要帧尾
    if (!config_.frameFooter.isEmpty()) {
        int footerPos = rxBuffer_.indexOf(config_.frameFooter);
        if (footerPos >= 0) {
            int frameEnd = footerPos + config_.frameFooter.size();
            QByteArray frame = rxBuffer_.left(frameEnd);
            rxBuffer_.remove(0, frameEnd);
            return frame;
        }
    }

    return QByteArray();
}

void SerialSensor::onDataReceived(const QByteArray &data)
{
    switch (config_.protocol) {
    case SerialProtocol::Modbus:
        handleModbusResponse(data);
        break;
    case SerialProtocol::Custom:
    case SerialProtocol::Raw:
        handleCustomData(data);
        break;
    }
}

void SerialSensor::handleModbusResponse(const QByteArray &data)
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
    quint16 expectedCrc = calcModbusCrc16(dataWithoutCrc);
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
    SensorReading reading = parseModbusResponse(data);
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

void SerialSensor::handleCustomData(const QByteArray &data)
{
    // 添加到接收缓冲区
    rxBuffer_.append(data);

    // 防止缓冲区溢出
    if (rxBuffer_.size() > kMaxBufferSize) {
        LOG_WARNING(kLogSource, QStringLiteral("Buffer overflow, clearing buffer"));
        rxBuffer_.clear();
        return;
    }

    // Raw协议：直接处理整个缓冲区
    if (config_.protocol == SerialProtocol::Raw) {
        SensorReading reading = parseCustomFrame(rxBuffer_);
        reading.timestampMs = QDateTime::currentMSecsSinceEpoch();

        if (reading.valid) {
            reading.value = reading.value * config_.scale + config_.offset;
            reading.unit = config_.unit;
            available_ = true;
            lastReading_ = reading;
            emit readingUpdated(reading);
            emit updated();
        }
        rxBuffer_.clear();
        return;
    }

    // Custom协议：尝试提取完整帧
    while (true) {
        QByteArray frame = extractCustomFrame();
        if (frame.isEmpty()) {
            break;
        }

        // 验证帧
        if (!validateCustomFrame(frame)) {
            LOG_DEBUG(kLogSource, QStringLiteral("Invalid frame discarded"));
            continue;
        }

        // 解析帧
        SensorReading reading = parseCustomFrame(frame);
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
}

}  // namespace device
}  // namespace fanzhou
