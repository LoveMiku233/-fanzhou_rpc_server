/**
 * @file connection_widget.cpp
 * @brief 连接设置页面实现
 */

#include "connection_widget.h"
#include "rpc_client.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QDateTime>
#include <QSettings>
#include <QScrollArea>
#include <QTextCursor>

ConnectionWidget::ConnectionWidget(RpcClient *rpcClient, QWidget *parent)
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
    , logTextEdit_(nullptr)
{
    setupUi();

    // 连接RPC客户端信号
    connect(rpcClient_, &RpcClient::connected, this, &ConnectionWidget::onRpcConnected);
    connect(rpcClient_, &RpcClient::disconnected, this, &ConnectionWidget::onRpcDisconnected);
    connect(rpcClient_, &RpcClient::transportError, this, &ConnectionWidget::onRpcError);
    connect(rpcClient_, &RpcClient::logMessage, this, &ConnectionWidget::onRpcLogMessage);

    // 加载保存的设置
    QSettings settings;
    hostEdit_->setText(settings.value(QStringLiteral("connection/host"), QStringLiteral("127.0.0.1")).toString());
    portSpinBox_->setValue(settings.value(QStringLiteral("connection/port"), 12345).toInt());

    updateConnectionStatus(false);
}

void ConnectionWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(16);

    // 页面标题
    QLabel *titleLabel = new QLabel(QStringLiteral("连接设置"), this);
    titleLabel->setProperty("type", QStringLiteral("title"));
    titleLabel->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: bold; color: #2c3e50; padding: 8px 0;"));
    mainLayout->addWidget(titleLabel);

    // 服务器设置组
    QGroupBox *serverGroupBox = new QGroupBox(QStringLiteral("服务器设置"), this);
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

    // 连接按钮组
    QGroupBox *actionGroupBox = new QGroupBox(QStringLiteral("连接操作"), this);
    QGridLayout *actionLayout = new QGridLayout(actionGroupBox);
    actionLayout->setSpacing(12);
    actionLayout->setContentsMargins(16, 20, 16, 16);

    connectButton_ = new QPushButton(QStringLiteral("连接服务器"), this);
    connectButton_->setProperty("type", QStringLiteral("success"));
    connectButton_->setMinimumHeight(56);
    connect(connectButton_, &QPushButton::clicked, this, &ConnectionWidget::onConnect);
    actionLayout->addWidget(connectButton_, 0, 0);

    disconnectButton_ = new QPushButton(QStringLiteral("断开连接"), this);
    disconnectButton_->setProperty("type", QStringLiteral("danger"));
    disconnectButton_->setMinimumHeight(56);
    disconnectButton_->setEnabled(false);
    connect(disconnectButton_, &QPushButton::clicked, this, &ConnectionWidget::onDisconnect);
    actionLayout->addWidget(disconnectButton_, 0, 1);

    mainLayout->addWidget(actionGroupBox);

    // 工具按钮组
    QGroupBox *toolsGroupBox = new QGroupBox(QStringLiteral("工具"), this);
    QGridLayout *toolsLayout = new QGridLayout(toolsGroupBox);
    toolsLayout->setSpacing(12);
    toolsLayout->setContentsMargins(16, 20, 16, 16);

    pingButton_ = new QPushButton(QStringLiteral("Ping 测试"), this);
    pingButton_->setMinimumHeight(56);
    connect(pingButton_, &QPushButton::clicked, this, &ConnectionWidget::onPing);
    toolsLayout->addWidget(pingButton_, 0, 0);

    sysInfoButton_ = new QPushButton(QStringLiteral("系统信息"), this);
    sysInfoButton_->setMinimumHeight(56);
    connect(sysInfoButton_, &QPushButton::clicked, this, &ConnectionWidget::onSysInfo);
    toolsLayout->addWidget(sysInfoButton_, 0, 1);

    saveConfigButton_ = new QPushButton(QStringLiteral("保存配置"), this);
    saveConfigButton_->setProperty("type", QStringLiteral("warning"));
    saveConfigButton_->setMinimumHeight(56);
    connect(saveConfigButton_, &QPushButton::clicked, this, &ConnectionWidget::onSaveConfig);
    toolsLayout->addWidget(saveConfigButton_, 1, 0, 1, 2);

    mainLayout->addWidget(toolsGroupBox);

    // 状态标签
    statusLabel_ = new QLabel(QStringLiteral("状态: 未连接"), this);
    statusLabel_->setStyleSheet(QStringLiteral("font-size: 16px; padding: 8px; background-color: #f0f0f0; border-radius: 6px;"));
    mainLayout->addWidget(statusLabel_);

    // 日志显示
    QGroupBox *logGroupBox = new QGroupBox(QStringLiteral("通信日志"), this);
    QVBoxLayout *logLayout = new QVBoxLayout(logGroupBox);
    logLayout->setContentsMargins(8, 16, 8, 8);
    logLayout->setSpacing(8);

    logTextEdit_ = new QTextEdit(this);
    logTextEdit_->setReadOnly(true);
    logTextEdit_->setMinimumHeight(150);

    QPushButton *clearLogButton = new QPushButton(QStringLiteral("清空日志"), this);
    clearLogButton->setMinimumHeight(44);
    connect(clearLogButton, &QPushButton::clicked, logTextEdit_, &QTextEdit::clear);

    logLayout->addWidget(logTextEdit_);
    logLayout->addWidget(clearLogButton);

    mainLayout->addWidget(logGroupBox, 1);
}

QString ConnectionWidget::host() const
{
    return hostEdit_->text().trimmed();
}

quint16 ConnectionWidget::port() const
{
    return static_cast<quint16>(portSpinBox_->value());
}

void ConnectionWidget::onConnect()
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
    
    appendLog(QStringLiteral("正在连接到 %1:%2...").arg(host).arg(port));
    
    if (rpcClient_->connectToServer()) {
        // 连接成功后发送ping测试
        onPing();
    }
}

void ConnectionWidget::onDisconnect()
{
    rpcClient_->disconnectFromServer();
}

void ConnectionWidget::onPing()
{
    if (!rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QJsonValue result = rpcClient_->call(QStringLiteral("rpc.ping"));
    appendLog(QStringLiteral("Ping结果: %1").arg(
        QString::fromUtf8(QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact))));
}

void ConnectionWidget::onSysInfo()
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

void ConnectionWidget::onSaveConfig()
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

void ConnectionWidget::onRpcConnected()
{
    updateConnectionStatus(true);
    appendLog(QStringLiteral("[成功] 服务器连接成功"));
    emit connectionStatusChanged(true);
}

void ConnectionWidget::onRpcDisconnected()
{
    updateConnectionStatus(false);
    appendLog(QStringLiteral("[断开] 服务器连接已断开"));
    emit connectionStatusChanged(false);
}

void ConnectionWidget::onRpcError(const QString &error)
{
    appendLog(QStringLiteral("[错误] %1").arg(error));
}

void ConnectionWidget::onRpcLogMessage(const QString &message)
{
    appendLog(message);
}

void ConnectionWidget::updateConnectionStatus(bool connected)
{
    if (connected) {
        statusLabel_->setText(QStringLiteral("状态: 已连接到 %1:%2")
            .arg(rpcClient_->host()).arg(rpcClient_->port()));
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: 16px; padding: 8px; background-color: #d4edda; color: #155724; border-radius: 6px;"));
        connectButton_->setEnabled(false);
        disconnectButton_->setEnabled(true);
    } else {
        statusLabel_->setText(QStringLiteral("状态: 未连接"));
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: 16px; padding: 8px; background-color: #f8d7da; color: #721c24; border-radius: 6px;"));
        connectButton_->setEnabled(true);
        disconnectButton_->setEnabled(false);
    }
}

void ConnectionWidget::appendLog(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    logTextEdit_->append(QStringLiteral("[%1] %2").arg(timestamp, message));
    
    // 滚动到底部
    QTextCursor cursor = logTextEdit_->textCursor();
    cursor.movePosition(QTextCursor::End);
    logTextEdit_->setTextCursor(cursor);
}
