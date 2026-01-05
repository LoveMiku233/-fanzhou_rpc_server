#include "comm_can.h"
#include "utils/utils.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <QDebug>

#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <fcntl.h>

#include <linux/can.h>
#include <linux/can/raw.h>


bool CommCan::open()
{
    if (s_ >= 0) return true;

    // 1. create a SocketCan Raw socket
    s_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s_ < 0) {
        emit errorOccurred(sysErrStr("socket(PF_CAN) failed"));
    }

    // 2. can fd
    if (cfg_.canFd) {
        int enable = 1;
        // s_ : socket fd, SOL_CAN_RAW : CAN RAW Protocol layer, CAN_RAW_FD_FRAMES : option enable can-fd frames
        if (setsockopt(s_, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable, sizeof(enable)) != 0) {
            emit errorOccurred(sysErrStr("socket(CAN_RAW_FD_FRAMES) failed"));
            return false;
        }
    }

    //
    const int flags = ::fcntl(s_, F_GETFL, 0);
    if (flags < 0 || ::fcntl(s_, F_SETFL, flags | O_NONBLOCK) < 0) {
        emit errorOccurred(sysErrStr("fcntl(O_NONBLOCK) failed"));
        close();
        return false;
    }

    // 3. get ifindex by api
    struct ifreq ifr {};
    // copy the qt interface name (e.g., can0) into the ifreq structure;
    // ifr_name : interface name, IF_NAMESIZE: Maximum len of network interface name;
    ::strncpy(ifr.ifr_name, cfg_.ifname.toLocal8Bit().constData(), IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    if (ioctl(s_, SIOCGIFINDEX, &ifr) < 0) {
        emit errorOccurred(sysErrStr("ioctl(SIOCGIFINDEX) failed (is can0 up?)"));
        close();
        return false;
    }

    // 4. bind to can0
    sockaddr_can addr {};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (::bind(s_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        emit errorOccurred(sysErrStr("bind(AF_CAN) failed"));
        close();
        return false;
    }

    if (!txTimer_) {
        txTimer_ = new QTimer(this);
        txTimer_->setSingleShot(false);
        txTimer_->setInterval(2); // 每 2ms 尝试发一帧（可调：1~10ms）
        connect(txTimer_, &QTimer::timeout, this, &CommCan::onTxPump);
    }
    txBackoffMs_ = 0;
    txTimer_->start();

    // 5. with the qt event loop
    rd_ = new QSocketNotifier(s_, QSocketNotifier::Read, this);
    connect(rd_, &QSocketNotifier::activated, this, &CommCan::onReadable);

    emit opened();
    return true;
}

void CommCan::close()
{
    // delete Qt notifier
    if (rd_) {
        rd_->setEnabled(false);
        rd_->deleteLater();
        rd_ = nullptr;
    }

    // delete socket
    if (s_ >= 0) {
        ::close(s_);
        s_ = -1;
    }

    if (txTimer_) txTimer_->stop();
    txq_.clear();
    txBackoffMs_ = 0;

    emit closed();
}

bool CommCan::sendFrame(quint32 canId, const QByteArray &payload, bool extended, bool rtr)
{
    if (s_ < 0) {
        emit errorOccurred("Can not opened");
        return false;
    }
    if (payload.size() > 8) {
        emit errorOccurred("Can payload must be <= 8 bytes");
        return false;
    }

    if (txq_.size() >= txMaxQueue_) {
        emit errorOccurred(QString("CAN tx queue overflow (%1), dropping").arg(txq_.size()));
        return false;
    }

    struct can_frame frame {};
    frame.can_id = canId;
    if (extended) frame.can_id |= CAN_EFF_FLAG;
    if (rtr)      frame.can_id |= CAN_RTR_FLAG;

    frame.can_dlc = static_cast<__u8>(payload.size());
    if (!payload.isEmpty()) {
        ::memcpy(frame.data, payload.constData(), size_t(payload.size()));
    }

    txq_.enqueue(TxItem{frame});
    return true; // 入队成功
}

void CommCan::onTxPump()
{
    if (s_ < 0) return;
    if (txq_.isEmpty()) return;

    if (txBackoffMs_ > 0) {
        txBackoffMs_ -= txTimer_->interval();
        if (txBackoffMs_ < 0) txBackoffMs_ = 0;
        return;
    }

    const TxItem item = txq_.head();
    errno = 0;
    const ssize_t n = ::write(s_, &item.frame, sizeof(item.frame));
    if (n == sizeof(item.frame)) {
        txq_.dequeue();
        return;
    }

    // n != sizeof(frame)
    if (errno == ENOBUFS || errno == EAGAIN || errno == EWOULDBLOCK) {
        // 内核队列满
        txBackoffMs_ = 10;
        return;
    }

    // 其他错误：丢掉这一帧
    emit errorOccurred(sysErrStr("CAN write failed"));
    txq_.dequeue();
}

int64_t CommCan::wirteBytes(const QByteArray& data)
{
    emit errorOccurred("[TODO] CommCan::wirteBytes");
    return -1;
}

void CommCan::onReadable() {
    if (s_ < 0) return;
    if (rd_) rd_->setEnabled(false);
    for (;;) {
        struct can_frame frame {};
        const ssize_t n = ::read(s_, &frame, sizeof(frame));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            emit errorOccurred(sysErrStr("CAN read failed"));
            break;
        }
        // size != frame
        if (n != sizeof(frame)) {
            break;
        }

        const bool extended = (frame.can_id & CAN_EFF_FLAG);
        const bool rtr      = (frame.can_id & CAN_RTR_FLAG);
        const quint32 canId = frame.can_id & (extended ? CAN_EFF_MASK : CAN_SFF_MASK);

        QByteArray payload;
        payload.append(reinterpret_cast<const char*>(frame.data), frame.can_dlc);
        emit canFrameReceived(canId, payload, extended, rtr);
    }
    if (rd_) rd_->setEnabled(true);
}


