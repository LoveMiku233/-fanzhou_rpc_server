/**
 * @file can_comm.cpp
 * @brief CAN总线通信适配器实现
 */

#include "can_comm.h"
#include "utils/logger.h"
#include "utils/utils.h"

#include <QDebug>

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

bool CanComm::open()
{
    if (socket_ >= 0) {
        return true;
    }

    // Create SocketCAN raw socket
    socket_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socket_ < 0) {
        emit errorOccurred(utils::sysErrorString("socket(PF_CAN) failed"));
        return false;
    }

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

    emit closed();
}

bool CanComm::sendFrame(quint32 canId, const QByteArray &payload, bool extended, bool rtr)
{
    if (socket_ < 0) {
        LOG_WARNING(kLogSource, QStringLiteral("sendFrame failed: CAN not opened"));
        emit errorOccurred(QStringLiteral("CAN not opened"));
        return false;
    }

    if (payload.size() > 8) {
        LOG_WARNING(kLogSource, QStringLiteral("sendFrame failed: payload size %1 > 8").arg(payload.size()));
        emit errorOccurred(QStringLiteral("CAN payload must be <= 8 bytes"));
        return false;
    }

    if (txQueue_.size() >= kMaxTxQueueSize) {
        LOG_WARNING(kLogSource, QStringLiteral("sendFrame failed: TX queue overflow (%1)").arg(txQueue_.size()));
        emit errorOccurred(
            QStringLiteral("CAN TX queue overflow (%1), dropping").arg(txQueue_.size()));
        return false;
    }

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
        const quint32 canIdWithoutFlags = item.frame.can_id & CAN_SFF_MASK;
        LOG_DEBUG(kLogSource,
                  QStringLiteral("Frame sent: id=0x%1, dlc=%2")
                      .arg(canIdWithoutFlags, 0, 16)
                      .arg(item.frame.can_dlc));
        txQueue_.dequeue();
        // 成功发送后重置退避乘数
        txBackoffMultiplier_ = 0;
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
        }
        LOG_DEBUG(kLogSource, QStringLiteral("TX buffer full, backing off %1ms").arg(backoff));
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

}  // namespace comm
}  // namespace fanzhou
