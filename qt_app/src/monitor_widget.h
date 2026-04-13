/**
 * @file monitor_widget.h
 * @brief 系统监控界面头文件 - 大棚控制柜
 *
 * 显示系统资源使用情况：CPU、内存、磁盘、网络等
 */

#ifndef MONITOR_WIDGET_H
#define MONITOR_WIDGET_H

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTextEdit>

class RpcClient;

/**
 * @brief 系统监控界面
 */
class MonitorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorWidget(RpcClient *rpcClient, QWidget *parent = nullptr);

public slots:
    void refreshData();

private slots:
    void onRefreshClicked();
    void onAutoRefreshToggled(bool checked);
    void updateLocalStats();

private:
    void setupUi();
    void updateCpuUsage(double usage);
    void updateMemoryUsage(double usedMB, double totalMB, double percent);
    void updateDiskUsage(double usedGB, double totalGB, double percent);
    void updateNetworkStats(qint64 rxBytes, qint64 txBytes);
    void updateUptime(const QString &uptime);
    void updateProcessCount(int count);
    void updateRpcStats(int pendingRequests, qint64 txCount, qint64 rxCount);
    void readProcMeminfo();
    void readProcStat();
    void readProcUptime();
    void readLoadAvg();

    RpcClient *rpcClient_;
    QTimer *refreshTimer_;
    QTimer *localStatsTimer_;

    // CPU状态
    QLabel *cpuLabel_;
    QProgressBar *cpuProgressBar_;
    QLabel *cpuValueLabel_;

    // 内存状态
    QLabel *memoryLabel_;
    QProgressBar *memoryProgressBar_;
    QLabel *memoryValueLabel_;

    // 磁盘状态
    QLabel *diskLabel_;
    QProgressBar *diskProgressBar_;
    QLabel *diskValueLabel_;

    // 网络状态
    QLabel *networkLabel_;
    QLabel *networkRxLabel_;
    QLabel *networkTxLabel_;

    // 系统运行时间
    QLabel *uptimeLabel_;
    QLabel *uptimeValueLabel_;

    // 进程数
    QLabel *processLabel_;
    QLabel *processValueLabel_;

    // 负载平均值
    QLabel *loadLabel_;
    QLabel *load1Label_;
    QLabel *load5Label_;
    QLabel *load15Label_;

    // RPC状态
    QLabel *rpcPendingLabel_;
    QLabel *rpcTxLabel_;
    QLabel *rpcRxLabel_;

    // 操作按钮
    QPushButton *refreshButton_;
    QPushButton *autoRefreshButton_;

    // 监控日志
    QTextEdit *monitorLogEdit_;

    // 统计数据
    double lastCpuIdle_;
    double lastCpuTotal_;
    qint64 lastRxBytes_;
    qint64 lastTxBytes_;
};

#endif // MONITOR_WIDGET_H
