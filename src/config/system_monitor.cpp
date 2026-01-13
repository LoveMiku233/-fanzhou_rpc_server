/**
 * @file system_monitor.cpp
 * @brief 系统资源监控实现
 */

#include "system_monitor.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QStorageInfo>
#include <QNetworkInterface>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>

namespace fanzhou {
namespace config {

namespace {

/**
 * @brief 解析 /proc/stat 获取CPU时间
 */
bool parseProcStat(quint64 &totalTime, quint64 &idleTime,
                   quint64 &userTime, quint64 &systemTime, quint64 &iowaitTime)
{
    QFile file(QStringLiteral("/proc/stat"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    const QString line = file.readLine();
    file.close();

    // cpu  user nice system idle iowait irq softirq steal guest guest_nice
    static const QRegularExpression re(QStringLiteral(
        R"(^cpu\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+))"));

    const auto match = re.match(line);
    if (!match.hasMatch()) {
        return false;
    }

    const quint64 user = match.captured(1).toULongLong();
    const quint64 nice = match.captured(2).toULongLong();
    const quint64 system = match.captured(3).toULongLong();
    const quint64 idle = match.captured(4).toULongLong();
    const quint64 iowait = match.captured(5).toULongLong();
    const quint64 irq = match.captured(6).toULongLong();
    const quint64 softirq = match.captured(7).toULongLong();
    const quint64 steal = match.captured(8).toULongLong();

    totalTime = user + nice + system + idle + iowait + irq + softirq + steal;
    idleTime = idle + iowait;
    userTime = user + nice;
    systemTime = system + irq + softirq;
    iowaitTime = iowait;

    return true;
}

/**
 * @brief 获取CPU核心数
 */
int getCpuCoreCount()
{
    QFile file(QStringLiteral("/proc/cpuinfo"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 1;
    }

    int count = 0;
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.startsWith(QStringLiteral("processor"))) {
            count++;
        }
    }
    return count > 0 ? count : 1;
}

/**
 * @brief 解析 /proc/meminfo
 */
bool parseProcMeminfo(MemoryUsage &mem)
{
    QFile file(QStringLiteral("/proc/meminfo"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")));
        if (parts.size() < 2) continue;

        const QString key = parts[0].chopped(1);  // 去掉冒号
        const quint64 value = parts[1].toULongLong() * 1024;  // kB转字节

        if (key == QStringLiteral("MemTotal")) {
            mem.totalBytes = value;
        } else if (key == QStringLiteral("MemFree")) {
            mem.freeBytes = value;
        } else if (key == QStringLiteral("MemAvailable")) {
            mem.availableBytes = value;
        } else if (key == QStringLiteral("Buffers")) {
            mem.buffersBytes = value;
        } else if (key == QStringLiteral("Cached")) {
            mem.cachedBytes = value;
        }
    }

    mem.usedBytes = mem.totalBytes - mem.freeBytes - mem.buffersBytes - mem.cachedBytes;
    if (mem.totalBytes > 0) {
        mem.usagePercent = static_cast<double>(mem.totalBytes - mem.availableBytes) * 100.0
                           / static_cast<double>(mem.totalBytes);
    }

    return true;
}

/**
 * @brief 解析 /proc/net/dev 获取网络流量
 */
QHash<QString, QPair<quint64, quint64>> parseNetDev()
{
    QHash<QString, QPair<quint64, quint64>> result;

    QFile file(QStringLiteral("/proc/net/dev"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return result;
    }

    QTextStream in(&file);
    // 跳过前两行标题
    in.readLine();
    in.readLine();

    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        // 格式: interface: rx_bytes rx_packets ... tx_bytes tx_packets ...
        const int colonPos = line.indexOf(':');
        if (colonPos < 0) continue;

        const QString iface = line.left(colonPos).trimmed();
        const QString data = line.mid(colonPos + 1).trimmed();
        const QStringList parts = data.split(QRegularExpression(QStringLiteral("\\s+")));

        if (parts.size() >= 9) {
            const quint64 rxBytes = parts[0].toULongLong();
            const quint64 txBytes = parts[8].toULongLong();
            result.insert(iface, qMakePair(rxBytes, txBytes));
        }
    }

    return result;
}

}  // namespace

SystemMonitor::SystemMonitor(QObject *parent)
    : QObject(parent)
    , timer_(new QTimer(this))
{
    connect(timer_, &QTimer::timeout, this, &SystemMonitor::onTimer);
}

SystemMonitor::~SystemMonitor()
{
    stop();
}

void SystemMonitor::start(int intervalMs)
{
    // 先采样一次初始化基准值
    refresh();
    timer_->start(intervalMs);
}

void SystemMonitor::stop()
{
    timer_->stop();
}

void SystemMonitor::refresh()
{
    onTimer();
}

SystemSnapshot SystemMonitor::currentSnapshot() const
{
    if (history_.isEmpty()) {
        return SystemSnapshot();
    }
    return history_.last();
}

QList<SystemSnapshot> SystemMonitor::historySnapshots(int count) const
{
    if (count <= 0 || count >= history_.size()) {
        return history_;
    }
    return history_.mid(history_.size() - count);
}

void SystemMonitor::setMaxHistorySize(int size)
{
    maxHistorySize_ = size;
    while (history_.size() > maxHistorySize_) {
        history_.removeFirst();
    }
}

void SystemMonitor::onTimer()
{
    SystemSnapshot snapshot;
    snapshot.timestampMs = QDateTime::currentMSecsSinceEpoch();
    snapshot.uptimeSec = readUptime();
    snapshot.cpu = readCpuUsage();
    snapshot.memory = readMemoryUsage();
    snapshot.storages = readStorageUsage();
    snapshot.networks = readNetworkTraffic();
    readLoadAverage(snapshot.loadAvg1, snapshot.loadAvg5, snapshot.loadAvg15);

    history_.append(snapshot);
    while (history_.size() > maxHistorySize_) {
        history_.removeFirst();
    }

    emit snapshotReady(snapshot);
}

CpuUsage SystemMonitor::readCpuUsage()
{
    CpuUsage usage;
    usage.coreCount = getCpuCoreCount();

    quint64 total = 0, idle = 0, user = 0, system = 0, iowait = 0;
    if (!parseProcStat(total, idle, user, system, iowait)) {
        return usage;
    }

    // 计算差值
    if (prevCpuTotal_ > 0 && total > prevCpuTotal_) {
        const quint64 totalDiff = total - prevCpuTotal_;
        const quint64 idleDiff = idle - prevCpuIdle_;

        usage.total = static_cast<double>(totalDiff - idleDiff) * 100.0
                      / static_cast<double>(totalDiff);
        usage.idle = static_cast<double>(idleDiff) * 100.0 / static_cast<double>(totalDiff);
    }

    prevCpuTotal_ = total;
    prevCpuIdle_ = idle;

    return usage;
}

MemoryUsage SystemMonitor::readMemoryUsage()
{
    MemoryUsage mem;
    parseProcMeminfo(mem);
    return mem;
}

QList<StorageUsage> SystemMonitor::readStorageUsage()
{
    QList<StorageUsage> list;

    const auto volumes = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &storage : volumes) {
        // 跳过虚拟文件系统
        if (!storage.isValid() || !storage.isReady()) {
            continue;
        }
        const QString fs = storage.fileSystemType();
        if (fs == QStringLiteral("tmpfs") || fs == QStringLiteral("devtmpfs") ||
            fs == QStringLiteral("overlay") || fs.startsWith(QStringLiteral("fuse"))) {
            continue;
        }

        StorageUsage su;
        su.mountPoint = storage.rootPath();
        su.filesystem = fs;
        su.totalBytes = static_cast<quint64>(storage.bytesTotal());
        su.freeBytes = static_cast<quint64>(storage.bytesFree());
        su.usedBytes = su.totalBytes - su.freeBytes;
        if (su.totalBytes > 0) {
            su.usagePercent = static_cast<double>(su.usedBytes) * 100.0
                              / static_cast<double>(su.totalBytes);
        }

        list.append(su);
    }

    return list;
}

QList<NetworkTraffic> SystemMonitor::readNetworkTraffic()
{
    QList<NetworkTraffic> list;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    const auto netData = parseNetDev();
    for (auto it = netData.constBegin(); it != netData.constEnd(); ++it) {
        const QString &iface = it.key();

        // 跳过回环接口
        if (iface == QStringLiteral("lo")) {
            continue;
        }

        NetworkTraffic nt;
        nt.interface = iface;
        nt.rxBytes = it.value().first;
        nt.txBytes = it.value().second;

        // 计算速率
        if (prevNetworkData_.contains(iface)) {
            const auto &prev = prevNetworkData_[iface];
            const qint64 timeDiff = now - prev.timestampMs;
            if (timeDiff > 0) {
                nt.rxBytesPerSec = static_cast<double>(nt.rxBytes - prev.rxBytes) * 1000.0
                                   / static_cast<double>(timeDiff);
                nt.txBytesPerSec = static_cast<double>(nt.txBytes - prev.txBytes) * 1000.0
                                   / static_cast<double>(timeDiff);
            }
        }

        // 更新上一次数据
        PrevNetworkData prevData;
        prevData.rxBytes = nt.rxBytes;
        prevData.txBytes = nt.txBytes;
        prevData.timestampMs = now;
        prevNetworkData_[iface] = prevData;

        list.append(nt);
    }

    return list;
}

void SystemMonitor::readLoadAverage(double &avg1, double &avg5, double &avg15)
{
    QFile file(QStringLiteral("/proc/loadavg"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    const QString line = file.readLine();
    const QStringList parts = line.split(' ');
    if (parts.size() >= 3) {
        avg1 = parts[0].toDouble();
        avg5 = parts[1].toDouble();
        avg15 = parts[2].toDouble();
    }
}

qint64 SystemMonitor::readUptime()
{
    QFile file(QStringLiteral("/proc/uptime"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }

    const QString line = file.readLine();
    const QStringList parts = line.split(' ');
    if (!parts.isEmpty()) {
        return static_cast<qint64>(parts[0].toDouble());
    }
    return 0;
}

QJsonObject SystemMonitor::currentSnapshotJson() const
{
    const auto snapshot = currentSnapshot();

    QJsonObject result;
    result[QStringLiteral("ok")] = true;
    result[QStringLiteral("timestamp")] = static_cast<double>(snapshot.timestampMs);
    result[QStringLiteral("uptimeSec")] = static_cast<double>(snapshot.uptimeSec);

    // CPU
    QJsonObject cpuObj;
    cpuObj[QStringLiteral("total")] = snapshot.cpu.total;
    cpuObj[QStringLiteral("user")] = snapshot.cpu.user;
    cpuObj[QStringLiteral("system")] = snapshot.cpu.system;
    cpuObj[QStringLiteral("idle")] = snapshot.cpu.idle;
    cpuObj[QStringLiteral("iowait")] = snapshot.cpu.iowait;
    cpuObj[QStringLiteral("coreCount")] = snapshot.cpu.coreCount;
    result[QStringLiteral("cpu")] = cpuObj;

    // 内存
    QJsonObject memObj;
    memObj[QStringLiteral("totalMB")] = static_cast<double>(snapshot.memory.totalBytes) / 1048576.0;
    memObj[QStringLiteral("usedMB")] = static_cast<double>(snapshot.memory.usedBytes) / 1048576.0;
    memObj[QStringLiteral("freeMB")] = static_cast<double>(snapshot.memory.freeBytes) / 1048576.0;
    memObj[QStringLiteral("availableMB")] = static_cast<double>(snapshot.memory.availableBytes) / 1048576.0;
    memObj[QStringLiteral("usagePercent")] = snapshot.memory.usagePercent;
    result[QStringLiteral("memory")] = memObj;

    // 负载
    QJsonObject loadObj;
    loadObj[QStringLiteral("avg1")] = snapshot.loadAvg1;
    loadObj[QStringLiteral("avg5")] = snapshot.loadAvg5;
    loadObj[QStringLiteral("avg15")] = snapshot.loadAvg15;
    result[QStringLiteral("load")] = loadObj;

    // 存储
    QJsonArray storageArr;
    for (const auto &st : snapshot.storages) {
        QJsonObject stObj;
        stObj[QStringLiteral("mount")] = st.mountPoint;
        stObj[QStringLiteral("fs")] = st.filesystem;
        stObj[QStringLiteral("totalGB")] = static_cast<double>(st.totalBytes) / 1073741824.0;
        stObj[QStringLiteral("usedGB")] = static_cast<double>(st.usedBytes) / 1073741824.0;
        stObj[QStringLiteral("freeGB")] = static_cast<double>(st.freeBytes) / 1073741824.0;
        stObj[QStringLiteral("usagePercent")] = st.usagePercent;
        storageArr.append(stObj);
    }
    result[QStringLiteral("storages")] = storageArr;

    // 网络
    QJsonArray netArr;
    for (const auto &nt : snapshot.networks) {
        QJsonObject ntObj;
        ntObj[QStringLiteral("interface")] = nt.interface;
        ntObj[QStringLiteral("rxMB")] = static_cast<double>(nt.rxBytes) / 1048576.0;
        ntObj[QStringLiteral("txMB")] = static_cast<double>(nt.txBytes) / 1048576.0;
        ntObj[QStringLiteral("rxKBps")] = nt.rxBytesPerSec / 1024.0;
        ntObj[QStringLiteral("txKBps")] = nt.txBytesPerSec / 1024.0;
        netArr.append(ntObj);
    }
    result[QStringLiteral("networks")] = netArr;

    return result;
}

QJsonObject SystemMonitor::historySnapshotsJson(int count) const
{
    const auto snapshots = historySnapshots(count);

    QJsonObject result;
    result[QStringLiteral("ok")] = true;
    result[QStringLiteral("count")] = snapshots.size();

    QJsonArray timestamps;
    QJsonArray cpuUsage;
    QJsonArray memUsage;
    QJsonArray loadAvg1;

    for (const auto &snapshot : snapshots) {
        timestamps.append(static_cast<double>(snapshot.timestampMs));
        cpuUsage.append(snapshot.cpu.total);
        memUsage.append(snapshot.memory.usagePercent);
        loadAvg1.append(snapshot.loadAvg1);
    }

    result[QStringLiteral("timestamps")] = timestamps;
    result[QStringLiteral("cpuUsage")] = cpuUsage;
    result[QStringLiteral("memUsage")] = memUsage;
    result[QStringLiteral("loadAvg1")] = loadAvg1;

    // 网络流量历史（只取第一个非回环接口）
    if (!snapshots.isEmpty() && !snapshots.first().networks.isEmpty()) {
        const QString iface = snapshots.first().networks.first().interface;
        QJsonArray rxKBps, txKBps;
        for (const auto &snapshot : snapshots) {
            for (const auto &nt : snapshot.networks) {
                if (nt.interface == iface) {
                    rxKBps.append(nt.rxBytesPerSec / 1024.0);
                    txKBps.append(nt.txBytesPerSec / 1024.0);
                    break;
                }
            }
        }
        result[QStringLiteral("networkInterface")] = iface;
        result[QStringLiteral("rxKBps")] = rxKBps;
        result[QStringLiteral("txKBps")] = txKBps;
    }

    return result;
}

}  // namespace config
}  // namespace fanzhou
