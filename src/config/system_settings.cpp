/**
 * @file system_settings.cpp
 * @brief 系统设置实现
 */

#include "system_settings.h"

#include <QProcess>

namespace fanzhou {
namespace config {

SystemSettings::SystemSettings(QObject *parent)
    : QObject(parent)
{
}

QString SystemSettings::runCommand(const QString &program,
                                    const QStringList &args,
                                    int timeoutMs)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(args);
    process.start();

    if (!process.waitForStarted(1000)) {
        emit errorOccurred(QStringLiteral("Failed to start: %1").arg(program));
        return {};
    }

    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        emit errorOccurred(
            QStringLiteral("Timeout running: %1 %2").arg(program, args.join(' ')));
        return {};
    }

    const int exitCode = process.exitCode();
    const QByteArray stdOut = process.readAllStandardOutput();
    const QByteArray stdErr = process.readAllStandardError();

    if (exitCode != 0) {
        emit errorOccurred(
            QStringLiteral("Command failed (%1): %2 %3 | stderr=%4")
                .arg(exitCode)
                .arg(program, args.join(' '),
                     QString::fromLocal8Bit(stdErr)));
        return {};
    }

    const QString output = QString::fromLocal8Bit(stdOut).trimmed();
    if (!output.isEmpty()) {
        emit commandOutput(output);
    }
    return output;
}

bool SystemSettings::canDown(const QString &interface)
{
    runCommand(QStringLiteral("ip"),
               {QStringLiteral("link"), QStringLiteral("set"), interface,
                QStringLiteral("down")});
    return true;
}

bool SystemSettings::canUp(const QString &interface)
{
    runCommand(QStringLiteral("ip"),
               {QStringLiteral("link"), QStringLiteral("set"), interface,
                QStringLiteral("up")});
    return true;
}

bool SystemSettings::setCanBitrate(const QString &interface, int bitrate,
                                    bool tripleSampling)
{
    if (interface.isEmpty() || bitrate <= 0) {
        emit errorOccurred(
            QStringLiteral("setCanBitrate: invalid args interface='%1' bitrate=%2")
                .arg(interface)
                .arg(bitrate));
        return false;
    }

    runCommand(QStringLiteral("ifconfig"), {interface, QStringLiteral("down")});

    QStringList args{interface, QStringLiteral("bitrate"), QString::number(bitrate)};
    if (tripleSampling) {
        args << QStringLiteral("ctrlmode") << QStringLiteral("triple-sampling")
             << QStringLiteral("on");
    }

    runCommand(QStringLiteral("canconfig"), args);
    runCommand(QStringLiteral("ifconfig"), {interface, QStringLiteral("up")});

    return true;
}

QString SystemSettings::toCanSendArg(quint32 canId, const QByteArray &data,
                                       bool extended)
{
    Q_UNUSED(extended);
    const QString idStr = QString::number(canId, 16).toUpper();
    const QString dataStr = data.toHex().toUpper();
    return idStr + QStringLiteral("#") + dataStr;
}

bool SystemSettings::sendCanFrame(const QString &interface, quint32 canId,
                                    const QByteArray &data, bool extended)
{
    if (data.size() > 8) {
        emit errorOccurred(QStringLiteral("CAN data too long (>8)"));
        return false;
    }

    QStringList args;
    args << interface << toCanSendArg(canId, data, extended);
    runCommand(QStringLiteral("cansend"), args);
    return true;
}

bool SystemSettings::startCanDump(const QString &interface,
                                    const QStringList &extraArgs)
{
    stopCanDump();

    dumpProcess_ = new QProcess(this);
    dumpProcess_->setProgram(QStringLiteral("candump"));

    QStringList args;
    args << extraArgs;
    args << interface;
    dumpProcess_->setArguments(args);

    connect(dumpProcess_, &QProcess::readyReadStandardOutput, this, [this]() {
        while (dumpProcess_ && dumpProcess_->canReadLine()) {
            const QByteArray line = dumpProcess_->readLine();
            const QString text = QString::fromLocal8Bit(line).trimmed();
            if (!text.isEmpty()) {
                emit candumpLine(text);
            }
        }
    });

    connect(dumpProcess_, &QProcess::readyReadStandardError, this, [this]() {
        const QByteArray err = dumpProcess_->readAllStandardError();
        const QString text = QString::fromLocal8Bit(err).trimmed();
        if (!text.isEmpty()) {
            emit errorOccurred(QStringLiteral("candump stderr: %1").arg(text));
        }
    });

    connect(dumpProcess_,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus status) {
        emit commandOutput(
            QStringLiteral("candump finished code=%1 status=%2")
                .arg(code)
                .arg(static_cast<int>(status)));
        if (dumpProcess_) {
            dumpProcess_->deleteLater();
            dumpProcess_ = nullptr;
        }
    });

    dumpProcess_->start();
    if (!dumpProcess_->waitForStarted(1000)) {
        emit errorOccurred(
            QStringLiteral("Failed to start candump (is can-utils installed?)"));
        stopCanDump();
        return false;
    }
    return true;
}

void SystemSettings::stopCanDump()
{
    if (!dumpProcess_) {
        return;
    }
    dumpProcess_->kill();
    dumpProcess_->waitForFinished(500);
    dumpProcess_->deleteLater();
    dumpProcess_ = nullptr;
}

}  // namespace config
}  // namespace fanzhou
