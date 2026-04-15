/**
 * @file mainwindow.cpp
 * @brief 主窗口实现 - 大棚控制系统
 */

#include "mainwindow.h"
#include "rpc_client.h"
#include "home_widget.h"
#include "greenhouse_3d_widget.h"
#include "device_widget.h"
#include "group_widget.h"
#include "strategy_widget.h"
#include "sensor_widget.h"
#include "log_widget.h"
#include "settings_widget.h"
#include "monitor_widget.h"
#include "debug_widget.h"
#include "screen_manager.h"
#include "style_constants.h"

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
#include <memory>

using namespace UIConstants;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , topStatusBar_(nullptr)
    , topStatusLayout_(nullptr)
    , sidebar_(nullptr)
    , sidebarLayout_(nullptr)
    , menuButtonGroup_(nullptr)
    , contentStack_(nullptr)
    , connectionStatusLabel_(nullptr)
    , timeLabel_(nullptr)
    , alertLabel_(nullptr)
    , homeWidget_(nullptr)
    , greenhouse3dWidget_(nullptr)
    , deviceWidget_(nullptr)
    , groupWidget_(nullptr)
    , strategyWidget_(nullptr)
    , sensorWidget_(nullptr)
    , logWidget_(nullptr)
    , settingsWidget_(nullptr)
    , monitorWidget_(nullptr)
    , debugWidget_(nullptr)
    , rpcClient_(new RpcClient(this))
    , screenManager_(new ScreenManager(this))
    , autoRefreshTimer_(new QTimer(this))
    , statusBarTimer_(new QTimer(this))
    , currentPageIndex_(0)
{
    setupUi();

    // 自动刷新定时器
    connect(autoRefreshTimer_, &QTimer::timeout, this, &MainWindow::onAutoRefreshTimeout);

    // 状态栏时间更新定时器
    connect(statusBarTimer_, &QTimer::timeout, this, &MainWindow::updateStatusBarTime);
    statusBarTimer_->start(1000);
    updateStatusBarTime();

    // 延迟执行自动连接
    QTimer::singleShot(800, this, [this]() {
        QSettings settings;
        bool autoConnect = settings.value(QStringLiteral("settings/autoConnect"), true).toBool();

        if (autoConnect) {
            QString host = settings.value(QStringLiteral("connection/host"), QStringLiteral("127.0.0.1")).toString();
            quint16 port = static_cast<quint16>(settings.value(QStringLiteral("connection/port"), 12345).toInt());

            onLogMessage(QStringLiteral("正在自动连接到服务器 %1:%2...").arg(host).arg(port));
            qDebug() << "[MAIN_WINDOW] 正在自动连接到服务器" << host << ":" << port;

            rpcClient_->setEndpoint(host, port);

            auto connOk = std::make_shared<QMetaObject::Connection>();
            auto connErr = std::make_shared<QMetaObject::Connection>();
            *connOk = connect(rpcClient_, &RpcClient::connected, this, [this, connOk, connErr]() {
                disconnect(*connOk);
                disconnect(*connErr);
                onLogMessage(QStringLiteral("[OK] 自动连接成功"));
                qDebug() << "[MAIN_WINDOW] 自动连接成功";
                onConnectionStatusChanged(true);
            });
            *connErr = connect(rpcClient_, &RpcClient::transportError, this, [this, connOk, connErr](const QString &error) {
                disconnect(*connOk);
                disconnect(*connErr);
                onLogMessage(QStringLiteral("[X] 自动连接失败: %1，请检查服务器是否运行").arg(error), QStringLiteral("WARN"));
                qDebug() << "[MAIN_WINDOW] 自动连接失败:" << error;
            });
            rpcClient_->connectToServerAsync();
        } else {
            qDebug() << "[MAIN_WINDOW] 自动连接未启用";
        }
    });

    qDebug() << "[MAIN_WINDOW] 主窗口初始化完成";
}

MainWindow::~MainWindow()
{
    autoRefreshTimer_->stop();
    statusBarTimer_->stop();
    qDebug() << "[MAIN_WINDOW] 主窗口销毁";
}

void MainWindow::setupUi()
{
    statusBar()->hide();
    setupTopStatusBar();
    setupCentralWidget();
}

void MainWindow::setupTopStatusBar()
{
    connectionStatusLabel_ = new QLabel(QStringLiteral("[X] 未连接"));
    connectionStatusLabel_->setStyleSheet(QStringLiteral(
        "color: #fbe9e7; font-weight: 700; padding: 6px 12px; "
        "background: #b6423a; border-radius: 14px;"));

    cloudStatusLabel_ = new QLabel(QStringLiteral("[云] 未连接"));
    cloudStatusLabel_->setToolTip(QStringLiteral("云/MQTT连接状态"));
    cloudStatusLabel_->setStyleSheet(QStringLiteral(
        "color: #e8edf0; padding: 6px 12px; "
        "background: #566872; border-radius: 14px;"));

    timeLabel_ = new QLabel(QStringLiteral("--:--:--"));
    timeLabel_->setStyleSheet(QStringLiteral(
        "color: #2f3a40; padding: 6px 12px; font-weight: 700; "
        "background: #f0c75e; border-radius: 14px;"));

    alertLabel_ = new QLabel(QStringLiteral("[OK] 系统就绪"));
    alertLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    alertLabel_->setStyleSheet(QStringLiteral(
        "color: #e8edf0; padding: 6px 12px; "
        "background: #3f4f58; border-radius: 14px;"));
}

void MainWindow::setupCentralWidget()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    topStatusBar_ = new QWidget(centralWidget);
    topStatusBar_->setObjectName(QStringLiteral("topStatusBar"));
    topStatusBar_->setMinimumHeight(44);
    topStatusBar_->setMaximumHeight(50);
    topStatusLayout_ = new QHBoxLayout(topStatusBar_);
    topStatusLayout_->setContentsMargins(6, 4, 6, 4);
    topStatusLayout_->setSpacing(6);
    topStatusLayout_->addWidget(connectionStatusLabel_);
    topStatusLayout_->addWidget(cloudStatusLabel_);
    topStatusLayout_->addWidget(timeLabel_);
    topStatusLayout_->addWidget(alertLabel_, 1);
    mainLayout->addWidget(topStatusBar_);

    createContentArea();
    mainLayout->addWidget(contentStack_, 1);

    createBottomNavBar();
    mainLayout->addWidget(sidebar_);
}

void MainWindow::createBottomNavBar()
{
    sidebar_ = new QWidget(this);
    sidebar_->setObjectName(QStringLiteral("sidebar"));
    sidebar_->setMinimumHeight(62);
    sidebar_->setMaximumHeight(68);

    sidebarLayout_ = new QHBoxLayout(sidebar_);
    sidebarLayout_->setContentsMargins(6, 6, 6, 6);
    sidebarLayout_->setSpacing(5);

    menuButtonGroup_ = new QButtonGroup(this);

    // 菜单项
    QStringList menuNames = {
        QStringLiteral("主页"),
        QStringLiteral("大棚"),
        QStringLiteral("设备"),
        QStringLiteral("分组"),
        QStringLiteral("策略"),
        QStringLiteral("传感"),
        QStringLiteral("日志"),
        QStringLiteral("设置"),
        QStringLiteral("监控"),
        QStringLiteral("调试")
    };

    for (int i = 0; i < menuNames.size(); ++i) {
        QPushButton *btn = new QPushButton(menuNames[i], sidebar_);
        btn->setCheckable(true);
        btn->setFixedHeight(48);
        btn->setMinimumWidth(70);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setCursor(Qt::PointingHandCursor);

        menuButtonGroup_->addButton(btn, i);
        menuButtons_.append(btn);
        sidebarLayout_->addWidget(btn, 1);
    }

    connect(menuButtonGroup_, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked),
            this, [this](QAbstractButton *button) {
        int index = menuButtonGroup_->id(button);
        if (index >= 0) {
            onMenuButtonClicked(index);
        }
    });

    // 默认选中第一个
    if (!menuButtons_.isEmpty()) {
        menuButtons_[0]->setChecked(true);
    }
}

void MainWindow::createContentArea()
{
    contentStack_ = new QStackedWidget(this);

    // 创建主页
    QScrollArea *homeScrollArea = new QScrollArea(this);
    homeScrollArea->setWidgetResizable(true);
    homeScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    homeScrollArea->setFrameShape(QFrame::NoFrame);
    homeWidget_ = new HomeWidget(rpcClient_, this);
    homeScrollArea->setWidget(homeWidget_);
    QScroller::grabGesture(homeScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    connect(homeWidget_, &HomeWidget::mqttStatusUpdated, this, &MainWindow::onMqttStatusFromDashboard);
    contentStack_->addWidget(homeScrollArea);

    // 创建设备管理页面
    QScrollArea *greenhouseScrollArea = new QScrollArea(this);
    greenhouseScrollArea->setWidgetResizable(true);
    greenhouseScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    greenhouseScrollArea->setFrameShape(QFrame::NoFrame);
    greenhouse3dWidget_ = new Greenhouse3DWidget(rpcClient_, this);
    greenhouseScrollArea->setWidget(greenhouse3dWidget_);
    QScroller::grabGesture(greenhouseScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    connect(greenhouse3dWidget_, &Greenhouse3DWidget::logMessage, this, &MainWindow::onLogMessage);
    contentStack_->addWidget(greenhouseScrollArea);

    // 创建设备管理页面
    QScrollArea *deviceScrollArea = new QScrollArea(this);
    deviceScrollArea->setWidgetResizable(true);
    deviceScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    deviceScrollArea->setFrameShape(QFrame::NoFrame);
    deviceWidget_ = new DeviceWidget(rpcClient_, this);
    deviceScrollArea->setWidget(deviceWidget_);
    QScroller::grabGesture(deviceScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    connect(deviceWidget_, &DeviceWidget::logMessage, this, &MainWindow::onLogMessage);
    contentStack_->addWidget(deviceScrollArea);

    // 创建分组管理页面
    QScrollArea *groupScrollArea = new QScrollArea(this);
    groupScrollArea->setWidgetResizable(true);
    groupScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    groupScrollArea->setFrameShape(QFrame::NoFrame);
    groupWidget_ = new GroupWidget(rpcClient_, this);
    groupScrollArea->setWidget(groupWidget_);
    QScroller::grabGesture(groupScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    connect(groupWidget_, &GroupWidget::logMessage, this, &MainWindow::onLogMessage);
    contentStack_->addWidget(groupScrollArea);

    // 创建策略管理页面
    QScrollArea *strategyScrollArea = new QScrollArea(this);
    strategyScrollArea->setWidgetResizable(true);
    strategyScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    strategyScrollArea->setFrameShape(QFrame::NoFrame);
    strategyWidget_ = new StrategyWidget(rpcClient_, this);
    strategyScrollArea->setWidget(strategyWidget_);
    QScroller::grabGesture(strategyScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    connect(strategyWidget_, &StrategyWidget::logMessage, this, &MainWindow::onLogMessage);
    contentStack_->addWidget(strategyScrollArea);

    // 创建传感器监控页面
    QScrollArea *sensorScrollArea = new QScrollArea(this);
    sensorScrollArea->setWidgetResizable(true);
    sensorScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sensorScrollArea->setFrameShape(QFrame::NoFrame);
    sensorWidget_ = new SensorWidget(rpcClient_, this);
    sensorScrollArea->setWidget(sensorWidget_);
    QScroller::grabGesture(sensorScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    connect(sensorWidget_, &SensorWidget::logMessage, this, &MainWindow::onLogMessage);
    contentStack_->addWidget(sensorScrollArea);

    // 创建日志页面
    QScrollArea *logScrollArea = new QScrollArea(this);
    logScrollArea->setWidgetResizable(true);
    logScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    logScrollArea->setFrameShape(QFrame::NoFrame);
    logWidget_ = new LogWidget(this);
    logScrollArea->setWidget(logWidget_);
    QScroller::grabGesture(logScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    connect(logWidget_, &LogWidget::newAlertMessage, this, [this](const QString &message) {
        lastAlertMessage_ = message;
        alertLabel_->setText(QStringLiteral("[警] %1").arg(message));
    });
    contentStack_->addWidget(logScrollArea);

    // 创建设置页面
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
    connect(settingsWidget_, &SettingsWidget::autoScreenOffSettingsChanged,
            this, &MainWindow::onAutoScreenOffSettingsChanged);
    contentStack_->addWidget(settingsScrollArea);

    // 创建监控页面
    QScrollArea *monitorScrollArea = new QScrollArea(this);
    monitorScrollArea->setWidgetResizable(true);
    monitorScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    monitorScrollArea->setFrameShape(QFrame::NoFrame);
    monitorWidget_ = new MonitorWidget(rpcClient_, this);
    monitorScrollArea->setWidget(monitorWidget_);
    QScroller::grabGesture(monitorScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    contentStack_->addWidget(monitorScrollArea);

    // 创建调试页面
    QScrollArea *debugScrollArea = new QScrollArea(this);
    debugScrollArea->setWidgetResizable(true);
    debugScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    debugScrollArea->setFrameShape(QFrame::NoFrame);
    debugWidget_ = new DebugWidget(rpcClient_, this);
    debugScrollArea->setWidget(debugWidget_);
    QScroller::grabGesture(debugScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    connect(debugWidget_, &DebugWidget::logMessage, this, &MainWindow::onLogMessage);
    contentStack_->addWidget(debugScrollArea);

    // 初始化自动息屏
    QSettings settings;
    bool autoScreenOff = settings.value(QStringLiteral("settings/autoScreenOff"), false).toBool();
    int screenOffTimeout = settings.value(QStringLiteral("settings/screenOffTimeout"), 60).toInt();
    if (autoScreenOff) {
        screenManager_->enableAutoScreenOff(screenOffTimeout);
    }
}

void MainWindow::onMenuButtonClicked(int index)
{
    qDebug() << "[MAIN_WINDOW] 菜单按钮点击 index=" << index;
    switchToPage(index);
}

void MainWindow::switchToPage(int index)
{
    if (index < 0 || index >= contentStack_->count()) {
        return;
    }

    currentPageIndex_ = index;
    contentStack_->setCurrentIndex(index);
    updateMenuSelection(index);

    if (index == 0 && homeWidget_ && rpcClient_->isConnected()) {
        homeWidget_->refreshData();
    }
    if (index == 1 && greenhouse3dWidget_ && rpcClient_->isConnected()) {
        // 3D页面会自动刷新分组映射，这里不做阻塞调用
    }
    if (index == 2 && deviceWidget_ && rpcClient_->isConnected()) {
        deviceWidget_->refreshDeviceList();
    }
    if (index == 3 && groupWidget_ && rpcClient_->isConnected()) {
        groupWidget_->refreshGroupList();
    }
    if (index == 4 && strategyWidget_ && rpcClient_->isConnected()) {
        strategyWidget_->refreshAllStrategies();
    }
    if (index == 5 && sensorWidget_ && rpcClient_->isConnected()) {
        sensorWidget_->refreshSensorList();
    }
    if (index == 8 && monitorWidget_) {
        monitorWidget_->refreshData();
    }
    if (index == 9 && debugWidget_ && rpcClient_->isConnected()) {
        debugWidget_->refreshAll();
    }
}

void MainWindow::updateMenuSelection(int activeIndex)
{
    if (activeIndex >= 0 && activeIndex < menuButtons_.size()) {
        menuButtons_[activeIndex]->setChecked(true);
    }
}

void MainWindow::onConnectionStatusChanged(bool connected)
{
    updateStatusBarConnection(connected);
    if (connected) {
        QSettings settings;
        int interval = settings.value(QStringLiteral("settings/refreshInterval"), 5).toInt();
        autoRefreshTimer_->start(interval * 1000);

        if (homeWidget_) homeWidget_->refreshData();
        if (deviceWidget_) deviceWidget_->refreshDeviceList();
        if (groupWidget_) groupWidget_->refreshGroupList();
        if (strategyWidget_) strategyWidget_->refreshAllStrategies();
        if (sensorWidget_) sensorWidget_->refreshSensorList();

        onLogMessage(QStringLiteral("[OK] 已连接到服务器 %1:%2")
            .arg(rpcClient_->host()).arg(rpcClient_->port()));
        qDebug() << "[MAIN_WINDOW] 已连接到服务器" << rpcClient_->host() << ":" << rpcClient_->port();
    } else {
        autoRefreshTimer_->stop();
        onLogMessage(QStringLiteral("[X] 服务器连接已断开"), QStringLiteral("WARN"));
        qDebug() << "[MAIN_WINDOW] 服务器连接已断开";
    }
}

void MainWindow::updateStatusBarConnection(bool connected)
{
    if (connected) {
        connectionStatusLabel_->setText(QStringLiteral("[OK] 已连接"));
        connectionStatusLabel_->setStyleSheet(QStringLiteral(
            "color: #ecf8ef; font-weight: 700; padding: 6px 12px; "
            "background: #2e7d32; border-radius: 14px;"));
        alertLabel_->setText(QStringLiteral("[OK] 系统运行正常"));
        alertLabel_->setStyleSheet(QStringLiteral(
            "color: #e8edf0; padding: 6px 12px; "
            "background: #3f4f58; border-radius: 14px;"));
    } else {
        connectionStatusLabel_->setText(QStringLiteral("[X] 未连接"));
        connectionStatusLabel_->setStyleSheet(QStringLiteral(
            "color: #fbe9e7; font-weight: 700; padding: 6px 12px; "
            "background: #b6423a; border-radius: 14px;"));
    }
}

void MainWindow::onAutoRefreshTimeout()
{
    if (rpcClient_->isConnected()) {
        switch (currentPageIndex_) {
        case 0:
            if (homeWidget_) homeWidget_->refreshData();
            break;
        case 2:
            if (deviceWidget_) deviceWidget_->refreshDevicePresence();
            updateCloudStatus();
            break;
        default:
            updateCloudStatus();
            break;
        }
    }
}

void MainWindow::onLogMessage(const QString &message, const QString &level)
{
    if (logWidget_) {
        logWidget_->appendLog(message, level);
    }

    if (level == QStringLiteral("ERROR")) {
        alertLabel_->setText(QStringLiteral("[X] %1").arg(message));
        alertLabel_->setStyleSheet(QStringLiteral(
            "color: #fbe9e7; padding: 6px 12px; font-weight: 700; "
            "background: #b6423a; border-radius: 14px;"));
    } else if (level == QStringLiteral("WARN")) {
        alertLabel_->setText(QStringLiteral("[警] %1").arg(message));
        alertLabel_->setStyleSheet(QStringLiteral(
            "color: #2f3a40; padding: 6px 12px; font-weight: 700; "
            "background: #f0c75e; border-radius: 14px;"));
    } else if (level == QStringLiteral("INFO")) {
        alertLabel_->setText(QStringLiteral("[OK] %1").arg(message));
        alertLabel_->setStyleSheet(QStringLiteral(
            "color: #e8edf0; padding: 6px 12px; "
            "background: #3f4f58; border-radius: 14px;"));
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

        QJsonValue result = rpcClient_->call(QStringLiteral("rpc.ping"), QJsonObject(), 1000);
        if (!result.isUndefined()) {
            onLogMessage(QStringLiteral("服务器响应正常"));
        }

        onConnectionStatusChanged(true);
    } else {
        onLogMessage(QStringLiteral("自动连接失败，请检查服务器是否运行"), QStringLiteral("WARN"));
    }
}

void MainWindow::updateCloudStatus()
{
    if (!rpcClient_->isConnected()) {
        cloudStatusLabel_->setText(QStringLiteral("[云] 未连接"));
        cloudStatusLabel_->setStyleSheet(QStringLiteral(
            "color: #e8edf0; padding: 6px 12px; "
            "background: #566872; border-radius: 14px;"));
        return;
    }

    static qint64 lastCallTime = 0;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (now - lastCallTime < 3000) {
        return;
    }
    lastCallTime = now;

    rpcClient_->callAsync(QStringLiteral("mqtt.channels.list"), QJsonObject(),
        [this](const QJsonValue &result, const QJsonObject &error) {
            QMetaObject::invokeMethod(this, [result, error, this]() {
                if (!cloudStatusLabel_) return;

                if (!error.isEmpty() || !result.isObject()) {
                    cloudStatusLabel_->setText(QStringLiteral("[云] 未知"));
                    cloudStatusLabel_->setStyleSheet(QStringLiteral(
                        "color: #e8edf0; padding: 6px 12px; "
                        "background: #566872; border-radius: 14px;"));
                    return;
                }

                QJsonObject resultObj = result.toObject();

                if (!resultObj.value(QStringLiteral("ok")).toBool()) {
                    cloudStatusLabel_->setText(QStringLiteral("[云] 未知"));
                    cloudStatusLabel_->setStyleSheet(QStringLiteral(
                        "color: #e8edf0; padding: 6px 12px; "
                        "background: #566872; border-radius: 14px;"));
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

                QString text;
                QString style;
                if (totalChannels == 0) {
                    text = QStringLiteral("[云] 未配置");
                    style = QStringLiteral("color: #e8edf0; padding: 6px 12px; background: #566872; border-radius: 14px;");
                } else if (connectedChannels == 0) {
                    text = QStringLiteral("[云] 断开 (0/%1)").arg(totalChannels);
                    style = QStringLiteral("color: #fbe9e7; padding: 6px 12px; font-weight: 700; background: #b6423a; border-radius: 14px;");
                } else if (connectedChannels == totalChannels) {
                    text = QStringLiteral("[云] 已连接 (%1)").arg(totalChannels);
                    style = QStringLiteral("color: #ecf8ef; padding: 6px 12px; font-weight: 700; background: #2e7d32; border-radius: 14px;");
                } else {
                    text = QStringLiteral("[云] 部分连接 (%1/%2)").arg(connectedChannels).arg(totalChannels);
                    style = QStringLiteral("color: #2f3a40; padding: 6px 12px; font-weight: 700; background: #f0c75e; border-radius: 14px;");
                }

                cloudStatusLabel_->setText(text);
                cloudStatusLabel_->setStyleSheet(style);
            }, Qt::QueuedConnection);
        }, 2000);
}

void MainWindow::onMqttStatusFromDashboard(int connected, int total)
{
    QString text;
    QString style;
    if (total == 0) {
        text = QStringLiteral("[云] 未配置");
        style = QStringLiteral("color: #e8edf0; padding: 6px 12px; background: #566872; border-radius: 14px;");
    } else if (connected == 0) {
        text = QStringLiteral("[云] 断开 (0/%1)").arg(total);
        style = QStringLiteral("color: #fbe9e7; padding: 6px 12px; font-weight: 700; background: #b6423a; border-radius: 14px;");
    } else if (connected == total) {
        text = QStringLiteral("[云] 已连接 (%1)").arg(total);
        style = QStringLiteral("color: #ecf8ef; padding: 6px 12px; font-weight: 700; background: #2e7d32; border-radius: 14px;");
    } else {
        text = QStringLiteral("[云] 部分连接 (%1/%2)").arg(connected).arg(total);
        style = QStringLiteral("color: #2f3a40; padding: 6px 12px; font-weight: 700; background: #f0c75e; border-radius: 14px;");
    }
    cloudStatusLabel_->setText(text);
    cloudStatusLabel_->setStyleSheet(style);
}

void MainWindow::onAutoScreenOffSettingsChanged(bool enabled, int timeoutSeconds)
{
    if (enabled) {
        screenManager_->setScreenOffTimeout(timeoutSeconds);
        screenManager_->enableAutoScreenOff(timeoutSeconds);
        qDebug() << "[MAIN_WINDOW] 自动息屏已启用，超时时间:" << timeoutSeconds << "秒";
    } else {
        screenManager_->disableAutoScreenOff();
        qDebug() << "[MAIN_WINDOW] 自动息屏已禁用";
    }
}
