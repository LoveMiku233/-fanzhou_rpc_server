/**
 * @file serial_comm.cpp
 * @brief Serial port communication adapter implementation
 */

#include "serial_comm.h"
#include "utils/utils.h"

#include <QDebug>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/serial.h>
#endif

namespace fanzhou {
namespace comm {

namespace {

/**
 * @brief Convert baud rate to termios speed constant
 */
speed_t toBaudConstant(int baud)
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

SerialComm::SerialComm(SerialConfig config, QObject *parent)
    : CommAdapter(parent)
    , config_(std::move(config))
{
}

SerialComm::~SerialComm()
{
    close();
}

bool SerialComm::open()
{
    if (fd_ >= 0) {
        return true;
    }

    fd_ = ::open(config_.device.toLocal8Bit().constData(),
                 O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        emit errorOccurred(utils::sysErrorString("open() failed"));
        return false;
    }

    if (!setupTermios()) {
        close();
        return false;
    }

    if (!setupRs485IfNeeded()) {
        close();
        return false;
    }

    readNotifier_ = new QSocketNotifier(fd_, QSocketNotifier::Read, this);
    connect(readNotifier_, &QSocketNotifier::activated, this, &SerialComm::onReadable);

    emit opened();
    return true;
}

void SerialComm::close()
{
    if (readNotifier_) {
        readNotifier_->setEnabled(false);
        readNotifier_->deleteLater();
        readNotifier_ = nullptr;
    }

    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }

    emit closed();
}

qint64 SerialComm::writeBytes(const QByteArray &data)
{
    if (fd_ < 0) {
        emit errorOccurred(QStringLiteral("Serial not opened"));
        return -1;
    }

    if (data.isEmpty()) {
        return 0;
    }

    const char *ptr = data.constData();
    auto remaining = data.size();
    qint64 totalWritten = 0;

    while (remaining > 0) {
        ssize_t n = ::write(fd_, ptr, static_cast<size_t>(remaining));
        if (n > 0) {
            totalWritten += n;
            ptr += n;
            remaining -= n;
            continue;
        }

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }

        emit errorOccurred(utils::sysErrorString("Serial write failed"));
        return -1;
    }

    return totalWritten;
}

void SerialComm::onReadable()
{
    if (fd_ < 0) {
        return;
    }

    if (readNotifier_) {
        readNotifier_->setEnabled(false);
    }

    QByteArray buffer;
    char buf[4096];

    for (;;) {
        ssize_t n = ::read(fd_, buf, sizeof(buf));
        if (n > 0) {
            buffer.append(buf, static_cast<int>(n));
            continue;
        }
        if (n == 0) {
            break;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }

        emit errorOccurred(utils::sysErrorString("Serial read failed"));
        break;
    }

    if (!buffer.isEmpty()) {
        emit bytesReceived(buffer);
    }

    if (readNotifier_) {
        readNotifier_->setEnabled(true);
    }
}

bool SerialComm::setupTermios()
{
    struct termios tio {};
    if (::tcgetattr(fd_, &tio) != 0) {
        emit errorOccurred(utils::sysErrorString("tcgetattr failed"));
        return false;
    }

    // Raw mode
    tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR |
                     IGNCR | ICRNL | IXON | IXOFF | IXANY);
    tio.c_oflag &= ~OPOST;
    tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tio.c_cflag |= (CLOCAL | CREAD);

    // Data bits
    tio.c_cflag &= ~CSIZE;
    switch (config_.dataBits) {
    case 5: tio.c_cflag |= CS5; break;
    case 6: tio.c_cflag |= CS6; break;
    case 7: tio.c_cflag |= CS7; break;
    case 8:
    default: tio.c_cflag |= CS8; break;
    }

    // Stop bits
    if (config_.stopBits == 2) {
        tio.c_cflag |= CSTOPB;
    } else {
        tio.c_cflag &= ~CSTOPB;
    }

    // Parity - convert to uppercase safely
    const unsigned char parityAsUnsigned = static_cast<unsigned char>(config_.parity);
    const char parityChar = static_cast<char>(std::toupper(parityAsUnsigned));
    if (parityChar == 'N') {
        tio.c_cflag &= ~PARENB;
        tio.c_iflag &= ~INPCK;
    } else if (parityChar == 'E') {
        tio.c_cflag |= PARENB;
        tio.c_cflag &= ~PARODD;
        tio.c_iflag |= INPCK;
    } else if (parityChar == 'O') {
        tio.c_cflag |= PARENB;
        tio.c_cflag |= PARODD;
        tio.c_iflag |= INPCK;
    }

    // Disable hardware flow control
#ifdef CRTSCTS
    tio.c_cflag &= ~CRTSCTS;
#endif

    // Read timeout
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 1;

    speed_t speed = toBaudConstant(config_.baudRate);
    ::cfsetispeed(&tio, speed);
    ::cfsetospeed(&tio, speed);

    if (::tcsetattr(fd_, TCSANOW, &tio) != 0) {
        emit errorOccurred(utils::sysErrorString("tcsetattr failed"));
        return false;
    }

    ::tcflush(fd_, TCIOFLUSH);
    return true;
}

bool SerialComm::setupRs485IfNeeded()
{
    if (!config_.rs485) {
        return true;
    }

#ifdef __linux__
    struct serial_rs485 rs485 {};
    rs485.flags |= SER_RS485_ENABLED;
    rs485.flags |= SER_RS485_RTS_ON_SEND;
    rs485.flags &= ~SER_RS485_RTS_AFTER_SEND;

    rs485.delay_rts_before_send = config_.rs485DelayBeforeUs;
    rs485.delay_rts_after_send = config_.rs485DelayAfterUs;

    if (::ioctl(fd_, TIOCSRS485, &rs485) < 0) {
        emit errorOccurred(utils::sysErrorString("TIOCSRS485 failed"));
        return false;
    }
    return true;
#else
    emit errorOccurred(QStringLiteral("RS485 mode requested but not on Linux"));
    return false;
#endif
}

}  // namespace comm
}  // namespace fanzhou
