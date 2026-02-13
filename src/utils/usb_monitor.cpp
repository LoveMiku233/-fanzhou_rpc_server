/**
 * @file usb_monitor.cpp
 * @brief U盘热插拔监控实现
 */

#include "usb_monitor.h"
#include "logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

namespace fanzhou {

namespace {
const char *const kLogSource = "UsbMonitor";
}

const QString UsbMonitor::kUsbDevice = QStringLiteral("/dev/sda1");
const QString UsbMonitor::kMountPoint = QStringLiteral("/mnt/usb");
const QString UsbMonitor::kRpcServerBin = QStringLiteral("/opt/fanzhou_rpc_server/bin");
const QString UsbMonitor::kRpcClientBin = QStringLiteral("/opt/fanzhou_rpc_client/bin");
const QString UsbMonitor::kConfigPath = QStringLiteral("/var/lib/fanzhou_core/core.json");

UsbMonitor::UsbMonitor(QObject *parent)
    : QObject(parent)
    , timer_(new QTimer(this))
    , rpcLogPath_(QStringLiteral("/var/log/fanzhou_core/core.log"))
    , qtLogPath_(QStringLiteral("/var/log/fanzhou_core/qt_app.log"))
{
    connect(timer_, &QTimer::timeout, this, &UsbMonitor::onCheckUsb);
}

UsbMonitor::~UsbMonitor()
{
    stop();
}

void UsbMonitor::start(int intervalMs)
{
    LOG_INFO(kLogSource, QStringLiteral("U盘监控已启动，检测间隔: %1ms").arg(intervalMs));
    timer_->start(intervalMs);
}

void UsbMonitor::stop()
{
    timer_->stop();
    LOG_INFO(kLogSource, QStringLiteral("U盘监控已停止"));
}

void UsbMonitor::setRpcLogPath(const QString &path)
{
    rpcLogPath_ = path;
}

void UsbMonitor::setQtLogPath(const QString &path)
{
    qtLogPath_ = path;
}

bool UsbMonitor::isUsbDevicePresent() const
{
    return QFile::exists(kUsbDevice);
}

QString UsbMonitor::generateTimestamp() const
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
}

bool UsbMonitor::mountUsb()
{
    // 确保挂载点目录存在
    QDir dir;
    if (!dir.exists(kMountPoint)) {
        if (!dir.mkpath(kMountPoint)) {
            LOG_ERROR(kLogSource, QStringLiteral("创建挂载点失败: %1").arg(kMountPoint));
            return false;
        }
    }

    // 执行挂载命令
    QProcess process;
    process.start(QStringLiteral("mount"),
                  QStringList() << QStringLiteral("-t") << QStringLiteral("vfat")
                                << kUsbDevice << kMountPoint);
    process.waitForFinished(10000);

    if (process.exitCode() != 0) {
        const QString errorMsg = QString::fromUtf8(process.readAllStandardError()).trimmed();
        // 如果已经挂载，视为成功
        if (errorMsg.contains(QStringLiteral("already mounted"))) {
            LOG_INFO(kLogSource, QStringLiteral("U盘已处于挂载状态"));
            return true;
        }
        LOG_ERROR(kLogSource, QStringLiteral("挂载U盘失败: %1").arg(errorMsg));
        return false;
    }

    LOG_INFO(kLogSource, QStringLiteral("U盘已挂载到 %1").arg(kMountPoint));
    return true;
}

bool UsbMonitor::unmountUsb()
{
    // 同步文件系统缓冲区
    QProcess::execute(QStringLiteral("sync"));

    QProcess process;
    process.start(QStringLiteral("umount"), QStringList() << kMountPoint);
    process.waitForFinished(10000);

    if (process.exitCode() != 0) {
        const QString errorMsg = QString::fromUtf8(process.readAllStandardError()).trimmed();
        LOG_ERROR(kLogSource, QStringLiteral("卸载U盘失败: %1").arg(errorMsg));
        return false;
    }

    LOG_INFO(kLogSource, QStringLiteral("U盘已卸载"));
    return true;
}

void UsbMonitor::backupLogs()
{
    const QString timestamp = generateTimestamp();
    const QString logDir = kMountPoint + QStringLiteral("/fanzhou_logs");

    // 创建日志备份目录
    QDir dir;
    if (!dir.exists(logDir)) {
        dir.mkpath(logDir);
    }

    // 备份RPC服务器日志
    if (QFile::exists(rpcLogPath_)) {
        const QString destPath = QStringLiteral("%1/core_%2.log").arg(logDir, timestamp);
        if (QFile::copy(rpcLogPath_, destPath)) {
            LOG_INFO(kLogSource, QStringLiteral("RPC服务器日志已备份: %1").arg(destPath));
        } else {
            LOG_ERROR(kLogSource, QStringLiteral("备份RPC服务器日志失败: %1 -> %2")
                          .arg(rpcLogPath_, destPath));
        }
    } else {
        LOG_WARNING(kLogSource, QStringLiteral("RPC服务器日志不存在: %1").arg(rpcLogPath_));
    }

    // 备份QT界面日志
    if (QFile::exists(qtLogPath_)) {
        const QString destPath = QStringLiteral("%1/qt_app_%2.log").arg(logDir, timestamp);
        if (QFile::copy(qtLogPath_, destPath)) {
            LOG_INFO(kLogSource, QStringLiteral("QT界面日志已备份: %1").arg(destPath));
        } else {
            LOG_ERROR(kLogSource, QStringLiteral("备份QT界面日志失败: %1 -> %2")
                          .arg(qtLogPath_, destPath));
        }
    } else {
        LOG_WARNING(kLogSource, QStringLiteral("QT界面日志不存在: %1").arg(qtLogPath_));
    }
}

bool UsbMonitor::deployUpdates()
{
    bool needRestart = false;

    // 检查并部署 fanzhou_rpc_server
    const QString serverSrc = kMountPoint + QStringLiteral("/fanzhou_rpc_server");
    if (QFile::exists(serverSrc)) {
        LOG_INFO(kLogSource, QStringLiteral("检测到 fanzhou_rpc_server 更新文件"));

        QDir dir;
        if (!dir.exists(kRpcServerBin)) {
            dir.mkpath(kRpcServerBin);
        }

        const QString serverDest = kRpcServerBin + QStringLiteral("/fanzhou_rpc_server");

        // 删除旧文件（如果存在）
        if (QFile::exists(serverDest)) {
            QFile::remove(serverDest);
        }

        if (QFile::copy(serverSrc, serverDest)) {
            // 设置执行权限 (owner+group: rwx, other: r-x)
            QFile destFile(serverDest);
            destFile.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                                    QFile::ReadGroup | QFile::ExeGroup);
            LOG_INFO(kLogSource, QStringLiteral("fanzhou_rpc_server 已部署到 %1").arg(serverDest));
            needRestart = true;
        } else {
            LOG_ERROR(kLogSource, QStringLiteral("部署 fanzhou_rpc_server 失败"));
        }
    }

    // 检查并部署 fanzhou_rpc_client
    const QString clientSrc = kMountPoint + QStringLiteral("/fanzhou_rpc_client");
    if (QFile::exists(clientSrc)) {
        LOG_INFO(kLogSource, QStringLiteral("检测到 fanzhou_rpc_client 更新文件"));

        QDir dir;
        if (!dir.exists(kRpcClientBin)) {
            dir.mkpath(kRpcClientBin);
        }

        const QString clientDest = kRpcClientBin + QStringLiteral("/fanzhou_rpc_client");

        // 删除旧文件（如果存在）
        if (QFile::exists(clientDest)) {
            QFile::remove(clientDest);
        }

        if (QFile::copy(clientSrc, clientDest)) {
            // 设置执行权限 (owner+group: rwx, other: r-x)
            QFile destFile(clientDest);
            destFile.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                                    QFile::ReadGroup | QFile::ExeGroup);
            LOG_INFO(kLogSource, QStringLiteral("fanzhou_rpc_client 已部署到 %1").arg(clientDest));
            needRestart = true;
        } else {
            LOG_ERROR(kLogSource, QStringLiteral("部署 fanzhou_rpc_client 失败"));
        }
    }

    return needRestart;
}

bool UsbMonitor::replaceConfig()
{
    const QString configSrc = kMountPoint + QStringLiteral("/core.json");
    if (!QFile::exists(configSrc)) {
        return false;
    }

    LOG_INFO(kLogSource, QStringLiteral("检测到 core.json 配置文件"));

    // 确保目标目录存在
    const QString configDir = QFileInfo(kConfigPath).absolutePath();
    QDir dir;
    if (!dir.exists(configDir)) {
        dir.mkpath(configDir);
    }

    // 备份旧配置
    if (QFile::exists(kConfigPath)) {
        const QString backupPath = kConfigPath + QStringLiteral(".bak.") + generateTimestamp();
        QFile::copy(kConfigPath, backupPath);
        QFile::remove(kConfigPath);
        LOG_INFO(kLogSource, QStringLiteral("旧配置已备份到: %1").arg(backupPath));
    }

    if (QFile::copy(configSrc, kConfigPath)) {
        LOG_INFO(kLogSource, QStringLiteral("core.json 已替换到 %1").arg(kConfigPath));
        return true;
    }

    LOG_ERROR(kLogSource, QStringLiteral("替换 core.json 失败"));
    return false;
}

void UsbMonitor::onCheckUsb()
{
    // 防止重入
    if (processing_) {
        return;
    }

    if (!isUsbDevicePresent()) {
        // U盘已拔出，重置处理标记
        if (usbHandled_) {
            usbHandled_ = false;
            LOG_INFO(kLogSource, QStringLiteral("U盘已拔出，监控已恢复"));
        }
        return;
    }

    // U盘已处理过，等待拔出后再处理
    if (usbHandled_) {
        return;
    }

    processing_ = true;
    LOG_INFO(kLogSource, QStringLiteral("检测到U盘设备: %1").arg(kUsbDevice));

    // 1. 挂载U盘
    if (!mountUsb()) {
        processing_ = false;
        return;
    }

    // 2. 备份日志到U盘
    backupLogs();

    // 3. 检测并部署程序更新
    bool needRestart = deployUpdates();

    // 4. 检测并替换配置文件
    replaceConfig();

    // 5. 同步并卸载U盘
    unmountUsb();

    QString message = QStringLiteral("U盘处理完成");
    if (needRestart) {
        message += QStringLiteral("，检测到程序更新，系统即将重启...");
    }

    LOG_INFO(kLogSource, message);
    emit usbProcessed(message);

    // 标记已处理，等待U盘拔出后重置
    usbHandled_ = true;
    processing_ = false;

    // 6. 如果有程序更新，重启系统
    if (needRestart) {
        LOG_INFO(kLogSource, QStringLiteral("正在重启系统..."));
        QProcess::execute(QStringLiteral("reboot"));
    }
}

}  // namespace fanzhou
