/**
 * @file can_comm.h
 * @brief CAN总线通信适配器
 *
 * 在Linux上使用SocketCAN提供CAN总线通信。
 */

#ifndef FANZHOU_CAN_COMM_H
#define FANZHOU_CAN_COMM_H

#include <QObject>
#include <QQueue>
#include <QSocketNotifier>
#include <QTimer>

#include "base/comm_adapter.h"

#include <linux/can.h>

namespace fanzhou {
namespace comm {

/**
 * @brief CAN总线配置
 */
struct CanConfig {
    QString interface = QStringLiteral("can0");  ///< CAN接口名
    bool canFd = false;                          ///< 启用CAN FD模式
};

/**
 * @brief CAN总线通信适配器
 *
 * 使用Linux SocketCAN实现CAN总线通信。
 * 支持标准CAN和CAN FD模式。
 */
class CanComm : public CommAdapter
{
    Q_OBJECT

public:
    /**
     * @brief 构造CAN通信适配器
     * @param config CAN配置
     * @param parent 父对象
     */
    explicit CanComm(CanConfig config, QObject *parent = nullptr);

    ~CanComm() override;

    bool open() override;
    void close() override;
    qint64 writeBytes(const QByteArray &data) override;

    /**
     * @brief 发送CAN帧
     * @param canId CAN标识符
     * @param payload 帧数据（经典CAN最大8字节）
     * @param extended 使用扩展（29位）标识符
     * @param rtr 远程传输请求
     * @return 帧成功入队返回true
     */
    bool sendFrame(quint32 canId, const QByteArray &payload,
                   bool extended = false, bool rtr = false);

signals:
    /**
     * @brief 接收到CAN帧时发出
     * @param canId CAN标识符
     * @param payload 帧数据
     * @param extended 是否为扩展标识符
     * @param rtr 是否为远程传输请求
     */
    void canFrameReceived(quint32 canId, QByteArray payload,
                          bool extended, bool rtr);

private slots:
    void onReadable();
    void onTxPump();

private:
    CanConfig config_;
    int socket_ = -1;
    QSocketNotifier *readNotifier_ = nullptr;

    struct TxItem {
        struct can_frame frame;
    };
    QQueue<TxItem> txQueue_;
    QTimer *txTimer_ = nullptr;
    int txBackoffMs_ = 0;

    static constexpr int kMaxTxQueueSize = 512;
    static constexpr int kTxIntervalMs = 2;
    static constexpr int kTxBackoffMs = 10;
};

}  // namespace comm
}  // namespace fanzhou

#endif  // FANZHOU_CAN_COMM_H
