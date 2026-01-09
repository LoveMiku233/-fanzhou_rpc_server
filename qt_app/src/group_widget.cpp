/**
 * @file group_widget.cpp
 * @brief 分组管理页面实现 - 按钮弹窗方式操作
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
#include <QDialog>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QComboBox>

GroupWidget::GroupWidget(RpcClient *rpcClient, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpcClient)
    , groupTable_(nullptr)
    , statusLabel_(nullptr)
    , selectedGroupId_(1)
{
    setupUi();
}

void GroupWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // 页面标题
    QLabel *titleLabel = new QLabel(QStringLiteral("分组管理"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: 18px; font-weight: bold; color: #2c3e50; padding: 4px 0;"));
    mainLayout->addWidget(titleLabel);

    // 工具栏 - 表格上方按钮
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(8);

    QPushButton *refreshButton = new QPushButton(QStringLiteral("刷新"), this);
    refreshButton->setMinimumHeight(40);
    connect(refreshButton, &QPushButton::clicked, this, &GroupWidget::refreshGroupList);
    toolbarLayout->addWidget(refreshButton);

    QPushButton *createButton = new QPushButton(QStringLiteral("创建分组"), this);
    createButton->setProperty("type", QStringLiteral("success"));
    createButton->setMinimumHeight(40);
    connect(createButton, &QPushButton::clicked, this, &GroupWidget::onCreateGroupClicked);
    toolbarLayout->addWidget(createButton);

    QPushButton *deleteButton = new QPushButton(QStringLiteral("删除分组"), this);
    deleteButton->setProperty("type", QStringLiteral("danger"));
    deleteButton->setMinimumHeight(40);
    connect(deleteButton, &QPushButton::clicked, this, &GroupWidget::onDeleteGroupClicked);
    toolbarLayout->addWidget(deleteButton);

    QPushButton *manageButton = new QPushButton(QStringLiteral("管理设备"), this);
    manageButton->setProperty("type", QStringLiteral("warning"));
    manageButton->setMinimumHeight(40);
    connect(manageButton, &QPushButton::clicked, this, &GroupWidget::onManageDevicesClicked);
    toolbarLayout->addWidget(manageButton);

    QPushButton *controlButton = new QPushButton(QStringLiteral("批量控制"), this);
    controlButton->setMinimumHeight(40);
    connect(controlButton, &QPushButton::clicked, this, &GroupWidget::onGroupControlClicked);
    toolbarLayout->addWidget(controlButton);

    toolbarLayout->addStretch();

    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 12px;"));
    toolbarLayout->addWidget(statusLabel_);

    mainLayout->addLayout(toolbarLayout);

    // 分组列表表格
    groupTable_ = new QTableWidget(this);
    groupTable_->setColumnCount(4);
    groupTable_->setHorizontalHeaderLabels({
        QStringLiteral("ID"),
        QStringLiteral("名称"),
        QStringLiteral("设备数"),
        QStringLiteral("设备列表")
    });

    groupTable_->horizontalHeader()->setStretchLastSection(true);
    groupTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    groupTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    groupTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    groupTable_->setAlternatingRowColors(true);
    groupTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(groupTable_, &QTableWidget::cellClicked,
            this, &GroupWidget::onGroupTableCellClicked);

    mainLayout->addWidget(groupTable_, 1);

    // 提示
    QLabel *helpLabel = new QLabel(
        QStringLiteral("提示：点击表格行选中分组，然后点击上方按钮操作"),
        this);
    helpLabel->setStyleSheet(QStringLiteral(
        "color: #7f8c8d; font-size: 11px; padding: 4px;"));
    helpLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(helpLabel);
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

    // 创建弹窗
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("创建分组"));
    dialog.setMinimumWidth(350);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QFormLayout *formLayout = new QFormLayout();
    
    QSpinBox *groupIdSpinBox = new QSpinBox(&dialog);
    groupIdSpinBox->setRange(1, 999);
    groupIdSpinBox->setValue(selectedGroupId_);
    groupIdSpinBox->setMinimumHeight(40);
    formLayout->addRow(QStringLiteral("分组ID:"), groupIdSpinBox);
    
    QLineEdit *nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText(QStringLiteral("分组名称(可空,自动生成)"));
    nameEdit->setMinimumHeight(40);
    formLayout->addRow(QStringLiteral("名称:"), nameEdit);
    
    layout->addLayout(formLayout);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("创建"));
    buttonBox->button(QDialogButtonBox::Ok)->setMinimumHeight(40);
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    buttonBox->button(QDialogButtonBox::Cancel)->setMinimumHeight(40);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    int groupId = groupIdSpinBox->value();
    QString name = nameEdit->text().trimmed();
    
    // 名称为空时自动生成
    if (name.isEmpty()) {
        name = QStringLiteral("分组-%1").arg(groupId);
    }

    QJsonObject params;
    params[QStringLiteral("groupId")] = groupId;
    params[QStringLiteral("name")] = name;

    QJsonValue result = rpcClient_->call(QStringLiteral("group.create"), params);
    
    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        statusLabel_->setText(QStringLiteral("分组 %1 创建成功").arg(groupId));
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

    int groupId = getSelectedGroupId();

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
        statusLabel_->setText(QStringLiteral("分组 %1 删除成功").arg(groupId));
        refreshGroupList();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), 
            QStringLiteral("删除分组失败: %1").arg(error));
    }
}

void GroupWidget::onManageDevicesClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    int groupId = getSelectedGroupId();
    
    // 创建弹窗
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("管理分组设备 - 分组 %1").arg(groupId));
    dialog.setMinimumWidth(350);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    QLabel *infoLabel = new QLabel(QStringLiteral("当前操作分组: %1").arg(groupId), &dialog);
    infoLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; padding: 8px;"));
    layout->addWidget(infoLabel);
    
    QFormLayout *formLayout = new QFormLayout();
    
    QSpinBox *nodeIdSpinBox = new QSpinBox(&dialog);
    nodeIdSpinBox->setRange(1, 255);
    nodeIdSpinBox->setValue(1);
    nodeIdSpinBox->setMinimumHeight(40);
    formLayout->addRow(QStringLiteral("设备节点ID:"), nodeIdSpinBox);
    
    layout->addLayout(formLayout);
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    
    QPushButton *addBtn = new QPushButton(QStringLiteral("添加设备"), &dialog);
    addBtn->setProperty("type", QStringLiteral("success"));
    addBtn->setMinimumHeight(44);
    btnLayout->addWidget(addBtn);
    
    QPushButton *removeBtn = new QPushButton(QStringLiteral("移除设备"), &dialog);
    removeBtn->setProperty("type", QStringLiteral("warning"));
    removeBtn->setMinimumHeight(44);
    btnLayout->addWidget(removeBtn);
    
    layout->addLayout(btnLayout);
    
    QLabel *resultLabel = new QLabel(&dialog);
    resultLabel->setAlignment(Qt::AlignCenter);
    resultLabel->setMinimumHeight(30);
    layout->addWidget(resultLabel);
    
    QPushButton *closeBtn = new QPushButton(QStringLiteral("关闭"), &dialog);
    closeBtn->setMinimumHeight(40);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeBtn);
    
    // 添加设备
    connect(addBtn, &QPushButton::clicked, [&]() {
        int nodeId = nodeIdSpinBox->value();
        QJsonObject params;
        params[QStringLiteral("groupId")] = groupId;
        params[QStringLiteral("node")] = nodeId;
        
        QJsonValue result = rpcClient_->call(QStringLiteral("group.addDevice"), params);
        
        if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
            resultLabel->setText(QStringLiteral("设备 %1 已添加到分组").arg(nodeId));
            resultLabel->setStyleSheet(QStringLiteral("color: #27ae60; font-weight: bold;"));
        } else {
            QString error = result.toObject().value(QStringLiteral("error")).toString();
            resultLabel->setText(QStringLiteral("添加失败: %1").arg(error));
            resultLabel->setStyleSheet(QStringLiteral("color: #e74c3c; font-weight: bold;"));
        }
    });
    
    // 移除设备
    connect(removeBtn, &QPushButton::clicked, [&]() {
        int nodeId = nodeIdSpinBox->value();
        QJsonObject params;
        params[QStringLiteral("groupId")] = groupId;
        params[QStringLiteral("node")] = nodeId;
        
        QJsonValue result = rpcClient_->call(QStringLiteral("group.removeDevice"), params);
        
        if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
            resultLabel->setText(QStringLiteral("设备 %1 已从分组移除").arg(nodeId));
            resultLabel->setStyleSheet(QStringLiteral("color: #27ae60; font-weight: bold;"));
        } else {
            QString error = result.toObject().value(QStringLiteral("error")).toString();
            resultLabel->setText(QStringLiteral("移除失败: %1").arg(error));
            resultLabel->setStyleSheet(QStringLiteral("color: #e74c3c; font-weight: bold;"));
        }
    });
    
    dialog.exec();
    refreshGroupList();
}

void GroupWidget::onGroupControlClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    int groupId = getSelectedGroupId();
    
    // 创建弹窗
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("分组批量控制 - 分组 %1").arg(groupId));
    dialog.setMinimumWidth(350);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    QLabel *infoLabel = new QLabel(QStringLiteral("当前操作分组: %1").arg(groupId), &dialog);
    infoLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; padding: 8px;"));
    layout->addWidget(infoLabel);
    
    QFormLayout *formLayout = new QFormLayout();
    
    QComboBox *channelCombo = new QComboBox(&dialog);
    channelCombo->addItem(QStringLiteral("CH0"), 0);
    channelCombo->addItem(QStringLiteral("CH1"), 1);
    channelCombo->addItem(QStringLiteral("CH2"), 2);
    channelCombo->addItem(QStringLiteral("CH3"), 3);
    channelCombo->addItem(QStringLiteral("全部通道"), -1);
    channelCombo->setCurrentIndex(4); // 默认全部
    channelCombo->setMinimumHeight(40);
    formLayout->addRow(QStringLiteral("通道:"), channelCombo);
    
    QComboBox *actionCombo = new QComboBox(&dialog);
    actionCombo->addItem(QStringLiteral("停止"), QStringLiteral("stop"));
    actionCombo->addItem(QStringLiteral("正转"), QStringLiteral("fwd"));
    actionCombo->addItem(QStringLiteral("反转"), QStringLiteral("rev"));
    actionCombo->setMinimumHeight(40);
    formLayout->addRow(QStringLiteral("动作:"), actionCombo);
    
    layout->addLayout(formLayout);
    
    QLabel *resultLabel = new QLabel(&dialog);
    resultLabel->setAlignment(Qt::AlignCenter);
    resultLabel->setMinimumHeight(30);
    layout->addWidget(resultLabel);
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    
    QPushButton *executeBtn = new QPushButton(QStringLiteral("执行控制"), &dialog);
    executeBtn->setProperty("type", QStringLiteral("success"));
    executeBtn->setMinimumHeight(44);
    btnLayout->addWidget(executeBtn);
    
    QPushButton *closeBtn = new QPushButton(QStringLiteral("关闭"), &dialog);
    closeBtn->setMinimumHeight(44);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    btnLayout->addWidget(closeBtn);
    
    layout->addLayout(btnLayout);
    
    connect(executeBtn, &QPushButton::clicked, [&]() {
        int channel = channelCombo->currentData().toInt();
        QString action = actionCombo->currentData().toString();
        
        QJsonObject params;
        params[QStringLiteral("groupId")] = groupId;
        params[QStringLiteral("ch")] = channel;
        params[QStringLiteral("action")] = action;
        
        QJsonValue result = rpcClient_->call(QStringLiteral("group.control"), params);
        
        if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
            int success = result.toObject().value(QStringLiteral("successCount")).toInt();
            int total = result.toObject().value(QStringLiteral("totalDevices")).toInt();
            resultLabel->setText(QStringLiteral("控制完成: %1/%2 成功").arg(success).arg(total));
            resultLabel->setStyleSheet(QStringLiteral("color: #27ae60; font-weight: bold;"));
        } else {
            QString error = result.toObject().value(QStringLiteral("error")).toString();
            resultLabel->setText(QStringLiteral("控制失败: %1").arg(error));
            resultLabel->setStyleSheet(QStringLiteral("color: #e74c3c; font-weight: bold;"));
        }
    });
    
    dialog.exec();
}

void GroupWidget::onGroupTableCellClicked(int row, int column)
{
    Q_UNUSED(column);
    
    QTableWidgetItem *groupIdItem = groupTable_->item(row, 0);
    if (groupIdItem) {
        selectedGroupId_ = groupIdItem->text().toInt();
        statusLabel_->setText(QStringLiteral("已选中分组: %1").arg(selectedGroupId_));
    }
}

int GroupWidget::getSelectedGroupId()
{
    // 获取表格中选中的行
    int currentRow = groupTable_->currentRow();
    if (currentRow >= 0) {
        QTableWidgetItem *idItem = groupTable_->item(currentRow, 0);
        if (idItem) {
            return idItem->text().toInt();
        }
    }
    return selectedGroupId_;
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
