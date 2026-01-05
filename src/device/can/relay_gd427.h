/**
 * @file relay_gd427.h
 * @brief GD427 CAN继电器设备
 *
 * 实现GD427 CAN继电器模块的控制和监控。
 */

#ifndef FANZHOU_RELAY_GD427_H
#define FANZHOU_RELAY_GD427_H

#include <QElapsedTimer>
#include <QObject>

#include "device/base/device_adapter.h"
#include "i_can_device.h"
#include "relay_protocol.h"

namespace fanzhou {

namespace comm {
class CanComm;
}

namespace device {

/**
 * @brief GD427 CAN继电器设备控制器
 *
 * 为通过CAN总线连接的4通道继电器模块提供控制和状态监控。
 */
class RelayGd427 : public DeviceAdapter, public ICanDevice
{
    Q_OBJECT

public:
    /**
     * @brief 构造继电器设备
     * @param nodeId CAN节点标识符
     * @param bus CAN通信总线
     * @param parent 父对象
     */
    RelayGd427(quint8 nodeId, comm::CanComm *bus, QObject *parent = nullptr);

    // DeviceAdapter接口
    bool init() override;
    void poll() override;
    QString name() const override;

    /**
     * @brief 获取节点ID
     * @return CAN节点标识符
     */
    quint8 nodeId() const { return nodeId_; }

    /**
     * @brief 控制继电器通道
     * @param channel 通道号（0-3）
     * @param action 继电器动作
     * @return 命令发送成功返回true
     */
    bool control(quint8 channel, RelayProtocol::Action action);

    /**
     * @brief 查询通道状态
     * @param channel 通道号（0-3）
     * @return 查询发送成功返回true
     */
    bool query(quint8 channel);

    /**
     * @brief 获取通道的最后状态
     * @param channel 通道号（0-3）
     * @return 通道状态
     */
    RelayProtocol::Status lastStatus(quint8 channel) const;

    /**
     * @brief 获取最后接收状态的时间戳
     * @return 自纪元以来的毫秒数，从未接收过返回0
     */
    qint64 lastSeenMs() const;

    // ICanDevice接口
    QString canDeviceName() const override { return name(); }
    bool canAccept(quint32 canId, bool extended, bool rtr) const override;
    void canOnFrame(quint32 canId, const QByteArray &payload,
                    bool extended, bool rtr) override;

signals:
    /**
     * @brief 通道状态更新时发出
     * @param channel 通道号
     * @param status 新状态
     */
    void statusUpdated(quint8 channel, fanzhou::device::RelayProtocol::Status status);

private:
    void markSeen();
    void onStatusFrame(quint32 canId, const QByteArray &payload);

    quint8 nodeId_;
    comm::CanComm *bus_;
    RelayProtocol::Status status_[4] {};
    QElapsedTimer lastRxTimer_;
    qint64 lastSeenMs_ = 0;

    static constexpr int kMaxChannel = 3;
};

}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_RELAY_GD427_H
