/**
 * @file can_comm.cpp
 * @brief CAN总线通信适配器实现
 * 
 * CAN通信核心模块说明：
 * 
 * 功能：
 * - 通过SocketCAN接口与CAN总线通信
 * - 发送和接收CAN帧
 * - 自动处理发送失败和重试
 * 
 * 如果无法控制设备（CAN发送失败），请检查：
 * 1. CAN接口是否存在：ip link show can0
 * 2. CAN接口是否启动：ip link set can0 up
 * 3. 波特率是否正确：canconfig can0 bitrate 125000
 * 4. 终端电阻是否正确：CAN总线两端需要120Ω终端电阻
 * 5. 接线是否正确：CAN_H和CAN_L不能接反
 * 6. 是否有其他节点：CAN总线至少需要两个节点才能正常通信（需要ACK）
 */

#include "can_comm.h"
#include "utils/logger.h"
#include "utils/utils.h"

#include <QDateTime>
#include <QDebug>
#include <QProcess>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <linux/can.h>
#include <linux/can/raw.h>

namespace fanzhou {
namespace comm {

namespace {
const char *const kLogSource = "CAN";
}  // namespace

CanComm::CanComm(CanConfig config, QObject *parent)
    : CommAdapter(parent)
    , config_(std::move(config))
{
}

CanComm::~CanComm()
{
    close();
}

/**
 * @brief 打开CAN总线连接
 * 
 * 创建SocketCAN套接字并绑定到指定的CAN接口。
 * 如果打开失败，请检查：
 * 1. CAN接口是否存在（ip link show can0）
 * 2. CAN接口是否启动（ip link set can0 up）
 * 3. 进程是否有足够权限（可能需要root）
 * 
 * @return 成功返回true，失败返回false
 */
bool CanComm::open()
{
    if (socket_ >= 0) {
        return true;
    }

    // 创建SocketCAN原始套接字
    // Create SocketCAN raw socket
    socket_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socket_ < 0) {
        emit errorOccurred(utils::sysErrorString("socket(PF_CAN) failed"));
        return false;
    }


    int sndbuf_size = 0;
    socklen_t len = sizeof(sndbuf_size);
    if (getsockopt(socket_, SOL_SOCKET, SO_SNDBUF, &sndbuf_size, &len) == 0) {
        LOG_DEBUG(kLogSource, QStringLiteral("Actual socket SO_SNDBUF size: %1 bytes").arg(sndbuf_size));
    }

    // 如果需要，启用CAN FD模式
    // Enable CAN FD if requested
    if (config_.canFd) {
        int enable = 1;
        if (::setsockopt(socket_, SOL_CAN_RAW, CAN_RAW_FD_FRAMES,
                         &enable, sizeof(enable)) != 0) {
            emit errorOccurred(utils::sysErrorString("setsockopt(CAN_RAW_FD_FRAMES) failed"));
            close();
            return false;
        }
    }

    // Set non-blocking mode
    const int flags = ::fcntl(socket_, F_GETFL, 0);
    if (flags < 0 || ::fcntl(socket_, F_SETFL, flags | O_NONBLOCK) < 0) {
        emit errorOccurred(utils::sysErrorString("fcntl(O_NONBLOCK) failed"));
        close();
        return false;
    }

    // Get interface index
    struct ifreq ifr {};
    std::strncpy(ifr.ifr_name, config_.interface.toLocal8Bit().constData(), IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    if (::ioctl(socket_, SIOCGIFINDEX, &ifr) < 0) {
        emit errorOccurred(utils::sysErrorString("ioctl(SIOCGIFINDEX) failed"));
        close();
        return false;
    }

    // Bind to CAN interface
    struct sockaddr_can addr {};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (::bind(socket_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        emit errorOccurred(utils::sysErrorString("bind(AF_CAN) failed"));
        close();
        return false;
    }

    // Setup TX timer
    if (!txTimer_) {
        txTimer_ = new QTimer(this);
        txTimer_->setSingleShot(false);
        txTimer_->setInterval(kTxIntervalMs);
        connect(txTimer_, &QTimer::timeout, this, &CanComm::onTxPump);
    }
    txBackoffMs_ = 0;
    txBackoffMultiplier_ = 0;
    txTimer_->start();

    // Setup read notifier
    readNotifier_ = new QSocketNotifier(socket_, QSocketNotifier::Read, this);
    connect(readNotifier_, &QSocketNotifier::activated, this, &CanComm::onReadable);

    emit opened();
    return true;
}

void CanComm::close()
{
    if (readNotifier_) {
        readNotifier_->setEnabled(false);
        readNotifier_->deleteLater();
        readNotifier_ = nullptr;
    }

    if (socket_ >= 0) {
        ::close(socket_);
        socket_ = -1;
    }

    if (txTimer_) {
        txTimer_->stop();
    }
    txQueue_.clear();
    txBackoffMs_ = 0;
    txBackoffMultiplier_ = 0;
    txDiagLogged_ = false;
    txConsecutiveMaxBackoffCount_ = 0;
    // 注意：不重置 droppedFrameCount_、txResetAttemptCount_ 和 lastResetTimeMs_
    // 以便在重新打开时继续追踪状态

    emit closed();
}

/**
 * @brief 发送CAN帧
 * 
 * 将CAN帧加入发送队列。帧会在下一个发送周期通过CAN总线发送。
 * 
 * 发送失败的可能原因：
 * 1. CAN总线未打开（socket_ < 0）
 * 2. 数据长度超过8字节
 * 3. 发送队列已满（通常是因为CAN总线没有正常连接，导致帧无法发出）
 * 
 * 如果控制命令无法发送，请检查：
 * 1. CAN接口是否启动：ip link set can0 up
 * 2. 是否有终端电阻：CAN总线两端需要120Ω
 * 3. 是否有其他节点响应ACK
 * 
 * @param canId CAN ID
 * @param payload 数据（最大8字节）
 * @param extended 是否使用扩展帧格式
 * @param rtr 是否为远程帧
 * @return 帧是否成功加入发送队列
 */
bool CanComm::sendFrame(quint32 canId, const QByteArray &payload, bool extended, bool rtr)
{
    // 检查CAN是否已打开
    if (socket_ < 0) {
        LOG_WARNING(kLogSource, QStringLiteral("sendFrame failed: CAN not opened"));
        emit errorOccurred(QStringLiteral("CAN not opened"));
        return false;
    }

    // 检查数据长度
    if (payload.size() > 8) {
        LOG_WARNING(kLogSource, QStringLiteral("sendFrame failed: payload size %1 > 8").arg(payload.size()));
        emit errorOccurred(QStringLiteral("CAN payload must be <= 8 bytes"));
        return false;
    }

    // 检查发送队列是否已满
    // 如果队列满，通常是因为CAN总线没有正常连接
    if (txQueue_.size() >= kMaxTxQueueSize) {
        LOG_WARNING(kLogSource, QStringLiteral("sendFrame failed: TX queue overflow (%1)").arg(txQueue_.size()));
        emit errorOccurred(
            QStringLiteral("CAN TX queue overflow (%1), dropping").arg(txQueue_.size()));
        return false;
    }

    // 构建CAN帧
    struct can_frame frame {};
    frame.can_id = canId;
    if (extended) {
        frame.can_id |= CAN_EFF_FLAG;
    }
    if (rtr) {
        frame.can_id |= CAN_RTR_FLAG;
    }

    frame.can_dlc = static_cast<__u8>(payload.size());
    if (!payload.isEmpty()) {
        std::memcpy(frame.data, payload.constData(), static_cast<size_t>(payload.size()));
    }

    txQueue_.enqueue(TxItem{frame});
    LOG_DEBUG(kLogSource,
              QStringLiteral("Frame queued: id=0x%1, dlc=%2, queueSize=%3")
                  .arg(canId, 0, 16)
                  .arg(payload.size())
                  .arg(txQueue_.size()));
    return true;
}

void CanComm::onTxPump()
{
    if (socket_ < 0 || txQueue_.isEmpty()) {
        return;
    }

    if (txBackoffMs_ > 0) {
        txBackoffMs_ -= txTimer_->interval();
        if (txBackoffMs_ < 0) {
            txBackoffMs_ = 0;
        }
        return;
    }

    const TxItem item = txQueue_.head();
    errno = 0;
    const ssize_t n = ::write(socket_, &item.frame, sizeof(item.frame));

    if (n == sizeof(item.frame)) {
        // 去除扩展帧标志位获取实际CAN ID
        const bool extended = (item.frame.can_id & CAN_EFF_FLAG) != 0;
        const quint32 canIdWithoutFlags = item.frame.can_id & (extended ? CAN_EFF_MASK : CAN_SFF_MASK);
        LOG_DEBUG(kLogSource,
                  QStringLiteral("Frame sent: id=0x%1, dlc=%2")
                      .arg(canIdWithoutFlags, 0, 16)
                      .arg(item.frame.can_dlc));
        txQueue_.dequeue();
        // 成功发送后重置退避乘数、诊断标志和丢帧计数
        txBackoffMultiplier_ = 0;
        txDiagLogged_ = false;
        txConsecutiveMaxBackoffCount_ = 0;
        droppedFrameCount_ = 0;  // 成功发送，重置丢帧计数
        return;
    }

    if (errno == ENOBUFS || errno == EAGAIN || errno == EWOULDBLOCK) {
        // 使用指数退避策略：退避时间 = kTxBackoffMs * 2^multiplier
        // 安全：multiplier <= kMaxBackoffMultiplier (5)，最大退避 = 10 * 32 = 320ms
        const int backoff = kTxBackoffMs * (1 << txBackoffMultiplier_);
        txBackoffMs_ = backoff;
        // 增加乘数用于下次退避（带上限）
        if (txBackoffMultiplier_ < kMaxBackoffMultiplier) {
            ++txBackoffMultiplier_;
            txConsecutiveMaxBackoffCount_ = 0;  // 未达到最大退避，重置计数器
        } else {
            // 已达到最大退避，增加连续计数
            ++txConsecutiveMaxBackoffCount_;
        }
        LOG_DEBUG(kLogSource, QStringLiteral("TX buffer full, backing off %1ms").arg(backoff));

        // 当达到最大退避时，输出一次详细的诊断信息
        // 这通常表示CAN总线未正确连接或配置
        if (txBackoffMultiplier_ == kMaxBackoffMultiplier && !txDiagLogged_) {
            txDiagLogged_ = true;
            LOG_WARNING(kLogSource,
                        QStringLiteral("CAN TX buffer full. Possible causes:\n"
                                       "  1. No CAN device connected (no ACK)\n"
                                       "  2. CAN interface misconfigured (bitrate mismatch)\n"
                                       "  3. Missing termination resistor (120 ohm)\n"
                                       "  4. Wiring issue (CAN_H/CAN_L)\n"
                                       "Check 'ip -details link show %1' for interface status")
                            .arg(config_.interface));
        }

        // 如果连续达到最大退避次数超过限制，丢弃当前帧并重置退避
        // 这防止系统因持续的总线问题而永久卡死
        if (txConsecutiveMaxBackoffCount_ >= kMaxConsecutiveMaxBackoffRetries) {
            const bool extended = (item.frame.can_id & CAN_EFF_FLAG) != 0;
            const quint32 droppedCanId = item.frame.can_id & (extended ? CAN_EFF_MASK : CAN_SFF_MASK);
            LOG_WARNING(kLogSource,
                        QStringLiteral("TX persistent failure, dropping frame: id=0x%1, dlc=%2, retried %3 times")
                            .arg(droppedCanId, 0, 16)
                            .arg(item.frame.can_dlc)
                            .arg(txConsecutiveMaxBackoffCount_));
            emit errorOccurred(
                QStringLiteral("CAN TX persistent failure, frame dropped (id=0x%1)")
                    .arg(droppedCanId, 0, 16));
            txQueue_.dequeue();
            // 重置退避状态，允许系统尝试恢复
            txBackoffMultiplier_ = 0;
            txBackoffMs_ = 0;
            txConsecutiveMaxBackoffCount_ = 0;
            txDiagLogged_ = false;

            // 增加丢帧计数
            ++droppedFrameCount_;

            // 如果丢帧次数超过阈值，尝试重置CAN接口
            // 这可以解决因总线状态异常导致的持续发送失败问题
            if (droppedFrameCount_ >= kResetThreshold) {
                LOG_WARNING(kLogSource,
                            QStringLiteral("Dropped %1 frames consecutively, attempting CAN interface reset...")
                                .arg(droppedFrameCount_));
                if (tryResetInterface()) {
                    droppedFrameCount_ = 0;
                    LOG_INFO(kLogSource, QStringLiteral("CAN interface reset successful, communication recovered"));
                } else {
                    // 重置失败，重置丢帧计数以避免每次丢帧都触发无效的重置尝试
                    droppedFrameCount_ = 0;
                    LOG_ERROR(kLogSource,
                              QStringLiteral("CAN interface reset failed. "
                                             "Will retry after dropping %1 more frames.")
                                  .arg(kResetThreshold));
                }
            }
        }
        return;
    }

    LOG_ERROR(kLogSource, utils::sysErrorString("CAN write failed"));
    emit errorOccurred(utils::sysErrorString("CAN write failed"));
    txQueue_.dequeue();
}

qint64 CanComm::writeBytes(const QByteArray &data)
{
    Q_UNUSED(data);
    emit errorOccurred(QStringLiteral("writeBytes() not implemented for CAN"));
    return -1;
}

void CanComm::onReadable()
{
    if (socket_ < 0) {
        return;
    }

    if (readNotifier_) {
        readNotifier_->setEnabled(false);
    }

    for (;;) {
        struct can_frame frame {};
        const ssize_t n = ::read(socket_, &frame, sizeof(frame));

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            emit errorOccurred(utils::sysErrorString("CAN read failed"));
            break;
        }

        if (n != sizeof(frame)) {
            break;
        }

        const bool extended = (frame.can_id & CAN_EFF_FLAG) != 0;
        const bool rtr = (frame.can_id & CAN_RTR_FLAG) != 0;
        const quint32 canId = frame.can_id & (extended ? CAN_EFF_MASK : CAN_SFF_MASK);

        QByteArray payload;
        payload.append(reinterpret_cast<const char *>(frame.data), frame.can_dlc);
        emit canFrameReceived(canId, payload, extended, rtr);
    }

    if (readNotifier_) {
        readNotifier_->setEnabled(true);
    }
}

bool CanComm::tryResetInterface()
{
    // 防止重入
    if (resetInProgress_) {
        LOG_DEBUG(kLogSource, QStringLiteral("接口重置正在进行中，跳过"));
        return false;
    }

    // 检查冷却时间
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (lastResetTimeMs_ > 0 && (now - lastResetTimeMs_) < kResetCooldownMs) {
        LOG_DEBUG(kLogSource,
                  QStringLiteral("接口重置冷却中，剩余%1ms")
                      .arg(kResetCooldownMs - (now - lastResetTimeMs_)));
        return false;
    }

    // 检查重置次数限制
    if (txResetAttemptCount_ >= kMaxResetAttempts) {
        LOG_ERROR(kLogSource,
                  QStringLiteral("Max CAN interface reset attempts reached (%1). "
                                 "Please manually check CAN bus connection and configuration.")
                      .arg(kMaxResetAttempts));
        return false;
    }

    resetInProgress_ = true;
    ++txResetAttemptCount_;
    lastResetTimeMs_ = now;

    LOG_INFO(kLogSource,
             QStringLiteral("开始重置CAN接口 %1 (第%2次尝试)")
                 .arg(config_.interface)
                 .arg(txResetAttemptCount_));

    // 1. 关闭当前socket
    close();

    // 2. 使用 ip link set down 关闭接口
    {
        QProcess process;
        process.setProgram(QStringLiteral("ip"));
        process.setArguments({QStringLiteral("link"), QStringLiteral("set"),
                              config_.interface, QStringLiteral("down")});
        process.start();
        if (!process.waitForFinished(kProcessTimeoutMs)) {
            LOG_ERROR(kLogSource, QStringLiteral("ip link set %1 down 超时").arg(config_.interface));
            resetInProgress_ = false;
            return false;
        }
        if (process.exitCode() != 0) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("ip link set %1 down 失败: %2")
                            .arg(config_.interface)
                            .arg(QString::fromUtf8(process.readAllStandardError())));
            // 继续尝试，因为接口可能已经是down状态
        } else {
            LOG_DEBUG(kLogSource, QStringLiteral("CAN接口 %1 已关闭").arg(config_.interface));
        }
    }

    // 3. 使用 ip link set up 重新启动接口
    {
        QProcess process;
        process.setProgram(QStringLiteral("ip"));
        process.setArguments({QStringLiteral("link"), QStringLiteral("set"),
                              config_.interface, QStringLiteral("up")});
        process.start();
        if (!process.waitForFinished(kProcessTimeoutMs)) {
            LOG_ERROR(kLogSource, QStringLiteral("ip link set %1 up 超时").arg(config_.interface));
            resetInProgress_ = false;
            return false;
        }
        if (process.exitCode() != 0) {
            LOG_ERROR(kLogSource,
                      QStringLiteral("ip link set %1 up 失败: %2")
                          .arg(config_.interface)
                          .arg(QString::fromUtf8(process.readAllStandardError())));
            resetInProgress_ = false;
            return false;
        }
        LOG_DEBUG(kLogSource, QStringLiteral("CAN接口 %1 已重新启动").arg(config_.interface));
    }

    // 4. 重新打开socket
    const bool reopened = open();
    resetInProgress_ = false;

    if (reopened) {
        LOG_INFO(kLogSource,
                 QStringLiteral("CAN接口 %1 重置完成，socket已重新打开")
                     .arg(config_.interface));
        // 重置成功后，重置重试计数器（允许未来再次重试）
        txResetAttemptCount_ = 0;
        return true;
    } else {
        LOG_ERROR(kLogSource,
                  QStringLiteral("CAN接口 %1 重置后无法重新打开socket")
                      .arg(config_.interface));
        return false;
    }
}

}  // namespace comm
}  // namespace fanzhou
