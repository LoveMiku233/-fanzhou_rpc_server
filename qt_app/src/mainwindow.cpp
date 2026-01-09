/**
 * @file mainwindow.cpp
 * @brief 主窗口实现
 */

#include "mainwindow.h"
#include "rpc_client.h"
#include "device_widget.h"
#include "group_widget.h"
#include "relay_control_widget.h"

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QSplitter>
#include <QMessageBox>
#include <QJsonDocument>
#include <QDateTime>
#include <QAction>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , tabWidget_(nullptr)
    , logTextEdit_(nullptr)
    , statusLabel_(nullptr)
    , connectionStatusLabel_(nullptr)
    , hostEdit_(nullptr)
    , portSpinBox_(nullptr)
    , connectButton_(nullptr)
    , disconnectButton_(nullptr)
    , deviceWidget_(nullptr)
    , groupWidget_(nullptr)
    , relayControlWidget_(nullptr)
    , rpcClient_(new RpcClient(this))
    , autoRefreshTimer_(new QTimer(this))
{
    setupUi();

    // 连接RPC客户端信号
    connect(rpcClient_, &RpcClient::connected, this, &MainWindow::onRpcConnected);
    connect(rpcClient_, &RpcClient::disconnected, this, &MainWindow::onRpcDisconnected);
    connect(rpcClient_, &RpcClient::transportError, this, &MainWindow::onRpcError);
    connect(rpcClient_, &RpcClient::logMessage, this, &MainWindow::onRpcLogMessage);

    // 自动刷新定时器
    connect(autoRefreshTimer_, &QTimer::timeout, this, &MainWindow::onAutoRefreshTimeout);

    // 加载保存的设置
    QSettings settings;
    hostEdit_->setText(settings.value(QStringLiteral("connection/host"), QStringLiteral("127.0.0.1")).toString());
    portSpinBox_->setValue(settings.value(QStringLiteral("connection/port"), 12345).toInt());

    updateConnectionStatus(false);
}

MainWindow::~MainWindow()
{
    // 保存设置
    QSettings settings;
    settings.setValue(QStringLiteral("connection/host"), hostEdit_->text());
    settings.setValue(QStringLiteral("connection/port"), portSpinBox_->value());

    autoRefreshTimer_->stop();
}

void MainWindow::setupUi()
{
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupCentralWidget();
}

void MainWindow::setupMenuBar()
{
    QMenuBar *menuBar = this->menuBar();

    // 文件菜单
    QMenu *fileMenu = menuBar->addMenu(QStringLiteral("文件(&F)"));
    
    QAction *connectAction = fileMenu->addAction(QStringLiteral("连接服务器(&C)"));
    connect(connectAction, &QAction::triggered, this, &MainWindow::onConnectButtonClicked);
    
    QAction *disconnectAction = fileMenu->addAction(QStringLiteral("断开连接(&D)"));
    connect(disconnectAction, &QAction::triggered, this, &MainWindow::onDisconnectButtonClicked);
    
    fileMenu->addSeparator();
    
    QAction *exitAction = fileMenu->addAction(QStringLiteral("退出(&X)"));
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);

    // 工具菜单
    QMenu *toolsMenu = menuBar->addMenu(QStringLiteral("工具(&T)"));
    
    QAction *pingAction = toolsMenu->addAction(QStringLiteral("Ping测试(&P)"));
    connect(pingAction, &QAction::triggered, this, &MainWindow::onPingButtonClicked);
    
    QAction *sysInfoAction = toolsMenu->addAction(QStringLiteral("系统信息(&I)"));
    connect(sysInfoAction, &QAction::triggered, this, &MainWindow::onSysInfoButtonClicked);
    
    toolsMenu->addSeparator();
    
    QAction *saveConfigAction = toolsMenu->addAction(QStringLiteral("保存配置(&S)"));
    connect(saveConfigAction, &QAction::triggered, this, &MainWindow::onSaveConfigButtonClicked);

    // 帮助菜单
    QMenu *helpMenu = menuBar->addMenu(QStringLiteral("帮助(&H)"));
    
    QAction *aboutAction = helpMenu->addAction(QStringLiteral("关于(&A)"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, QStringLiteral("关于"),
            QStringLiteral("泛舟RPC客户端 v1.0.0\n\n"
                           "温室控制系统GUI客户端\n"
                           "目标平台: Ubuntu Desktop\n"
                           "Qt版本: %1").arg(QString::fromLatin1(qVersion())));
    });
}

void MainWindow::setupToolBar()
{
    QToolBar *toolBar = addToolBar(QStringLiteral("主工具栏"));
    toolBar->setMovable(false);

    toolBar->addAction(QStringLiteral("🔌 连接"), this, &MainWindow::onConnectButtonClicked);
    toolBar->addAction(QStringLiteral("❌ 断开"), this, &MainWindow::onDisconnectButtonClicked);
    toolBar->addSeparator();
    toolBar->addAction(QStringLiteral("🔔 Ping"), this, &MainWindow::onPingButtonClicked);
    toolBar->addAction(QStringLiteral("ℹ️ 系统信息"), this, &MainWindow::onSysInfoButtonClicked);
    toolBar->addSeparator();
    toolBar->addAction(QStringLiteral("💾 保存配置"), this, &MainWindow::onSaveConfigButtonClicked);
}

void MainWindow::setupStatusBar()
{
    QStatusBar *statusBar = this->statusBar();

    connectionStatusLabel_ = new QLabel(QStringLiteral("未连接"));
    connectionStatusLabel_->setProperty("type", QStringLiteral("status-disconnected"));
    statusBar->addWidget(connectionStatusLabel_);

    statusLabel_ = new QLabel();
    statusBar->addPermanentWidget(statusLabel_);
}

void MainWindow::setupCentralWidget()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // 创建连接面板
    createConnectionPanel();
    mainLayout->addWidget(findChild<QGroupBox*>(QStringLiteral("connectionGroupBox")));

    // 创建主分割器
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, this);

    // 创建标签页
    tabWidget_ = new QTabWidget(this);

    // 设备管理页面
    deviceWidget_ = new DeviceWidget(rpcClient_, this);
    tabWidget_->addTab(deviceWidget_, QStringLiteral("🔌 设备管理"));

    // 分组管理页面
    groupWidget_ = new GroupWidget(rpcClient_, this);
    tabWidget_->addTab(groupWidget_, QStringLiteral("📂 分组管理"));

    // 继电器控制页面
    relayControlWidget_ = new RelayControlWidget(rpcClient_, this);
    tabWidget_->addTab(relayControlWidget_, QStringLiteral("🎛️ 继电器控制"));

    mainSplitter->addWidget(tabWidget_);

    // 创建日志面板
    createLogPanel();
    QGroupBox *logGroupBox = findChild<QGroupBox*>(QStringLiteral("logGroupBox"));
    mainSplitter->addWidget(logGroupBox);

    // 设置分割比例
    mainSplitter->setStretchFactor(0, 3);
    mainSplitter->setStretchFactor(1, 1);

    mainLayout->addWidget(mainSplitter, 1);
}

void MainWindow::createConnectionPanel()
{
    QGroupBox *groupBox = new QGroupBox(QStringLiteral("📡 连接设置"), this);
    groupBox->setObjectName(QStringLiteral("connectionGroupBox"));

    QHBoxLayout *layout = new QHBoxLayout(groupBox);

    // 服务器地址
    QLabel *hostLabel = new QLabel(QStringLiteral("服务器地址:"), this);
    hostEdit_ = new QLineEdit(this);
    hostEdit_->setPlaceholderText(QStringLiteral("例如: 192.168.1.100"));
    hostEdit_->setMinimumWidth(200);

    // 端口
    QLabel *portLabel = new QLabel(QStringLiteral("端口:"), this);
    portSpinBox_ = new QSpinBox(this);
    portSpinBox_->setRange(1, 65535);
    portSpinBox_->setValue(12345);

    // 连接按钮
    connectButton_ = new QPushButton(QStringLiteral("🔌 连接"), this);
    connectButton_->setProperty("type", QStringLiteral("success"));
    connect(connectButton_, &QPushButton::clicked, this, &MainWindow::onConnectButtonClicked);

    // 断开按钮
    disconnectButton_ = new QPushButton(QStringLiteral("❌ 断开"), this);
    disconnectButton_->setProperty("type", QStringLiteral("danger"));
    disconnectButton_->setEnabled(false);
    connect(disconnectButton_, &QPushButton::clicked, this, &MainWindow::onDisconnectButtonClicked);

    layout->addWidget(hostLabel);
    layout->addWidget(hostEdit_);
    layout->addWidget(portLabel);
    layout->addWidget(portSpinBox_);
    layout->addWidget(connectButton_);
    layout->addWidget(disconnectButton_);
    layout->addStretch();
}

void MainWindow::createLogPanel()
{
    QGroupBox *groupBox = new QGroupBox(QStringLiteral("📝 通信日志"), this);
    groupBox->setObjectName(QStringLiteral("logGroupBox"));

    QVBoxLayout *layout = new QVBoxLayout(groupBox);

    logTextEdit_ = new QTextEdit(this);
    logTextEdit_->setReadOnly(true);
    logTextEdit_->setMinimumWidth(350);

    QPushButton *clearButton = new QPushButton(QStringLiteral("🗑️ 清空日志"), this);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::clearLog);

    layout->addWidget(logTextEdit_);
    layout->addWidget(clearButton);
}

void MainWindow::onConnectButtonClicked()
{
    QString host = hostEdit_->text().trimmed();
    quint16 port = static_cast<quint16>(portSpinBox_->value());

    if (host.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请输入服务器地址"));
        return;
    }

    rpcClient_->setEndpoint(host, port);
    
    appendLog(QStringLiteral("正在连接到 %1:%2...").arg(host).arg(port));
    
    if (rpcClient_->connectToServer()) {
        // 连接成功后发送ping测试
        onPingButtonClicked();
    }
}

void MainWindow::onDisconnectButtonClicked()
{
    rpcClient_->disconnectFromServer();
    autoRefreshTimer_->stop();
}

void MainWindow::onPingButtonClicked()
{
    if (!rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QJsonValue result = rpcClient_->call(QStringLiteral("rpc.ping"));
    appendLog(QStringLiteral("Ping结果: %1").arg(
        QString::fromUtf8(QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact))));
}

void MainWindow::onSysInfoButtonClicked()
{
    if (!rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QJsonValue result = rpcClient_->call(QStringLiteral("sys.info"));
    
    QString infoText = QString::fromUtf8(QJsonDocument(result.toObject()).toJson(QJsonDocument::Indented));
    
    QMessageBox::information(this, QStringLiteral("系统信息"), infoText);
    appendLog(QStringLiteral("系统信息: %1").arg(
        QString::fromUtf8(QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact))));
}

void MainWindow::onSaveConfigButtonClicked()
{
    if (!rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QJsonValue result = rpcClient_->call(QStringLiteral("config.save"));
    
    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("配置保存成功！"));
        appendLog(QStringLiteral("配置保存成功"));
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("配置保存失败: %1").arg(error));
        appendLog(QStringLiteral("配置保存失败: %1").arg(error));
    }
}

void MainWindow::onRpcConnected()
{
    updateConnectionStatus(true);
    appendLog(QStringLiteral("✅ 服务器连接成功"));
    
    // 启动自动刷新
    autoRefreshTimer_->start(5000);
    
    // 刷新设备列表
    if (deviceWidget_) {
        deviceWidget_->refreshDeviceList();
    }
    if (groupWidget_) {
        groupWidget_->refreshGroupList();
    }
}

void MainWindow::onRpcDisconnected()
{
    updateConnectionStatus(false);
    appendLog(QStringLiteral("❌ 服务器连接已断开"));
    autoRefreshTimer_->stop();
}

void MainWindow::onRpcError(const QString &error)
{
    appendLog(QStringLiteral("⚠️ 错误: %1").arg(error));
}

void MainWindow::onRpcLogMessage(const QString &message)
{
    appendLog(message);
}

void MainWindow::onAutoRefreshTimeout()
{
    if (rpcClient_->isConnected()) {
        // 静默刷新设备状态
        if (deviceWidget_ && tabWidget_->currentWidget() == deviceWidget_) {
            deviceWidget_->refreshDeviceStatus();
        }
    }
}

void MainWindow::updateConnectionStatus(bool connected)
{
    if (connected) {
        connectionStatusLabel_->setText(QStringLiteral("已连接到 %1:%2")
            .arg(rpcClient_->host()).arg(rpcClient_->port()));
        connectionStatusLabel_->setProperty("type", QStringLiteral("status-connected"));
        connectButton_->setEnabled(false);
        disconnectButton_->setEnabled(true);
    } else {
        connectionStatusLabel_->setText(QStringLiteral("未连接"));
        connectionStatusLabel_->setProperty("type", QStringLiteral("status-disconnected"));
        connectButton_->setEnabled(true);
        disconnectButton_->setEnabled(false);
    }
    connectionStatusLabel_->style()->unpolish(connectionStatusLabel_);
    connectionStatusLabel_->style()->polish(connectionStatusLabel_);
}

void MainWindow::appendLog(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    logTextEdit_->append(QStringLiteral("[%1] %2").arg(timestamp, message));
    
    // 滚动到底部
    QTextCursor cursor = logTextEdit_->textCursor();
    cursor.movePosition(QTextCursor::End);
    logTextEdit_->setTextCursor(cursor);
}

void MainWindow::clearLog()
{
    logTextEdit_->clear();
    appendLog(QStringLiteral("日志已清空"));
}
