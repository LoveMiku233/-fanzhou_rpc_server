/**
 * @file settings_widget.cpp
 * @brief 设置页面实现
 */

#include "settings_widget.h"
#include "rpc_client.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QSettings>

SettingsWidget::SettingsWidget(RpcClient *rpcClient, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpcClient)
    , hostEdit_(nullptr)
    , portSpinBox_(nullptr)
    , connectButton_(nullptr)
    , disconnectButton_(nullptr)
    , pingButton_(nullptr)
    , sysInfoButton_(nullptr)
    , saveConfigButton_(nullptr)
    , statusLabel_(nullptr)
    , refreshIntervalSpinBox_(nullptr)
    , autoConnectCheckBox_(nullptr)
{
    setupUi();

    // 连接RPC客户端信号
    connect(rpcClient_, &RpcClient::connected, this, &SettingsWidget::onRpcConnected);
    connect(rpcClient_, &RpcClient::disconnected, this, &SettingsWidget::onRpcDisconnected);
    connect(rpcClient_, &RpcClient::transportError, this, &SettingsWidget::onRpcError);

    // 加载保存的设置
    QSettings settings;
    hostEdit_->setText(settings.value(QStringLiteral("connection/host"), QStringLiteral("127.0.0.1")).toString());
    portSpinBox_->setValue(settings.value(QStringLiteral("connection/port"), 12345).toInt());
    refreshIntervalSpinBox_->setValue(settings.value(QStringLiteral("settings/refreshInterval"), 5).toInt());
    autoConnectCheckBox_->setChecked(settings.value(QStringLiteral("settings/autoConnect"), false).toBool());

    updateConnectionStatus(false);
}

void SettingsWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(16);

    // 页面标题
    QLabel *titleLabel = new QLabel(QStringLiteral("⚙️ 系统设置"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: 20px; font-weight: bold; color: #2c3e50; padding: 8px 0;"));
    mainLayout->addWidget(titleLabel);

    // 服务器连接设置组
    QGroupBox *serverGroupBox = new QGroupBox(QStringLiteral("服务器连接"), this);
    QFormLayout *serverLayout = new QFormLayout(serverGroupBox);
    serverLayout->setSpacing(12);
    serverLayout->setContentsMargins(16, 20, 16, 16);

    hostEdit_ = new QLineEdit(this);
    hostEdit_->setPlaceholderText(QStringLiteral("192.168.1.100"));
    hostEdit_->setMinimumHeight(48);
    serverLayout->addRow(QStringLiteral("服务器地址:"), hostEdit_);

    portSpinBox_ = new QSpinBox(this);
    portSpinBox_->setRange(1, 65535);
    portSpinBox_->setValue(12345);
    portSpinBox_->setMinimumHeight(48);
    serverLayout->addRow(QStringLiteral("端口号:"), portSpinBox_);

    mainLayout->addWidget(serverGroupBox);

    // 连接状态卡片
    statusLabel_ = new QLabel(QStringLiteral("状态: 未连接"), this);
    statusLabel_->setStyleSheet(QStringLiteral(
        "font-size: 16px; padding: 12px; background-color: #f8d7da; color: #721c24; border-radius: 8px;"));
    mainLayout->addWidget(statusLabel_);

    // 连接操作按钮
    QHBoxLayout *connBtnLayout = new QHBoxLayout();
    connBtnLayout->setSpacing(12);

    connectButton_ = new QPushButton(QStringLiteral("🔗 连接"), this);
    connectButton_->setProperty("type", QStringLiteral("success"));
    connectButton_->setMinimumHeight(56);
    connect(connectButton_, &QPushButton::clicked, this, &SettingsWidget::onConnect);
    connBtnLayout->addWidget(connectButton_);

    disconnectButton_ = new QPushButton(QStringLiteral("🔌 断开"), this);
    disconnectButton_->setProperty("type", QStringLiteral("danger"));
    disconnectButton_->setMinimumHeight(56);
    disconnectButton_->setEnabled(false);
    connect(disconnectButton_, &QPushButton::clicked, this, &SettingsWidget::onDisconnect);
    connBtnLayout->addWidget(disconnectButton_);

    mainLayout->addLayout(connBtnLayout);

    // 工具按钮组
    QGroupBox *toolsGroupBox = new QGroupBox(QStringLiteral("诊断工具"), this);
    QGridLayout *toolsLayout = new QGridLayout(toolsGroupBox);
    toolsLayout->setSpacing(12);
    toolsLayout->setContentsMargins(16, 20, 16, 16);

    pingButton_ = new QPushButton(QStringLiteral("📡 Ping 测试"), this);
    pingButton_->setMinimumHeight(50);
    connect(pingButton_, &QPushButton::clicked, this, &SettingsWidget::onPing);
    toolsLayout->addWidget(pingButton_, 0, 0);

    sysInfoButton_ = new QPushButton(QStringLiteral("ℹ️ 系统信息"), this);
    sysInfoButton_->setMinimumHeight(50);
    connect(sysInfoButton_, &QPushButton::clicked, this, &SettingsWidget::onSysInfo);
    toolsLayout->addWidget(sysInfoButton_, 0, 1);

    saveConfigButton_ = new QPushButton(QStringLiteral("💾 保存服务器配置"), this);
    saveConfigButton_->setProperty("type", QStringLiteral("warning"));
    saveConfigButton_->setMinimumHeight(50);
    connect(saveConfigButton_, &QPushButton::clicked, this, &SettingsWidget::onSaveConfig);
    toolsLayout->addWidget(saveConfigButton_, 1, 0, 1, 2);

    mainLayout->addWidget(toolsGroupBox);

    // 系统设置组
    QGroupBox *systemGroupBox = new QGroupBox(QStringLiteral("系统设置"), this);
    QFormLayout *systemLayout = new QFormLayout(systemGroupBox);
    systemLayout->setSpacing(12);
    systemLayout->setContentsMargins(16, 20, 16, 16);

    refreshIntervalSpinBox_ = new QSpinBox(this);
    refreshIntervalSpinBox_->setRange(1, 60);
    refreshIntervalSpinBox_->setValue(5);
    refreshIntervalSpinBox_->setSuffix(QStringLiteral(" 秒"));
    refreshIntervalSpinBox_->setMinimumHeight(48);
    connect(refreshIntervalSpinBox_, QOverload<int>::of(&QSpinBox::valueChanged), [](int value) {
        QSettings settings;
        settings.setValue(QStringLiteral("settings/refreshInterval"), value);
    });
    systemLayout->addRow(QStringLiteral("自动刷新间隔:"), refreshIntervalSpinBox_);

    autoConnectCheckBox_ = new QCheckBox(QStringLiteral("启动时自动连接"), this);
    autoConnectCheckBox_->setMinimumHeight(40);
    connect(autoConnectCheckBox_, &QCheckBox::toggled, [](bool checked) {
        QSettings settings;
        settings.setValue(QStringLiteral("settings/autoConnect"), checked);
    });
    systemLayout->addRow(autoConnectCheckBox_);

    mainLayout->addWidget(systemGroupBox);

    mainLayout->addStretch();
}

QString SettingsWidget::host() const
{
    return hostEdit_->text().trimmed();
}

quint16 SettingsWidget::port() const
{
    return static_cast<quint16>(portSpinBox_->value());
}

void SettingsWidget::onConnect()
{
    QString host = hostEdit_->text().trimmed();
    quint16 port = static_cast<quint16>(portSpinBox_->value());

    if (host.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请输入服务器地址"));
        return;
    }

    // 保存设置
    QSettings settings;
    settings.setValue(QStringLiteral("connection/host"), host);
    settings.setValue(QStringLiteral("connection/port"), port);

    rpcClient_->setEndpoint(host, port);

    emit logMessage(QStringLiteral("正在连接到 %1:%2...").arg(host).arg(port));

    if (rpcClient_->connectToServer()) {
        onPing();
    }
}

void SettingsWidget::onDisconnect()
{
    rpcClient_->disconnectFromServer();
}

void SettingsWidget::onPing()
{
    if (!rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QJsonValue result = rpcClient_->call(QStringLiteral("rpc.ping"));
    QString resultStr = QString::fromUtf8(QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact));
    emit logMessage(QStringLiteral("Ping结果: %1").arg(resultStr));
}

void SettingsWidget::onSysInfo()
{
    if (!rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QJsonValue result = rpcClient_->call(QStringLiteral("sys.info"));
    QString infoText = QString::fromUtf8(QJsonDocument(result.toObject()).toJson(QJsonDocument::Indented));

    QMessageBox::information(this, QStringLiteral("系统信息"), infoText);
    emit logMessage(QStringLiteral("获取系统信息成功"));
}

void SettingsWidget::onSaveConfig()
{
    if (!rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QJsonValue result = rpcClient_->call(QStringLiteral("config.save"));

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("配置保存成功！"));
        emit logMessage(QStringLiteral("配置保存成功"));
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("配置保存失败: %1").arg(error));
        emit logMessage(QStringLiteral("配置保存失败: %1").arg(error), QStringLiteral("ERROR"));
    }
}

void SettingsWidget::onRpcConnected()
{
    updateConnectionStatus(true);
    emit logMessage(QStringLiteral("服务器连接成功"));
    emit connectionStatusChanged(true);
}

void SettingsWidget::onRpcDisconnected()
{
    updateConnectionStatus(false);
    emit logMessage(QStringLiteral("服务器连接已断开"), QStringLiteral("WARN"));
    emit connectionStatusChanged(false);
}

void SettingsWidget::onRpcError(const QString &error)
{
    emit logMessage(QStringLiteral("连接错误: %1").arg(error), QStringLiteral("ERROR"));
}

void SettingsWidget::updateConnectionStatus(bool connected)
{
    if (connected) {
        statusLabel_->setText(QStringLiteral("✅ 已连接到 %1:%2")
            .arg(rpcClient_->host()).arg(rpcClient_->port()));
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: 16px; padding: 12px; background-color: #d4edda; color: #155724; border-radius: 8px;"));
        connectButton_->setEnabled(false);
        disconnectButton_->setEnabled(true);
    } else {
        statusLabel_->setText(QStringLiteral("❌ 未连接"));
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: 16px; padding: 12px; background-color: #f8d7da; color: #721c24; border-radius: 8px;"));
        connectButton_->setEnabled(true);
        disconnectButton_->setEnabled(false);
    }
}
