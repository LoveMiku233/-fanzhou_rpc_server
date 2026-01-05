/**
 * @file system_settings.h
 * @brief System settings and command execution
 *
 * Provides CAN configuration and system command execution functionality.
 */

#ifndef FANZHOU_SYSTEM_SETTINGS_H
#define FANZHOU_SYSTEM_SETTINGS_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;

namespace fanzhou {
namespace config {

/**
 * @brief System settings controller
 *
 * Provides CAN bus configuration and system command execution,
 * including CAN interface start/stop, bitrate setting, and frame sending.
 */
class SystemSettings : public QObject
{
    Q_OBJECT

public:
    explicit SystemSettings(QObject *parent = nullptr);

    /**
     * @brief Execute a system command (blocking)
     * @param program Program name
     * @param args Command arguments
     * @param timeoutMs Timeout in milliseconds
     * @return Command output, or empty on failure
     */
    QString runCommand(const QString &program,
                       const QStringList &args,
                       int timeoutMs = 5000);

    /**
     * @brief Bring CAN interface down
     * @param interface Interface name
     * @return True if successful
     */
    bool canDown(const QString &interface);

    /**
     * @brief Bring CAN interface up
     * @param interface Interface name
     * @return True if successful
     */
    bool canUp(const QString &interface);

    /**
     * @brief Set CAN bitrate
     * @param interface Interface name
     * @param bitrate Bitrate in bits per second
     * @param tripleSampling Enable triple sampling
     * @return True if successful
     */
    bool setCanBitrate(const QString &interface, int bitrate,
                       bool tripleSampling = false);

    /**
     * @brief Send CAN frame using cansend utility
     * @param interface Interface name
     * @param canId CAN identifier
     * @param data Frame data
     * @param extended Use extended identifier
     * @return True if successful
     */
    bool sendCanFrame(const QString &interface, quint32 canId,
                      const QByteArray &data, bool extended = false);

    /**
     * @brief Start candump capture
     * @param interface Interface name
     * @param extraArgs Additional arguments
     * @return True if started successfully
     */
    bool startCanDump(const QString &interface,
                      const QStringList &extraArgs = {});

    /**
     * @brief Stop candump capture
     */
    void stopCanDump();

signals:
    /**
     * @brief Emitted with command output
     * @param line Output line
     */
    void commandOutput(const QString &line);

    /**
     * @brief Emitted on error
     * @param error Error message
     */
    void errorOccurred(const QString &error);

    /**
     * @brief Emitted with candump output line
     * @param line CAN frame data
     */
    void candumpLine(const QString &line);

private:
    static QString toCanSendArg(quint32 canId, const QByteArray &data,
                                 bool extended);

    QProcess *dumpProcess_ = nullptr;
};

}  // namespace config
}  // namespace fanzhou

#endif  // FANZHOU_SYSTEM_SETTINGS_H
