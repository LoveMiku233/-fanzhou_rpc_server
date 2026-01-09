/**
 * @file home_widget.cpp
 * @brief 主页实现 - 大棚控制系统总览
 */

#include "home_widget.h"
#include "rpc_client.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QFrame>
#include <QMessageBox>

HomeWidget::HomeWidget(RpcClient *rpcClient, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpcClient)
    , totalDevicesLabel_(nullptr)
    , onlineDevicesLabel_(nullptr)
    , offlineDevicesLabel_(nullptr)
    , totalGroupsLabel_(nullptr)
    , connectionStatusLabel_(nullptr)
    , lastUpdateLabel_(nullptr)
    , refreshButton_(nullptr)
    , stopAllButton_(nullptr)
{
    setupUi();
}

void HomeWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(16);

    // 页面标题
    QLabel *titleLabel = new QLabel(QStringLiteral("🌱 大棚控制系统"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: 24px; font-weight: bold; color: #27ae60; padding: 8px 0;"));
    mainLayout->addWidget(titleLabel);

    // 连接状态卡片
    QFrame *statusCard = new QFrame(this);
    statusCard->setObjectName(QStringLiteral("statusCard"));
    statusCard->setStyleSheet(QStringLiteral(
        "#statusCard { background-color: #ecf0f1; border-radius: 12px; padding: 16px; }"));
    QHBoxLayout *statusLayout = new QHBoxLayout(statusCard);
    
    QLabel *statusIcon = new QLabel(QStringLiteral("📡"), this);
    statusIcon->setStyleSheet(QStringLiteral("font-size: 32px;"));
    statusLayout->addWidget(statusIcon);
    
    connectionStatusLabel_ = new QLabel(QStringLiteral("未连接"), this);
    connectionStatusLabel_->setStyleSheet(QStringLiteral(
        "font-size: 18px; font-weight: bold; color: #e74c3c;"));
    statusLayout->addWidget(connectionStatusLabel_);
    statusLayout->addStretch();
    
    mainLayout->addWidget(statusCard);

    // 统计信息卡片网格
    QGridLayout *statsGrid = new QGridLayout();
    statsGrid->setSpacing(12);

    // 设备总数卡片
    QFrame *totalCard = new QFrame(this);
    totalCard->setObjectName(QStringLiteral("statCard"));
    totalCard->setStyleSheet(QStringLiteral(
        "#statCard { background-color: #3498db; border-radius: 12px; padding: 20px; }"));
    QVBoxLayout *totalLayout = new QVBoxLayout(totalCard);
    QLabel *totalTitle = new QLabel(QStringLiteral("设备总数"), this);
    totalTitle->setStyleSheet(QStringLiteral("color: white; font-size: 14px;"));
    totalDevicesLabel_ = new QLabel(QStringLiteral("0"), this);
    totalDevicesLabel_->setStyleSheet(QStringLiteral(
        "color: white; font-size: 36px; font-weight: bold;"));
    totalLayout->addWidget(totalTitle);
    totalLayout->addWidget(totalDevicesLabel_);
    statsGrid->addWidget(totalCard, 0, 0);

    // 在线设备卡片
    QFrame *onlineCard = new QFrame(this);
    onlineCard->setObjectName(QStringLiteral("statCardOnline"));
    onlineCard->setStyleSheet(QStringLiteral(
        "#statCardOnline { background-color: #27ae60; border-radius: 12px; padding: 20px; }"));
    QVBoxLayout *onlineLayout = new QVBoxLayout(onlineCard);
    QLabel *onlineTitle = new QLabel(QStringLiteral("在线设备"), this);
    onlineTitle->setStyleSheet(QStringLiteral("color: white; font-size: 14px;"));
    onlineDevicesLabel_ = new QLabel(QStringLiteral("0"), this);
    onlineDevicesLabel_->setStyleSheet(QStringLiteral(
        "color: white; font-size: 36px; font-weight: bold;"));
    onlineLayout->addWidget(onlineTitle);
    onlineLayout->addWidget(onlineDevicesLabel_);
    statsGrid->addWidget(onlineCard, 0, 1);

    // 离线设备卡片
    QFrame *offlineCard = new QFrame(this);
    offlineCard->setObjectName(QStringLiteral("statCardOffline"));
    offlineCard->setStyleSheet(QStringLiteral(
        "#statCardOffline { background-color: #e74c3c; border-radius: 12px; padding: 20px; }"));
    QVBoxLayout *offlineLayout = new QVBoxLayout(offlineCard);
    QLabel *offlineTitle = new QLabel(QStringLiteral("离线设备"), this);
    offlineTitle->setStyleSheet(QStringLiteral("color: white; font-size: 14px;"));
    offlineDevicesLabel_ = new QLabel(QStringLiteral("0"), this);
    offlineDevicesLabel_->setStyleSheet(QStringLiteral(
        "color: white; font-size: 36px; font-weight: bold;"));
    offlineLayout->addWidget(offlineTitle);
    offlineLayout->addWidget(offlineDevicesLabel_);
    statsGrid->addWidget(offlineCard, 1, 0);

    // 分组数量卡片
    QFrame *groupCard = new QFrame(this);
    groupCard->setObjectName(QStringLiteral("statCardGroup"));
    groupCard->setStyleSheet(QStringLiteral(
        "#statCardGroup { background-color: #9b59b6; border-radius: 12px; padding: 20px; }"));
    QVBoxLayout *groupLayout = new QVBoxLayout(groupCard);
    QLabel *groupTitle = new QLabel(QStringLiteral("分组数量"), this);
    groupTitle->setStyleSheet(QStringLiteral("color: white; font-size: 14px;"));
    totalGroupsLabel_ = new QLabel(QStringLiteral("0"), this);
    totalGroupsLabel_->setStyleSheet(QStringLiteral(
        "color: white; font-size: 36px; font-weight: bold;"));
    groupLayout->addWidget(groupTitle);
    groupLayout->addWidget(totalGroupsLabel_);
    statsGrid->addWidget(groupCard, 1, 1);

    mainLayout->addLayout(statsGrid);

    // 快捷操作区
    QGroupBox *actionsBox = new QGroupBox(QStringLiteral("快捷操作"), this);
    QHBoxLayout *actionsLayout = new QHBoxLayout(actionsBox);
    actionsLayout->setSpacing(12);

    refreshButton_ = new QPushButton(QStringLiteral("🔄 刷新数据"), this);
    refreshButton_->setMinimumHeight(60);
    refreshButton_->setProperty("type", QStringLiteral("success"));
    connect(refreshButton_, &QPushButton::clicked, this, &HomeWidget::refreshData);
    actionsLayout->addWidget(refreshButton_);

    stopAllButton_ = new QPushButton(QStringLiteral("🛑 全部停止"), this);
    stopAllButton_->setMinimumHeight(60);
    stopAllButton_->setProperty("type", QStringLiteral("danger"));
    connect(stopAllButton_, &QPushButton::clicked, this, [this]() {
        if (!rpcClient_ || !rpcClient_->isConnected()) {
            QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
            return;
        }
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            QStringLiteral("确认"),
            QStringLiteral("确定要停止所有设备吗？"),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            rpcClient_->call(QStringLiteral("relay.stopAll"));
        }
    });
    actionsLayout->addWidget(stopAllButton_);

    mainLayout->addWidget(actionsBox);

    // 最后更新时间
    lastUpdateLabel_ = new QLabel(QStringLiteral("最后更新: --"), this);
    lastUpdateLabel_->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 12px;"));
    lastUpdateLabel_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(lastUpdateLabel_);

    mainLayout->addStretch();
}

void HomeWidget::refreshData()
{
    updateStats();
}

void HomeWidget::updateStats()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        connectionStatusLabel_->setText(QStringLiteral("❌ 未连接"));
        connectionStatusLabel_->setStyleSheet(QStringLiteral(
            "font-size: 18px; font-weight: bold; color: #e74c3c;"));
        totalDevicesLabel_->setText(QStringLiteral("--"));
        onlineDevicesLabel_->setText(QStringLiteral("--"));
        offlineDevicesLabel_->setText(QStringLiteral("--"));
        totalGroupsLabel_->setText(QStringLiteral("--"));
        return;
    }

    connectionStatusLabel_->setText(QStringLiteral("✅ 已连接 - %1:%2")
        .arg(rpcClient_->host()).arg(rpcClient_->port()));
    connectionStatusLabel_->setStyleSheet(QStringLiteral(
        "font-size: 18px; font-weight: bold; color: #27ae60;"));

    // 获取设备列表
    QJsonValue devicesResult = rpcClient_->call(QStringLiteral("relay.nodes"));
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
    } else if (devicesResult.isArray()) {
        QJsonArray nodes = devicesResult.toArray();
        totalDevices = nodes.size();
        for (const QJsonValue &node : nodes) {
            if (node.toObject().value(QStringLiteral("online")).toBool()) {
                onlineDevices++;
            } else {
                offlineDevices++;
            }
        }
    }

    totalDevicesLabel_->setText(QString::number(totalDevices));
    onlineDevicesLabel_->setText(QString::number(onlineDevices));
    offlineDevicesLabel_->setText(QString::number(offlineDevices));

    // 获取分组列表
    QJsonValue groupsResult = rpcClient_->call(QStringLiteral("group.list"));
    int totalGroups = 0;

    if (groupsResult.isObject()) {
        QJsonObject obj = groupsResult.toObject();
        if (obj.contains(QStringLiteral("groups"))) {
            totalGroups = obj.value(QStringLiteral("groups")).toArray().size();
        }
    } else if (groupsResult.isArray()) {
        totalGroups = groupsResult.toArray().size();
    }

    totalGroupsLabel_->setText(QString::number(totalGroups));

    // 更新时间
    lastUpdateLabel_->setText(QStringLiteral("最后更新: %1")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
}
