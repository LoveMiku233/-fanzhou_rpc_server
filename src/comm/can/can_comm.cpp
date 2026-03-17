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
#include <QElapsedTimer>
#include <QProcess>
#include <QThread>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/can/error.h>
#include <termios.h>

namespace fanzhou {
namespace comm {

namespace {
const char *const kLogSource = "CAN";
}  // namespace

namespace {

/**
 * @brief 将波特率转换为termios速度常量（用于fake CAN）
 */
speed_t fakeCanToBaudConstant(int baud)
{
    switch (baud) {
    case 50: return B50;
    case 75: return B75;
    case 110: return B110;
    case 134: return B134;
    case 150: return B150;
    case 200: return B200;
    case 300: return B300;
    case 600: return B600;
    case 1200: return B1200;
    case 1800: return B1800;
    case 2400: return B2400;
    case 4800: return B4800;
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: return B115200;
    case 230400: return B230400;
#ifdef B460800
    case 460800: return B460800;
#endif
#ifdef B921600
    case 921600: return B921600;
#endif
    default: return B115200;
    }
}

}  // namespace

CanComm::CanComm(CanConfig config, QObject *parent)
    : CommAdapter(parent)
    , config_(std::move(config))
{
}

CanComm::~CanComm()
{
    // 停止恢复重试定时器
    if (recoveryRetryTimer_) {
        recoveryRetryTimer_->stop();
    }
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
// 辅助函数：配置fake CAN串口
static bool setupFakeCanTermios(int fd, int baudRate)
{
    struct termios tio {};
    if (::tcgetattr(fd, &tio) != 0) {
        return false;
    }

    // Raw mode
    tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR |
                     IGNCR | ICRNL | IXON | IXOFF | IXANY);
    tio.c_oflag &= ~OPOST;
    tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tio.c_cflag |= (CLOCAL | CREAD);

    // 8N1
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;
    tio.c_cflag &= ~CSTOPB;
    tio.c_cflag &= ~PARENB;
    tio.c_iflag &= ~INPCK;

    // Read timeout
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 1;

    speed_t speed = fakeCanToBaudConstant(baudRate);
    ::cfsetispeed(&tio, speed);
    ::cfsetospeed(&tio, speed);

    if (::tcsetattr(fd, TCSANOW, &tio) != 0) {
        return false;
    }

    ::tcflush(fd, TCIOFLUSH);
    return true;
}

bool CanComm::open()
{
    // Fake CAN mode: use serial port
    if (config_.is_fake) {
        if (fakeFd_ >= 0) {
            return true;
        }

        LOG_INFO(kLogSource, QStringLiteral("Opening fake CAN on serial: %1").arg(config_.fake_can));

        fakeFd_ = ::open(config_.fake_can.toLocal8Bit().constData(),
                         O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fakeFd_ < 0) {
            LOG_ERROR(kLogSource, QStringLiteral("open() failed for fake CAN: %1")
                          .arg(utils::sysErrorString("open() failed")));
            emit errorOccurred(utils::sysErrorString("open() failed"));
            return false;
        }

        if (!setupFakeCanTermios(fakeFd_, config_.fake_can_baudrate)) {
            LOG_ERROR(kLogSource, QStringLiteral("setupFakeCanTermios() failed"));
            emit errorOccurred(QStringLiteral("Failed to configure serial port"));
            ::close(fakeFd_);
            fakeFd_ = -1;
            return false;
        }

        fakeRxBuf_.clear();

        fakeReadNotifier_ = new QSocketNotifier(fakeFd_, QSocketNotifier::Read, this);
        connect(fakeReadNotifier_, &QSocketNotifier::activated, this, &CanComm::onFakeReadable);

        // Setup TX timer (same as real CAN)
        if (!txTimer_) {
            txTimer_ = new QTimer(this);
            txTimer_->setSingleShot(false);
            txTimer_->setInterval(kTxIntervalMs);
            connect(txTimer_, &QTimer::timeout, this, &CanComm::onTxPump);
        }
        txTimer_->start();

        LOG_INFO(kLogSource, QStringLiteral("Fake CAN opened successfully on %1 (baud=%2)")
                     .arg(config_.fake_can).arg(config_.fake_can_baudrate));
        emit opened();
        return true;
    }

    // Real CAN mode
    if (socket_ >= 0) {
        return true;
    }

    LOG_INFO(kLogSource, QStringLiteral("Opening CAN interface: %1").arg(config_.interface));

    // 配置CAN接口（设置波特率、restart-ms并重新启动接口）
    // 无论接口之前的状态如何（bus-off、未配置等），均确保以正确参数启动
    // 注意：configureInterface()需要bitrate > 0才能配置CAN接口参数
    if (config_.bitrate > 0 && !configureInterface()) {
        return false;
    }

    // 创建SocketCAN原始套接字
    // Create SocketCAN raw socket
    socket_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socket_ < 0) {
        LOG_ERROR(kLogSource, QStringLiteral("socket(PF_CAN) failed: %1")
                      .arg(utils::sysErrorString("socket(PF_CAN) failed")));
        emit errorOccurred(utils::sysErrorString("socket(PF_CAN) failed"));
        return false;
    }


    int sndbuf_size = 0;
    socklen_t len = sizeof(sndbuf_size);
    if (getsockopt(socket_, SOL_SOCKET, SO_SNDBUF, &sndbuf_size, &len) == 0) {
        LOG_DEBUG(kLogSource, QStringLiteral("Actual socket SO_SNDBUF size: %1 bytes").arg(sndbuf_size));
    }

    // 尝试增大发送缓冲区以减少ENOBUFS频率
    int desired_sndbuf = 65536;  // 64KB
    if (setsockopt(socket_, SOL_SOCKET, SO_SNDBUF, &desired_sndbuf, sizeof(desired_sndbuf)) == 0) {
        // 再次读取实际值（内核可能会翻倍）
        socklen_t actual_len = sizeof(sndbuf_size);
        if (getsockopt(socket_, SOL_SOCKET, SO_SNDBUF, &sndbuf_size, &actual_len) == 0) {
            LOG_INFO(kLogSource, QStringLiteral("SO_SNDBUF set to %1 bytes").arg(sndbuf_size));
        }
    } else {
        LOG_WARNING(kLogSource, QStringLiteral("Failed to set SO_SNDBUF: %1").arg(utils::sysErrorString("setsockopt")));
    }

    // 如果需要，启用CAN FD模式
    // Enable CAN FD if requested
    if (config_.canFd) {
        int enable = 1;
        if (::setsockopt(socket_, SOL_CAN_RAW, CAN_RAW_FD_FRAMES,
                         &enable, sizeof(enable)) != 0) {
            LOG_ERROR(kLogSource, QStringLiteral("setsockopt(CAN_RAW_FD_FRAMES) failed: %1")
                          .arg(utils::sysErrorString("setsockopt")));
            emit errorOccurred(utils::sysErrorString("setsockopt(CAN_RAW_FD_FRAMES) failed"));
            close();
            return false;
        }
    }

    // Set non-blocking mode
    const int flags = ::fcntl(socket_, F_GETFL, 0);
    if (flags < 0 || ::fcntl(socket_, F_SETFL, flags | O_NONBLOCK) < 0) {
        LOG_ERROR(kLogSource, QStringLiteral("fcntl(O_NONBLOCK) failed: %1")
                      .arg(utils::sysErrorString("fcntl")));
        emit errorOccurred(utils::sysErrorString("fcntl(O_NONBLOCK) failed"));
        close();
        return false;
    }

    // Get interface index
    struct ifreq ifr {};
    std::strncpy(ifr.ifr_name, config_.interface.toLocal8Bit().constData(), IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    if (::ioctl(socket_, SIOCGIFINDEX, &ifr) < 0) {
        LOG_ERROR(kLogSource, QStringLiteral("ioctl(SIOCGIFINDEX) failed for %1: %2")
                      .arg(config_.interface)
                      .arg(utils::sysErrorString("ioctl")));
        emit errorOccurred(utils::sysErrorString("ioctl(SIOCGIFINDEX) failed"));
        close();
        return false;
    }

    // Bind to CAN interface
    struct sockaddr_can addr {};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (::bind(socket_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        LOG_ERROR(kLogSource, QStringLiteral("bind(AF_CAN) failed for %1: %2")
                      .arg(config_.interface)
                      .arg(utils::sysErrorString("bind")));
        emit errorOccurred(utils::sysErrorString("bind(AF_CAN) failed"));
        close();
        return false;
    }

    // 关闭本地loopback
    int loopback = 0;
    setsockopt(socket_, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &loopback, sizeof(loopback));

    int recv_own_msgs = 0;
    setsockopt(socket_, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &recv_own_msgs, sizeof(recv_own_msgs));

    // ERROR
    int err_mask = CAN_ERR_MASK;
    setsockopt(socket_, SOL_CAN_RAW, CAN_RAW_ERR_FILTER,
               &err_mask, sizeof(err_mask));
    LOG_INFO(kLogSource, "CAN error frame monitoring enabled");

    // Setup TX timer
    if (!txTimer_) {
        txTimer_ = new QTimer(this);
        txTimer_->setSingleShot(false);
        txTimer_->setInterval(kTxIntervalMs);
        connect(txTimer_, &QTimer::timeout, this, &CanComm::onTxPump);
    }
    txTimer_->start();

    // Setup idle probe timer
    if (!idleProbeTimer_) {
        idleProbeTimer_ = new QTimer(this);
        idleProbeTimer_->setSingleShot(false);
        idleProbeTimer_->setInterval(kIdleProbeIntervalMs);
        connect(idleProbeTimer_, &QTimer::timeout, this, &CanComm::onIdleCheck);
    }
    idleProbeTimer_->start();
    lastSuccessfulTxMs_ = QDateTime::currentMSecsSinceEpoch();

    // Setup periodic restart timer
    if (config_.periodicRestartMin > 0) {
        if (!periodicRestartTimer_) {
            periodicRestartTimer_ = new QTimer(this);
            periodicRestartTimer_->setSingleShot(false);
            connect(periodicRestartTimer_, &QTimer::timeout, this, [this]() {
                LOG_INFO(kLogSource,
                         QStringLiteral("Periodic CAN restart triggered (interval %1 min)")
                             .arg(config_.periodicRestartMin));
                tryResetInterface();
            });
        }
        periodicRestartTimer_->setInterval(config_.periodicRestartMin * 60 * 1000);
        periodicRestartTimer_->start();
        LOG_INFO(kLogSource,
                 QStringLiteral("Periodic CAN restart enabled: every %1 min")
                     .arg(config_.periodicRestartMin));
    }

    // Setup read notifier
    readNotifier_ = new QSocketNotifier(socket_, QSocketNotifier::Read, this);
    connect(readNotifier_, &QSocketNotifier::activated, this, &CanComm::onReadable);

    LOG_INFO(kLogSource, QStringLiteral("CAN interface %1 opened successfully").arg(config_.interface));
    // 成功打开后，重置总恢复计数器
    totalRecoveryAttempts_ = 0;
    lastSuccessfulOpenMs_ = QDateTime::currentMSecsSinceEpoch();
    emit opened();
    return true;
}

void CanComm::close()
{
    // Fake CAN mode
    if (config_.is_fake) {
        if (fakeFd_ >= 0) {
            LOG_INFO(kLogSource, QStringLiteral("Closing fake CAN %1").arg(config_.fake_can));
        }

        if (fakeReadNotifier_) {
            fakeReadNotifier_->setEnabled(false);
            fakeReadNotifier_->deleteLater();
            fakeReadNotifier_ = nullptr;
        }

        if (fakeFd_ >= 0) {
            ::close(fakeFd_);
            fakeFd_ = -1;
        }

        fakeRxBuf_.clear();

        if (txTimer_) {
            txTimer_->stop();
        }
        txQueue_.clear();

        emit closed();
        return;
    }

    // Real CAN mode
    if (socket_ >= 0) {
        LOG_INFO(kLogSource, QStringLiteral("Closing CAN interface %1").arg(config_.interface));
    }

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
    if (idleProbeTimer_) {
        idleProbeTimer_->stop();
    }
    if (periodicRestartTimer_) {
        periodicRestartTimer_->stop();
    }
    QThread::msleep(10);
    txQueue_.clear();
    txConsecutiveNobufs_ = 0;
    idleProbeErrors_ = 0;
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
    // Fake CAN mode
    if (config_.is_fake) {
        if (fakeFd_ < 0) {
            LOG_ERROR(kLogSource, QStringLiteral("Failed to send frame: fake CAN not opened"));
            emit errorOccurred(QStringLiteral("Fake CAN not opened"));
            return false;
        }

        // 检查数据长度
        if (payload.size() > 8) {
            LOG_WARNING(kLogSource, QStringLiteral("sendFrame failed: payload size %1 > 8").arg(payload.size()));
            emit errorOccurred(QStringLiteral("CAN payload must be <= 8 bytes"));
            return false;
        }

        // 检查发送队列是否已满
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
        return true;
    }

    // Real CAN mode: 检查CAN是否已打开
    if (socket_ < 0) {
        // 使用ERROR级别日志，因为这是一个需要注意的错误状态
        const bool recoveryScheduled = (recoveryRetryTimer_ && recoveryRetryTimer_->isActive());
        const bool willAttemptRecovery = !resetInProgress_ && !recoveryScheduled;

        if (willAttemptRecovery) {
            LOG_ERROR(kLogSource, QStringLiteral("Failed to send frame: CAN interface not opened. Attempting automatic recovery..."));
        } else if (recoveryScheduled) {
            LOG_ERROR(kLogSource, QStringLiteral("Failed to send frame: CAN interface not opened. Recovery already scheduled."));
        } else {
            LOG_ERROR(kLogSource, QStringLiteral("Failed to send frame: CAN interface not opened. Recovery in progress."));
        }

        emit errorOccurred(QStringLiteral("CAN not opened"));
        // 如果没有正在进行的重置且没有计划中的恢复，尝试恢复
        if (willAttemptRecovery) {
            // 使用QTimer::singleShot避免在调用栈中递归
            QTimer::singleShot(0, this, [this]() {
                tryResetInterface();
            });
        }
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
    return true;
}

void CanComm::onTxPump()
{
    // Fake CAN mode
    if (config_.is_fake) {
        if (fakeFd_ < 0 || txQueue_.isEmpty()) {
            return;
        }

        const TxItem item = txQueue_.head();

        // 构建串口数据包: [ID_HIGH][ID_LOW][DATA...]
        // CAN ID 是 32 位，我们只取低 16 位（兼容标准帧）
        QByteArray serialPacket;
        quint32 canId = item.frame.can_id & CAN_SFF_MASK;  // 只取标准ID部分
        serialPacket.append(static_cast<char>((canId >> 8) & 0xFF));  // ID high byte
        serialPacket.append(static_cast<char>(canId & 0xFF));         // ID low byte
        serialPacket.append(reinterpret_cast<const char *>(item.frame.data), item.frame.can_dlc);

        const ssize_t n = ::write(fakeFd_, serialPacket.constData(), static_cast<size_t>(serialPacket.size()));

        if (n == serialPacket.size()) {
            txQueue_.dequeue();
            LOG_DEBUG(kLogSource, QStringLiteral("Fake CAN sent: ID=0x%1, data=%2")
                         .arg(canId, 0, 16)
                         .arg(QString::fromLatin1(serialPacket.mid(2).toHex(' '))));
            return;
        }

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;  // Try again later
        }

        LOG_ERROR(kLogSource, utils::sysErrorString("Fake CAN write failed"));
        emit errorOccurred(utils::sysErrorString("Fake CAN write failed"));
        txQueue_.dequeue();
        return;
    }

    // Real CAN mode
    if (socket_ < 0 || txQueue_.isEmpty()) {
        return;
    }

    const TxItem item = txQueue_.head();
    errno = 0;
    const ssize_t n = ::write(socket_, &item.frame, sizeof(item.frame));

    if (n == sizeof(item.frame)) {
        txQueue_.dequeue();
        txConsecutiveNobufs_ = 0;
        // 成功发送后重置丢帧计数
        droppedFrameCount_ = 0;
        // 更新最后成功发送时间，重置空闲探测错误计数
        lastSuccessfulTxMs_ = QDateTime::currentMSecsSinceEpoch();
        idleProbeErrors_ = 0;
        return;
    }

    if (errno == ENOBUFS || errno == EAGAIN || errno == EWOULDBLOCK) {
        ++txConsecutiveNobufs_;

        // 偶尔的buffer full：不做任何操作，帧留在队头，等下一个timer周期(2ms后)自动重试
        if (txConsecutiveNobufs_ <= kNobufsRetryThreshold) {
            // 只在首次和每10次输出日志，避免日志刷屏
            if (txConsecutiveNobufs_ == 1 || txConsecutiveNobufs_ % kNobufsLogInterval == 0) {
                LOG_DEBUG(kLogSource,
                          QStringLiteral("TX buffer full (errno=%1), will retry next cycle (%2/%3), pending=%4")
                              .arg(errno)
                              .arg(txConsecutiveNobufs_)
                              .arg(kNobufsRetryThreshold)
                              .arg(txQueue_.size()));
            }
            return;  // 不dequeue，不重启，2ms后onTxPump会再次被调用
        }

        // 连续多次都失败 → 可能是真正的总线问题，才重启接口
        LOG_WARNING(kLogSource,
                    QStringLiteral("TX buffer full %1 consecutive times (~%2ms), "
                                   "resetting CAN interface %3")
                        .arg(txConsecutiveNobufs_)
                        .arg(txConsecutiveNobufs_ * kTxIntervalMs)
                        .arg(config_.interface));
        dumpCanInterfaceState();
        txConsecutiveNobufs_ = 0;

        if (!tryResetInterface()) {
            const int droppedCount = txQueue_.size();
            if (droppedCount > 0) {
                droppedFrameCount_ += droppedCount;
                LOG_ERROR(kLogSource,
                          QStringLiteral("CAN interface reset failed, clearing TX queue: "
                                         "dropped %1 frames (total dropped: %2)")
                              .arg(droppedCount)
                              .arg(droppedFrameCount_));
                emit errorOccurred(
                    QStringLiteral("CAN TX buffer full, reset failed, dropped %1 frames")
                        .arg(droppedCount));
                txQueue_.clear();
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

        if ((frame.can_id & CAN_ERR_FLAG) != 0) {
            LOG_DEBUG(kLogSource, "CAN error frame received");
            handleErrorFrame(frame);
            continue;
        }

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            LOG_ERROR(kLogSource, QStringLiteral("CAN read failed on %1: %2")
                          .arg(config_.interface)
                          .arg(utils::sysErrorString("read")));
            emit errorOccurred(utils::sysErrorString("CAN read failed"));
            break;
        }

        if (n != sizeof(frame)) {
            LOG_WARNING(kLogSource, QStringLiteral("CAN read incomplete frame: got %1 bytes, expected %2")
                            .arg(n).arg(sizeof(frame)));
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


void CanComm::dumpCanInterfaceState()
{
    // ---------- 1. CAN interface state ----------
    QProcess process;
    process.start("ip", {"-details", "-statistics", "link", "show", config_.interface});

    if (!process.waitForFinished(2000)) {
        LOG_ERROR(kLogSource, "Failed to get CAN interface state (timeout)");
    } else {
        QString output = process.readAllStandardOutput();
        QString error  = process.readAllStandardError();

        if (!output.isEmpty()) {
            LOG_ERROR(kLogSource,
                      QStringLiteral("CAN interface state:\n%1").arg(output));
        }

        if (!error.isEmpty()) {
            LOG_ERROR(kLogSource,
                      QStringLiteral("CAN interface error:\n%1").arg(error));
        }
    }

    // ---------- 2. Kernel CAN logs (mcp251x / can) ----------
    QProcess dmesgProc;

    QString cmd = "dmesg | tail -200 | grep -E 'mcp251x|can'";
    dmesgProc.start("sh", QStringList() << "-c" << cmd);

    if (!dmesgProc.waitForFinished(2000)) {
        LOG_ERROR(kLogSource, "Failed to read dmesg (timeout)");
        return;
    }

    QString dmesgOutput = dmesgProc.readAllStandardOutput();
    QString dmesgError  = dmesgProc.readAllStandardError();

    if (!dmesgOutput.isEmpty()) {
        LOG_ERROR(kLogSource,
                  QStringLiteral("Kernel CAN log (dmesg):\n%1").arg(dmesgOutput));
    }

    if (!dmesgError.isEmpty()) {
        LOG_ERROR(kLogSource,
                  QStringLiteral("dmesg error:\n%1").arg(dmesgError));
    }
}


/**
 * @brief error检查
 * 错误类型	     说明
 * ACK	      没有节点 ACK
 * BUSOFF	  控制器关闭
 * BUSERROR	  EMI干扰
 * RESTARTED  自动恢复
 * LOSTARB	  仲裁丢失
 */
void CanComm::handleErrorFrame(const can_frame& frame)
{
    if (frame.can_id & CAN_ERR_ACK) {
        LOG_WARNING(kLogSource, "CAN ACK error (no other node on bus?)");
    }

    if (frame.can_id & CAN_ERR_BUSOFF) {
        LOG_ERROR(kLogSource, "CAN BUS-OFF detected");
        dumpCanInterfaceState();
        tryResetInterface();
    }

    if (frame.can_id & CAN_ERR_BUSERROR) {
        LOG_WARNING(kLogSource, "CAN BUS error (bit/stuff/form error)");
    }

    if (frame.can_id & CAN_ERR_RESTARTED) {
        LOG_INFO(kLogSource, "CAN controller restarted after BUS-OFF");
    }

    if (frame.can_id & CAN_ERR_LOSTARB) {
        LOG_DEBUG(kLogSource, "CAN arbitration lost");
    }

    if (frame.data[1] & CAN_ERR_CRTL_RX_PASSIVE ||
        frame.data[1] & CAN_ERR_CRTL_TX_PASSIVE) {
        LOG_WARNING(kLogSource, "CAN controller entered ERROR-PASSIVE");
    }
}

/**
 * @brief 空闲探测检查
 * 
 * 当CAN总线长时间无成功发送时，通过直接写入socket的方式
 * 探测总线是否正常工作。如果连续多次探测失败，说明总线
 * 可能处于错误状态（如bus-off），此时触发接口重置。
 * 
 * 这解决了"无控制指令下发时CAN总线错误无法被检测和重置"的问题。
 */
void CanComm::onIdleCheck()
{
    if (socket_ < 0 || resetInProgress_) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 idleDuration = now - lastSuccessfulTxMs_;

    // 总线未处于空闲状态（有正常发送活动），重置空闲错误计数
    if (idleDuration < kIdleThresholdMs) {
        idleProbeErrors_ = 0;
        return;
    }

    // 总线空闲，发送探测帧以检查总线健康状态
    // 使用CAN ID 0x7FF（标准帧最高ID、最低优先级），空数据，不影响设备。
    // 选择此ID因为：1) 继电器协议使用 0x100-0x380 范围，不会冲突；
    // 2) 最低优先级不会抢占正常通信；3) 无设备监听此ID，帧将被忽略。
    struct can_frame frame {};
    frame.can_id = 0x7FF;
    frame.can_dlc = 0;

    errno = 0;
    const ssize_t n = ::write(socket_, &frame, sizeof(frame));

    if (n == sizeof(frame)) {
        // 探测成功，总线正常工作
        idleProbeErrors_ = 0;
        lastSuccessfulTxMs_ = now;
        return;
    }

    // 探测失败，累计错误
    ++idleProbeErrors_;
    LOG_WARNING(kLogSource,
                QStringLiteral("Idle probe failed (errno=%1, consecutive=%2/%3, idle %4ms)")
                    .arg(errno)
                    .arg(idleProbeErrors_)
                    .arg(kIdleProbeErrorThreshold)
                    .arg(idleDuration));

    // 仅在首次失败时通知上层模块发送设备查询帧，避免重复触发过多查询
    if (idleProbeErrors_ == 1) {
        emit idleProbeNeeded();
    }

    // 连续失败次数超过阈值，重置CAN接口
    if (idleProbeErrors_ >= kIdleProbeErrorThreshold) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Idle probe error threshold reached (%1), "
                                   "resetting CAN interface %2")
                        .arg(idleProbeErrors_)
                        .arg(config_.interface));
        idleProbeErrors_ = 0;
        dumpCanInterfaceState();
        tryResetInterface();
    }
}

bool CanComm::configureInterface()
{
    LOG_INFO(kLogSource,
             QStringLiteral("Configuring CAN interface %1 (bitrate=%2, restart-ms=%3)")
                 .arg(config_.interface)
                 .arg(config_.bitrate)
                 .arg(config_.restartMs > 0 ? QString::number(config_.restartMs) : QStringLiteral("disabled")));

    // 1. 将接口设为 down（接口必须在down状态下才能修改CAN参数）
    // 带重试逻辑，防止临时性故障导致失败
    for (int attempt = 1; attempt <= kConfigRetryAttempts; ++attempt) {
        QProcess process;
        process.setProgram(QStringLiteral("ip"));
        process.setArguments({QStringLiteral("link"), QStringLiteral("set"),
                              config_.interface, QStringLiteral("down")});
        process.start();
        if (!process.waitForFinished(kProcessTimeoutMs)) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("ip link set %1 down timed out (attempt %2/%3)")
                            .arg(config_.interface)
                            .arg(attempt)
                            .arg(kConfigRetryAttempts));
            if (attempt < kConfigRetryAttempts) {
                QThread::msleep(kConfigRetryDelayMs);
                continue;
            }
            LOG_ERROR(kLogSource, QStringLiteral("ip link set %1 down failed after %2 attempts")
                          .arg(config_.interface)
                          .arg(kConfigRetryAttempts));
            return false;
        }
        if (process.exitCode() != 0) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("ip link set %1 down failed: %2")
                            .arg(config_.interface)
                            .arg(QString::fromUtf8(process.readAllStandardError())));
            // 继续执行后续步骤，因为接口可能已经处于down状态
            // 这种情况下 ip link set down 会返回非零退出码，但实际上接口已经是down了
        } else {
            LOG_INFO(kLogSource, QStringLiteral("CAN interface %1 brought down").arg(config_.interface));
        }
        // 命令已执行完毕（无论是否成功），退出重试循环
        // 注意：对于 "set down" 操作，非零退出码可能意味着接口已经是down状态，
        // 这不是致命错误，所以继续执行后续配置步骤
        break;
    }

    // 2. 配置CAN波特率和restart-ms
    // 带重试逻辑，这是最容易超时的步骤
    for (int attempt = 1; attempt <= kConfigRetryAttempts; ++attempt) {
        QProcess process;
        process.setProgram(QStringLiteral("ip"));
        QStringList args{QStringLiteral("link"), QStringLiteral("set"),
                         config_.interface, QStringLiteral("type"), QStringLiteral("can"),
                         QStringLiteral("bitrate"), QString::number(config_.bitrate)};
        if (config_.tripleSampling) {
            args << QStringLiteral("triple-sampling") << QStringLiteral("on");
        }
        // 配置 restart-ms：CAN控制器在bus-off后自动重启的延迟
        // 这样即使没有手动重置，硬件也能自动从bus-off状态恢复
        // 限制范围：1-10000ms，防止误配置
        if (config_.restartMs > 0) {
            const int clampedRestartMs = qBound(1, config_.restartMs, 10000);
            args << QStringLiteral("restart-ms") << QString::number(clampedRestartMs);
        }
        process.setArguments(args);
        process.start();
        if (!process.waitForFinished(kProcessTimeoutMs)) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("ip link set %1 type can bitrate %2 timed out (attempt %3/%4)")
                            .arg(config_.interface)
                            .arg(config_.bitrate)
                            .arg(attempt)
                            .arg(kConfigRetryAttempts));
            if (attempt < kConfigRetryAttempts) {
                QThread::msleep(kConfigRetryDelayMs);
                continue;
            }
            LOG_ERROR(kLogSource,
                      QStringLiteral("ip link set %1 type can bitrate %2 failed after %3 attempts")
                          .arg(config_.interface)
                          .arg(config_.bitrate)
                          .arg(kConfigRetryAttempts));
            return false;
        }
        if (process.exitCode() != 0) {
            const QString errorMsg = QString::fromUtf8(process.readAllStandardError());
            LOG_WARNING(kLogSource,
                        QStringLiteral("ip link set %1 type can bitrate %2 failed (attempt %3/%4): %5")
                            .arg(config_.interface)
                            .arg(config_.bitrate)
                            .arg(attempt)
                            .arg(kConfigRetryAttempts)
                            .arg(errorMsg));
            if (attempt < kConfigRetryAttempts) {
                QThread::msleep(kConfigRetryDelayMs);
                continue;
            }
            // 最后一次尝试仍然失败，返回错误
            LOG_ERROR(kLogSource,
                      QStringLiteral("CAN bitrate configuration failed after %1 attempts")
                          .arg(kConfigRetryAttempts));
            return false;
        }
        // exitCode == 0，配置成功
        LOG_INFO(kLogSource,
                 QStringLiteral("CAN interface %1 configured: bitrate=%2, restart-ms=%3")
                     .arg(config_.interface)
                     .arg(config_.bitrate)
                     .arg(config_.restartMs));
        break;  // 配置成功（exitCode == 0），退出重试循环
    }

    // 3. 将接口设为 up（带txqueuelen配置）
    // 带重试逻辑
    for (int attempt = 1; attempt <= kConfigRetryAttempts; ++attempt) {
        QProcess process;
        process.setProgram(QStringLiteral("ip"));
        process.setArguments({QStringLiteral("link"), QStringLiteral("set"),
                              config_.interface,
                              QStringLiteral("txqueuelen"), QStringLiteral("1024"),
                              QStringLiteral("up")});
        process.start();
        if (!process.waitForFinished(kProcessTimeoutMs)) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("ip link set %1 up timed out (attempt %2/%3)")
                            .arg(config_.interface)
                            .arg(attempt)
                            .arg(kConfigRetryAttempts));
            if (attempt < kConfigRetryAttempts) {
                QThread::msleep(kConfigRetryDelayMs);
                continue;
            }
            LOG_ERROR(kLogSource, QStringLiteral("ip link set %1 up failed after %2 attempts")
                          .arg(config_.interface)
                          .arg(kConfigRetryAttempts));
            return false;
        }
        if (process.exitCode() != 0) {
            const QString errorMsg = QString::fromUtf8(process.readAllStandardError());
            LOG_WARNING(kLogSource,
                        QStringLiteral("ip link set %1 up failed (attempt %2/%3): %4")
                            .arg(config_.interface)
                            .arg(attempt)
                            .arg(kConfigRetryAttempts)
                            .arg(errorMsg));
            if (attempt < kConfigRetryAttempts) {
                QThread::msleep(kConfigRetryDelayMs);
                continue;
            }
            LOG_ERROR(kLogSource,
                      QStringLiteral("ip link set %1 up failed after %2 attempts: %3")
                          .arg(config_.interface)
                          .arg(kConfigRetryAttempts)
                          .arg(errorMsg));
            return false;
        }
        LOG_INFO(kLogSource, QStringLiteral("CAN interface %1 brought up").arg(config_.interface));
        break;
    }

    return true;
}

bool CanComm::tryResetInterface()
{
    // 防止重入
    if (resetInProgress_) {
        LOG_WARNING(kLogSource, QStringLiteral("CAN interface reset already in progress, skipping"));
        return false;
    }

    // 检查总恢复尝试次数限制（防止无限重试）
    if (totalRecoveryAttempts_ >= kMaxTotalRecoveryAttempts) {
        LOG_ERROR(kLogSource,
                  QStringLiteral("Max total CAN recovery attempts reached (%1). "
                                 "Please manually check CAN bus connection and configuration. "
                                 "Restart the application to reset recovery counter.")
                      .arg(kMaxTotalRecoveryAttempts));
        return false;
    }
    ++totalRecoveryAttempts_;

    // 检查冷却时间
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (lastResetTimeMs_ > 0 && (now - lastResetTimeMs_) < kResetCooldownMs) {
        LOG_WARNING(kLogSource,
                  QStringLiteral("CAN interface reset cooldown active, remaining %1ms")
                      .arg(kResetCooldownMs - (now - lastResetTimeMs_)));
        // 如果冷却中但socket未打开，安排延迟恢复
        if (socket_ < 0) {
            scheduleRecoveryRetry();
        }
        return false;
    }

    // 冷却时间已过，重置尝试计数器（允许再次重试）
    if (lastResetTimeMs_ > 0) {
        txResetAttemptCount_ = 0;
    }

    // 检查重置次数限制
    if (txResetAttemptCount_ >= kMaxResetAttempts) {
        LOG_ERROR(kLogSource,
                  QStringLiteral("Max CAN interface reset attempts reached (%1). "
                                 "Scheduling delayed recovery attempt (total %2/%3).")
                      .arg(kMaxResetAttempts)
                      .arg(totalRecoveryAttempts_)
                      .arg(kMaxTotalRecoveryAttempts));
        // 即使达到最大重试次数，也安排延迟恢复（如果总次数未超限）
        scheduleRecoveryRetry();
        return false;
    }

    resetInProgress_ = true;
    ++txResetAttemptCount_;
    lastResetTimeMs_ = now;

    // 计时：测量整个重置过程的耗时
    QElapsedTimer resetTimer;
    resetTimer.start();

    LOG_WARNING(kLogSource,
             QStringLiteral("Resetting CAN interface %1 (attempt %2/%3, total %4/%5)")
                 .arg(config_.interface)
                 .arg(txResetAttemptCount_)
                 .arg(kMaxResetAttempts)
                 .arg(totalRecoveryAttempts_)
                 .arg(kMaxTotalRecoveryAttempts));

    // 1. 关闭当前socket
    close();

    // 2. 重新打开socket（open()会自动配置接口：down → 配置bitrate/restart-ms → up）
    const bool reopened = open();
    lastResetDurationMs_ = resetTimer.elapsed();
    resetInProgress_ = false;

    if (reopened) {
        LOG_INFO(kLogSource,
                 QStringLiteral("CAN interface %1 reset successful in %2ms, socket reopened")
                     .arg(config_.interface)
                     .arg(lastResetDurationMs_));
        // 重置成功后，重置重试计数器（允许未来再次重试）
        txResetAttemptCount_ = 0;
        // 记录成功打开时间，用于重置总恢复计数器
        lastSuccessfulOpenMs_ = QDateTime::currentMSecsSinceEpoch();
        // 重置总恢复计数器，因为现在成功了
        totalRecoveryAttempts_ = 0;
        return true;
    } else {
        LOG_ERROR(kLogSource,
                  QStringLiteral("CAN interface %1 reset completed in %2ms but socket reopen failed")
                      .arg(config_.interface)
                      .arg(lastResetDurationMs_));
        // 安排延迟恢复尝试
        scheduleRecoveryRetry();
        return false;
    }
}

void CanComm::scheduleRecoveryRetry()
{
    // 检查总恢复尝试次数限制
    if (totalRecoveryAttempts_ >= kMaxTotalRecoveryAttempts) {
        LOG_ERROR(kLogSource,
                  QStringLiteral("Max total CAN recovery attempts reached (%1), not scheduling retry")
                      .arg(kMaxTotalRecoveryAttempts));
        return;
    }

    // 如果定时器已经在运行，不重复安排
    if (recoveryRetryTimer_ && recoveryRetryTimer_->isActive()) {
        LOG_DEBUG(kLogSource, QStringLiteral("Recovery retry already scheduled"));
        return;
    }

    if (!recoveryRetryTimer_) {
        recoveryRetryTimer_ = new QTimer(this);
        recoveryRetryTimer_->setSingleShot(true);
        connect(recoveryRetryTimer_, &QTimer::timeout, this, [this]() {
            LOG_INFO(kLogSource,
                     QStringLiteral("Executing scheduled CAN interface recovery for %1")
                         .arg(config_.interface));
            // 重置短期重试计数器（txResetAttemptCount_）如果冷却时间已过
            // 这与 tryResetInterface() 中的逻辑一致，确保定时器触发后
            // 可以立即进行新的一轮重试而不是被冷却时间阻止
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (lastResetTimeMs_ > 0 && (now - lastResetTimeMs_) >= kResetCooldownMs) {
                txResetAttemptCount_ = 0;
            }
            tryResetInterface();
        });
    }

    // 安排在冷却时间后重试
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    int delayMs = kRecoveryRetryDelayMs;
    if (lastResetTimeMs_ > 0) {
        const qint64 elapsed = now - lastResetTimeMs_;
        if (elapsed < kResetCooldownMs) {
            // 等待冷却时间结束后再试，加上缓冲时间确保冷却完全结束
            delayMs = static_cast<int>(kResetCooldownMs - elapsed) + kRecoveryRetryBufferMs;
        }
    }

    LOG_INFO(kLogSource,
             QStringLiteral("Scheduling CAN interface recovery in %1ms (total attempts %2/%3)")
                 .arg(delayMs)
                 .arg(totalRecoveryAttempts_)
                 .arg(kMaxTotalRecoveryAttempts));
    recoveryRetryTimer_->start(delayMs);
}

void CanComm::onFakeReadable()
{
    if (fakeFd_ < 0) {
        return;
    }

    if (fakeReadNotifier_) {
        fakeReadNotifier_->setEnabled(false);
    }

    // 读取串口数据到缓冲区
    char buf[256];
    for (;;) {
        ssize_t n = ::read(fakeFd_, buf, sizeof(buf));
        if (n > 0) {
            fakeRxBuf_.append(buf, static_cast<int>(n));
            continue;
        }
        if (n == 0) {
            break;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        LOG_ERROR(kLogSource, utils::sysErrorString("Fake CAN read failed"));
        emit errorOccurred(utils::sysErrorString("Fake CAN read failed"));
        break;
    }

    // 解析缓冲区中的帧
    // 格式: [ID_HIGH][ID_LOW][DATA...], DATA 至少1字节，最多8字节
    while (fakeRxBuf_.size() >= 3) {  // 至少需要ID(2) + DATA(1) = 3字节
        // 从缓冲区中查找可能的帧
        // 我们尝试每次解析一个帧: 2字节ID + 1-8字节数据

        // 先检查是否有足够的字节（最小帧: 2+1=3）
        // 这里我们采用简单的策略: 每次取前2字节作为ID，后面直到缓冲区结束或8字节作为数据
        // 更复杂的协议可能需要帧头/帧尾来同步

        quint32 canId = (static_cast<quint8>(fakeRxBuf_[0]) << 8) |
                         static_cast<quint8>(fakeRxBuf_[1]);

        // 数据长度: 剩余所有字节，但最多8字节
        int dataLen = qMin(fakeRxBuf_.size() - 2, 8);
        QByteArray payload = fakeRxBuf_.mid(2, dataLen);

        LOG_DEBUG(kLogSource, QStringLiteral("Fake CAN received: ID=0x%1, data=%2")
                     .arg(canId, 0, 16)
                     .arg(QString::fromLatin1(payload.toHex(' '))));

        // 发出CAN帧接收信号
        emit canFrameReceived(canId, payload, false, false);

        // 从缓冲区移除已处理的字节
        fakeRxBuf_ = fakeRxBuf_.mid(2 + dataLen);
    }

    if (fakeReadNotifier_) {
        fakeReadNotifier_->setEnabled(true);
    }
}

}  // namespace comm
}  // namespace fanzhou
