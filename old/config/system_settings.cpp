#include "system_settings.h"

#include <QProcess>
#include <QDateTime>

SystemSettings::SystemSettings(QObject* parent) : QObject(parent) {}

QString SystemSettings::runCommand(const QString& program,
                                  const QStringList& args,
                                  int timeoutMs)
{
    QProcess p;
    p.setProgram(program);
    p.setArguments(args);
    p.start();

    if (!p.waitForStarted(1000)) {
        emit errorOccurred(QString("Failed to start: %1").arg(program));
        return {};
    }
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(1000);
        emit errorOccurred(QString("Timeout running: %1 %2").arg(program, args.join(' ')));
        return {};
    }

    const int ec = p.exitCode();
    const QByteArray out = p.readAllStandardOutput();
    const QByteArray err = p.readAllStandardError();

    if (ec != 0) {
        emit errorOccurred(QString("Command failed (%1): %2 %3 | stderr=%4")
                           .arg(ec).arg(program, args.join(' '),
                                QString::fromLocal8Bit(err)));
        return {};
    }

    const QString s = QString::fromLocal8Bit(out).trimmed();
    if (!s.isEmpty()) emit commandOutput(s);
    return s;
}

bool SystemSettings::canDown(const QString& ifname)
{
    // 更推荐 ip；ifconfig 也行，这里用 ip
    runCommand("ip", {"link", "set", ifname, "down"});
    return true;
}

bool SystemSettings::canUp(const QString& ifname)
{
    runCommand("ip", {"link", "set", ifname, "up"});
    return true;
}

bool SystemSettings::setCanBitrate(const QString& ifname, int bitrate, bool tripleSampling)
{
    if (ifname.isEmpty() || bitrate <= 0) {
        emit errorOccurred(QString("setCanBitrate: invalid args ifname='%1' bitrate=%2")
                           .arg(ifname).arg(bitrate));
        return false;
    }

    if (runCommand("ifconfig", {ifname, "down"}).isNull()) {

    }
    QStringList args {ifname, "bitrate", QString::number(bitrate)};
    if (tripleSampling) {
        args << "ctrlmode" << "triple-sampling" << "on";
    }

    runCommand("canconfig", args);

    runCommand("ifconfig", {ifname, "up"});

    return true;
}

QString SystemSettings::toCanSendArg(quint32 canId, const QByteArray& data, bool extended)
{
    // cansend 格式：<can_id>#<data>
    // extended: 用 8 位 ID（cansend 会自动按 ID 位宽识别；也可用 -e 参数，但我们用标准格式即可）
    const QString idStr = QString::number(canId, 16).toUpper();
    const QString dataStr = data.toHex().toUpper(); // 不带空格
    return idStr + "#" + dataStr;
}

bool SystemSettings::sendCanFrame(const QString& ifname, quint32 canId, const QByteArray& data, bool extended)
{
    if (data.size() > 8) {
        emit errorOccurred("CAN data too long (>8)");
        return false;
    }
    // 使用 can-utils 的 cansend：cansend can0 123#112233...
    // 你的示例是：cansend can0 -e 0x11 0x22 ...（那更像另一个工具/参数风格）
    // 这里采用通用 can-utils 格式。
    QStringList args;
    args << ifname << toCanSendArg(canId, data, extended);
    runCommand("cansend", args);
    return true;
}

bool SystemSettings::startCanDump(const QString& ifname, const QStringList& extraArgs)
{
    stopCanDump();

    dumpProc_ = new QProcess(this);
    dumpProc_->setProgram("candump");

    QStringList args;
    args << extraArgs;
    args << ifname;
    dumpProc_->setArguments(args);

    connect(dumpProc_, &QProcess::readyReadStandardOutput, this, [this](){
        while (dumpProc_ && dumpProc_->canReadLine()) {
            const QByteArray line = dumpProc_->readLine();
            const QString s = QString::fromLocal8Bit(line).trimmed();
            if (!s.isEmpty()) emit candumpLine(s);
        }
    });
    connect(dumpProc_, &QProcess::readyReadStandardError, this, [this](){
        const QByteArray e = dumpProc_->readAllStandardError();
        const QString s = QString::fromLocal8Bit(e).trimmed();
        if (!s.isEmpty()) emit errorOccurred(QString("candump stderr: %1").arg(s));
    });

    connect(dumpProc_,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [this](int code, QProcess::ExitStatus st){
                emit commandOutput(QString("candump finished code=%1 status=%2")
                                   .arg(code).arg(int(st)));
                if (dumpProc_) {
                    dumpProc_->deleteLater();
                    dumpProc_ = nullptr;
                }
            });

    dumpProc_->start();
    if (!dumpProc_->waitForStarted(1000)) {
        emit errorOccurred("Failed to start candump (is can-utils installed?)");
        stopCanDump();
        return false;
    }
    return true;
}

void SystemSettings::stopCanDump()
{
    if (!dumpProc_) return;
    dumpProc_->kill();
    dumpProc_->waitForFinished(500);
    dumpProc_->deleteLater();
    dumpProc_ = nullptr;
}
