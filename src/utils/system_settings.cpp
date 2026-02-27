/**
 * @file system_settings.cpp
 * @brief 系统设置实现
 */

#include "system_settings.h"

#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>

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

bool SystemSettings::runCommandWithStatus(const QString &program,
                                           const QStringList &args,
                                           int timeoutMs)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(args);
    process.start();

    if (!process.waitForStarted(1000)) {
        emit errorOccurred(QStringLiteral("Failed to start: %1").arg(program));
        return false;
    }

    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        emit errorOccurred(
            QStringLiteral("Timeout running: %1 %2").arg(program, args.join(' ')));
        return false;
    }

    const int exitCode = process.exitCode();
    if (exitCode != 0) {
        const QByteArray stdErr = process.readAllStandardError();
        emit errorOccurred(
            QStringLiteral("Command failed (%1): %2 %3 | stderr=%4")
                .arg(exitCode)
                .arg(program, args.join(' '),
                     QString::fromLocal8Bit(stdErr)));
        return false;
    }

    const QByteArray stdOut = process.readAllStandardOutput();
    const QString output = QString::fromLocal8Bit(stdOut).trimmed();
    if (!output.isEmpty()) {
        emit commandOutput(output);
    }
    return true;
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

    runCommand(QStringLiteral("ip"),
               {QStringLiteral("link"), QStringLiteral("set"), interface,
                QStringLiteral("down")});

    QStringList args{QStringLiteral("link"), QStringLiteral("set"), interface,
                     QStringLiteral("type"), QStringLiteral("can"),
                     QStringLiteral("bitrate"), QString::number(bitrate)};
    if (tripleSampling) {
        args << QStringLiteral("triple-sampling") << QStringLiteral("on");
    }

    runCommand(QStringLiteral("ip"), args);
    runCommand(QStringLiteral("ip"),
               {QStringLiteral("link"), QStringLiteral("set"), interface,
                QStringLiteral("up")});

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

// ===================== RTC时间管理实现 =====================

QString SystemSettings::getSystemTime()
{
    // 使用date命令获取ISO格式时间
    return runCommand(QStringLiteral("date"),
                      {QStringLiteral("+%Y-%m-%d %H:%M:%S")});
}

bool SystemSettings::setSystemTime(const QString &datetime)
{
    if (datetime.isEmpty()) {
        emit errorOccurred(QStringLiteral("setSystemTime: datetime is empty"));
        return false;
    }

    // 使用date -s命令设置系统时间
    const QString result = runCommand(QStringLiteral("date"),
                                       {QStringLiteral("-s"), datetime});
    return !result.isEmpty();
}

bool SystemSettings::saveHardwareClock()
{
    // 使用hwclock -w将系统时间写入硬件时钟
    return runCommandWithStatus(QStringLiteral("hwclock"),
                                 {QStringLiteral("-w")});
}

QString SystemSettings::readHardwareClock()
{
    // 使用hwclock -r读取硬件时钟
    return runCommand(QStringLiteral("hwclock"), {QStringLiteral("-r")});
}

// ===================== 网络配置管理实现 =====================

QString SystemSettings::getNetworkInfo(const QString &interface)
{
    QStringList args;
    if (!interface.isEmpty()) {
        args << interface;
    }
    return runCommand(QStringLiteral("ifconfig"), args);
}

QJsonObject SystemSettings::getNetworkInfoDetailed(const QString &interface)
{
    QJsonObject result;
    
    // 获取IP地址信息
    QString ipOutput = runCommand(QStringLiteral("ip"),
        {QStringLiteral("addr"), QStringLiteral("show")});
    result[QStringLiteral("ipAddr")] = ipOutput;
    
    // 获取路由信息
    QString routeOutput = runCommand(QStringLiteral("ip"),
        {QStringLiteral("route"), QStringLiteral("show")});
    result[QStringLiteral("routes")] = routeOutput;
    
    // 获取DNS信息
    QString dnsOutput = runCommand(QStringLiteral("cat"),
        {QStringLiteral("/etc/resolv.conf")});
    result[QStringLiteral("dns")] = dnsOutput;
    
    // 获取接口列表
    QString interfaceList = runCommand(QStringLiteral("ls"),
        {QStringLiteral("/sys/class/net/")});
    result[QStringLiteral("interfaces")] = interfaceList;
    
    // 如果指定了接口，获取该接口的详细信息
    if (!interface.isEmpty()) {
        // 获取接口状态
        QString stateOutput = runCommand(QStringLiteral("cat"),
            {QStringLiteral("/sys/class/net/%1/operstate").arg(interface)});
        result[QStringLiteral("state")] = stateOutput;
        
        // 获取MAC地址
        QString macOutput = runCommand(QStringLiteral("cat"),
            {QStringLiteral("/sys/class/net/%1/address").arg(interface)});
        result[QStringLiteral("mac")] = macOutput;
    }
    
    return result;
}

bool SystemSettings::pingTest(const QString &host, int count, int timeoutSec)
{
    if (host.isEmpty()) {
        emit errorOccurred(QStringLiteral("pingTest: host is empty"));
        return false;
    }

    // ping -c count -W timeout host
    QStringList args;
    args << QStringLiteral("-c") << QString::number(count);
    args << QStringLiteral("-W") << QString::number(timeoutSec);
    args << host;

    // 使用 runCommandWithStatus 检查退出码，ping成功返回0，失败返回非0
    return runCommandWithStatus(QStringLiteral("ping"), args,
                                 (timeoutSec + 2) * 1000);
}

bool SystemSettings::setStaticIp(const QString &interface,
                                  const QString &address,
                                  const QString &netmask,
                                  const QString &gateway)
{
    if (interface.isEmpty() || address.isEmpty()) {
        emit errorOccurred(
            QStringLiteral("setStaticIp: interface or address is empty"));
        return false;
    }

    // 使用ifconfig设置IP地址和子网掩码
    QStringList ifArgs;
    ifArgs << interface << address;
    if (!netmask.isEmpty()) {
        ifArgs << QStringLiteral("netmask") << netmask;
    }
    
    bool success = runCommandWithStatus(QStringLiteral("ifconfig"), ifArgs);
    if (!success) {
        return false;
    }

    // 设置网关（如果提供）
    if (!gateway.isEmpty()) {
        // 先删除默认路由，再添加新的
        // 删除默认路由可能失败（如果不存在），这不是错误
        runCommand(QStringLiteral("route"),
                   {QStringLiteral("del"), QStringLiteral("default")});
        success = runCommandWithStatus(QStringLiteral("route"),
                   {QStringLiteral("add"), QStringLiteral("default"),
                    QStringLiteral("gw"), gateway});
    }

    return success;
}

bool SystemSettings::enableDhcp(const QString &interface)
{
    if (interface.isEmpty()) {
        emit errorOccurred(QStringLiteral("enableDhcp: interface is empty"));
        return false;
    }

    // 先释放现有的DHCP租约
    runCommand(QStringLiteral("dhclient"),
               {QStringLiteral("-r"), interface});

    // 使用dhclient获取DHCP地址
    return runCommandWithStatus(QStringLiteral("dhclient"),
                                 {interface}, 30000);
}

bool SystemSettings::setDns(const QString &primary, const QString &secondary)
{
    if (primary.isEmpty()) {
        emit errorOccurred(QStringLiteral("setDns: primary DNS is empty"));
        return false;
    }

    // 验证DNS地址格式（简单的IP地址格式检查）
    static QRegularExpression ipRegex(
        QStringLiteral("^\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}$"));
    
    if (!ipRegex.match(primary).hasMatch()) {
        emit errorOccurred(QStringLiteral("setDns: invalid primary DNS format"));
        return false;
    }
    
    if (!secondary.isEmpty() && !ipRegex.match(secondary).hasMatch()) {
        emit errorOccurred(QStringLiteral("setDns: invalid secondary DNS format"));
        return false;
    }

    // 构建resolv.conf内容
    QString content = QStringLiteral("nameserver %1\n").arg(primary);
    if (!secondary.isEmpty()) {
        content += QStringLiteral("nameserver %1\n").arg(secondary);
    }

    // 使用QFile直接写入文件，避免命令注入风险
    QFile file(QStringLiteral("/etc/resolv.conf"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorOccurred(QStringLiteral("setDns: cannot open /etc/resolv.conf for writing"));
        return false;
    }
    
    QTextStream out(&file);
    out << content;
    file.close();
    
    return true;
}

}  // namespace config
}  // namespace fanzhou
