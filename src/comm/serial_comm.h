/**
 * @file serial_comm.h
 * @brief Serial port communication adapter
 *
 * Provides serial port communication with RS485 support on Linux.
 */

#ifndef FANZHOU_SERIAL_COMM_H
#define FANZHOU_SERIAL_COMM_H

#include <QObject>
#include <QSocketNotifier>

#include "base/comm_adapter.h"

namespace fanzhou {
namespace comm {

/**
 * @brief Serial port configuration
 */
struct SerialConfig {
    QString device;                 ///< Device path (e.g., "/dev/ttyS0")
    int baudRate = 115200;          ///< Baud rate
    int dataBits = 8;               ///< Data bits (5-8)
    int stopBits = 1;               ///< Stop bits (1 or 2)
    char parity = 'N';              ///< Parity: 'N'=None, 'E'=Even, 'O'=Odd
    bool rs485 = false;             ///< Enable RS485 mode
    int rs485DelayBeforeUs = 0;     ///< RS485 delay before send (microseconds)
    int rs485DelayAfterUs = 0;      ///< RS485 delay after send (microseconds)
};

/**
 * @brief Serial port communication adapter
 *
 * Implements serial port communication with optional RS485 mode support.
 */
class SerialComm : public CommAdapter
{
    Q_OBJECT

public:
    /**
     * @brief Construct a serial communication adapter
     * @param config Serial port configuration
     * @param parent Parent object
     */
    explicit SerialComm(SerialConfig config, QObject *parent = nullptr);

    ~SerialComm() override;

    bool open() override;
    void close() override;
    qint64 writeBytes(const QByteArray &data) override;

private slots:
    void onReadable();

private:
    bool setupTermios();
    bool setupRs485IfNeeded();

    SerialConfig config_;
    int fd_ = -1;
    QSocketNotifier *readNotifier_ = nullptr;
};

}  // namespace comm
}  // namespace fanzhou

#endif  // FANZHOU_SERIAL_COMM_H
