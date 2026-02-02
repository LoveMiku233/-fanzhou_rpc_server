/**
 * @file home_widget.cpp
 * @brief 主页实现 - 大棚控制系统总览（增强版）
 */

#include "home_widget.h"
#include "rpc_client.h"

#include <QVBoxLayout>
#include <QDebug>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QFrame>
#include <QMessageBox>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QScrollArea>

HomeWidget::HomeWidget(RpcClient *rpcClient, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpcClient)
    , autoRefreshTimer_(new QTimer(this))
    , totalDevicesLabel_(nullptr)
    , onlineDevicesLabel_(nullptr)
    , offlineDevicesLabel_(nullptr)
    , totalGroupsLabel_(nullptr)
    , totalStrategiesLabel_(nullptr)
    , totalSensorsLabel_(nullptr)
    , canStatusLabel_(nullptr)
    , mqttStatusLabel_(nullptr)
    , connectionStatusLabel_(nullptr)
    , systemUptimeLabel_(nullptr)
    , lastUpdateLabel_(nullptr)
    , refreshButton_(nullptr)
    , stopAllButton_(nullptr)
    , emergencyStopButton_(nullptr)
{
    setupUi();
    
    // 自动刷新定时器 - 每5秒刷新一次
    connect(autoRefreshTimer_, &QTimer::timeout, this, &HomeWidget::onAutoRefreshTimeout);
    autoRefreshTimer_->start(5000);
    
    qDebug() << "[HOME_WIDGET] 主页初始化完成";
}

void HomeWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(16);

    // 页面标题
    QLabel *titleLabel = new QLabel(QStringLiteral("大棚控制系统总览"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: 28px; font-weight: bold; color: #27ae60; padding: 8px 0;"));
    mainLayout->addWidget(titleLabel);

    // 连接状态卡片
    QFrame *statusCard = new QFrame(this);
    statusCard->setObjectName(QStringLiteral("statusCard"));
    statusCard->setStyleSheet(QStringLiteral(
        "#statusCard { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ecf0f1, stop:1 #d5dbdb); "
        "border-radius: 12px; padding: 12px; border: 1px solid #bdc3c7; }"));
    
    QGraphicsDropShadowEffect *statusShadow = new QGraphicsDropShadowEffect(this);
    statusShadow->setBlurRadius(10);
    statusShadow->setColor(QColor(0, 0, 0, 30));
    statusShadow->setOffset(0, 3);
    statusCard->setGraphicsEffect(statusShadow);
    
    QHBoxLayout *statusLayout = new QHBoxLayout(statusCard);
    statusLayout->setContentsMargins(16, 12, 16, 12);
    
    connectionStatusLabel_ = new QLabel(QStringLiteral("未连接"), this);
    connectionStatusLabel_->setStyleSheet(QStringLiteral(
        "font-size: 18px; font-weight: bold; color: #e74c3c;"));
    statusLayout->addWidget(connectionStatusLabel_);
    statusLayout->addStretch();
    
    systemUptimeLabel_ = new QLabel(QStringLiteral("运行时间: --"), this);
    systemUptimeLabel_->setStyleSheet(QStringLiteral(
        "font-size: 13px; color: #5d6d7e; padding: 6px 12px; background-color: white; border-radius: 6px;"));
    statusLayout->addWidget(systemUptimeLabel_);
    
    mainLayout->addWidget(statusCard);

    // 统计信息卡片网格 - 2行4列
    QGridLayout *statsGrid = new QGridLayout();
    statsGrid->setSpacing(12);

    auto createStatCard = [this](const QString &title, const QString &bgColor) -> QPair<QFrame*, QLabel*> {
        QFrame *card = new QFrame(this);
        QString darkerBg = bgColor;
        darkerBg.replace(1, 1, QStringLiteral("d"));
        card->setStyleSheet(QStringLiteral(
            "QFrame { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2); "
            "border-radius: 12px; padding: 12px; }").arg(bgColor, darkerBg));
        
        QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(10);
        shadow->setColor(QColor(0, 0, 0, 35));
        shadow->setOffset(0, 3);
        card->setGraphicsEffect(shadow);
        
        QVBoxLayout *layout = new QVBoxLayout(card);
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(6);
        
        QLabel *titleLabel = new QLabel(title, card);
        titleLabel->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.85); font-size: 12px;"));
        layout->addWidget(titleLabel);
        
        QLabel *valueLabel = new QLabel(QStringLiteral("--"), card);
        valueLabel->setStyleSheet(QStringLiteral(
            "color: white; font-size: 32px; font-weight: bold; background: transparent;"));
        layout->addWidget(valueLabel);
        
        return qMakePair(card, valueLabel);
    };

    // 第1行: 设备统计
    auto deviceCard = createStatCard(QStringLiteral("设备总数"), QStringLiteral("#3498db"));
    totalDevicesLabel_ = deviceCard.second;
    statsGrid->addWidget(deviceCard.first, 0, 0);

    auto onlineCard = createStatCard(QStringLiteral("在线设备"), QStringLiteral("#27ae60"));
    onlineDevicesLabel_ = onlineCard.second;
    statsGrid->addWidget(onlineCard.first, 0, 1);

    auto offlineCard = createStatCard(QStringLiteral("离线设备"), QStringLiteral("#e74c3c"));
    offlineDevicesLabel_ = offlineCard.second;
    statsGrid->addWidget(offlineCard.first, 0, 2);

    auto groupCard = createStatCard(QStringLiteral("分组数量"), QStringLiteral("#9b59b6"));
    totalGroupsLabel_ = groupCard.second;
    statsGrid->addWidget(groupCard.first, 0, 3);

    // 第2行: 策略和系统状态
    auto strategyCard = createStatCard(QStringLiteral("策略数量"), QStringLiteral("#e67e22"));
    totalStrategiesLabel_ = strategyCard.second;
    statsGrid->addWidget(strategyCard.first, 1, 0);

    auto sensorCard = createStatCard(QStringLiteral("传感器数量"), QStringLiteral("#1abc9c"));
    totalSensorsLabel_ = sensorCard.second;
    statsGrid->addWidget(sensorCard.first, 1, 1);

    auto canCard = createStatCard(QStringLiteral("CAN状态"), QStringLiteral("#34495e"));
    canStatusLabel_ = canCard.second;
    statsGrid->addWidget(canCard.first, 1, 2);

    auto mqttCard = createStatCard(QStringLiteral("MQTT状态"), QStringLiteral("#16a085"));
    mqttStatusLabel_ = mqttCard.second;
    statsGrid->addWidget(mqttCard.first, 1, 3);

    mainLayout->addLayout(statsGrid);

    // 快捷操作区
    QGroupBox *actionsBox = new QGroupBox(QStringLiteral("快捷操作"), this);
    actionsBox->setStyleSheet(QStringLiteral(
        "QGroupBox { font-weight: bold; font-size: 14px; border: 2px solid #e0e0e0; border-radius: 12px; margin-top: 12px; padding-top: 14px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 10px; color: #3498db; }"));
    
    QHBoxLayout *actionsLayout = new QHBoxLayout(actionsBox);
    actionsLayout->setSpacing(12);
    actionsLayout->setContentsMargins(16, 16, 16, 16);

    refreshButton_ = new QPushButton(QStringLiteral("刷新数据"), this);
    refreshButton_->setMinimumHeight(48);
    refreshButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #3498db; color: white; border: none; "
        "border-radius: 10px; padding: 0 24px; font-weight: bold; font-size: 14px; }"
        "QPushButton:hover { background-color: #2980b9; }"
        "QPushButton:pressed { background-color: #1c5a8a; }"));
    connect(refreshButton_, &QPushButton::clicked, this, &HomeWidget::refreshData);
    actionsLayout->addWidget(refreshButton_);

    stopAllButton_ = new QPushButton(QStringLiteral("全部停止"), this);
    stopAllButton_->setMinimumHeight(48);
    stopAllButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #f39c12; color: white; border: none; "
        "border-radius: 10px; padding: 0 24px; font-weight: bold; font-size: 14px; }"
        "QPushButton:hover { background-color: #d68910; }"
        "QPushButton:pressed { background-color: #b7791f; }"));
    connect(stopAllButton_, &QPushButton::clicked, this, &HomeWidget::onStopAllClicked);
    actionsLayout->addWidget(stopAllButton_);
    
    actionsLayout->addStretch();
    
    mainLayout->addWidget(actionsBox);

    // 急停按钮
    emergencyStopButton_ = new QPushButton(QStringLiteral("紧急停止"), this);
    emergencyStopButton_->setMinimumHeight(70);
    emergencyStopButton_->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e74c3c, stop:1 #c0392b);"
        "  color: white;"
        "  font-size: 22px;"
        "  font-weight: bold;"
        "  border: 3px solid #922b21;"
        "  border-radius: 12px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #c0392b, stop:1 #a93226);"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #922b21, stop:1 #7b241c);"
        "}"
    ));
    connect(emergencyStopButton_, &QPushButton::clicked, this, &HomeWidget::onEmergencyStopClicked);
    mainLayout->addWidget(emergencyStopButton_);

    // 最后更新时间
    lastUpdateLabel_ = new QLabel(QStringLiteral("最后更新: --"), this);
    lastUpdateLabel_->setStyleSheet(QStringLiteral(
        "color: #7f8c8d; font-size: 12px; padding: 8px;"));
    lastUpdateLabel_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(lastUpdateLabel_);

    mainLayout->addStretch();
}

void HomeWidget::onStopAllClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }
    
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        QStringLiteral("确认"),
        QStringLiteral("确定要停止所有设备吗？"),
        QMessageBox::Yes | QMessageBox::No);
        
    if (reply == QMessageBox::Yes) {
        qDebug() << "[HOME_WIDGET] 执行全部停止";
        QJsonValue result = rpcClient_->call(QStringLiteral("relay.emergencyStop"));
        qDebug() << "[HOME_WIDGET] 全部停止结果:" << QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact);
    }
}

void HomeWidget::onEmergencyStopClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    qDebug() << "[HOME_WIDGET] 执行紧急停止";
    
    QJsonValue result = rpcClient_->call(QStringLiteral("relay.emergencyStop"));
    
    qDebug() << "[HOME_WIDGET] 紧急停止结果:" << QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact);

    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.value(QStringLiteral("ok")).toBool()) {
            int stopped = obj.value(QStringLiteral("stoppedChannels")).toInt();
            int devices = obj.value(QStringLiteral("deviceCount")).toInt();
            QMessageBox::information(this, QStringLiteral("急停执行完成"),
                QStringLiteral("已停止 %1 个设备的 %2 个通道").arg(devices).arg(stopped));
        } else {
            QMessageBox::warning(this, QStringLiteral("急停执行失败"),
                QStringLiteral("执行急停命令时发生错误"));
        }
    }
}

void HomeWidget::onAutoRefreshTimeout()
{
    if (rpcClient_ && rpcClient_->isConnected()) {
        updateStats();
    }
}

void HomeWidget::refreshData()
{
    qDebug() << "[HOME_WIDGET] 刷新数据";
    updateStats();
}

void HomeWidget::updateStats()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        connectionStatusLabel_->setText(QStringLiteral("未连接"));
        connectionStatusLabel_->setStyleSheet(QStringLiteral(
            "font-size: 18px; font-weight: bold; color: #e74c3c;"));
        totalDevicesLabel_->setText(QStringLiteral("--"));
        onlineDevicesLabel_->setText(QStringLiteral("--"));
        offlineDevicesLabel_->setText(QStringLiteral("--"));
        totalGroupsLabel_->setText(QStringLiteral("--"));
        totalStrategiesLabel_->setText(QStringLiteral("--"));
        totalSensorsLabel_->setText(QStringLiteral("--"));
        canStatusLabel_->setText(QStringLiteral("--"));
        mqttStatusLabel_->setText(QStringLiteral("--"));
        systemUptimeLabel_->setText(QStringLiteral("运行时间: --"));
        return;
    }

    connectionStatusLabel_->setText(QStringLiteral("已连接 %1:%2")
        .arg(rpcClient_->host()).arg(rpcClient_->port()));
    connectionStatusLabel_->setStyleSheet(QStringLiteral(
        "font-size: 18px; font-weight: bold; color: #27ae60;"));

    // 获取设备列表
    QJsonValue devicesResult = rpcClient_->call(QStringLiteral("relay.nodes"), QJsonObject(), 2000);
    int totalDevices = 0;
    int onlineDevices = 0;
    int offlineDevices = 0;

    if (devicesResult.isObject()) {
        QJsonObject obj = devicesResult.toObject();
        if (obj.contains(QStringLiteral("nodes"))) {
            QJsonArray nodes = obj.value(QStringLiteral("nodes")).toArray();
            totalDevices = nodes.size();
            for (const QJsonValue &node : nodes) {
                if (node.toObject().value(QStringLiteral("online")).toBool()) {
                    onlineDevices++;
                } else {
                    offlineDevices++;
                }
            }
        }
    }

    totalDevicesLabel_->setText(QString::number(totalDevices));
    onlineDevicesLabel_->setText(QString::number(onlineDevices));
    offlineDevicesLabel_->setText(QString::number(offlineDevices));

    // 获取分组列表
    QJsonValue groupsResult = rpcClient_->call(QStringLiteral("group.list"), QJsonObject(), 2000);
    int totalGroups = 0;
    if (groupsResult.isObject()) {
        QJsonObject obj = groupsResult.toObject();
        if (obj.contains(QStringLiteral("groups"))) {
            totalGroups = obj.value(QStringLiteral("groups")).toArray().size();
        }
    }
    totalGroupsLabel_->setText(QString::number(totalGroups));

    // 获取策略列表
    QJsonValue strategiesResult = rpcClient_->call(QStringLiteral("auto.strategy.list"), QJsonObject(), 2000);
    int totalStrategies = 0;
    if (strategiesResult.isObject()) {
        QJsonObject obj = strategiesResult.toObject();
        if (obj.contains(QStringLiteral("strategies"))) {
            totalStrategies = obj.value(QStringLiteral("strategies")).toArray().size();
        }
    }
    totalStrategiesLabel_->setText(QString::number(totalStrategies));

    // 获取传感器列表
    QJsonValue sensorsResult = rpcClient_->call(QStringLiteral("sensor.list"), QJsonObject(), 2000);
    int totalSensors = 0;
    if (sensorsResult.isObject()) {
        QJsonObject obj = sensorsResult.toObject();
        if (obj.contains(QStringLiteral("sensors"))) {
            totalSensors = obj.value(QStringLiteral("sensors")).toArray().size();
        }
    }
    totalSensorsLabel_->setText(QString::number(totalSensors));

    // 获取CAN状态
    QJsonValue canResult = rpcClient_->call(QStringLiteral("can.status"), QJsonObject(), 2000);
    if (canResult.isObject()) {
        QJsonObject obj = canResult.toObject();
        bool isOpen = obj.value(QStringLiteral("isOpen")).toBool();
        if (isOpen) {
            QString interface = obj.value(QStringLiteral("interface")).toString();
            canStatusLabel_->setText(QStringLiteral("正常"));
            canStatusLabel_->parentWidget()->setStyleSheet(
                canStatusLabel_->parentWidget()->styleSheet().replace(QStringLiteral("#34495e"), QStringLiteral("#27ae60")));
        } else {
            canStatusLabel_->setText(QStringLiteral("关闭"));
        }
    } else {
        canStatusLabel_->setText(QStringLiteral("未知"));
    }

    // 获取MQTT状态
    QJsonValue mqttResult = rpcClient_->call(QStringLiteral("mqtt.channels.list"), QJsonObject(), 2000);
    if (mqttResult.isObject()) {
        QJsonObject obj = mqttResult.toObject();
        if (obj.value(QStringLiteral("ok")).toBool()) {
            QJsonArray channels = obj.value(QStringLiteral("channels")).toArray();
            int connected = 0;
            for (const QJsonValue &ch : channels) {
                if (ch.toObject().value(QStringLiteral("connected")).toBool()) {
                    connected++;
                }
            }
            if (channels.size() > 0) {
                mqttStatusLabel_->setText(QStringLiteral("%1/%2").arg(connected).arg(channels.size()));
            } else {
                mqttStatusLabel_->setText(QStringLiteral("未配置"));
            }
        } else {
            mqttStatusLabel_->setText(QStringLiteral("未启用"));
        }
    } else {
        mqttStatusLabel_->setText(QStringLiteral("未知"));
    }

    // 获取系统信息
    QJsonValue sysInfo = rpcClient_->call(QStringLiteral("sys.info"), QJsonObject(), 2000);
    if (sysInfo.isObject()) {
        QJsonObject obj = sysInfo.toObject();
        QString uptime = obj.value(QStringLiteral("uptime")).toString();
        if (!uptime.isEmpty()) {
            systemUptimeLabel_->setText(QStringLiteral("运行时间: %1").arg(uptime));
        }
    }

    // 更新时间
    lastUpdateLabel_->setText(QStringLiteral("最后更新: %1")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
        
    qDebug() << "[HOME_WIDGET] 统计数据更新完成";
}
