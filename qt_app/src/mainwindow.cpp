/**
 * @file mainwindow.cpp
 * @brief 主窗口实现
 */

#include "mainwindow.h"
#include "style_constants.h"
#include "rpc_client.h"
#include "screen_manager.h"

#include "views/dashboard_page.h"
#include "views/device_control_page.h"
#include "views/scene_page.h"
#include "views/alarm_page.h"
#include "views/sensor_page.h"
#include "views/settings_page.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QDateTime>
#include <QStyle>

namespace {
const int kRefreshIntervalMs = 5000;
const int kReconnectIntervalMs = 3000;
}

// ── Construction / Destruction ───────────────────────────

MainWindow::MainWindow(RpcClient *rpc, ScreenManager *screen, QWidget *parent)
    : QMainWindow(parent)
    , rpcClient_(rpc)
    , screenManager_(screen)
    , sidebar_(nullptr)
    , logoLabel_(nullptr)
    , alarmBadge_(nullptr)
    , headerBar_(nullptr)
    , headerTitle_(nullptr)
    , headerStatus_(nullptr)
    , connectionIndicator_(nullptr)
    , headerTime_(nullptr)
    , contentStack_(nullptr)
    , dashboardPage_(nullptr)
    , deviceControlPage_(nullptr)
    , scenePage_(nullptr)
    , alarmPage_(nullptr)
    , sensorPage_(nullptr)
    , settingsPage_(nullptr)
    , clockTimer_(nullptr)
    , refreshTimer_(nullptr)
    , reconnectTimer_(nullptr)
    , currentPage_(Style::PageDashboard)
    , wasConnected_(false)
{
    for (int i = 0; i < 6; ++i)
        navButtons_[i] = nullptr;

    setupUi();
    setupConnections();

    // Start clock
    clockTimer_ = new QTimer(this);
    clockTimer_->setInterval(1000);
    connect(clockTimer_, &QTimer::timeout, this, &MainWindow::updateClock);
    clockTimer_->start();
    updateClock();

    // Start periodic data refresh (every 5 seconds)
    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(kRefreshIntervalMs);
    connect(refreshTimer_, &QTimer::timeout, this, &MainWindow::refreshCurrentPage);
    refreshTimer_->start();

    // Setup reconnect timer
    reconnectTimer_ = new QTimer(this);
    reconnectTimer_->setInterval(kReconnectIntervalMs);
    connect(reconnectTimer_, &QTimer::timeout, this, &MainWindow::tryConnect);

    // Auto-connect to RPC server on startup
    startAutoConnect();
}

MainWindow::~MainWindow() = default;

// ── UI Setup ─────────────────────────────────────────────

void MainWindow::setupUi()
{
    setFixedSize(Style::kScreenWidth, Style::kScreenHeight);
    setWindowTitle(QStringLiteral("泛舟智能科技控制柜系统"));

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout *rootLayout = new QHBoxLayout(centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ── Left sidebar ─────────────────────────────────────
    sidebar_ = new QWidget(centralWidget);
    sidebar_->setObjectName(QStringLiteral("sidebar"));
    sidebar_->setFixedWidth(Style::kSidebarWidth);

    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebar_);
    sidebarLayout->setContentsMargins(0, 8, 0, 8);
    sidebarLayout->setSpacing(4);

    // Logo
    logoLabel_ = new QLabel(QStringLiteral("⚡"), sidebar_);
    logoLabel_->setObjectName(QStringLiteral("sidebarLogo"));
    logoLabel_->setAlignment(Qt::AlignCenter);
    logoLabel_->setFixedSize(48, 48);
    logoLabel_->setStyleSheet(
        QStringLiteral("QLabel#sidebarLogo {"
                       "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
                       "      stop:0 %1, stop:1 %2);"
                       "  border-radius: 12px;"
                       "  color: white;"
                       "  font-size: 24px;"
                       "}")
            .arg(Style::kColorGradientStart, Style::kColorGradientEnd));

    sidebarLayout->addWidget(logoLabel_, 0, Qt::AlignHCenter);
    sidebarLayout->addSpacing(12);

    // Navigation icons / labels per page
    struct NavDef {
        QString icon;
        QString label;
    };
    const NavDef navDefs[6] = {
        { QStringLiteral("⊞"), QStringLiteral("驾驶舱")   },
        { QStringLiteral("⚙"), QStringLiteral("设备控制") },
        { QStringLiteral("📋"), QStringLiteral("场景管理") },
        { QStringLiteral("⚠"), QStringLiteral("报警看板") },
        { QStringLiteral("📊"), QStringLiteral("传感器")   },
        { QStringLiteral("⚙"), QStringLiteral("系统设置") },
    };

    for (int i = 0; i < 6; ++i) {
        navButtons_[i] = createNavButton(navDefs[i].icon, navDefs[i].label, i);
        sidebarLayout->addWidget(navButtons_[i], 0, Qt::AlignHCenter);
    }

    sidebarLayout->addStretch(1);

    // Alarm badge on the alarm button (index 3)
    alarmBadge_ = new QLabel(navButtons_[Style::PageAlarms]);
    alarmBadge_->setObjectName(QStringLiteral("alarmBadge"));
    alarmBadge_->setAlignment(Qt::AlignCenter);
    alarmBadge_->setFixedSize(16, 16);
    alarmBadge_->move(navButtons_[Style::PageAlarms]->width() - 18, 2);
    alarmBadge_->setStyleSheet(
        QStringLiteral("QLabel#alarmBadge {"
                       "  background: %1;"
                       "  color: white;"
                       "  border-radius: 8px;"
                       "  font-size: 8px;"
                       "  font-weight: bold;"
                       "}")
            .arg(Style::kColorDanger));
    alarmBadge_->setText(QStringLiteral("0"));
    alarmBadge_->hide();

    rootLayout->addWidget(sidebar_);

    // ── Right area (header + content) ────────────────────
    QWidget *rightArea = new QWidget(centralWidget);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightArea);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    // Header bar
    headerBar_ = new QWidget(rightArea);
    headerBar_->setObjectName(QStringLiteral("headerBar"));
    headerBar_->setFixedHeight(Style::kHeaderHeight);

    QHBoxLayout *headerLayout = new QHBoxLayout(headerBar_);
    headerLayout->setContentsMargins(12, 0, 12, 0);

    headerTitle_ = new QLabel(QStringLiteral("泛舟智能科技控制柜系统"), headerBar_);
    headerTitle_->setObjectName(QStringLiteral("headerTitle"));

    headerStatus_ = new QLabel(QStringLiteral("运行中"), headerBar_);
    headerStatus_->setObjectName(QStringLiteral("headerStatus"));
    headerStatus_->setStyleSheet(
        QStringLiteral("QLabel#headerStatus {"
                       "  background: rgba(16,185,129,0.2);"
                       "  color: %1;"
                       "  border: 1px solid rgba(16,185,129,0.3);"
                       "  border-radius: 10px;"
                       "  padding: 2px 8px;"
                       "  font-size: %2px;"
                       "}")
            .arg(Style::kColorSuccess)
            .arg(Style::kFontSmall));

    headerLayout->addWidget(headerTitle_);
    headerLayout->addSpacing(8);
    headerLayout->addWidget(headerStatus_);
    headerLayout->addStretch(1);

    // Connection indicator
    connectionIndicator_ = new QLabel(QStringLiteral("● 在线"), headerBar_);
    connectionIndicator_->setObjectName(QStringLiteral("connectionIndicator"));
    connectionIndicator_->setStyleSheet(
        QStringLiteral("color: %1; font-size: %2px;")
            .arg(Style::kColorSuccess)
            .arg(Style::kFontSmall));
    headerLayout->addWidget(connectionIndicator_);
    headerLayout->addSpacing(16);

    // Clock
    headerTime_ = new QLabel(headerBar_);
    headerTime_->setObjectName(QStringLiteral("headerTime"));
    headerLayout->addWidget(headerTime_);

    rightLayout->addWidget(headerBar_);

    // Stacked content area
    contentStack_ = new QStackedWidget(rightArea);
    contentStack_->setObjectName(QStringLiteral("contentStack"));

    dashboardPage_     = new DashboardPage(rpcClient_, contentStack_);
    deviceControlPage_ = new DeviceControlPage(rpcClient_, contentStack_);
    scenePage_         = new ScenePage(rpcClient_, contentStack_);
    alarmPage_         = new AlarmPage(rpcClient_, contentStack_);
    sensorPage_        = new SensorPage(rpcClient_, contentStack_);
    settingsPage_      = new SettingsPage(rpcClient_, contentStack_);

    contentStack_->addWidget(dashboardPage_);
    contentStack_->addWidget(deviceControlPage_);
    contentStack_->addWidget(scenePage_);
    contentStack_->addWidget(alarmPage_);
    contentStack_->addWidget(sensorPage_);
    contentStack_->addWidget(settingsPage_);

    rightLayout->addWidget(contentStack_, 1);
    rootLayout->addWidget(rightArea, 1);

    // Default to dashboard
    switchToPage(Style::PageDashboard);
}

QPushButton *MainWindow::createNavButton(const QString &icon,
                                         const QString &label,
                                         int pageIndex)
{
    QPushButton *btn = new QPushButton(sidebar_);
    btn->setFixedSize(Style::kSidebarWidth - 8, Style::kSidebarWidth - 8);
    btn->setText(icon + QStringLiteral("\n") + label);
    btn->setProperty("active", QStringLiteral("false"));
    btn->setProperty("pageIndex", pageIndex);
    btn->setObjectName(
        QStringLiteral("navBtn_%1").arg(pageIndex));

    connect(btn, &QPushButton::clicked, this, [this, pageIndex]() {
        switchToPage(pageIndex);
    });

    return btn;
}

// ── Connections ──────────────────────────────────────────

void MainWindow::setupConnections()
{
    if (rpcClient_) {
        connect(rpcClient_, &RpcClient::connected, this, [this]() {
            onConnectionChanged(true);
            wasConnected_ = true;
            // Stop reconnect timer on successful connection
            if (reconnectTimer_->isActive())
                reconnectTimer_->stop();
            // Refresh current page on reconnect
            refreshCurrentPage();
        });
        connect(rpcClient_, &RpcClient::disconnected, this, [this]() {
            onConnectionChanged(false);
            // Start auto-reconnect timer when disconnected
            if (!reconnectTimer_->isActive())
                reconnectTimer_->start();
        });
        connect(rpcClient_, &RpcClient::transportError, this, [this](const QString &) {
            // On transport error, ensure reconnect timer is running
            if (!rpcClient_->isConnected() && !reconnectTimer_->isActive())
                reconnectTimer_->start();
        });
    }

    // Connect alarm count changes to badge
    if (alarmPage_) {
        connect(alarmPage_, &AlarmPage::alarmCountChanged, this, &MainWindow::setAlarmCount);
    }

    // Connect emergency stop to RPC call
    if (dashboardPage_ && rpcClient_) {
        connect(dashboardPage_, &DashboardPage::emergencyStopClicked, this, [this]() {
            if (rpcClient_->isConnected()) {
                rpcClient_->callAsync(
                    QStringLiteral("relay.emergencyStop"),
                    QJsonObject());
            }
        });
    }
}

// ── Page Switching ───────────────────────────────────────

void MainWindow::switchToPage(int index)
{
    if (index < 0 || index >= Style::PageCount)
        return;

    currentPage_ = index;
    contentStack_->setCurrentIndex(index);

    for (int i = 0; i < 6; ++i) {
        bool active = (i == index);
        navButtons_[i]->setProperty("active",
                                    active ? QStringLiteral("true")
                                           : QStringLiteral("false"));
        // Force style refresh
        navButtons_[i]->style()->unpolish(navButtons_[i]);
        navButtons_[i]->style()->polish(navButtons_[i]);
    }

    // Refresh page data on switch
    refreshCurrentPage();
}

// ── Alarm Badge ──────────────────────────────────────────

void MainWindow::setAlarmCount(int count)
{
    if (count > 0) {
        alarmBadge_->setText(QString::number(count));
        alarmBadge_->show();
    } else {
        alarmBadge_->hide();
    }
}

// ── Clock ────────────────────────────────────────────────

void MainWindow::updateClock()
{
    headerTime_->setText(
        QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")));
}

// ── Connection State ─────────────────────────────────────

void MainWindow::onConnectionChanged(bool connected)
{
    if (connected) {
        connectionIndicator_->setText(QStringLiteral("● 在线"));
        connectionIndicator_->setStyleSheet(
            QStringLiteral("color: %1; font-size: %2px;")
                .arg(Style::kColorSuccess)
                .arg(Style::kFontSmall));
        headerStatus_->setText(QStringLiteral("运行中"));
        headerStatus_->setStyleSheet(
            QStringLiteral("QLabel#headerStatus {"
                           "  background: rgba(16,185,129,0.2);"
                           "  color: %1;"
                           "  border: 1px solid rgba(16,185,129,0.3);"
                           "  border-radius: 10px;"
                           "  padding: 2px 8px;"
                           "  font-size: %2px;"
                           "}")
                .arg(Style::kColorSuccess)
                .arg(Style::kFontSmall));
    } else {
        bool isReconnecting = reconnectTimer_ && reconnectTimer_->isActive();
        connectionIndicator_->setText(
            isReconnecting ? QStringLiteral("● 重连中") : QStringLiteral("● 离线"));
        connectionIndicator_->setStyleSheet(
            QStringLiteral("color: %1; font-size: %2px;")
                .arg(isReconnecting ? Style::kColorWarning : Style::kColorDanger)
                .arg(Style::kFontSmall));
        headerStatus_->setText(
            isReconnecting ? QStringLiteral("连接中...") : QStringLiteral("离线"));
        headerStatus_->setStyleSheet(
            QStringLiteral("QLabel#headerStatus {"
                           "  background: rgba(%1,0.2);"
                           "  color: %2;"
                           "  border: 1px solid rgba(%1,0.3);"
                           "  border-radius: 10px;"
                           "  padding: 2px 8px;"
                           "  font-size: %3px;"
                           "}")
                .arg(isReconnecting ? QStringLiteral("245,158,11")
                                    : QStringLiteral("239,68,68"))
                .arg(isReconnecting ? Style::kColorWarning : Style::kColorDanger)
                .arg(Style::kFontSmall));
    }
}

// ── Auto Connect ─────────────────────────────────────────

void MainWindow::startAutoConnect()
{
    if (!rpcClient_)
        return;

    // Initial connection attempt (async, non-blocking)
    onConnectionChanged(false);
    rpcClient_->connectToServerAsync();

    // Start reconnect timer in case initial connection fails
    if (!reconnectTimer_->isActive())
        reconnectTimer_->start();
}

void MainWindow::tryConnect()
{
    if (!rpcClient_)
        return;

    if (rpcClient_->isConnected()) {
        reconnectTimer_->stop();
        return;
    }

    rpcClient_->connectToServerAsync();
}

// ── Auto Refresh ─────────────────────────────────────────

void MainWindow::refreshCurrentPage()
{
    if (!rpcClient_ || !rpcClient_->isConnected())
        return;

    switch (currentPage_) {
    case Style::PageDashboard:
        if (dashboardPage_) dashboardPage_->refreshData();
        break;
    case Style::PageDeviceControl:
        if (deviceControlPage_) deviceControlPage_->refreshData();
        break;
    case Style::PageScenes:
        if (scenePage_) scenePage_->refreshData();
        break;
    case Style::PageAlarms:
        if (alarmPage_) alarmPage_->refreshData();
        break;
    case Style::PageSensors:
        if (sensorPage_) sensorPage_->refreshData();
        break;
    case Style::PageSettings:
        if (settingsPage_) settingsPage_->refreshSysInfo();
        break;
    default:
        break;
    }
}
