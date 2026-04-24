/**
 * @file monitor_widget.cpp
 * @brief 系统监控界面实现 - 大棚控制柜
 *
 * 显示系统资源使用情况：CPU、内存、磁盘、网络等
 */

#include "monitor_widget.h"
#include "rpc_client.h"
#include "style_constants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QDateTime>
#include <QStorageInfo>
#include <QScrollArea>
#include <QScroller>

using namespace UIConstants;

MonitorWidget::MonitorWidget(RpcClient *rpcClient, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpcClient)
    , refreshTimer_(new QTimer(this))
    , localStatsTimer_(new QTimer(this))
    , lastCpuIdle_(0)
    , lastCpuTotal_(0)
    , lastRxBytes_(0)
    , lastTxBytes_(0)
{
    setupUi();

    // 本地状态更新定时器（每秒）
    connect(localStatsTimer_, &QTimer::timeout, this, &MonitorWidget::updateLocalStats);
    localStatsTimer_->start(1000);

    // RPC刷新定时器（5秒）
    connect(refreshTimer_, &QTimer::timeout, this, &MonitorWidget::refreshData);

    qDebug() << "[MONITOR_WIDGET] 监控页面初始化完成";
}

void MonitorWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 8, 10, 8);
    mainLayout->setSpacing(8);

    // 页面标题
    QLabel *titleLabel = new QLabel(QStringLiteral("系统监控"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: 20px; font-weight: 900; color: #17364a; padding: 0 2px;"));
    mainLayout->addWidget(titleLabel);

    // 工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(CARD_SPACING);

    refreshButton_ = new QPushButton(QStringLiteral("刷新"), this);
    refreshButton_->setMinimumHeight(BTN_HEIGHT);
    refreshButton_->setMinimumWidth(BTN_MIN_WIDTH);
    connect(refreshButton_, &QPushButton::clicked, this, &MonitorWidget::refreshData);
    toolbarLayout->addWidget(refreshButton_);

    autoRefreshButton_ = new QPushButton(QStringLiteral("自动刷新"), this);
    autoRefreshButton_->setMinimumHeight(BTN_HEIGHT);
    autoRefreshButton_->setMinimumWidth(BTN_MIN_WIDTH);
    autoRefreshButton_->setCheckable(true);
    autoRefreshButton_->setStyleSheet(QStringLiteral(
        "QPushButton:checked { background-color: #27ae60; }"));
    connect(autoRefreshButton_, &QPushButton::toggled, this, &MonitorWidget::onAutoRefreshToggled);
    toolbarLayout->addWidget(autoRefreshButton_);

    toolbarLayout->addStretch();
    mainLayout->addLayout(toolbarLayout);

    // 创建滚动区域
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget *contentWidget = new QWidget(this);
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setSpacing(PAGE_SPACING);

    // 系统资源组
    QGroupBox *resourceGroup = new QGroupBox(QStringLiteral("系统资源"), this);
    QGridLayout *resourceLayout = new QGridLayout(resourceGroup);
    resourceLayout->setSpacing(CARD_SPACING);
    resourceLayout->setContentsMargins(CARD_MARGIN, CARD_MARGIN, CARD_MARGIN, CARD_MARGIN);

    // CPU使用
    cpuLabel_ = new QLabel(QStringLiteral("CPU使用率"), this);
    cpuLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #2c3e50;"));
    resourceLayout->addWidget(cpuLabel_, 0, 0);

    cpuProgressBar_ = new QProgressBar(this);
    cpuProgressBar_->setRange(0, 100);
    cpuProgressBar_->setValue(0);
    cpuProgressBar_->setStyleSheet(QStringLiteral(
        "QProgressBar { border: 2px solid #e1e8ed; border-radius: 6px; text-align: center; height: 24px; }"
        "QProgressBar::chunk { background-color: #3498db; border-radius: 4px; }"));
    resourceLayout->addWidget(cpuProgressBar_, 0, 1);

    cpuValueLabel_ = new QLabel(QStringLiteral("0%"), this);
    cpuValueLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #3498db; min-width: 50px;"));
    resourceLayout->addWidget(cpuValueLabel_, 0, 2);

    // 内存使用
    memoryLabel_ = new QLabel(QStringLiteral("内存使用"), this);
    memoryLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #2c3e50;"));
    resourceLayout->addWidget(memoryLabel_, 1, 0);

    memoryProgressBar_ = new QProgressBar(this);
    memoryProgressBar_->setRange(0, 100);
    memoryProgressBar_->setValue(0);
    memoryProgressBar_->setStyleSheet(QStringLiteral(
        "QProgressBar { border: 2px solid #e1e8ed; border-radius: 6px; text-align: center; height: 24px; }"
        "QProgressBar::chunk { background-color: #27ae60; border-radius: 4px; }"));
    resourceLayout->addWidget(memoryProgressBar_, 1, 1);

    memoryValueLabel_ = new QLabel(QStringLiteral("0 MB / 0 MB"), this);
    memoryValueLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #27ae60; min-width: 120px;"));
    resourceLayout->addWidget(memoryValueLabel_, 1, 2);

    // 磁盘使用
    diskLabel_ = new QLabel(QStringLiteral("磁盘使用"), this);
    diskLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #2c3e50;"));
    resourceLayout->addWidget(diskLabel_, 2, 0);

    diskProgressBar_ = new QProgressBar(this);
    diskProgressBar_->setRange(0, 100);
    diskProgressBar_->setValue(0);
    diskProgressBar_->setStyleSheet(QStringLiteral(
        "QProgressBar { border: 2px solid #e1e8ed; border-radius: 6px; text-align: center; height: 24px; }"
        "QProgressBar::chunk { background-color: #e67e22; border-radius: 4px; }"));
    resourceLayout->addWidget(diskProgressBar_, 2, 1);

    diskValueLabel_ = new QLabel(QStringLiteral("0 GB / 0 GB"), this);
    diskValueLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #e67e22; min-width: 120px;"));
    resourceLayout->addWidget(diskValueLabel_, 2, 2);

    contentLayout->addWidget(resourceGroup);

    // 系统状态组
    QGroupBox *systemGroup = new QGroupBox(QStringLiteral("系统状态"), this);
    QGridLayout *systemLayout = new QGridLayout(systemGroup);
    systemLayout->setSpacing(CARD_SPACING);
    systemLayout->setContentsMargins(CARD_MARGIN, CARD_MARGIN, CARD_MARGIN, CARD_MARGIN);

    // 运行时间
    uptimeLabel_ = new QLabel(QStringLiteral("运行时间"), this);
    uptimeLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #2c3e50;"));
    systemLayout->addWidget(uptimeLabel_, 0, 0);

    uptimeValueLabel_ = new QLabel(QStringLiteral("--:--:--"), this);
    uptimeValueLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #34495e;"));
    systemLayout->addWidget(uptimeValueLabel_, 0, 1);

    // 进程数
    processLabel_ = new QLabel(QStringLiteral("进程数"), this);
    processLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #2c3e50;"));
    systemLayout->addWidget(processLabel_, 1, 0);

    processValueLabel_ = new QLabel(QStringLiteral("--"), this);
    processValueLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #34495e;"));
    systemLayout->addWidget(processValueLabel_, 1, 1);

    // 负载平均
    loadLabel_ = new QLabel(QStringLiteral("系统负载"), this);
    loadLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #2c3e50;"));
    systemLayout->addWidget(loadLabel_, 2, 0);

    QHBoxLayout *loadLayout = new QHBoxLayout();
    load1Label_ = new QLabel(QStringLiteral("1min: --"), this);
    load1Label_->setStyleSheet(QStringLiteral("color: #34495e;"));
    loadLayout->addWidget(load1Label_);

    load5Label_ = new QLabel(QStringLiteral("5min: --"), this);
    load5Label_->setStyleSheet(QStringLiteral("color: #34495e;"));
    loadLayout->addWidget(load5Label_);

    load15Label_ = new QLabel(QStringLiteral("15min: --"), this);
    load15Label_->setStyleSheet(QStringLiteral("color: #34495e;"));
    loadLayout->addWidget(load15Label_);

    systemLayout->addLayout(loadLayout, 2, 1);

    contentLayout->addWidget(systemGroup);

    // 网络状态组
    QGroupBox *networkGroup = new QGroupBox(QStringLiteral("网络状态"), this);
    QGridLayout *networkLayout = new QGridLayout(networkGroup);
    networkLayout->setSpacing(CARD_SPACING);
    networkLayout->setContentsMargins(CARD_MARGIN, CARD_MARGIN, CARD_MARGIN, CARD_MARGIN);

    networkLabel_ = new QLabel(QStringLiteral("流量统计"), this);
    networkLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #2c3e50;"));
    networkLayout->addWidget(networkLabel_, 0, 0, 1, 2);

    networkRxLabel_ = new QLabel(QStringLiteral("接收: 0 KB/s"), this);
    networkRxLabel_->setStyleSheet(QStringLiteral("color: #27ae60;"));
    networkLayout->addWidget(networkRxLabel_, 1, 0);

    networkTxLabel_ = new QLabel(QStringLiteral("发送: 0 KB/s"), this);
    networkTxLabel_->setStyleSheet(QStringLiteral("color: #3498db;"));
    networkLayout->addWidget(networkTxLabel_, 1, 1);

    contentLayout->addWidget(networkGroup);

    // 监控日志
    QGroupBox *logGroup = new QGroupBox(QStringLiteral("监控日志"), this);
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    logLayout->setContentsMargins(CARD_MARGIN, CARD_MARGIN, CARD_MARGIN, CARD_MARGIN);

    monitorLogEdit_ = new QTextEdit(this);
    monitorLogEdit_->setReadOnly(true);
    monitorLogEdit_->setMaximumHeight(120);
    monitorLogEdit_->setStyleSheet(QStringLiteral(
        "QTextEdit { background-color: #1e272e; color: #dfe6e9; "
        "font-family: 'Consolas', 'Monaco', monospace; font-size: 11px; "
        "border: 2px solid #2c3e50; border-radius: 8px; padding: 8px; }"));
    logLayout->addWidget(monitorLogEdit_);

    contentLayout->addWidget(logGroup);

    contentLayout->addStretch();
    scrollArea->setWidget(contentWidget);

    QScroller::grabGesture(scrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    mainLayout->addWidget(scrollArea, 1);

    // 初始更新本地状态
    updateLocalStats();
}

void MonitorWidget::onAutoRefreshToggled(bool checked)
{
    if (checked) {
        refreshTimer_->start(5000);
        qDebug() << "[MONITOR_WIDGET] 自动刷新已启用";
    } else {
        refreshTimer_->stop();
        qDebug() << "[MONITOR_WIDGET] 自动刷新已禁用";
    }
}

void MonitorWidget::onRefreshClicked()
{
    refreshData();
}

void MonitorWidget::refreshData()
{
    qDebug() << "[MONITOR_WIDGET] 刷新数据";

    if (rpcClient_ && rpcClient_->isConnected()) {
        // 调用RPC获取服务器状态
        rpcClient_->callAsync(QStringLiteral("sys.info"), QJsonObject(), this,
            [this](const QJsonValue &result, const QJsonObject &error) {
                Q_UNUSED(result);
                Q_UNUSED(error);
            }, 2000);
    }

    // 更新本地状态
    updateLocalStats();
}

void MonitorWidget::updateLocalStats()
{
    // 读取/proc/meminfo获取内存信息
    readProcMeminfo();

    // 读取/proc/stat获取CPU信息
    readProcStat();

    // 读取/proc/uptime
    readProcUptime();

    // 读取负载平均
    readLoadAvg();

    // 获取磁盘使用
    QStorageInfo storage = QStorageInfo::root();
    if (storage.isValid()) {
        qint64 totalBytes = storage.bytesTotal();
        qint64 usedBytes = totalBytes - storage.bytesAvailable();
        double usedGB = usedBytes / (1024.0 * 1024.0 * 1024.0);
        double totalGB = totalBytes / (1024.0 * 1024.0 * 1024.0);
        double percent = (static_cast<double>(usedBytes) / totalBytes) * 100.0;
        updateDiskUsage(usedGB, totalGB, percent);
    }

    // 添加日志
    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    monitorLogEdit_->append(QStringLiteral("[%1] 系统状态已更新").arg(timestamp));
}

void MonitorWidget::readProcMeminfo()
{
    QFile file(QStringLiteral("/proc/meminfo"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString line;
        qint64 memTotal = 0;
        qint64 memAvailable = 0;

        while (in.readLineInto(&line)) {
            if (line.startsWith(QStringLiteral("MemTotal:"))) {
                QStringList parts = line.split(QStringLiteral(" "), QString::SkipEmptyParts);
                if (parts.size() >= 2) {
                    memTotal = parts[1].toLongLong();
                }
            } else if (line.startsWith(QStringLiteral("MemAvailable:"))) {
                QStringList parts = line.split(QStringLiteral(" "), QString::SkipEmptyParts);
                if (parts.size() >= 2) {
                    memAvailable = parts[1].toLongLong();
                }
            }
        }

        if (memTotal > 0) {
            qint64 memUsed = memTotal - memAvailable;
            double usedMB = memUsed / 1024.0;
            double totalMB = memTotal / 1024.0;
            double percent = (static_cast<double>(memUsed) / memTotal) * 100.0;
            updateMemoryUsage(usedMB, totalMB, percent);
        }

        file.close();
    }
}

void MonitorWidget::readProcStat()
{
    QFile file(QStringLiteral("/proc/stat"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString line = in.readLine();

        if (line.startsWith(QStringLiteral("cpu "))) {
            QStringList parts = line.split(QStringLiteral(" "), QString::SkipEmptyParts);
            if (parts.size() >= 8) {
                double user = parts[1].toDouble();
                double nice = parts[2].toDouble();
                double system = parts[3].toDouble();
                double idle = parts[4].toDouble();
                double iowait = parts[5].toDouble();
                double irq = parts[6].toDouble();
                double softirq = parts[7].toDouble();

                double total = user + nice + system + idle + iowait + irq + softirq;
                double totalDiff = total - lastCpuTotal_;
                double idleDiff = idle - lastCpuIdle_;

                if (totalDiff > 0) {
                    double usage = 100.0 * (1.0 - idleDiff / totalDiff);
                    updateCpuUsage(usage);
                }

                lastCpuTotal_ = total;
                lastCpuIdle_ = idle;
            }
        }

        file.close();
    }
}

void MonitorWidget::readProcUptime()
{
    QFile file(QStringLiteral("/proc/uptime"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString line = in.readLine();
        QStringList parts = line.split(QStringLiteral(" "), QString::SkipEmptyParts);

        if (parts.size() >= 1) {
            double uptimeSeconds = parts[0].toDouble();
            int hours = static_cast<int>(uptimeSeconds / 3600);
            int minutes = static_cast<int>((uptimeSeconds - hours * 3600) / 60);
            int seconds = static_cast<int>(uptimeSeconds - hours * 3600 - minutes * 60);

            QString uptimeStr = QStringLiteral("%1:%2:%3")
                .arg(hours, 2, 10, QLatin1Char('0'))
                .arg(minutes, 2, 10, QLatin1Char('0'))
                .arg(seconds, 2, 10, QLatin1Char('0'));

            updateUptime(uptimeStr);
        }

        file.close();
    }
}

void MonitorWidget::readLoadAvg()
{
    QFile file(QStringLiteral("/proc/loadavg"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString line = in.readLine();
        QStringList parts = line.split(QStringLiteral(" "), QString::SkipEmptyParts);

        if (parts.size() >= 5) {
            load1Label_->setText(QStringLiteral("1min: %1").arg(parts[0]));
            load5Label_->setText(QStringLiteral("5min: %1").arg(parts[1]));
            load15Label_->setText(QStringLiteral("15min: %1").arg(parts[2]));

            // 进程数
            QString procParts = parts[3];
            QStringList procList = procParts.split(QStringLiteral("/"));
            if (procList.size() >= 2) {
                processValueLabel_->setText(procList[1]);
            }
        }

        file.close();
    }
}

void MonitorWidget::updateCpuUsage(double usage)
{
    int value = qBound(0, static_cast<int>(usage + 0.5), 100);
    cpuProgressBar_->setValue(value);
    cpuValueLabel_->setText(QStringLiteral("%1%").arg(value));

    // 根据使用率改变颜色
    if (value > 80) {
        cpuProgressBar_->setStyleSheet(QStringLiteral(
            "QProgressBar { border: 2px solid #e1e8ed; border-radius: 6px; text-align: center; height: 24px; }"
            "QProgressBar::chunk { background-color: #e74c3c; border-radius: 4px; }"));
        cpuValueLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #e74c3c; min-width: 50px;"));
    } else if (value > 60) {
        cpuProgressBar_->setStyleSheet(QStringLiteral(
            "QProgressBar { border: 2px solid #e1e8ed; border-radius: 6px; text-align: center; height: 24px; }"
            "QProgressBar::chunk { background-color: #f39c12; border-radius: 4px; }"));
        cpuValueLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #f39c12; min-width: 50px;"));
    } else {
        cpuProgressBar_->setStyleSheet(QStringLiteral(
            "QProgressBar { border: 2px solid #e1e8ed; border-radius: 6px; text-align: center; height: 24px; }"
            "QProgressBar::chunk { background-color: #3498db; border-radius: 4px; }"));
        cpuValueLabel_->setStyleSheet(QStringLiteral("font-weight: bold; color: #3498db; min-width: 50px;"));
    }
}

void MonitorWidget::updateMemoryUsage(double usedMB, double totalMB, double percent)
{
    int value = qBound(0, static_cast<int>(percent + 0.5), 100);
    memoryProgressBar_->setValue(value);
    memoryValueLabel_->setText(QStringLiteral("%1 MB / %2 MB")
        .arg(static_cast<int>(usedMB))
        .arg(static_cast<int>(totalMB)));
}

void MonitorWidget::updateDiskUsage(double usedGB, double totalGB, double percent)
{
    int value = qBound(0, static_cast<int>(percent + 0.5), 100);
    diskProgressBar_->setValue(value);
    diskValueLabel_->setText(QStringLiteral("%1 GB / %2 GB")
        .arg(usedGB, 0, 'f', 1)
        .arg(totalGB, 0, 'f', 1));
}

void MonitorWidget::updateNetworkStats(qint64 rxBytes, qint64 txBytes)
{
    Q_UNUSED(rxBytes);
    Q_UNUSED(txBytes);
    // 可以从/proc/net/dev读取网络统计
}

void MonitorWidget::updateUptime(const QString &uptime)
{
    uptimeValueLabel_->setText(uptime);
}

void MonitorWidget::updateProcessCount(int count)
{
    processValueLabel_->setText(QString::number(count));
}

void MonitorWidget::updateRpcStats(int pendingRequests, qint64 txCount, qint64 rxCount)
{
    Q_UNUSED(pendingRequests);
    Q_UNUSED(txCount);
    Q_UNUSED(rxCount);
}
