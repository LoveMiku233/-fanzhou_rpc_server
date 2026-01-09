/**
 * @file group_widget.cpp
 * @brief 分组管理页面实现 - 优化垂直布局
 */

#include "group_widget.h"
#include "rpc_client.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QFormLayout>
#include <QFrame>

GroupWidget::GroupWidget(RpcClient *rpcClient, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpcClient)
    , groupTable_(nullptr)
    , statusLabel_(nullptr)
    , newGroupIdSpinBox_(nullptr)
    , newGroupNameEdit_(nullptr)
    , targetGroupIdSpinBox_(nullptr)
    , deviceNodeIdSpinBox_(nullptr)
    , controlGroupIdSpinBox_(nullptr)
    , controlChannelCombo_(nullptr)
    , controlActionCombo_(nullptr)
{
    setupUi();
}

void GroupWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // 页面标题
    QLabel *titleLabel = new QLabel(QStringLiteral("📂 分组管理"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: 20px; font-weight: bold; color: #2c3e50; padding: 8px 0;"));
    mainLayout->addWidget(titleLabel);

    // 工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(12);

    QPushButton *refreshButton = new QPushButton(QStringLiteral("🔄 刷新分组"), this);
    refreshButton->setMinimumHeight(50);
    connect(refreshButton, &QPushButton::clicked, this, &GroupWidget::refreshGroupList);
    toolbarLayout->addWidget(refreshButton);

    toolbarLayout->addStretch();

    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet(QStringLiteral("color: #7f8c8d;"));
    toolbarLayout->addWidget(statusLabel_);

    mainLayout->addLayout(toolbarLayout);

    // 分组列表表格
    QGroupBox *tableGroupBox = new QGroupBox(QStringLiteral("分组列表"), this);
    QVBoxLayout *tableLayout = new QVBoxLayout(tableGroupBox);
    tableLayout->setContentsMargins(12, 16, 12, 12);

    groupTable_ = new QTableWidget(this);
    groupTable_->setColumnCount(4);
    groupTable_->setHorizontalHeaderLabels({
        QStringLiteral("ID"),
        QStringLiteral("名称"),
        QStringLiteral("设备数"),
        QStringLiteral("包含设备")
    });

    groupTable_->horizontalHeader()->setStretchLastSection(true);
    groupTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    groupTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    groupTable_->setAlternatingRowColors(true);
    groupTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    groupTable_->setMinimumHeight(150);
    groupTable_->setMaximumHeight(200);

    connect(groupTable_, &QTableWidget::cellClicked,
            this, &GroupWidget::onGroupTableCellClicked);

    tableLayout->addWidget(groupTable_);
    mainLayout->addWidget(tableGroupBox);

    // 创建分组区域
    QGroupBox *createGroupBox = new QGroupBox(QStringLiteral("创建/删除分组"), this);
    QVBoxLayout *createLayout = new QVBoxLayout(createGroupBox);
    createLayout->setSpacing(12);

    QHBoxLayout *createRow1 = new QHBoxLayout();
    createRow1->setSpacing(12);
    
    QLabel *groupIdLabel = new QLabel(QStringLiteral("分组ID:"), this);
    createRow1->addWidget(groupIdLabel);
    
    newGroupIdSpinBox_ = new QSpinBox(this);
    newGroupIdSpinBox_->setRange(1, 999);
    newGroupIdSpinBox_->setValue(1);
    newGroupIdSpinBox_->setMinimumHeight(45);
    newGroupIdSpinBox_->setMinimumWidth(80);
    createRow1->addWidget(newGroupIdSpinBox_);
    
    QLabel *nameLabel = new QLabel(QStringLiteral("名称:"), this);
    createRow1->addWidget(nameLabel);
    
    newGroupNameEdit_ = new QLineEdit(this);
    newGroupNameEdit_->setPlaceholderText(QStringLiteral("输入分组名称"));
    newGroupNameEdit_->setMinimumHeight(45);
    createRow1->addWidget(newGroupNameEdit_, 1);
    
    createLayout->addLayout(createRow1);

    QHBoxLayout *createRow2 = new QHBoxLayout();
    createRow2->setSpacing(12);
    
    QPushButton *createButton = new QPushButton(QStringLiteral("✅ 创建分组"), this);
    createButton->setProperty("type", QStringLiteral("success"));
    createButton->setMinimumHeight(50);
    connect(createButton, &QPushButton::clicked, this, &GroupWidget::onCreateGroupClicked);
    createRow2->addWidget(createButton);

    QPushButton *deleteButton = new QPushButton(QStringLiteral("🗑️ 删除分组"), this);
    deleteButton->setProperty("type", QStringLiteral("danger"));
    deleteButton->setMinimumHeight(50);
    connect(deleteButton, &QPushButton::clicked, this, &GroupWidget::onDeleteGroupClicked);
    createRow2->addWidget(deleteButton);
    
    createLayout->addLayout(createRow2);
    mainLayout->addWidget(createGroupBox);

    // 管理设备区域
    QGroupBox *deviceGroupBox = new QGroupBox(QStringLiteral("管理分组设备"), this);
    QVBoxLayout *deviceLayout = new QVBoxLayout(deviceGroupBox);
    deviceLayout->setSpacing(12);

    QHBoxLayout *deviceRow1 = new QHBoxLayout();
    deviceRow1->setSpacing(12);
    
    QLabel *targetGroupLabel = new QLabel(QStringLiteral("目标分组:"), this);
    deviceRow1->addWidget(targetGroupLabel);
    
    targetGroupIdSpinBox_ = new QSpinBox(this);
    targetGroupIdSpinBox_->setRange(1, 999);
    targetGroupIdSpinBox_->setValue(1);
    targetGroupIdSpinBox_->setMinimumHeight(45);
    targetGroupIdSpinBox_->setMinimumWidth(80);
    deviceRow1->addWidget(targetGroupIdSpinBox_);
    
    QLabel *nodeLabel = new QLabel(QStringLiteral("设备节点:"), this);
    deviceRow1->addWidget(nodeLabel);
    
    deviceNodeIdSpinBox_ = new QSpinBox(this);
    deviceNodeIdSpinBox_->setRange(1, 255);
    deviceNodeIdSpinBox_->setValue(1);
    deviceNodeIdSpinBox_->setMinimumHeight(45);
    deviceNodeIdSpinBox_->setMinimumWidth(80);
    deviceRow1->addWidget(deviceNodeIdSpinBox_);
    
    deviceRow1->addStretch();
    deviceLayout->addLayout(deviceRow1);

    QHBoxLayout *deviceRow2 = new QHBoxLayout();
    deviceRow2->setSpacing(12);
    
    QPushButton *addDeviceButton = new QPushButton(QStringLiteral("➕ 添加设备"), this);
    addDeviceButton->setProperty("type", QStringLiteral("success"));
    addDeviceButton->setMinimumHeight(50);
    connect(addDeviceButton, &QPushButton::clicked, this, &GroupWidget::onAddDeviceClicked);
    deviceRow2->addWidget(addDeviceButton);

    QPushButton *removeDeviceButton = new QPushButton(QStringLiteral("➖ 移除设备"), this);
    removeDeviceButton->setProperty("type", QStringLiteral("warning"));
    removeDeviceButton->setMinimumHeight(50);
    connect(removeDeviceButton, &QPushButton::clicked, this, &GroupWidget::onRemoveDeviceClicked);
    deviceRow2->addWidget(removeDeviceButton);
    
    deviceLayout->addLayout(deviceRow2);
    mainLayout->addWidget(deviceGroupBox);

    // 分组控制区域
    QGroupBox *controlGroupBox = new QGroupBox(QStringLiteral("分组批量控制"), this);
    QVBoxLayout *controlLayout = new QVBoxLayout(controlGroupBox);
    controlLayout->setSpacing(12);

    QHBoxLayout *controlRow1 = new QHBoxLayout();
    controlRow1->setSpacing(12);
    
    QLabel *ctrlGroupLabel = new QLabel(QStringLiteral("控制分组:"), this);
    controlRow1->addWidget(ctrlGroupLabel);
    
    controlGroupIdSpinBox_ = new QSpinBox(this);
    controlGroupIdSpinBox_->setRange(1, 999);
    controlGroupIdSpinBox_->setValue(1);
    controlGroupIdSpinBox_->setMinimumHeight(45);
    controlGroupIdSpinBox_->setMinimumWidth(80);
    controlRow1->addWidget(controlGroupIdSpinBox_);
    
    QLabel *channelLabel = new QLabel(QStringLiteral("通道:"), this);
    controlRow1->addWidget(channelLabel);
    
    controlChannelCombo_ = new QComboBox(this);
    controlChannelCombo_->addItem(QStringLiteral("CH0"), 0);
    controlChannelCombo_->addItem(QStringLiteral("CH1"), 1);
    controlChannelCombo_->addItem(QStringLiteral("CH2"), 2);
    controlChannelCombo_->addItem(QStringLiteral("CH3"), 3);
    controlChannelCombo_->addItem(QStringLiteral("全部"), -1);
    controlChannelCombo_->setMinimumHeight(45);
    controlChannelCombo_->setMinimumWidth(100);
    controlRow1->addWidget(controlChannelCombo_);
    
    QLabel *actionLabel = new QLabel(QStringLiteral("动作:"), this);
    controlRow1->addWidget(actionLabel);
    
    controlActionCombo_ = new QComboBox(this);
    controlActionCombo_->addItem(QStringLiteral("停止"), QStringLiteral("stop"));
    controlActionCombo_->addItem(QStringLiteral("正转"), QStringLiteral("fwd"));
    controlActionCombo_->addItem(QStringLiteral("反转"), QStringLiteral("rev"));
    controlActionCombo_->setMinimumHeight(45);
    controlActionCombo_->setMinimumWidth(100);
    controlRow1->addWidget(controlActionCombo_);
    
    controlRow1->addStretch();
    controlLayout->addLayout(controlRow1);

    QPushButton *controlButton = new QPushButton(QStringLiteral("⚡ 执行分组控制"), this);
    controlButton->setProperty("type", QStringLiteral("success"));
    controlButton->setMinimumHeight(56);
    connect(controlButton, &QPushButton::clicked, this, &GroupWidget::onGroupControlClicked);
    controlLayout->addWidget(controlButton);
    
    mainLayout->addWidget(controlGroupBox);

    // 添加弹性空间
    mainLayout->addStretch();
}

void GroupWidget::refreshGroupList()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        statusLabel_->setText(QStringLiteral("[警告] 未连接服务器"));
        return;
    }

    statusLabel_->setText(QStringLiteral("正在刷新..."));

    QJsonValue result = rpcClient_->call(QStringLiteral("group.list"));
    
    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.contains(QStringLiteral("groups"))) {
            QJsonArray groups = obj.value(QStringLiteral("groups")).toArray();
            updateGroupTable(groups);
            statusLabel_->setText(QStringLiteral("共 %1 个分组").arg(groups.size()));
            return;
        }
    }
    
    if (result.isArray()) {
        QJsonArray groups = result.toArray();
        updateGroupTable(groups);
        statusLabel_->setText(QStringLiteral("共 %1 个分组").arg(groups.size()));
        return;
    }

    statusLabel_->setText(QStringLiteral("[错误] 获取分组列表失败"));
}

void GroupWidget::onCreateGroupClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    int groupId = newGroupIdSpinBox_->value();
    QString name = newGroupNameEdit_->text().trimmed();
    
    if (name.isEmpty()) {
        name = QStringLiteral("group-%1").arg(groupId);
    }

    QJsonObject params;
    params[QStringLiteral("groupId")] = groupId;
    params[QStringLiteral("name")] = name;

    QJsonValue result = rpcClient_->call(QStringLiteral("group.create"), params);
    
    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        QMessageBox::information(this, QStringLiteral("成功"), 
            QStringLiteral("分组 %1 创建成功！").arg(groupId));
        refreshGroupList();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), 
            QStringLiteral("创建分组失败: %1").arg(error));
    }
}

void GroupWidget::onDeleteGroupClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    int groupId = newGroupIdSpinBox_->value();

    QMessageBox::StandardButton reply = QMessageBox::question(this, 
        QStringLiteral("确认删除"),
        QStringLiteral("确定要删除分组 %1 吗？").arg(groupId),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    QJsonObject params;
    params[QStringLiteral("groupId")] = groupId;

    QJsonValue result = rpcClient_->call(QStringLiteral("group.delete"), params);
    
    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        QMessageBox::information(this, QStringLiteral("成功"), 
            QStringLiteral("分组 %1 删除成功！").arg(groupId));
        refreshGroupList();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), 
            QStringLiteral("删除分组失败: %1").arg(error));
    }
}

void GroupWidget::onAddDeviceClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    int groupId = targetGroupIdSpinBox_->value();
    int nodeId = deviceNodeIdSpinBox_->value();

    QJsonObject params;
    params[QStringLiteral("groupId")] = groupId;
    params[QStringLiteral("node")] = nodeId;

    QJsonValue result = rpcClient_->call(QStringLiteral("group.addDevice"), params);
    
    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        statusLabel_->setText(QStringLiteral("[成功] 设备 %1 已添加到分组 %2").arg(nodeId).arg(groupId));
        refreshGroupList();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), 
            QStringLiteral("添加设备失败: %1").arg(error));
    }
}

void GroupWidget::onRemoveDeviceClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    int groupId = targetGroupIdSpinBox_->value();
    int nodeId = deviceNodeIdSpinBox_->value();

    QJsonObject params;
    params[QStringLiteral("groupId")] = groupId;
    params[QStringLiteral("node")] = nodeId;

    QJsonValue result = rpcClient_->call(QStringLiteral("group.removeDevice"), params);
    
    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        statusLabel_->setText(QStringLiteral("[成功] 设备 %1 已从分组 %2 移除").arg(nodeId).arg(groupId));
        refreshGroupList();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), 
            QStringLiteral("移除设备失败: %1").arg(error));
    }
}

void GroupWidget::onGroupControlClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    int groupId = controlGroupIdSpinBox_->value();
    int channel = controlChannelCombo_->currentData().toInt();
    QString action = controlActionCombo_->currentData().toString();

    QJsonObject params;
    params[QStringLiteral("groupId")] = groupId;
    params[QStringLiteral("ch")] = channel;
    params[QStringLiteral("action")] = action;

    QJsonValue result = rpcClient_->call(QStringLiteral("group.control"), params);
    
    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        int success = result.toObject().value(QStringLiteral("successCount")).toInt();
        int total = result.toObject().value(QStringLiteral("totalDevices")).toInt();
        statusLabel_->setText(QStringLiteral("[成功] 分组控制完成: %1/%2 成功").arg(success).arg(total));
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), 
            QStringLiteral("分组控制失败: %1").arg(error));
    }
}

void GroupWidget::onGroupTableCellClicked(int row, int column)
{
    Q_UNUSED(column);
    
    QTableWidgetItem *groupIdItem = groupTable_->item(row, 0);
    if (groupIdItem) {
        int groupId = groupIdItem->text().toInt();
        newGroupIdSpinBox_->setValue(groupId);
        targetGroupIdSpinBox_->setValue(groupId);
        controlGroupIdSpinBox_->setValue(groupId);
    }
}

void GroupWidget::updateGroupTable(const QJsonArray &groups)
{
    groupTable_->setRowCount(0);

    for (const QJsonValue &value : groups) {
        QJsonObject group = value.toObject();
        
        int row = groupTable_->rowCount();
        groupTable_->insertRow(row);

        // 分组ID
        int groupId = group.value(QStringLiteral("groupId")).toInt();
        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(groupId));
        idItem->setTextAlignment(Qt::AlignCenter);
        groupTable_->setItem(row, 0, idItem);

        // 分组名称
        QString name = group.value(QStringLiteral("name")).toString();
        groupTable_->setItem(row, 1, new QTableWidgetItem(name));

        // 设备数量
        QJsonArray devices = group.value(QStringLiteral("devices")).toArray();
        QTableWidgetItem *countItem = new QTableWidgetItem(QString::number(devices.size()));
        countItem->setTextAlignment(Qt::AlignCenter);
        groupTable_->setItem(row, 2, countItem);

        // 包含设备
        QStringList deviceList;
        for (const QJsonValue &dev : devices) {
            deviceList << QString::number(dev.toInt());
        }
        groupTable_->setItem(row, 3, new QTableWidgetItem(deviceList.join(QStringLiteral(", "))));
    }
}
