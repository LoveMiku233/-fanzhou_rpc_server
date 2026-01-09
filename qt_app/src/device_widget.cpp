/**
 * @file device_widget.cpp
 * @brief 设备管理页面实现
 */

#include "device_widget.h"
#include "rpc_client.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QTimer>
#include <QColor>

DeviceWidget::DeviceWidget(RpcClient *rpcClient, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpcClient)
    , deviceTable_(nullptr)
    , statusLabel_(nullptr)
    , refreshButton_(nullptr)
    , queryAllButton_(nullptr)
{
    setupUi();
}

void DeviceWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // 工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(8);

    refreshButton_ = new QPushButton(QStringLiteral("刷新设备"), this);
    connect(refreshButton_, &QPushButton::clicked, this, &DeviceWidget::refreshDeviceList);
    toolbarLayout->addWidget(refreshButton_);

    queryAllButton_ = new QPushButton(QStringLiteral("查询全部"), this);
    queryAllButton_->setProperty("type", QStringLiteral("success"));
    connect(queryAllButton_, &QPushButton::clicked, this, &DeviceWidget::onQueryAllClicked);
    toolbarLayout->addWidget(queryAllButton_);

    toolbarLayout->addStretch();

    statusLabel_ = new QLabel(this);
    toolbarLayout->addWidget(statusLabel_);

    mainLayout->addLayout(toolbarLayout);

    // 设备表格
    QGroupBox *tableGroupBox = new QGroupBox(QStringLiteral("设备列表"), this);
    QVBoxLayout *tableLayout = new QVBoxLayout(tableGroupBox);
    tableLayout->setContentsMargins(6, 6, 6, 6);

    deviceTable_ = new QTableWidget(this);
    deviceTable_->setColumnCount(7);
    deviceTable_->setHorizontalHeaderLabels({
        QStringLiteral("节点"),
        QStringLiteral("名称"),
        QStringLiteral("类型"),
        QStringLiteral("状态"),
        QStringLiteral("CH0"),
        QStringLiteral("CH1"),
        QStringLiteral("CH2/3")
    });
    
    deviceTable_->horizontalHeader()->setStretchLastSection(true);
    deviceTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    deviceTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    deviceTable_->setAlternatingRowColors(true);
    deviceTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(deviceTable_, &QTableWidget::cellClicked, 
            this, &DeviceWidget::onDeviceTableCellClicked);

    tableLayout->addWidget(deviceTable_);
    mainLayout->addWidget(tableGroupBox, 1);

    // 说明 - 简化文本
    QLabel *helpLabel = new QLabel(
        QStringLiteral("提示：点击设备行查看详细状态"),
        this);
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet(QStringLiteral("color: #666; padding: 4px;"));
    mainLayout->addWidget(helpLabel);
}

void DeviceWidget::refreshDeviceList()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        statusLabel_->setText(QStringLiteral("[警告] 未连接服务器"));
        return;
    }

    statusLabel_->setText(QStringLiteral("正在刷新..."));

    QJsonValue result = rpcClient_->call(QStringLiteral("relay.nodes"));
    
    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.contains(QStringLiteral("nodes"))) {
            QJsonArray nodes = obj.value(QStringLiteral("nodes")).toArray();
            updateDeviceTable(nodes);
            statusLabel_->setText(QStringLiteral("共 %1 个设备").arg(nodes.size()));
            return;
        }
    }
    
    if (result.isArray()) {
        QJsonArray nodes = result.toArray();
        updateDeviceTable(nodes);
        statusLabel_->setText(QStringLiteral("共 %1 个设备").arg(nodes.size()));
        return;
    }

    statusLabel_->setText(QStringLiteral("[错误] 获取失败"));
}

void DeviceWidget::refreshDeviceStatus()
{
    // 静默刷新所有设备状态
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        return;
    }

    // 遍历表格中的设备并刷新状态
    for (int row = 0; row < deviceTable_->rowCount(); ++row) {
        QTableWidgetItem *nodeItem = deviceTable_->item(row, 0);
        if (nodeItem) {
            int nodeId = nodeItem->text().toInt();
            
            QJsonObject params;
            params[QStringLiteral("node")] = nodeId;
            
            QJsonValue result = rpcClient_->call(QStringLiteral("relay.statusAll"), params);
            if (result.isObject()) {
                updateDeviceStatus(nodeId, result.toObject());
            }
        }
    }
}

void DeviceWidget::onQueryAllClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    statusLabel_->setText(QStringLiteral("正在查询所有设备..."));

    QJsonValue result = rpcClient_->call(QStringLiteral("relay.queryAll"));
    
    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.value(QStringLiteral("ok")).toBool()) {
            int queried = obj.value(QStringLiteral("queriedDevices")).toInt();
            statusLabel_->setText(QStringLiteral("已查询 %1 个设备").arg(queried));
            
            // 延迟后刷新状态
            QTimer::singleShot(500, this, &DeviceWidget::refreshDeviceStatus);
            return;
        }
    }

    statusLabel_->setText(QStringLiteral("[错误] 查询失败"));
}

void DeviceWidget::onDeviceTableCellClicked(int row, int column)
{
    Q_UNUSED(column);
    
    QTableWidgetItem *nodeItem = deviceTable_->item(row, 0);
    if (!nodeItem) return;

    int nodeId = nodeItem->text().toInt();
    
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        return;
    }

    // 获取设备详细状态
    QJsonObject params;
    params[QStringLiteral("node")] = nodeId;
    
    QJsonValue result = rpcClient_->call(QStringLiteral("relay.statusAll"), params);
    
    if (result.isObject()) {
        updateDeviceStatus(nodeId, result.toObject());
        
        // 显示详细信息
        QString info = QString::fromUtf8(QJsonDocument(result.toObject()).toJson(QJsonDocument::Indented));
        statusLabel_->setText(QStringLiteral("节点 %1 状态已更新").arg(nodeId));
    }
}

void DeviceWidget::updateDeviceTable(const QJsonArray &devices)
{
    deviceTable_->setRowCount(0);

    for (const QJsonValue &value : devices) {
        QJsonObject device = value.toObject();
        
        int row = deviceTable_->rowCount();
        deviceTable_->insertRow(row);

        // 节点ID
        int nodeId = device.value(QStringLiteral("nodeId")).toInt();
        QTableWidgetItem *nodeItem = new QTableWidgetItem(QString::number(nodeId));
        nodeItem->setTextAlignment(Qt::AlignCenter);
        deviceTable_->setItem(row, 0, nodeItem);

        // 设备名称
        QString name = device.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            name = QStringLiteral("Relay-%1").arg(nodeId);
        }
        deviceTable_->setItem(row, 1, new QTableWidgetItem(name));

        // 类型
        QString type = device.value(QStringLiteral("type")).toString();
        if (type.isEmpty()) {
            type = QStringLiteral("CAN继电器");
        }
        deviceTable_->setItem(row, 2, new QTableWidgetItem(type));

        // 在线状态 - 使用文字代替emoji
        bool online = device.value(QStringLiteral("online")).toBool();
        QTableWidgetItem *statusItem = new QTableWidgetItem(
            online ? QStringLiteral("[在线]") : QStringLiteral("[离线]"));
        statusItem->setTextAlignment(Qt::AlignCenter);
        statusItem->setForeground(online ? QColor("#27ae60") : QColor("#e74c3c"));
        deviceTable_->setItem(row, 3, statusItem);

        // 通道状态（占位符）
        for (int ch = 0; ch < 3; ++ch) {
            QTableWidgetItem *chItem = new QTableWidgetItem(QStringLiteral("-"));
            chItem->setTextAlignment(Qt::AlignCenter);
            deviceTable_->setItem(row, 4 + ch, chItem);
        }
    }
}

void DeviceWidget::updateDeviceStatus(int nodeId, const QJsonObject &status)
{
    // 查找设备行
    for (int row = 0; row < deviceTable_->rowCount(); ++row) {
        QTableWidgetItem *nodeItem = deviceTable_->item(row, 0);
        if (nodeItem && nodeItem->text().toInt() == nodeId) {
            // 更新在线状态
            bool online = status.value(QStringLiteral("online")).toBool();
            qint64 ageMs = static_cast<qint64>(status.value(QStringLiteral("ageMs")).toDouble(-1));
            
            QString statusText;
            QColor statusColor;
            if (ageMs < 0) {
                statusText = QStringLiteral("[无响应]");
                statusColor = QColor("#666666");
            } else if (online) {
                statusText = QStringLiteral("[在线] %1ms").arg(ageMs);
                statusColor = QColor("#27ae60");
            } else {
                statusText = QStringLiteral("[离线] %1s").arg(ageMs / 1000);
                statusColor = QColor("#e74c3c");
            }
            
            QTableWidgetItem *statusItem = deviceTable_->item(row, 3);
            if (statusItem) {
                statusItem->setText(statusText);
                statusItem->setForeground(statusColor);
            }

            // 更新通道状态
            QJsonObject channels = status.value(QStringLiteral("channels")).toObject();
            
            // First, collect all channel modes for channels 2 and 3
            QString ch2Text, ch3Text;
            
            for (int ch = 0; ch < 4; ++ch) {
                QString chKey = QString::number(ch);
                if (channels.contains(chKey)) {
                    QJsonObject chStatus = channels.value(chKey).toObject();
                    int mode = chStatus.value(QStringLiteral("mode")).toInt(0);
                    
                    QString modeText;
                    switch (mode) {
                        case 0: modeText = QStringLiteral("停"); break;
                        case 1: modeText = QStringLiteral("正"); break;
                        case 2: modeText = QStringLiteral("反"); break;
                        default: modeText = QStringLiteral("?"); break;
                    }

                    if (ch < 2) {
                        // Update columns 4 and 5 for channels 0 and 1
                        QTableWidgetItem *chItem = deviceTable_->item(row, 4 + ch);
                        if (chItem) {
                            chItem->setText(modeText);
                        }
                    } else if (ch == 2) {
                        ch2Text = modeText;
                    } else {
                        ch3Text = modeText;
                    }
                }
            }
            
            // Update column 6 for channels 2 and 3 combined
            QTableWidgetItem *ch23Item = deviceTable_->item(row, 6);
            if (ch23Item) {
                QString combinedText;
                if (!ch2Text.isEmpty() && !ch3Text.isEmpty()) {
                    combinedText = QStringLiteral("%1/%2").arg(ch2Text, ch3Text);
                } else if (!ch2Text.isEmpty()) {
                    combinedText = ch2Text;
                } else if (!ch3Text.isEmpty()) {
                    combinedText = ch3Text;
                } else {
                    combinedText = QStringLiteral("-");
                }
                ch23Item->setText(combinedText);
            }
            
            break;
        }
    }
}
