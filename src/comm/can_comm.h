/**
 * @file can_comm.h
 * @brief CAN bus communication adapter
 *
 * Provides CAN bus communication using SocketCAN on Linux.
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
 * @brief CAN bus configuration
 */
struct CanConfig {
    QString interface = QStringLiteral("can0");  ///< CAN interface name
    bool canFd = false;                          ///< Enable CAN FD mode
};

/**
 * @brief CAN bus communication adapter
 *
 * Implements CAN bus communication using Linux SocketCAN.
 * Supports standard CAN and CAN FD modes.
 */
class CanComm : public CommAdapter
{
    Q_OBJECT

public:
    /**
     * @brief Construct a CAN communication adapter
     * @param config CAN configuration
     * @param parent Parent object
     */
    explicit CanComm(CanConfig config, QObject *parent = nullptr);

    ~CanComm() override;

    bool open() override;
    void close() override;
    qint64 writeBytes(const QByteArray &data) override;

    /**
     * @brief Send a CAN frame
     * @param canId CAN identifier
     * @param payload Frame data (max 8 bytes for classic CAN)
     * @param extended Use extended (29-bit) identifier
     * @param rtr Remote transmission request
     * @return True if frame was queued successfully
     */
    bool sendFrame(quint32 canId, const QByteArray &payload,
                   bool extended = false, bool rtr = false);

signals:
    /**
     * @brief Emitted when a CAN frame is received
     * @param canId CAN identifier
     * @param payload Frame data
     * @param extended True if extended identifier
     * @param rtr True if remote transmission request
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
