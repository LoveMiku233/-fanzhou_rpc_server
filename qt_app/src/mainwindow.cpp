/**
 * @file mainwindow.cpp
 * @brief 主窗口实现 - 左侧菜单栏 + 右侧内容区布局
 */

#include "mainwindow.h"
#include "rpc_client.h"
#include "connection_widget.h"
#include "device_widget.h"
#include "group_widget.h"
#include "relay_control_widget.h"

#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QScroller>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , sidebar_(nullptr)
    , sidebarLayout_(nullptr)
    , contentStack_(nullptr)
    , connectionStatusLabel_(nullptr)
    , connectionWidget_(nullptr)
    , deviceWidget_(nullptr)
    , groupWidget_(nullptr)
    , relayControlWidget_(nullptr)
    , rpcClient_(new RpcClient(this))
    , autoRefreshTimer_(new QTimer(this))
    , currentPageIndex_(0)
{
    setupUi();

    // 自动刷新定时器
    connect(autoRefreshTimer_, &QTimer::timeout, this, &MainWindow::onAutoRefreshTimeout);
}

MainWindow::~MainWindow()
{
    autoRefreshTimer_->stop();
}

void MainWindow::setupUi()
{
    setupStatusBar();
    setupCentralWidget();
}

void MainWindow::setupStatusBar()
{
    QStatusBar *statusBar = this->statusBar();

    connectionStatusLabel_ = new QLabel(QStringLiteral("未连接"));
    connectionStatusLabel_->setProperty("type", QStringLiteral("status-disconnected"));
    statusBar->addWidget(connectionStatusLabel_);
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
    sidebar_->setFixedWidth(140);

    sidebarLayout_ = new QVBoxLayout(sidebar_);
    sidebarLayout_->setContentsMargins(8, 16, 8, 16);
    sidebarLayout_->setSpacing(8);

    // Logo/标题
    QLabel *logoLabel = new QLabel(QStringLiteral("泛舟控制"), sidebar_);
    logoLabel->setObjectName(QStringLiteral("sidebarLogo"));
    logoLabel->setAlignment(Qt::AlignCenter);
    sidebarLayout_->addWidget(logoLabel);

    sidebarLayout_->addSpacing(16);

    // 菜单按钮
    struct MenuItem {
        QString text;
        QString icon;
    };
    
    QList<MenuItem> menuItems = {
        {QStringLiteral("连接设置"), QStringLiteral("🔌")},
        {QStringLiteral("设备管理"), QStringLiteral("📱")},
        {QStringLiteral("分组管理"), QStringLiteral("📂")},
        {QStringLiteral("继电器控制"), QStringLiteral("⚡")}
    };

    for (int i = 0; i < menuItems.size(); ++i) {
        QPushButton *btn = new QPushButton(menuItems[i].text, sidebar_);
        btn->setObjectName(QStringLiteral("menuButton"));
        btn->setProperty("menuIndex", i);
        btn->setCheckable(true);
        btn->setMinimumHeight(56);
        connect(btn, &QPushButton::clicked, this, &MainWindow::onMenuButtonClicked);
        sidebarLayout_->addWidget(btn);
        menuButtons_.append(btn);
    }

    sidebarLayout_->addStretch();

    // 版本信息
    QLabel *versionLabel = new QLabel(QStringLiteral("v1.0.0"), sidebar_);
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

    // 创建连接设置页面（带滚动）
    QScrollArea *connectionScrollArea = new QScrollArea(this);
    connectionScrollArea->setWidgetResizable(true);
    connectionScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connectionScrollArea->setFrameShape(QFrame::NoFrame);
    connectionWidget_ = new ConnectionWidget(rpcClient_, this);
    connectionScrollArea->setWidget(connectionWidget_);
    // 启用触控滑动
    QScroller::grabGesture(connectionScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    connect(connectionWidget_, &ConnectionWidget::connectionStatusChanged,
            this, &MainWindow::onConnectionStatusChanged);
    contentStack_->addWidget(connectionScrollArea);

    // 创建设备管理页面（带滚动）
    QScrollArea *deviceScrollArea = new QScrollArea(this);
    deviceScrollArea->setWidgetResizable(true);
    deviceScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    deviceScrollArea->setFrameShape(QFrame::NoFrame);
    deviceWidget_ = new DeviceWidget(rpcClient_, this);
    deviceScrollArea->setWidget(deviceWidget_);
    QScroller::grabGesture(deviceScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    contentStack_->addWidget(deviceScrollArea);

    // 创建分组管理页面（带滚动）
    QScrollArea *groupScrollArea = new QScrollArea(this);
    groupScrollArea->setWidgetResizable(true);
    groupScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    groupScrollArea->setFrameShape(QFrame::NoFrame);
    groupWidget_ = new GroupWidget(rpcClient_, this);
    groupScrollArea->setWidget(groupWidget_);
    QScroller::grabGesture(groupScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    contentStack_->addWidget(groupScrollArea);

    // 创建继电器控制页面（带滚动）
    QScrollArea *relayScrollArea = new QScrollArea(this);
    relayScrollArea->setWidgetResizable(true);
    relayScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    relayScrollArea->setFrameShape(QFrame::NoFrame);
    relayControlWidget_ = new RelayControlWidget(rpcClient_, this);
    relayScrollArea->setWidget(relayControlWidget_);
    QScroller::grabGesture(relayScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    contentStack_->addWidget(relayScrollArea);
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

    // 切换到设备管理页面时刷新设备列表
    if (index == 1 && deviceWidget_ && rpcClient_->isConnected()) {
        deviceWidget_->refreshDeviceList();
    }
    // 切换到分组管理页面时刷新分组列表
    if (index == 2 && groupWidget_ && rpcClient_->isConnected()) {
        groupWidget_->refreshGroupList();
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
    if (connected) {
        connectionStatusLabel_->setText(QStringLiteral("已连接到 %1:%2")
            .arg(rpcClient_->host()).arg(rpcClient_->port()));
        connectionStatusLabel_->setProperty("type", QStringLiteral("status-connected"));
        
        // 启动自动刷新
        autoRefreshTimer_->start(5000);
        
        // 刷新设备和分组列表
        if (deviceWidget_) {
            deviceWidget_->refreshDeviceList();
        }
        if (groupWidget_) {
            groupWidget_->refreshGroupList();
        }
    } else {
        connectionStatusLabel_->setText(QStringLiteral("未连接"));
        connectionStatusLabel_->setProperty("type", QStringLiteral("status-disconnected"));
        autoRefreshTimer_->stop();
    }
    connectionStatusLabel_->style()->unpolish(connectionStatusLabel_);
    connectionStatusLabel_->style()->polish(connectionStatusLabel_);
}

void MainWindow::onAutoRefreshTimeout()
{
    if (rpcClient_->isConnected()) {
        // 静默刷新设备状态（仅当设备页面可见时）
        if (currentPageIndex_ == 1 && deviceWidget_) {
            deviceWidget_->refreshDeviceStatus();
        }
    }
}
