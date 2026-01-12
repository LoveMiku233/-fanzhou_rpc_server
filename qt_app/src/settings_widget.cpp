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
#include <QScrollArea>
#include <QTabWidget>
#include <QRegularExpression>
#include <QSlider>

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
    , networkInterfaceEdit_(nullptr)
    , ipAddressEdit_(nullptr)
    , netmaskEdit_(nullptr)
    , gatewayEdit_(nullptr)
    , networkStatusLabel_(nullptr)
    , mqttBrokerEdit_(nullptr)
    , mqttPortSpinBox_(nullptr)
    , mqttClientIdEdit_(nullptr)
    , mqttUsernameEdit_(nullptr)
    , mqttPasswordEdit_(nullptr)
    , mqttTopicEdit_(nullptr)
    , mqttEnabledCheckBox_(nullptr)
    , mqttStatusLabel_(nullptr)
    , brightnessSlider_(nullptr)
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
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // 页面标题
    QLabel *titleLabel = new QLabel(QStringLiteral("系统设置"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: 18px; font-weight: bold; color: #2c3e50; padding: 4px 0;"));
    mainLayout->addWidget(titleLabel);

    // 使用TabWidget组织设置页面
    QTabWidget *tabWidget = new QTabWidget(this);
    tabWidget->setStyleSheet(QStringLiteral(
        "QTabWidget::pane { border: 1px solid #ddd; border-radius: 4px; }"
        "QTabBar::tab { min-width: 80px; padding: 8px 12px; }"
        "QTabBar::tab:selected { background-color: #3498db; color: white; }"));

    // ==================== 连接设置标签页 ====================
    QWidget *connectionTab = new QWidget();
    QVBoxLayout *connLayout = new QVBoxLayout(connectionTab);
    connLayout->setContentsMargins(10, 10, 10, 10);
    connLayout->setSpacing(10);

    // 服务器连接设置组
    QGroupBox *serverGroupBox = new QGroupBox(QStringLiteral("RPC服务器"), connectionTab);
    QFormLayout *serverLayout = new QFormLayout(serverGroupBox);
    serverLayout->setSpacing(8);
    serverLayout->setContentsMargins(10, 14, 10, 10);

    hostEdit_ = new QLineEdit(connectionTab);
    hostEdit_->setPlaceholderText(QStringLiteral("192.168.1.100"));
    hostEdit_->setMinimumHeight(32);
    serverLayout->addRow(QStringLiteral("地址:"), hostEdit_);

    portSpinBox_ = new QSpinBox(connectionTab);
    portSpinBox_->setRange(1, 65535);
    portSpinBox_->setValue(12345);
    portSpinBox_->setMinimumHeight(32);
    serverLayout->addRow(QStringLiteral("端口:"), portSpinBox_);

    connLayout->addWidget(serverGroupBox);

    // 连接状态卡片
    statusLabel_ = new QLabel(QStringLiteral("状态: 未连接"), connectionTab);
    statusLabel_->setStyleSheet(QStringLiteral(
        "font-size: 13px; padding: 8px; background-color: #f8d7da; color: #721c24; border-radius: 6px;"));
    connLayout->addWidget(statusLabel_);

    // 连接操作按钮
    QHBoxLayout *connBtnLayout = new QHBoxLayout();
    connBtnLayout->setSpacing(8);

    connectButton_ = new QPushButton(QStringLiteral("连接"), connectionTab);
    connectButton_->setProperty("type", QStringLiteral("success"));
    connectButton_->setMinimumHeight(40);
    connect(connectButton_, &QPushButton::clicked, this, &SettingsWidget::onConnect);
    connBtnLayout->addWidget(connectButton_);

    disconnectButton_ = new QPushButton(QStringLiteral("断开"), connectionTab);
    disconnectButton_->setProperty("type", QStringLiteral("danger"));
    disconnectButton_->setMinimumHeight(40);
    disconnectButton_->setEnabled(false);
    connect(disconnectButton_, &QPushButton::clicked, this, &SettingsWidget::onDisconnect);
    connBtnLayout->addWidget(disconnectButton_);

    connLayout->addLayout(connBtnLayout);

    // 工具按钮组
    QGroupBox *toolsGroupBox = new QGroupBox(QStringLiteral("诊断工具"), connectionTab);
    QGridLayout *toolsLayout = new QGridLayout(toolsGroupBox);
    toolsLayout->setSpacing(8);
    toolsLayout->setContentsMargins(10, 14, 10, 10);

    pingButton_ = new QPushButton(QStringLiteral("Ping 测试"), connectionTab);
    pingButton_->setMinimumHeight(36);
    connect(pingButton_, &QPushButton::clicked, this, &SettingsWidget::onPing);
    toolsLayout->addWidget(pingButton_, 0, 0);

    sysInfoButton_ = new QPushButton(QStringLiteral("系统信息"), connectionTab);
    sysInfoButton_->setMinimumHeight(36);
    connect(sysInfoButton_, &QPushButton::clicked, this, &SettingsWidget::onSysInfo);
    toolsLayout->addWidget(sysInfoButton_, 0, 1);

    saveConfigButton_ = new QPushButton(QStringLiteral("保存服务器配置"), connectionTab);
    saveConfigButton_->setProperty("type", QStringLiteral("warning"));
    saveConfigButton_->setMinimumHeight(36);
    connect(saveConfigButton_, &QPushButton::clicked, this, &SettingsWidget::onSaveConfig);
    toolsLayout->addWidget(saveConfigButton_, 1, 0, 1, 2);

    connLayout->addWidget(toolsGroupBox);

    // 系统设置组
    QGroupBox *systemGroupBox = new QGroupBox(QStringLiteral("本地设置"), connectionTab);
    QFormLayout *systemLayout = new QFormLayout(systemGroupBox);
    systemLayout->setSpacing(8);
    systemLayout->setContentsMargins(10, 14, 10, 10);

    refreshIntervalSpinBox_ = new QSpinBox(connectionTab);
    refreshIntervalSpinBox_->setRange(1, 60);
    refreshIntervalSpinBox_->setValue(5);
    refreshIntervalSpinBox_->setSuffix(QStringLiteral(" 秒"));
    refreshIntervalSpinBox_->setMinimumHeight(32);
    connect(refreshIntervalSpinBox_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SettingsWidget::onRefreshIntervalChanged);
    systemLayout->addRow(QStringLiteral("刷新间隔:"), refreshIntervalSpinBox_);

    autoConnectCheckBox_ = new QCheckBox(QStringLiteral("启动时自动连接"), connectionTab);
    autoConnectCheckBox_->setMinimumHeight(28);
    connect(autoConnectCheckBox_, &QCheckBox::toggled,
            this, &SettingsWidget::onAutoConnectToggled);
    systemLayout->addRow(autoConnectCheckBox_);

    connLayout->addWidget(systemGroupBox);
    connLayout->addStretch();

    tabWidget->addTab(connectionTab, QStringLiteral("连接"));

    // ==================== 网络设置标签页 ====================
    QWidget *networkTab = new QWidget();
    QVBoxLayout *netLayout = new QVBoxLayout(networkTab);
    netLayout->setContentsMargins(10, 10, 10, 10);
    netLayout->setSpacing(10);

    // 网络状态
    networkStatusLabel_ = new QLabel(QStringLiteral("网络状态: 未获取"), networkTab);
    networkStatusLabel_->setStyleSheet(QStringLiteral(
        "font-size: 12px; padding: 8px; background-color: #e9ecef; border-radius: 6px;"));
    networkStatusLabel_->setWordWrap(true);
    netLayout->addWidget(networkStatusLabel_);

    // 网络配置组
    QGroupBox *netConfigBox = new QGroupBox(QStringLiteral("网络配置"), networkTab);
    QFormLayout *netFormLayout = new QFormLayout(netConfigBox);
    netFormLayout->setSpacing(8);
    netFormLayout->setContentsMargins(10, 14, 10, 10);

    networkInterfaceEdit_ = new QLineEdit(networkTab);
    networkInterfaceEdit_->setText(QStringLiteral("eth0"));
    networkInterfaceEdit_->setMinimumHeight(32);
    netFormLayout->addRow(QStringLiteral("网络接口:"), networkInterfaceEdit_);

    ipAddressEdit_ = new QLineEdit(networkTab);
    ipAddressEdit_->setPlaceholderText(QStringLiteral("192.168.1.100"));
    ipAddressEdit_->setMinimumHeight(32);
    netFormLayout->addRow(QStringLiteral("IP地址:"), ipAddressEdit_);

    netmaskEdit_ = new QLineEdit(networkTab);
    netmaskEdit_->setText(QStringLiteral("255.255.255.0"));
    netmaskEdit_->setMinimumHeight(32);
    netFormLayout->addRow(QStringLiteral("子网掩码:"), netmaskEdit_);

    gatewayEdit_ = new QLineEdit(networkTab);
    gatewayEdit_->setPlaceholderText(QStringLiteral("192.168.1.1"));
    gatewayEdit_->setMinimumHeight(32);
    netFormLayout->addRow(QStringLiteral("网关:"), gatewayEdit_);

    netLayout->addWidget(netConfigBox);

    // 网络操作按钮
    QHBoxLayout *netBtnLayout = new QHBoxLayout();
    netBtnLayout->setSpacing(8);

    QPushButton *getNetInfoBtn = new QPushButton(QStringLiteral("获取网络信息"), networkTab);
    getNetInfoBtn->setMinimumHeight(40);
    connect(getNetInfoBtn, &QPushButton::clicked, this, &SettingsWidget::onGetNetworkInfo);
    netBtnLayout->addWidget(getNetInfoBtn);

    QPushButton *setStaticIpBtn = new QPushButton(QStringLiteral("设置静态IP"), networkTab);
    setStaticIpBtn->setProperty("type", QStringLiteral("warning"));
    setStaticIpBtn->setMinimumHeight(40);
    connect(setStaticIpBtn, &QPushButton::clicked, this, &SettingsWidget::onSetStaticIp);
    netBtnLayout->addWidget(setStaticIpBtn);

    QPushButton *enableDhcpBtn = new QPushButton(QStringLiteral("启用DHCP"), networkTab);
    enableDhcpBtn->setProperty("type", QStringLiteral("success"));
    enableDhcpBtn->setMinimumHeight(40);
    connect(enableDhcpBtn, &QPushButton::clicked, this, &SettingsWidget::onEnableDhcp);
    netBtnLayout->addWidget(enableDhcpBtn);

    netLayout->addLayout(netBtnLayout);
    netLayout->addStretch();

    tabWidget->addTab(networkTab, QStringLiteral("网络"));

    // ==================== MQTT云平台标签页 ====================
    QWidget *mqttTab = new QWidget();
    QVBoxLayout *mqttLayout = new QVBoxLayout(mqttTab);
    mqttLayout->setContentsMargins(10, 10, 10, 10);
    mqttLayout->setSpacing(10);

    // MQTT状态
    mqttStatusLabel_ = new QLabel(QStringLiteral("MQTT状态: 未配置"), mqttTab);
    mqttStatusLabel_->setStyleSheet(QStringLiteral(
        "font-size: 12px; padding: 8px; background-color: #e9ecef; border-radius: 6px;"));
    mqttLayout->addWidget(mqttStatusLabel_);

    // MQTT配置组
    QGroupBox *mqttConfigBox = new QGroupBox(QStringLiteral("MQTT服务器配置"), mqttTab);
    QFormLayout *mqttFormLayout = new QFormLayout(mqttConfigBox);
    mqttFormLayout->setSpacing(8);
    mqttFormLayout->setContentsMargins(10, 14, 10, 10);

    mqttEnabledCheckBox_ = new QCheckBox(QStringLiteral("启用MQTT云平台"), mqttTab);
    mqttEnabledCheckBox_->setMinimumHeight(28);
    mqttFormLayout->addRow(mqttEnabledCheckBox_);

    mqttBrokerEdit_ = new QLineEdit(mqttTab);
    mqttBrokerEdit_->setPlaceholderText(QStringLiteral("mqtt.example.com"));
    mqttBrokerEdit_->setMinimumHeight(32);
    mqttFormLayout->addRow(QStringLiteral("Broker地址:"), mqttBrokerEdit_);

    mqttPortSpinBox_ = new QSpinBox(mqttTab);
    mqttPortSpinBox_->setRange(1, 65535);
    mqttPortSpinBox_->setValue(1883);
    mqttPortSpinBox_->setMinimumHeight(32);
    mqttFormLayout->addRow(QStringLiteral("端口:"), mqttPortSpinBox_);

    mqttClientIdEdit_ = new QLineEdit(mqttTab);
    mqttClientIdEdit_->setPlaceholderText(QStringLiteral("fanzhou_device_001"));
    mqttClientIdEdit_->setMinimumHeight(32);
    mqttFormLayout->addRow(QStringLiteral("Client ID:"), mqttClientIdEdit_);

    mqttUsernameEdit_ = new QLineEdit(mqttTab);
    mqttUsernameEdit_->setMinimumHeight(32);
    mqttFormLayout->addRow(QStringLiteral("用户名:"), mqttUsernameEdit_);

    mqttPasswordEdit_ = new QLineEdit(mqttTab);
    mqttPasswordEdit_->setEchoMode(QLineEdit::Password);
    mqttPasswordEdit_->setMinimumHeight(32);
    mqttFormLayout->addRow(QStringLiteral("密码:"), mqttPasswordEdit_);

    mqttTopicEdit_ = new QLineEdit(mqttTab);
    mqttTopicEdit_->setPlaceholderText(QStringLiteral("fanzhou/device/status"));
    mqttTopicEdit_->setMinimumHeight(32);
    mqttFormLayout->addRow(QStringLiteral("主题:"), mqttTopicEdit_);

    mqttLayout->addWidget(mqttConfigBox);

    // MQTT操作按钮
    QHBoxLayout *mqttBtnLayout = new QHBoxLayout();
    mqttBtnLayout->setSpacing(8);

    QPushButton *getMqttBtn = new QPushButton(QStringLiteral("读取配置"), mqttTab);
    getMqttBtn->setMinimumHeight(40);
    connect(getMqttBtn, &QPushButton::clicked, this, &SettingsWidget::onGetMqttConfig);
    mqttBtnLayout->addWidget(getMqttBtn);

    QPushButton *setMqttBtn = new QPushButton(QStringLiteral("保存配置"), mqttTab);
    setMqttBtn->setProperty("type", QStringLiteral("success"));
    setMqttBtn->setMinimumHeight(40);
    connect(setMqttBtn, &QPushButton::clicked, this, &SettingsWidget::onSetMqttConfig);
    mqttBtnLayout->addWidget(setMqttBtn);

    QPushButton *testMqttBtn = new QPushButton(QStringLiteral("测试连接"), mqttTab);
    testMqttBtn->setProperty("type", QStringLiteral("warning"));
    testMqttBtn->setMinimumHeight(40);
    connect(testMqttBtn, &QPushButton::clicked, this, &SettingsWidget::onTestMqtt);
    mqttBtnLayout->addWidget(testMqttBtn);

    mqttLayout->addLayout(mqttBtnLayout);
    mqttLayout->addStretch();

    tabWidget->addTab(mqttTab, QStringLiteral("云平台"));

    // ==================== 系统控制标签页 ====================
    QWidget *systemTab = new QWidget();
    QVBoxLayout *sysLayout = new QVBoxLayout(systemTab);
    sysLayout->setContentsMargins(10, 10, 10, 10);
    sysLayout->setSpacing(10);

    // 屏幕亮度控制组
    QGroupBox *screenGroupBox = new QGroupBox(QStringLiteral("屏幕设置"), systemTab);
    QFormLayout *screenLayout = new QFormLayout(screenGroupBox);
    screenLayout->setSpacing(8);
    screenLayout->setContentsMargins(10, 14, 10, 10);

    brightnessSlider_ = new QSlider(Qt::Horizontal, systemTab);
    brightnessSlider_->setRange(0, 100);
    brightnessSlider_->setValue(80);
    brightnessSlider_->setMinimumHeight(32);
    screenLayout->addRow(QStringLiteral("亮度:"), brightnessSlider_);

    QHBoxLayout *screenBtnLayout = new QHBoxLayout();
    QPushButton *getBrightnessBtn = new QPushButton(QStringLiteral("读取亮度"), systemTab);
    getBrightnessBtn->setMinimumHeight(36);
    connect(getBrightnessBtn, &QPushButton::clicked, this, &SettingsWidget::onGetBrightness);
    screenBtnLayout->addWidget(getBrightnessBtn);

    QPushButton *setBrightnessBtn = new QPushButton(QStringLiteral("设置亮度"), systemTab);
    setBrightnessBtn->setProperty("type", QStringLiteral("success"));
    setBrightnessBtn->setMinimumHeight(36);
    connect(setBrightnessBtn, &QPushButton::clicked, this, &SettingsWidget::onSetBrightness);
    screenBtnLayout->addWidget(setBrightnessBtn);

    screenLayout->addRow(screenBtnLayout);
    sysLayout->addWidget(screenGroupBox);

    // 系统操作组
    QGroupBox *sysOpGroupBox = new QGroupBox(QStringLiteral("系统操作"), systemTab);
    QVBoxLayout *sysOpLayout = new QVBoxLayout(sysOpGroupBox);
    sysOpLayout->setSpacing(8);
    sysOpLayout->setContentsMargins(10, 14, 10, 10);

    QLabel *warningLabel = new QLabel(
        QStringLiteral("⚠️ 以下操作需要管理员权限，请谨慎使用"), systemTab);
    warningLabel->setStyleSheet(QStringLiteral(
        "color: #856404; font-size: 12px; padding: 8px; "
        "background-color: #fff3cd; border-radius: 6px;"));
    sysOpLayout->addWidget(warningLabel);

    QHBoxLayout *sysOpBtnLayout = new QHBoxLayout();
    
    QPushButton *rebootBtn = new QPushButton(QStringLiteral("🔄 重启系统"), systemTab);
    rebootBtn->setProperty("type", QStringLiteral("warning"));
    rebootBtn->setMinimumHeight(44);
    connect(rebootBtn, &QPushButton::clicked, this, &SettingsWidget::onRebootSystem);
    sysOpBtnLayout->addWidget(rebootBtn);

    QPushButton *shutdownBtn = new QPushButton(QStringLiteral("⏻ 关闭系统"), systemTab);
    shutdownBtn->setProperty("type", QStringLiteral("danger"));
    shutdownBtn->setMinimumHeight(44);
    connect(shutdownBtn, &QPushButton::clicked, this, &SettingsWidget::onShutdownSystem);
    sysOpBtnLayout->addWidget(shutdownBtn);

    sysOpLayout->addLayout(sysOpBtnLayout);
    sysLayout->addWidget(sysOpGroupBox);

    sysLayout->addStretch();
    tabWidget->addTab(systemTab, QStringLiteral("系统"));

    mainLayout->addWidget(tabWidget, 1);
}

void SettingsWidget::onRefreshIntervalChanged(int value)
{
    QSettings settings;
    settings.setValue(QStringLiteral("settings/refreshInterval"), value);
}

void SettingsWidget::onAutoConnectToggled(bool checked)
{
    QSettings settings;
    settings.setValue(QStringLiteral("settings/autoConnect"), checked);
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
        statusLabel_->setText(QStringLiteral("[OK] 已连接到 %1:%2")
            .arg(rpcClient_->host()).arg(rpcClient_->port()));
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: 13px; padding: 8px; background-color: #d4edda; color: #155724; border-radius: 6px;"));
        connectButton_->setEnabled(false);
        disconnectButton_->setEnabled(true);
    } else {
        statusLabel_->setText(QStringLiteral("[X] 未连接"));
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: 13px; padding: 8px; background-color: #f8d7da; color: #721c24; border-radius: 6px;"));
        connectButton_->setEnabled(true);
        disconnectButton_->setEnabled(false);
    }
}

// ==================== 网络设置槽函数 ====================

void SettingsWidget::onGetNetworkInfo()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QString interface = networkInterfaceEdit_->text().trimmed();
    QJsonObject params;
    if (!interface.isEmpty()) {
        params[QStringLiteral("interface")] = interface;
    }

    QJsonValue result = rpcClient_->call(QStringLiteral("sys.network.infoDetailed"), params);

    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.value(QStringLiteral("ok")).toBool()) {
            QString infoText;
            
            // 接口列表
            QString interfaces = obj.value(QStringLiteral("interfaces")).toString();
            if (!interfaces.isEmpty()) {
                infoText += QStringLiteral("📡 接口: %1\n").arg(interfaces.replace(QStringLiteral("\n"), QStringLiteral(" ")));
            }
            
            // 接口状态
            QString state = obj.value(QStringLiteral("state")).toString();
            if (!state.isEmpty()) {
                QString stateIcon = state.contains(QStringLiteral("up")) ? QStringLiteral("🟢") : QStringLiteral("🔴");
                infoText += QStringLiteral("%1 状态: %2\n").arg(stateIcon, state);
            }
            
            // MAC地址
            QString mac = obj.value(QStringLiteral("mac")).toString();
            if (!mac.isEmpty()) {
                infoText += QStringLiteral("🔗 MAC: %1\n").arg(mac);
            }
            
            // IP地址信息（从ipAddr中提取）
            QString ipAddr = obj.value(QStringLiteral("ipAddr")).toString();
            if (!ipAddr.isEmpty()) {
                // 尝试提取IPv4地址
                QRegularExpression ipRegex(QStringLiteral("inet\\s+(\\d+\\.\\d+\\.\\d+\\.\\d+)"));
                QRegularExpressionMatchIterator it = ipRegex.globalMatch(ipAddr);
                QStringList ips;
                while (it.hasNext()) {
                    QRegularExpressionMatch match = it.next();
                    QString ip = match.captured(1);
                    if (!ip.startsWith(QStringLiteral("127."))) {  // 排除回环地址
                        ips << ip;
                    }
                }
                if (!ips.isEmpty()) {
                    infoText += QStringLiteral("🌐 IP: %1\n").arg(ips.join(QStringLiteral(", ")));
                }
            }
            
            // 路由信息（提取默认网关）
            QString routes = obj.value(QStringLiteral("routes")).toString();
            if (!routes.isEmpty()) {
                QRegularExpression gwRegex(QStringLiteral("default via (\\d+\\.\\d+\\.\\d+\\.\\d+)"));
                QRegularExpressionMatch gwMatch = gwRegex.match(routes);
                if (gwMatch.hasMatch()) {
                    infoText += QStringLiteral("🚪 网关: %1\n").arg(gwMatch.captured(1));
                }
            }
            
            // DNS信息
            QString dns = obj.value(QStringLiteral("dns")).toString();
            if (!dns.isEmpty()) {
                QRegularExpression dnsRegex(QStringLiteral("nameserver\\s+(\\S+)"));
                QRegularExpressionMatchIterator dnsIt = dnsRegex.globalMatch(dns);
                QStringList dnsServers;
                while (dnsIt.hasNext()) {
                    dnsServers << dnsIt.next().captured(1);
                }
                if (!dnsServers.isEmpty()) {
                    infoText += QStringLiteral("🔍 DNS: %1").arg(dnsServers.join(QStringLiteral(", ")));
                }
            }
            
            if (infoText.isEmpty()) {
                infoText = QStringLiteral("未能获取网络详细信息");
            }
            
            networkStatusLabel_->setText(infoText);
            networkStatusLabel_->setStyleSheet(QStringLiteral(
                "font-size: 12px; padding: 8px; background-color: #d4edda; color: #155724; border-radius: 6px;"));
            emit logMessage(QStringLiteral("获取网络信息成功"));
        } else {
            QString error = obj.value(QStringLiteral("error")).toString();
            networkStatusLabel_->setText(QStringLiteral("获取网络信息失败: %1").arg(error));
            networkStatusLabel_->setStyleSheet(QStringLiteral(
                "font-size: 12px; padding: 8px; background-color: #f8d7da; color: #721c24; border-radius: 6px;"));
        }
    } else {
        networkStatusLabel_->setText(QStringLiteral("获取网络信息失败: 返回格式错误"));
        networkStatusLabel_->setStyleSheet(QStringLiteral(
            "font-size: 12px; padding: 8px; background-color: #f8d7da; color: #721c24; border-radius: 6px;"));
    }
}

void SettingsWidget::onSetStaticIp()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QString interface = networkInterfaceEdit_->text().trimmed();
    QString address = ipAddressEdit_->text().trimmed();
    QString netmask = netmaskEdit_->text().trimmed();
    QString gateway = gatewayEdit_->text().trimmed();

    if (interface.isEmpty() || address.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请输入接口名和IP地址"));
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        QStringLiteral("确认"),
        QStringLiteral("确定要设置静态IP吗？\n接口: %1\nIP: %2\n子网掩码: %3\n网关: %4")
            .arg(interface, address, netmask, gateway),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    QJsonObject params;
    params[QStringLiteral("interface")] = interface;
    params[QStringLiteral("address")] = address;
    if (!netmask.isEmpty()) params[QStringLiteral("netmask")] = netmask;
    if (!gateway.isEmpty()) params[QStringLiteral("gateway")] = gateway;

    QJsonValue result = rpcClient_->call(QStringLiteral("sys.network.setStaticIp"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("静态IP设置成功！"));
        emit logMessage(QStringLiteral("静态IP设置成功"));
        onGetNetworkInfo();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("设置失败: %1").arg(error));
        emit logMessage(QStringLiteral("静态IP设置失败: %1").arg(error), QStringLiteral("ERROR"));
    }
}

void SettingsWidget::onEnableDhcp()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QString interface = networkInterfaceEdit_->text().trimmed();
    if (interface.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请输入网络接口名"));
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        QStringLiteral("确认"),
        QStringLiteral("确定要在接口 %1 上启用DHCP吗？").arg(interface),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    QJsonObject params;
    params[QStringLiteral("interface")] = interface;

    QJsonValue result = rpcClient_->call(QStringLiteral("sys.network.enableDhcp"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("DHCP已启用！"));
        emit logMessage(QStringLiteral("DHCP启用成功"));
        onGetNetworkInfo();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("启用DHCP失败: %1").arg(error));
        emit logMessage(QStringLiteral("启用DHCP失败: %1").arg(error), QStringLiteral("ERROR"));
    }
}

// ==================== MQTT云平台槽函数 ====================

void SettingsWidget::onGetMqttConfig()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QJsonValue result = rpcClient_->call(QStringLiteral("cloud.mqtt.get"));

    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.value(QStringLiteral("ok")).toBool()) {
            mqttEnabledCheckBox_->setChecked(obj.value(QStringLiteral("enabled")).toBool());
            mqttBrokerEdit_->setText(obj.value(QStringLiteral("broker")).toString());
            mqttPortSpinBox_->setValue(obj.value(QStringLiteral("port")).toInt(1883));
            mqttClientIdEdit_->setText(obj.value(QStringLiteral("clientId")).toString());
            mqttUsernameEdit_->setText(obj.value(QStringLiteral("username")).toString());
            mqttTopicEdit_->setText(obj.value(QStringLiteral("topic")).toString());

            bool connected = obj.value(QStringLiteral("connected")).toBool();
            bool enabled = obj.value(QStringLiteral("enabled")).toBool();
            if (enabled && connected) {
                mqttStatusLabel_->setText(QStringLiteral("MQTT状态: 已连接"));
                mqttStatusLabel_->setStyleSheet(QStringLiteral(
                    "font-size: 12px; padding: 8px; background-color: #d4edda; color: #155724; border-radius: 6px;"));
            } else if (enabled) {
                mqttStatusLabel_->setText(QStringLiteral("MQTT状态: 已启用但未连接"));
                mqttStatusLabel_->setStyleSheet(QStringLiteral(
                    "font-size: 12px; padding: 8px; background-color: #fff3cd; color: #856404; border-radius: 6px;"));
            } else {
                mqttStatusLabel_->setText(QStringLiteral("MQTT状态: 未启用"));
                mqttStatusLabel_->setStyleSheet(QStringLiteral(
                    "font-size: 12px; padding: 8px; background-color: #e9ecef; color: #495057; border-radius: 6px;"));
            }

            emit logMessage(QStringLiteral("MQTT配置读取成功"));
        }
    }
}

void SettingsWidget::onSetMqttConfig()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QJsonObject params;
    params[QStringLiteral("enabled")] = mqttEnabledCheckBox_->isChecked();
    params[QStringLiteral("broker")] = mqttBrokerEdit_->text().trimmed();
    params[QStringLiteral("port")] = mqttPortSpinBox_->value();
    params[QStringLiteral("clientId")] = mqttClientIdEdit_->text().trimmed();
    params[QStringLiteral("username")] = mqttUsernameEdit_->text().trimmed();
    params[QStringLiteral("password")] = mqttPasswordEdit_->text();
    params[QStringLiteral("topic")] = mqttTopicEdit_->text().trimmed();

    QJsonValue result = rpcClient_->call(QStringLiteral("cloud.mqtt.set"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("MQTT配置保存成功！"));
        emit logMessage(QStringLiteral("MQTT配置保存成功"));
        onGetMqttConfig();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("保存MQTT配置失败: %1").arg(error));
        emit logMessage(QStringLiteral("保存MQTT配置失败: %1").arg(error), QStringLiteral("ERROR"));
    }
}

void SettingsWidget::onTestMqtt()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QJsonValue result = rpcClient_->call(QStringLiteral("cloud.mqtt.test"));

    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        QString message = obj.value(QStringLiteral("message")).toString();
        QString broker = obj.value(QStringLiteral("broker")).toString();
        int port = obj.value(QStringLiteral("port")).toInt();

        QMessageBox::information(this, QStringLiteral("MQTT测试"),
            QStringLiteral("Broker: %1:%2\n\n%3").arg(broker).arg(port).arg(message));
        emit logMessage(QStringLiteral("MQTT测试: %1").arg(message));
    }
}

// ==================== 系统控制槽函数 ====================

void SettingsWidget::onGetBrightness()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QJsonValue result = rpcClient_->call(QStringLiteral("screen.brightness.get"));

    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.value(QStringLiteral("ok")).toBool()) {
            int brightness = obj.value(QStringLiteral("brightness")).toInt();
            brightnessSlider_->setValue(brightness);
            emit logMessage(QStringLiteral("获取亮度成功: %1%").arg(brightness));
        } else {
            QString error = obj.value(QStringLiteral("error")).toString();
            emit logMessage(QStringLiteral("获取亮度失败: %1").arg(error), QStringLiteral("ERROR"));
        }
    }
}

void SettingsWidget::onSetBrightness()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    int brightness = brightnessSlider_->value();

    QJsonObject params;
    params[QStringLiteral("brightness")] = brightness;

    QJsonValue result = rpcClient_->call(QStringLiteral("screen.brightness.set"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        emit logMessage(QStringLiteral("设置亮度成功: %1%").arg(brightness));
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("设置亮度失败: %1").arg(error));
        emit logMessage(QStringLiteral("设置亮度失败: %1").arg(error), QStringLiteral("ERROR"));
    }
}

void SettingsWidget::onRebootSystem()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::warning(this,
        QStringLiteral("确认重启"),
        QStringLiteral("确定要重启系统吗？\n\n设备将在几秒后重新启动，请稍后重新连接。"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    QJsonValue result = rpcClient_->call(QStringLiteral("sys.reboot"));

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        QMessageBox::information(this, QStringLiteral("重启中"), 
            QStringLiteral("系统正在重启，请稍后重新连接..."));
        emit logMessage(QStringLiteral("系统重启命令已发送"));
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("重启失败: %1").arg(error));
    }
}

void SettingsWidget::onShutdownSystem()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::critical(this,
        QStringLiteral("确认关机"),
        QStringLiteral("确定要关闭系统吗？\n\n⚠️ 关机后需要手动重新上电才能启动设备！"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    QJsonValue result = rpcClient_->call(QStringLiteral("sys.shutdown"));

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        QMessageBox::information(this, QStringLiteral("关机中"), 
            QStringLiteral("系统正在关机..."));
        emit logMessage(QStringLiteral("系统关机命令已发送"));
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("关机失败: %1").arg(error));
    }
}
