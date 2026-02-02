/**
 * @file mainwindow.cpp
 * @brief 主窗口实现 - 大棚控制系统
 */

#include "mainwindow.h"
#include "rpc_client.h"
#include "home_widget.h"
#include "device_widget.h"
#include "group_widget.h"
#include "strategy_widget.h"
#include "log_widget.h"
#include "settings_widget.h"

#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QScroller>
#include <QSettings>
#include <QFrame>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonDocument>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , sidebar_(nullptr)
    , sidebarLayout_(nullptr)
    , contentStack_(nullptr)
    , connectionStatusLabel_(nullptr)
    , timeLabel_(nullptr)
    , alertLabel_(nullptr)
    , homeWidget_(nullptr)
    , deviceWidget_(nullptr)
    , groupWidget_(nullptr)
    , strategyWidget_(nullptr)
    , logWidget_(nullptr)
    , settingsWidget_(nullptr)
    , rpcClient_(new RpcClient(this))
    , autoRefreshTimer_(new QTimer(this))
    , statusBarTimer_(new QTimer(this))
    , cloudStatusTimer_(new QTimer(this))
    , currentPageIndex_(0)
{
    setupUi();

    // 自动刷新定时器
    connect(autoRefreshTimer_, &QTimer::timeout, this, &MainWindow::onAutoRefreshTimeout);

    // 状态栏时间更新定时器
    connect(statusBarTimer_, &QTimer::timeout, this, &MainWindow::updateStatusBarTime);
    statusBarTimer_->start(1000);
    updateStatusBarTime();
    
    // 云状态检查定时器
    connect(cloudStatusTimer_, &QTimer::timeout, this, &MainWindow::updateCloudStatus);
    cloudStatusTimer_->start(5000);  // 每5秒检查一次云连接状态
    
    // 延迟执行自动连接（等待UI初始化完成）
    QTimer::singleShot(500, this, &MainWindow::attemptAutoConnect);
}

MainWindow::~MainWindow()
{
    autoRefreshTimer_->stop();
    statusBarTimer_->stop();
    cloudStatusTimer_->stop();
}

void MainWindow::setupUi()
{
    setupStatusBar();
    setupCentralWidget();
}

void MainWindow::setupStatusBar()
{
    QStatusBar *statusBar = this->statusBar();
    statusBar->setStyleSheet(QStringLiteral(
        "QStatusBar { "
        "  background-color: #1a252f; "
        "  color: white; "
        "  padding: 2px 6px; "
        "  font-size: 11px; "
        "}"));

    // 连接状态
    connectionStatusLabel_ = new QLabel(QStringLiteral("[X] 未连接"));
    connectionStatusLabel_->setStyleSheet(QStringLiteral(
        "color: #e74c3c; font-weight: bold; padding: 2px 8px;"));
    statusBar->addWidget(connectionStatusLabel_);

    // 分隔符
    QFrame *sep1 = new QFrame();
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet(QStringLiteral("color: #7f8c8d;"));
    statusBar->addWidget(sep1);
    
    // 云连接状态
    cloudStatusLabel_ = new QLabel(QStringLiteral("[云] 未连接"));
    cloudStatusLabel_->setStyleSheet(QStringLiteral(
        "color: #95a5a6; padding: 2px 8px;"));
    cloudStatusLabel_->setToolTip(QStringLiteral("云/MQTT连接状态"));
    statusBar->addWidget(cloudStatusLabel_);

    // 分隔符
    QFrame *sep1a = new QFrame();
    sep1a->setFrameShape(QFrame::VLine);
    sep1a->setStyleSheet(QStringLiteral("color: #7f8c8d;"));
    statusBar->addWidget(sep1a);

    // 时间
    timeLabel_ = new QLabel(QStringLiteral("--:--:--"));
    timeLabel_->setStyleSheet(QStringLiteral(
        "color: #ecf0f1; padding: 2px 8px;"));
    statusBar->addWidget(timeLabel_);

    // 分隔符
    QFrame *sep2 = new QFrame();
    sep2->setFrameShape(QFrame::VLine);
    sep2->setStyleSheet(QStringLiteral("color: #7f8c8d;"));
    statusBar->addWidget(sep2);

    // 报警/日志信息
    alertLabel_ = new QLabel(QStringLiteral("系统就绪"));
    alertLabel_->setStyleSheet(QStringLiteral(
        "color: #bdc3c7; padding: 2px 8px;"));
    statusBar->addWidget(alertLabel_, 1);
}

void MainWindow::setupCentralWidget()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 创建左侧菜单栏
    createSidebar();
    mainLayout->addWidget(sidebar_);

    // 创建右侧内容区
    createContentArea();
    mainLayout->addWidget(contentStack_, 1);
}

void MainWindow::createSidebar()
{
    sidebar_ = new QWidget(this);
    sidebar_->setObjectName(QStringLiteral("sidebar"));
    sidebar_->setFixedWidth(90);  // 稍微增宽侧边栏以适应更大字体

    sidebarLayout_ = new QVBoxLayout(sidebar_);
    sidebarLayout_->setContentsMargins(4, 8, 4, 8);
    sidebarLayout_->setSpacing(4);

    // Logo/标题 - 使用纯文本
    QLabel *logoLabel = new QLabel(QStringLiteral("大棚\n控制"), sidebar_);
    logoLabel->setObjectName(QStringLiteral("sidebarLogo"));
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setWordWrap(true);
    sidebarLayout_->addWidget(logoLabel);

    sidebarLayout_->addSpacing(8);

    // 菜单按钮 - 使用纯文本，无emoji，添加策略菜单
    struct MenuItem {
        QString text;
    };

    QList<MenuItem> menuItems = {
        {QStringLiteral("主页")},
        {QStringLiteral("设备")},
        {QStringLiteral("分组")},
        {QStringLiteral("策略")},
        {QStringLiteral("日志")},
        {QStringLiteral("设置")}
    };

    for (int i = 0; i < menuItems.size(); ++i) {
        QPushButton *btn = new QPushButton(menuItems[i].text, sidebar_);
        btn->setObjectName(QStringLiteral("menuButton"));
        btn->setProperty("menuIndex", i);
        btn->setCheckable(true);
        btn->setMinimumHeight(50);
        connect(btn, &QPushButton::clicked, this, &MainWindow::onMenuButtonClicked);
        sidebarLayout_->addWidget(btn);
        menuButtons_.append(btn);
    }

    sidebarLayout_->addStretch();

    // 版本信息
    QLabel *versionLabel = new QLabel(QStringLiteral("v1.0"), sidebar_);
    versionLabel->setObjectName(QStringLiteral("sidebarVersion"));
    versionLabel->setAlignment(Qt::AlignCenter);
    sidebarLayout_->addWidget(versionLabel);

    // 默认选中第一个菜单
    if (!menuButtons_.isEmpty()) {
        menuButtons_[0]->setChecked(true);
    }
}

void MainWindow::createContentArea()
{
    contentStack_ = new QStackedWidget(this);
    contentStack_->setObjectName(QStringLiteral("contentStack"));

    // 创建主页（带滚动）
    QScrollArea *homeScrollArea = new QScrollArea(this);
    homeScrollArea->setWidgetResizable(true);
    homeScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    homeScrollArea->setFrameShape(QFrame::NoFrame);
    homeWidget_ = new HomeWidget(rpcClient_, this);
    homeScrollArea->setWidget(homeWidget_);
    QScroller::grabGesture(homeScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    contentStack_->addWidget(homeScrollArea);

    // 创建设备管理页面（带滚动）
    QScrollArea *deviceScrollArea = new QScrollArea(this);
    deviceScrollArea->setWidgetResizable(true);
    deviceScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    deviceScrollArea->setFrameShape(QFrame::NoFrame);
    deviceWidget_ = new DeviceWidget(rpcClient_, this);
    deviceScrollArea->setWidget(deviceWidget_);
    QScroller::grabGesture(deviceScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    connect(deviceWidget_, &DeviceWidget::logMessage, this, &MainWindow::onLogMessage);
    contentStack_->addWidget(deviceScrollArea);

    // 创建分组管理页面（带滚动）
    QScrollArea *groupScrollArea = new QScrollArea(this);
    groupScrollArea->setWidgetResizable(true);
    groupScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    groupScrollArea->setFrameShape(QFrame::NoFrame);
    groupWidget_ = new GroupWidget(rpcClient_, this);
    groupScrollArea->setWidget(groupWidget_);
    QScroller::grabGesture(groupScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    connect(groupWidget_, &GroupWidget::logMessage, this, &MainWindow::onLogMessage);
    contentStack_->addWidget(groupScrollArea);

    // 创建策略管理页面（带滚动）
    QScrollArea *strategyScrollArea = new QScrollArea(this);
    strategyScrollArea->setWidgetResizable(true);
    strategyScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    strategyScrollArea->setFrameShape(QFrame::NoFrame);
    strategyWidget_ = new StrategyWidget(rpcClient_, this);
    strategyScrollArea->setWidget(strategyWidget_);
    QScroller::grabGesture(strategyScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    connect(strategyWidget_, &StrategyWidget::logMessage, this, &MainWindow::onLogMessage);
    contentStack_->addWidget(strategyScrollArea);

    // 创建日志页面（带滚动）
    QScrollArea *logScrollArea = new QScrollArea(this);
    logScrollArea->setWidgetResizable(true);
    logScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    logScrollArea->setFrameShape(QFrame::NoFrame);
    logWidget_ = new LogWidget(this);
    logScrollArea->setWidget(logWidget_);
    QScroller::grabGesture(logScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    connect(logWidget_, &LogWidget::newAlertMessage, this, [this](const QString &message) {
        lastAlertMessage_ = message;
        alertLabel_->setText(QStringLiteral("[!] %1").arg(message));
        alertLabel_->setStyleSheet(QStringLiteral(
            "color: #f39c12; padding: 2px 8px; font-weight: bold;"));
    });
    contentStack_->addWidget(logScrollArea);

    // 创建设置页面（带滚动）
    QScrollArea *settingsScrollArea = new QScrollArea(this);
    settingsScrollArea->setWidgetResizable(true);
    settingsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    settingsScrollArea->setFrameShape(QFrame::NoFrame);
    settingsWidget_ = new SettingsWidget(rpcClient_, this);
    settingsScrollArea->setWidget(settingsWidget_);
    QScroller::grabGesture(settingsScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    connect(settingsWidget_, &SettingsWidget::connectionStatusChanged,
            this, &MainWindow::onConnectionStatusChanged);
    connect(settingsWidget_, &SettingsWidget::logMessage, this, &MainWindow::onLogMessage);
    contentStack_->addWidget(settingsScrollArea);
}

void MainWindow::onMenuButtonClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    int index = btn->property("menuIndex").toInt();
    switchToPage(index);
}

void MainWindow::switchToPage(int index)
{
    if (index < 0 || index >= contentStack_->count()) {
        return;
    }

    currentPageIndex_ = index;
    contentStack_->setCurrentIndex(index);
    updateMenuButtonStyles(index);

    // 切换到主页时刷新数据
    if (index == 0 && homeWidget_ && rpcClient_->isConnected()) {
        homeWidget_->refreshData();
    }
    // 切换到设备管理页面时刷新设备列表
    if (index == 1 && deviceWidget_ && rpcClient_->isConnected()) {
        deviceWidget_->refreshDeviceList();
    }
    // 切换到分组管理页面时刷新分组列表
    if (index == 2 && groupWidget_ && rpcClient_->isConnected()) {
        groupWidget_->refreshGroupList();
    }
    // 切换到策略管理页面时刷新策略列表
    if (index == 3 && strategyWidget_ && rpcClient_->isConnected()) {
        strategyWidget_->refreshAllStrategies();
    }
}

void MainWindow::updateMenuButtonStyles(int activeIndex)
{
    for (int i = 0; i < menuButtons_.size(); ++i) {
        menuButtons_[i]->setChecked(i == activeIndex);
    }
}

void MainWindow::onConnectionStatusChanged(bool connected)
{
    updateStatusBarConnection(connected);

    if (connected) {
        // 启动自动刷新
        QSettings settings;
        int interval = settings.value(QStringLiteral("settings/refreshInterval"), 5).toInt();
        autoRefreshTimer_->start(interval * 1000);

        // 刷新各页面数据
        if (homeWidget_) {
            homeWidget_->refreshData();
        }
        if (deviceWidget_) {
            deviceWidget_->refreshDeviceList();
        }
        if (groupWidget_) {
            groupWidget_->refreshGroupList();
        }
        if (strategyWidget_) {
            strategyWidget_->refreshAllStrategies();
        }

        onLogMessage(QStringLiteral("已连接到服务器 %1:%2")
            .arg(rpcClient_->host()).arg(rpcClient_->port()));
    } else {
        autoRefreshTimer_->stop();
        onLogMessage(QStringLiteral("服务器连接已断开"), QStringLiteral("WARN"));
    }
}

void MainWindow::updateStatusBarConnection(bool connected)
{
    if (connected) {
        connectionStatusLabel_->setText(QStringLiteral("[OK] 已连接"));
        connectionStatusLabel_->setStyleSheet(QStringLiteral(
            "color: #27ae60; font-weight: bold; padding: 2px 8px;"));
        alertLabel_->setText(QStringLiteral("系统运行正常"));
        alertLabel_->setStyleSheet(QStringLiteral(
            "color: #bdc3c7; padding: 2px 8px;"));
    } else {
        connectionStatusLabel_->setText(QStringLiteral("[X] 未连接"));
        connectionStatusLabel_->setStyleSheet(QStringLiteral(
            "color: #e74c3c; font-weight: bold; padding: 2px 8px;"));
    }
}

void MainWindow::onAutoRefreshTimeout()
{
    if (rpcClient_->isConnected()) {
        // 静默刷新主页数据
        if (currentPageIndex_ == 0 && homeWidget_) {
            homeWidget_->refreshData();
        }
        // 注意：移除了设备状态的自动轮询
        // 原因：节点设备会主动通过CAN总线上报状态数据（CAN ID: 0x200+nodeId）
        // 如果QT同时通过RPC轮询查询状态，会导致CAN TX buffer拥塞
        // 用户可以通过手动点击"刷新设备"或"查询全部"按钮获取最新状态
    }
}

void MainWindow::onLogMessage(const QString &message, const QString &level)
{
    if (logWidget_) {
        logWidget_->appendLog(message, level);
    }

    // 更新状态栏报警信息
    if (level == QStringLiteral("ERROR")) {
        alertLabel_->setText(QStringLiteral("[ERR] %1").arg(message));
        alertLabel_->setStyleSheet(QStringLiteral(
            "color: #e74c3c; padding: 2px 8px; font-weight: bold;"));
    } else if (level == QStringLiteral("WARN")) {
        alertLabel_->setText(QStringLiteral("[!] %1").arg(message));
        alertLabel_->setStyleSheet(QStringLiteral(
            "color: #f39c12; padding: 2px 8px;"));
    }
}

void MainWindow::updateStatusBarTime()
{
    timeLabel_->setText(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")));
}

void MainWindow::attemptAutoConnect()
{
    QSettings settings;
    bool autoConnect = settings.value(QStringLiteral("settings/autoConnect"), false).toBool();
    
    if (!autoConnect) {
        onLogMessage(QStringLiteral("自动连接未启用"));
        return;
    }
    
    QString host = settings.value(QStringLiteral("connection/host"), QStringLiteral("127.0.0.1")).toString();
    quint16 port = static_cast<quint16>(settings.value(QStringLiteral("connection/port"), 12345).toInt());
    
    onLogMessage(QStringLiteral("正在自动连接到 %1:%2...").arg(host).arg(port));
    
    rpcClient_->setEndpoint(host, port);
    
    if (rpcClient_->connectToServer(3000)) {
        onLogMessage(QStringLiteral("自动连接成功"));
        
        // 尝试ping服务器验证连接
        QJsonValue result = rpcClient_->call(QStringLiteral("rpc.ping"), QJsonObject(), 1000);
        if (!result.isUndefined()) {
            onLogMessage(QStringLiteral("服务器响应正常"));
        }
        
        // 手动触发连接状态更新（因为waitForConnected同步调用不会触发信号）
        onConnectionStatusChanged(true);
    } else {
        onLogMessage(QStringLiteral("自动连接失败，请检查服务器是否运行"), QStringLiteral("WARN"));
    }
}

void MainWindow::updateCloudStatus()
{
    if (!rpcClient_->isConnected()) {
        cloudStatusLabel_->setText(QStringLiteral("[云] 未连接"));
        cloudStatusLabel_->setStyleSheet(QStringLiteral("color: #95a5a6; padding: 2px 8px;"));
        return;
    }
    
    // 调用RPC获取MQTT通道状态
    QJsonValue result = rpcClient_->call(QStringLiteral("mqtt.channels.list"), QJsonObject(), 1000);
    
    if (result.isObject()) {
        QJsonObject resultObj = result.toObject();
        
        // 检查是否成功
        if (!resultObj.value(QStringLiteral("ok")).toBool()) {
            cloudStatusLabel_->setText(QStringLiteral("[云] 未知"));
            cloudStatusLabel_->setStyleSheet(QStringLiteral("color: #95a5a6; padding: 2px 8px;"));
            return;
        }
        
        QJsonArray channels = resultObj.value(QStringLiteral("channels")).toArray();
        
        int totalChannels = channels.size();
        int connectedChannels = 0;
        
        for (const QJsonValue &channelVal : channels) {
            QJsonObject channel = channelVal.toObject();
            if (channel.value(QStringLiteral("connected")).toBool()) {
                connectedChannels++;
            }
        }
        
        if (totalChannels == 0) {
            cloudStatusLabel_->setText(QStringLiteral("[云] 未配置"));
            cloudStatusLabel_->setStyleSheet(QStringLiteral("color: #95a5a6; padding: 2px 8px;"));
        } else if (connectedChannels == 0) {
            cloudStatusLabel_->setText(QStringLiteral("[云] 断开 (0/%1)").arg(totalChannels));
            cloudStatusLabel_->setStyleSheet(QStringLiteral("color: #e67e22; padding: 2px 8px;"));
        } else if (connectedChannels == totalChannels) {
            cloudStatusLabel_->setText(QStringLiteral("[云] 已连接 (%1)").arg(totalChannels));
            cloudStatusLabel_->setStyleSheet(QStringLiteral("color: #27ae60; font-weight: bold; padding: 2px 8px;"));
        } else {
            cloudStatusLabel_->setText(QStringLiteral("[云] 部分连接 (%1/%2)").arg(connectedChannels).arg(totalChannels));
            cloudStatusLabel_->setStyleSheet(QStringLiteral("color: #f39c12; padding: 2px 8px;"));
        }
    } else {
        // RPC调用失败或方法不存在
        cloudStatusLabel_->setText(QStringLiteral("[云] 未知"));
        cloudStatusLabel_->setStyleSheet(QStringLiteral("color: #95a5a6; padding: 2px 8px;"));
    }
}
