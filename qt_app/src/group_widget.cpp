/**
 * @file group_widget.cpp
 * @brief 分组管理页面实现
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
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // 工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();

    QPushButton *refreshButton = new QPushButton(QStringLiteral("🔄 刷新分组列表"), this);
    connect(refreshButton, &QPushButton::clicked, this, &GroupWidget::refreshGroupList);
    toolbarLayout->addWidget(refreshButton);

    toolbarLayout->addStretch();

    statusLabel_ = new QLabel(this);
    toolbarLayout->addWidget(statusLabel_);

    mainLayout->addLayout(toolbarLayout);

    // 创建水平布局容纳操作面板
    QHBoxLayout *panelsLayout = new QHBoxLayout();

    // 创建分组面板
    QGroupBox *createGroupBox = new QGroupBox(QStringLiteral("➕ 创建分组"), this);
    QFormLayout *createLayout = new QFormLayout(createGroupBox);
    
    newGroupIdSpinBox_ = new QSpinBox(this);
    newGroupIdSpinBox_->setRange(1, 999);
    newGroupIdSpinBox_->setValue(1);
    createLayout->addRow(QStringLiteral("分组ID:"), newGroupIdSpinBox_);
    
    newGroupNameEdit_ = new QLineEdit(this);
    newGroupNameEdit_->setPlaceholderText(QStringLiteral("例如: main-group"));
    createLayout->addRow(QStringLiteral("分组名称:"), newGroupNameEdit_);
    
    QHBoxLayout *createBtnLayout = new QHBoxLayout();
    QPushButton *createButton = new QPushButton(QStringLiteral("✅ 创建"), this);
    createButton->setProperty("type", QStringLiteral("success"));
    connect(createButton, &QPushButton::clicked, this, &GroupWidget::onCreateGroupClicked);
    createBtnLayout->addWidget(createButton);
    
    QPushButton *deleteButton = new QPushButton(QStringLiteral("❌ 删除"), this);
    deleteButton->setProperty("type", QStringLiteral("danger"));
    connect(deleteButton, &QPushButton::clicked, this, &GroupWidget::onDeleteGroupClicked);
    createBtnLayout->addWidget(deleteButton);
    createLayout->addRow(createBtnLayout);
    
    panelsLayout->addWidget(createGroupBox);

    // 添加/移除设备面板
    QGroupBox *deviceGroupBox = new QGroupBox(QStringLiteral("🔗 管理设备"), this);
    QFormLayout *deviceLayout = new QFormLayout(deviceGroupBox);
    
    targetGroupIdSpinBox_ = new QSpinBox(this);
    targetGroupIdSpinBox_->setRange(1, 999);
    targetGroupIdSpinBox_->setValue(1);
    deviceLayout->addRow(QStringLiteral("分组ID:"), targetGroupIdSpinBox_);
    
    deviceNodeIdSpinBox_ = new QSpinBox(this);
    deviceNodeIdSpinBox_->setRange(1, 255);
    deviceNodeIdSpinBox_->setValue(1);
    deviceLayout->addRow(QStringLiteral("设备节点ID:"), deviceNodeIdSpinBox_);
    
    QHBoxLayout *deviceBtnLayout = new QHBoxLayout();
    QPushButton *addDeviceButton = new QPushButton(QStringLiteral("➕ 添加"), this);
    addDeviceButton->setProperty("type", QStringLiteral("success"));
    connect(addDeviceButton, &QPushButton::clicked, this, &GroupWidget::onAddDeviceClicked);
    deviceBtnLayout->addWidget(addDeviceButton);
    
    QPushButton *removeDeviceButton = new QPushButton(QStringLiteral("➖ 移除"), this);
    removeDeviceButton->setProperty("type", QStringLiteral("warning"));
    connect(removeDeviceButton, &QPushButton::clicked, this, &GroupWidget::onRemoveDeviceClicked);
    deviceBtnLayout->addWidget(removeDeviceButton);
    deviceLayout->addRow(deviceBtnLayout);
    
    panelsLayout->addWidget(deviceGroupBox);

    // 分组控制面板
    QGroupBox *controlGroupBox = new QGroupBox(QStringLiteral("⚡ 分组控制"), this);
    QFormLayout *controlLayout = new QFormLayout(controlGroupBox);
    
    controlGroupIdSpinBox_ = new QSpinBox(this);
    controlGroupIdSpinBox_->setRange(1, 999);
    controlGroupIdSpinBox_->setValue(1);
    controlLayout->addRow(QStringLiteral("分组ID:"), controlGroupIdSpinBox_);
    
    controlChannelCombo_ = new QComboBox(this);
    controlChannelCombo_->addItem(QStringLiteral("通道 0"), 0);
    controlChannelCombo_->addItem(QStringLiteral("通道 1"), 1);
    controlChannelCombo_->addItem(QStringLiteral("通道 2"), 2);
    controlChannelCombo_->addItem(QStringLiteral("通道 3"), 3);
    controlChannelCombo_->addItem(QStringLiteral("全部通道"), -1);
    controlLayout->addRow(QStringLiteral("通道:"), controlChannelCombo_);
    
    controlActionCombo_ = new QComboBox(this);
    controlActionCombo_->addItem(QStringLiteral("⏹️ 停止"), QStringLiteral("stop"));
    controlActionCombo_->addItem(QStringLiteral("▶️ 正转"), QStringLiteral("fwd"));
    controlActionCombo_->addItem(QStringLiteral("◀️ 反转"), QStringLiteral("rev"));
    controlLayout->addRow(QStringLiteral("动作:"), controlActionCombo_);
    
    QPushButton *controlButton = new QPushButton(QStringLiteral("⚡ 执行"), this);
    controlButton->setProperty("type", QStringLiteral("success"));
    connect(controlButton, &QPushButton::clicked, this, &GroupWidget::onGroupControlClicked);
    controlLayout->addRow(controlButton);
    
    panelsLayout->addWidget(controlGroupBox);

    mainLayout->addLayout(panelsLayout);

    // 分组表格
    QGroupBox *tableGroupBox = new QGroupBox(QStringLiteral("分组列表"), this);
    QVBoxLayout *tableLayout = new QVBoxLayout(tableGroupBox);

    groupTable_ = new QTableWidget(this);
    groupTable_->setColumnCount(4);
    groupTable_->setHorizontalHeaderLabels({
        QStringLiteral("分组ID"),
        QStringLiteral("分组名称"),
        QStringLiteral("设备数量"),
        QStringLiteral("包含设备")
    });
    
    groupTable_->horizontalHeader()->setStretchLastSection(true);
    groupTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    groupTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    groupTable_->setAlternatingRowColors(true);
    groupTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(groupTable_, &QTableWidget::cellClicked, 
            this, &GroupWidget::onGroupTableCellClicked);

    tableLayout->addWidget(groupTable_);
    mainLayout->addWidget(tableGroupBox, 1);
}

void GroupWidget::refreshGroupList()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        statusLabel_->setText(QStringLiteral("⚠️ 未连接服务器"));
        return;
    }

    statusLabel_->setText(QStringLiteral("正在刷新..."));

    QJsonValue result = rpcClient_->call(QStringLiteral("group.list"));
    
    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.contains(QStringLiteral("groups"))) {
            QJsonArray groups = obj.value(QStringLiteral("groups")).toArray();
            updateGroupTable(groups);
            statusLabel_->setText(QStringLiteral("✅ 共 %1 个分组").arg(groups.size()));
            return;
        }
    }
    
    if (result.isArray()) {
        QJsonArray groups = result.toArray();
        updateGroupTable(groups);
        statusLabel_->setText(QStringLiteral("✅ 共 %1 个分组").arg(groups.size()));
        return;
    }

    statusLabel_->setText(QStringLiteral("⚠️ 获取分组列表失败"));
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
        statusLabel_->setText(QStringLiteral("✅ 设备 %1 已添加到分组 %2").arg(nodeId).arg(groupId));
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
        statusLabel_->setText(QStringLiteral("✅ 设备 %1 已从分组 %2 移除").arg(nodeId).arg(groupId));
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
        statusLabel_->setText(QStringLiteral("✅ 分组控制完成: %1/%2 成功").arg(success).arg(total));
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
