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
    int bitrate = 250000;                        ///< CAN波特率
    bool tripleSampling = true;                  ///< 启用三次采样
    bool canFd = false;                          ///< 启用CAN FD模式
    int restartMs = 100;                         ///< CAN控制器bus-off自动重启延迟（毫秒），0表示禁用
    int periodicRestartMin = 0;                  ///< 定时重启CAN接口的间隔（分钟），0表示禁用
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
     * @brief 获取接口名称
     * @return CAN接口名称
     */
    QString interfaceName() const { return config_.interface; }

    /**
     * @brief 获取重置尝试次数
     * @return 重置尝试次数
     */
    int resetAttemptCount() const { return txResetAttemptCount_; }

    /**
     * @brief 获取丢帧计数
     * @return 丢帧计数
     */
    int droppedFrameCount() const { return droppedFrameCount_; }

    /**
     * @brief 获取最后一次重置的时间戳
     * @return 最后重置时间戳（毫秒）
     */
    qint64 lastResetTimeMs() const { return lastResetTimeMs_; }

    /**
     * @brief 获取重置是否正在进行中
     * @return 重置进行中返回true
     */
    bool isResetInProgress() const { return resetInProgress_; }

    /**
     * @brief 获取最后一次重置的耗时（毫秒）
     * @return 最后重置耗时，未重置过返回0
     */
    qint64 lastResetDurationMs() const { return lastResetDurationMs_; }

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

    /**
     * @brief 总线空闲时发出，上层可连接此信号发送设备查询帧
     *
     * 当CAN总线长时间无发送活动时定期发出，便于上层模块
     * 发送查询帧以保持总线活跃并检测错误。
     */
    void idleProbeNeeded();

private slots:
    void onReadable();
    void onTxPump();
    void onIdleCheck();

private:
    CanConfig config_;
    int socket_ = -1;
    QSocketNotifier *readNotifier_ = nullptr;

    struct TxItem {
        struct can_frame frame;
    };
    QQueue<TxItem> txQueue_;
    QTimer *txTimer_ = nullptr;
    int txResetAttemptCount_ = 0;  ///< 接口重置尝试次数
    bool resetInProgress_ = false; ///< 接口重置是否正在进行中
    int droppedFrameCount_ = 0;    ///< 连续丢帧计数
    qint64 lastResetTimeMs_ = 0;   ///< 最后一次接口重置的时间戳
    qint64 lastResetDurationMs_ = 0; ///< 最后一次接口重置的耗时（毫秒）
    int txConsecutiveNobufs_ = 0;  ///< 连续ENOBUFS次数
    int totalRecoveryAttempts_ = 0; ///< 总恢复尝试次数（防止无限重试）
    qint64 lastSuccessfulOpenMs_ = 0; ///< 最后一次成功打开的时间戳（用于重置恢复计数器）

    // 空闲探测相关
    QTimer *idleProbeTimer_ = nullptr;      ///< 空闲探测定时器
    qint64 lastSuccessfulTxMs_ = 0;         ///< 最后一次成功发送的时间戳（毫秒）
    int idleProbeErrors_ = 0;               ///< 连续空闲探测失败次数

    // 定时重启相关
    QTimer *periodicRestartTimer_ = nullptr; ///< 定时重启CAN接口定时器

    // 恢复重试相关
    QTimer *recoveryRetryTimer_ = nullptr;  ///< 恢复重试定时器（重置失败后延迟重试）

    static constexpr int kMaxTxQueueSize = 512;
    static constexpr int kTxIntervalMs = 2;
    static constexpr int kMaxResetAttempts = 3;  ///< 最大接口重置尝试次数
    static constexpr int kMaxTotalRecoveryAttempts = 10; ///< 最大总恢复尝试次数（防止无限重试）
    static constexpr int kResetCooldownMs = 5000;  ///< 接口重置冷却时间（毫秒）
    static constexpr int kProcessTimeoutMs = 3000;  ///< 外部进程执行超时（毫秒），ip命令通常在几十毫秒内完成
    static constexpr int kConfigRetryAttempts = 3;  ///< 接口配置重试次数（ip link命令失败时）
    static constexpr int kConfigRetryDelayMs = 500; ///< 接口配置重试间隔（毫秒）
    static constexpr int kRecoveryRetryDelayMs = 2000; ///< 恢复重试延迟（毫秒），重置失败后等待此时间再次尝试
    static constexpr int kRecoveryRetryBufferMs = 100; ///< 恢复重试缓冲时间（毫秒），确保冷却时间完全结束
    static constexpr int kNobufsRetryThreshold = 50;  ///< 连续ENOBUFS次数阈值，超过才重启接口（约100ms）
    static constexpr int kNobufsLogInterval = 10;     ///< ENOBUFS日志输出间隔（避免刷屏）
    static constexpr int kIdleProbeIntervalMs = 5000;  ///< 空闲探测定时器间隔（毫秒）
    static constexpr int kIdleThresholdMs = 10000;     ///< 判定总线空闲的阈值（毫秒）
    static constexpr int kIdleProbeErrorThreshold = 3; ///< 连续空闲探测失败次数阈值，超过则重置接口

    /**
     * @brief 配置CAN接口（down → 设置bitrate/restart-ms → up）
     * 
     * 通过 ip link 命令将接口设置为正确的波特率和自动重启参数。
     * 在 open() 和 tryResetInterface() 中均调用此方法，确保接口始终
     * 以正确配置启动，避免因缺少 restart-ms 导致 bus-off 后无法恢复。
     * 
     * @return 配置并启动成功返回true
     */
    bool configureInterface();

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

    /**
     * @brief 安排延迟重置CAN接口
     * 
     * 当tryResetInterface()失败时，安排一个延迟的重置尝试，
     * 以便在短暂等待后再次尝试恢复通信。
     */
    void scheduleRecoveryRetry();
};

}  // namespace comm
}  // namespace fanzhou

#endif  // FANZHOU_CAN_COMM_H
