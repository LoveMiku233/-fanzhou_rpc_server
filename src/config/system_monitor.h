/**
 * @file system_monitor.h
 * @brief 系统资源监控
 *
 * 提供CPU、内存、存储、网络等系统资源的实时监控。
 */

#ifndef FANZHOU_SYSTEM_MONITOR_H
#define FANZHOU_SYSTEM_MONITOR_H

#include <QObject>
#include <QJsonObject>
#include <QTimer>
#include <QList>
#include <QPair>

namespace fanzhou {
namespace config {

/**
 * @brief CPU使用率信息
 */
struct CpuUsage {
    double total = 0.0;      ///< 总CPU使用率（0-100）
    double user = 0.0;       ///< 用户态使用率
    double system = 0.0;     ///< 内核态使用率
    double idle = 0.0;       ///< 空闲率
    double iowait = 0.0;     ///< IO等待率
    int coreCount = 1;       ///< CPU核心数
};

/**
 * @brief 内存使用信息
 */
struct MemoryUsage {
    quint64 totalBytes = 0;     ///< 总内存（字节）
    quint64 usedBytes = 0;      ///< 已用内存（字节）
    quint64 freeBytes = 0;      ///< 空闲内存（字节）
    quint64 availableBytes = 0; ///< 可用内存（字节）
    quint64 buffersBytes = 0;   ///< 缓冲区（字节）
    quint64 cachedBytes = 0;    ///< 缓存（字节）
    double usagePercent = 0.0;  ///< 使用率（0-100）
};

/**
 * @brief 存储使用信息
 */
struct StorageUsage {
    QString mountPoint;         ///< 挂载点
    QString filesystem;         ///< 文件系统
    quint64 totalBytes = 0;     ///< 总空间（字节）
    quint64 usedBytes = 0;      ///< 已用空间（字节）
    quint64 freeBytes = 0;      ///< 可用空间（字节）
    double usagePercent = 0.0;  ///< 使用率（0-100）
};

/**
 * @brief 网络流量信息
 */
struct NetworkTraffic {
    QString interface;          ///< 网络接口名
    quint64 rxBytes = 0;        ///< 接收字节数
    quint64 txBytes = 0;        ///< 发送字节数
    quint64 rxPackets = 0;      ///< 接收包数
    quint64 txPackets = 0;      ///< 发送包数
    quint64 rxErrors = 0;       ///< 接收错误数
    quint64 txErrors = 0;       ///< 发送错误数
    double rxBytesPerSec = 0.0; ///< 接收速率（字节/秒）
    double txBytesPerSec = 0.0; ///< 发送速率（字节/秒）
};

/**
 * @brief 系统信息快照
 */
struct SystemSnapshot {
    qint64 timestampMs = 0;           ///< 时间戳（毫秒）
    qint64 uptimeSec = 0;             ///< 系统运行时间（秒）
    CpuUsage cpu;                      ///< CPU使用率
    MemoryUsage memory;                ///< 内存使用
    QList<StorageUsage> storages;      ///< 存储使用列表
    QList<NetworkTraffic> networks;    ///< 网络流量列表
    double loadAvg1 = 0.0;            ///< 1分钟负载
    double loadAvg5 = 0.0;            ///< 5分钟负载
    double loadAvg15 = 0.0;           ///< 15分钟负载
};

/**
 * @brief 系统资源监控器
 *
 * 定期采集系统资源使用情况，支持历史数据存储用于图表展示。
 */
class SystemMonitor : public QObject
{
    Q_OBJECT

public:
    explicit SystemMonitor(QObject *parent = nullptr);
    ~SystemMonitor() override;

    /**
     * @brief 启动监控
     * @param intervalMs 采样间隔（毫秒），默认1000ms
     */
    void start(int intervalMs = 1000);

    /**
     * @brief 停止监控
     */
    void stop();

    /**
     * @brief 获取当前系统快照
     */
    SystemSnapshot currentSnapshot() const;

    /**
     * @brief 获取历史快照（用于图表）
     * @param count 获取的条数，0表示全部
     */
    QList<SystemSnapshot> historySnapshots(int count = 0) const;

    /**
     * @brief 获取当前快照的JSON表示
     */
    QJsonObject currentSnapshotJson() const;

    /**
     * @brief 获取历史数据的JSON表示（用于图表）
     * @param count 获取的条数
     */
    QJsonObject historySnapshotsJson(int count = 60) const;

    /**
     * @brief 设置历史数据最大条数
     */
    void setMaxHistorySize(int size);

    /**
     * @brief 刷新一次数据（不需要启动定时器）
     */
    void refresh();

signals:
    /**
     * @brief 新快照可用
     */
    void snapshotReady(const SystemSnapshot &snapshot);

private slots:
    void onTimer();

private:
    CpuUsage readCpuUsage();
    MemoryUsage readMemoryUsage();
    QList<StorageUsage> readStorageUsage();
    QList<NetworkTraffic> readNetworkTraffic();
    void readLoadAverage(double &avg1, double &avg5, double &avg15);
    qint64 readUptime();

    QTimer *timer_ = nullptr;
    QList<SystemSnapshot> history_;
    int maxHistorySize_ = 300;  ///< 保留5分钟的数据（每秒1条）

    // 用于计算CPU使用率的上一次采样值
    quint64 prevCpuTotal_ = 0;
    quint64 prevCpuIdle_ = 0;

    // 用于计算网络速率的上一次采样值
    struct PrevNetworkData {
        quint64 rxBytes = 0;
        quint64 txBytes = 0;
        qint64 timestampMs = 0;
    };
    QHash<QString, PrevNetworkData> prevNetworkData_;
};

}  // namespace config
}  // namespace fanzhou

#endif  // FANZHOU_SYSTEM_MONITOR_H
