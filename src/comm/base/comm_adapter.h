/**
 * @file comm_adapter.h
 * @brief Abstract communication adapter interface
 *
 * Defines the base interface for all communication adapters (CAN, Serial, etc.)
 */

#ifndef FANZHOU_COMM_ADAPTER_H
#define FANZHOU_COMM_ADAPTER_H

#include <QByteArray>
#include <QObject>

namespace fanzhou {
namespace comm {

/**
 * @brief Abstract base class for communication adapters
 *
 * Provides a common interface for different communication protocols.
 * Subclasses implement specific protocols like CAN or Serial.
 */
class CommAdapter : public QObject
{
    Q_OBJECT

public:
    explicit CommAdapter(QObject *parent = nullptr)
        : QObject(parent) {}

    virtual ~CommAdapter() = default;

    /**
     * @brief Open the communication channel
     * @return True if successful, false otherwise
     */
    virtual bool open() = 0;

    /**
     * @brief Close the communication channel
     */
    virtual void close() = 0;

    /**
     * @brief Write bytes to the communication channel
     * @param data Data to write
     * @return Number of bytes written, or -1 on error
     */
    virtual qint64 writeBytes(const QByteArray &data) = 0;

signals:
    /**
     * @brief Emitted when data is received
     * @param data Received data
     */
    void bytesReceived(const QByteArray &data);

    /**
     * @brief Emitted when an error occurs
     * @param msg Error message
     */
    void errorOccurred(const QString &msg);

    /**
     * @brief Emitted when the channel is opened
     */
    void opened();

    /**
     * @brief Emitted when the channel is closed
     */
    void closed();
};

}  // namespace comm
}  // namespace fanzhou

#endif  // FANZHOU_COMM_ADAPTER_H
