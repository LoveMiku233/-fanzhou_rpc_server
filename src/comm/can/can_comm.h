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

#include "comm/base/comm_adapter.h"

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
     * @brief 检查CAN通道是否已打开
     * @return 已打开返回true
     */
    bool isOpened() const { return socket_ >= 0; }

    /**
     * @brief 获取发送队列大小
     * @return 当前队列中待发送的帧数
     */
    int txQueueSize() const { return txQueue_.size(); }

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
    int txBackoffMultiplier_ = 0;  ///< 用于指数退避的乘数
    bool txDiagLogged_ = false;    ///< 是否已输出TX诊断信息（避免重复输出）
    int txConsecutiveMaxBackoffCount_ = 0;  ///< 连续达到最大退避的次数
    int txResetAttemptCount_ = 0;  ///< 接口重置尝试次数
    bool resetInProgress_ = false; ///< 接口重置是否正在进行中
    int droppedFrameCount_ = 0;    ///< 连续丢帧计数
    qint64 lastResetTimeMs_ = 0;   ///< 最后一次接口重置的时间戳

    static constexpr int kMaxTxQueueSize = 512;
    static constexpr int kTxIntervalMs = 2;
    static constexpr int kTxBackoffMs = 10;
    static constexpr int kMaxBackoffMultiplier = 5;  ///< 最大退避乘数 (10ms * 2^5 = 320ms)
    static constexpr int kMaxConsecutiveMaxBackoffRetries = 3;  ///< 最大连续重试次数，超过后尝试重置接口（减少等待时间）
    static constexpr int kResetThreshold = 3;  ///< 丢弃帧次数阈值，超过后触发接口重置
    static constexpr int kMaxResetAttempts = 3;  ///< 最大接口重置尝试次数
    static constexpr int kResetCooldownMs = 30000;  ///< 接口重置冷却时间（毫秒）
    static constexpr int kProcessTimeoutMs = 5000;  ///< 外部进程执行超时（毫秒）

    /**
     * @brief 尝试重置CAN接口以恢复通信
     * 
     * 当TX buffer持续满载时，通过 ip link set down/up 命令刷新接口，
     * 然后重新打开socket。这可以解决因目标设备断开连接或总线问题
     * 导致的发送卡死问题。
     * 
     * @return 重置成功返回true
     */
    bool tryResetInterface();
};

}  // namespace comm
}  // namespace fanzhou

#endif  // FANZHOU_CAN_COMM_H
