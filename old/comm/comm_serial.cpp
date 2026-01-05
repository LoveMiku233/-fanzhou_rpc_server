#include "comm_serial.h"
#include "utils/utils.h"

#include <QDebug>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <termios.h>

#ifdef __linux__
#include <linux/serial.h>
#endif

static speed_t toBaud(int baud)
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
    default:
        return B115200;
    }
}


// serial open
bool CommSerial::open() {
    if (fd_ >= 0) return true;

    fd_ = ::open(cfg_.dev.toLocal8Bit().constData(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        // open failed, emit error
        emit errorOccurred(sysErrStr("open(%1) failed: %2"));
        return false;
    }
    // config
    if (!setupTermios()) {
        close();
        return false;
    }

    if (!setupRs485IfNeeded()) {
        close();
        return false;
    }

    rd_ = new QSocketNotifier(fd_, QSocketNotifier::Read, this);
    connect(rd_, &QSocketNotifier::activated, this, &CommSerial::onReadable);

    emit opened();
    return true;
}

void CommSerial::close()
{
    if (rd_) {
        rd_->setEnabled(false);
        rd_->deleteLater();
        rd_ = nullptr;
    }

    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }

    emit closed();
}

int64_t CommSerial::wirteBytes(const QByteArray &data) {
    if (fd_ < 0) {
        emit errorOccurred("serial not opened");
        return -1;
    }
    if (data.isEmpty()) return 0;

    const char* p = data.constData();
    auto left = data.size();
    int64_t total = 0;

    while (left > 0) {
        auto n = ::write(fd_, p, size_t(left));
        if (n > 0) {
            total += n;
            p += n;
            left -= n;
            continue;
        }

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        emit errorOccurred(sysErrStr("serial write failed: %1"));
        return -1;
    }
    return total;
}

void CommSerial::onReadable() {
    if (fd_ < 0) return;
    if (rd_) rd_->setEnabled(false);

    QByteArray out;
    char buf[4096];

    for (;;) {
        ssize_t n = ::read(fd_, buf, sizeof(buf));
        if (n > 0) {
            out.append(buf, int(n));
            continue;
        }
        if (n == 0) {
            // EOF
            break;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }

        emit errorOccurred(sysErrStr("serial read failed: %1"));
        break;
    }

    if (!out.isEmpty()) {
        emit bytesReceived(out);
    }

    if (rd_) rd_->setEnabled(true);
}

bool CommSerial::setupTermios()
{
    termios tio {};
    if (tcgetattr(fd_, &tio) != 0) {
        emit errorOccurred(sysErrStr("tcgetattr failed: %1"));
        return false;
    }

    // raw mode
    tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY);
    tio.c_oflag &= ~OPOST;
    tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tio.c_cflag |= (CLOCAL | CREAD);

    // 数据位
    tio.c_cflag &= ~CSIZE;
    switch (cfg_.dataBits) {
    case 5: tio.c_cflag |= CS5; break;
    case 6: tio.c_cflag |= CS6; break;
    case 7: tio.c_cflag |= CS7; break;
    case 8:
    default: tio.c_cflag |= CS8; break;
    }

    // 停止位
    if (cfg_.stopBits == 2) tio.c_cflag |= CSTOPB;
    else tio.c_cflag &= ~CSTOPB;

    // 校验
    if (cfg_.parity == 'N' || cfg_.parity == 'n') {
        tio.c_cflag &= ~PARENB;
        tio.c_iflag &= ~INPCK;
    } else if (cfg_.parity == 'E' || cfg_.parity == 'e') {
        tio.c_cflag |= PARENB;
        tio.c_cflag &= ~PARODD;
        tio.c_iflag |= INPCK;
    } else if (cfg_.parity == 'O' || cfg_.parity == 'o') {
        tio.c_cflag |= PARENB;
        tio.c_cflag |= PARODD;
        tio.c_iflag |= INPCK;
    }

    // 关闭硬件流控
#ifdef CRTSCTS
    tio.c_cflag &= ~CRTSCTS;
#endif

    // 读取超时策略（VTIME 是 0.1s 单位）
    // 非 canonical 模式下：
    // VMIN=0, VTIME=1 => read 至多等 100ms，有数据就返回
    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 1;

    speed_t spd = toBaud(cfg_.baud);
    cfsetispeed(&tio, spd);
    cfsetospeed(&tio, spd);

    if (tcsetattr(fd_, TCSANOW, &tio) != 0) {
        emit errorOccurred(QString("tcsetattr failed: %1")
                               .arg(QString::fromLocal8Bit(strerror(errno))));
        return false;
    }

    // 清空缓冲
    tcflush(fd_, TCIOFLUSH);
    return true;
}

bool CommSerial::setupRs485IfNeeded()
{
    if (!cfg_.rs485) return true;

#ifdef __linux__
    serial_rs485 rs485 {};
    rs485.flags |= SER_RS485_ENABLED;
    rs485.flags |= SER_RS485_RTS_ON_SEND;
    rs485.flags &= ~SER_RS485_RTS_AFTER_SEND;

    rs485.delay_rts_before_send = cfg_.rs485DelayBeforeUs;
    rs485.delay_rts_after_send  = cfg_.rs485DelayAfterUs;

    if (ioctl(fd_, TIOCSRS485, &rs485) < 0) {
        emit errorOccurred(QString("TIOCSRS485 failed (dev=%1): %2")
                               .arg(cfg_.dev, QString::fromLocal8Bit(strerror(errno))));
        return false;
    }
    return true;
#else
    emit errorOccurred("RS485 mode requested but not on linux");
    return false;
#endif
}
