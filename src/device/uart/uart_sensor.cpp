/**
 * @file uart_sensor.cpp
 * @brief UART传感器抽象基类实现
 */

#include "uart_sensor.h"
#include "comm/serial/serial_comm.h"
#include "utils/logger.h"

#include <QDateTime>

namespace fanzhou {
namespace device {

namespace {
const char* const kLogSource = "UartSensor";
constexpr int kMaxBufferSize = 4096;  ///< 最大缓冲区大小
}  // namespace

UartSensor::UartSensor(quint8 nodeId, const UartSensorConfig &config,
                       comm::SerialComm *comm, QObject *parent)
    : DeviceAdapter(parent)
    , nodeId_(nodeId)
    , config_(config)
    , comm_(comm)
{
}

bool UartSensor::init()
{
    if (!comm_) {
        LOG_ERROR(kLogSource, QStringLiteral("Serial comm not set for node %1").arg(nodeId_));
        return false;
    }

    connect(comm_, &comm::CommAdapter::bytesReceived,
            this, &UartSensor::onDataReceived);

    LOG_INFO(kLogSource, QStringLiteral("UartSensor initialized: node=%1").arg(nodeId_));
    return true;
}

void UartSensor::poll()
{
    // UART传感器通常是被动接收数据，不需要主动轮询
    // 如果传感器需要命令触发，子类可以重写此方法
}

QString UartSensor::name() const
{
    return QStringLiteral("UartSensor_%1").arg(nodeId_);
}

bool UartSensor::validateFrame(const QByteArray &frame)
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

QByteArray UartSensor::extractFrame()
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

void UartSensor::onDataReceived(const QByteArray &data)
{
    // 添加到接收缓冲区
    rxBuffer_.append(data);

    // 防止缓冲区溢出
    if (rxBuffer_.size() > kMaxBufferSize) {
        LOG_WARNING(kLogSource, QStringLiteral("Buffer overflow, clearing buffer"));
        rxBuffer_.clear();
        return;
    }

    // 尝试提取完整帧
    while (true) {
        QByteArray frame = extractFrame();
        if (frame.isEmpty()) {
            break;
        }

        // 验证帧
        if (!validateFrame(frame)) {
            LOG_DEBUG(kLogSource, QStringLiteral("Invalid frame discarded"));
            continue;
        }

        // 解析帧
        SensorReading reading = parseFrame(frame);
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
