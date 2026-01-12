/**
 * @file group_widget.cpp
 * @brief 分组管理页面实现 - 卡片式布局
 * 
 * 按通道绑定设置分组，参考Web端设计
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
#include <QScrollArea>
#include <QMouseEvent>

// ==================== GroupCard Implementation ====================

GroupCard::GroupCard(int groupId, const QString &name, QWidget *parent)
    : QFrame(parent)
    , groupId_(groupId)
    , name_(name)
    , nameLabel_(nullptr)
    , idLabel_(nullptr)
    , deviceCountLabel_(nullptr)
    , channelCountLabel_(nullptr)
    , channelsLabel_(nullptr)
{
    setupUi();
}

void GroupCard::setupUi()
{
    setObjectName(QStringLiteral("groupCard"));
    setStyleSheet(QStringLiteral(
        "#groupCard {"
        "  background-color: white;"
        "  border: 1px solid #d0d5dd;"
        "  border-radius: 10px;"
        "  padding: 12px;"
        "}"
        "#groupCard:hover {"
        "  border-color: #27ae60;"
        "  background-color: #f8f9fa;"
        "}"));
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(140);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // 顶部行：名称和ID
    QHBoxLayout *topRow = new QHBoxLayout();
    
    nameLabel_ = new QLabel(QStringLiteral("[G] %1").arg(name_), this);
    nameLabel_->setStyleSheet(QStringLiteral(
        "font-size: 15px; font-weight: bold; color: #2c3e50;"));
    topRow->addWidget(nameLabel_);
    
    topRow->addStretch();
    
    idLabel_ = new QLabel(QStringLiteral("ID: %1").arg(groupId_), this);
    idLabel_->setStyleSheet(QStringLiteral(
        "font-size: 12px; color: #7f8c8d; background-color: #ecf0f1; "
        "padding: 3px 8px; border-radius: 4px;"));
    topRow->addWidget(idLabel_);
    
    mainLayout->addLayout(topRow);

    // 中间行：设备数和通道数
    QHBoxLayout *middleRow = new QHBoxLayout();
    
    deviceCountLabel_ = new QLabel(QStringLiteral("0 设备"), this);
    deviceCountLabel_->setStyleSheet(QStringLiteral("font-size: 13px; color: #3498db;"));
    middleRow->addWidget(deviceCountLabel_);
    
    channelCountLabel_ = new QLabel(QStringLiteral("0 通道"), this);
    channelCountLabel_->setStyleSheet(QStringLiteral("font-size: 13px; color: #27ae60;"));
    middleRow->addWidget(channelCountLabel_);
    
    middleRow->addStretch();
    
    mainLayout->addLayout(middleRow);

    // 通道信息行
    channelsLabel_ = new QLabel(QStringLiteral("暂无绑定通道"), this);
    channelsLabel_->setStyleSheet(QStringLiteral(
        "font-size: 11px; color: #95a5a6; padding: 4px; "
        "background-color: #f8f9fa; border-radius: 4px;"));
    channelsLabel_->setWordWrap(true);
    channelsLabel_->setMinimumHeight(30);
    mainLayout->addWidget(channelsLabel_);

    // 底部行：控制按钮
    QHBoxLayout *buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(6);
    
    QPushButton *stopBtn = new QPushButton(QStringLiteral("[=] 停止"), this);
    stopBtn->setMinimumHeight(32);
    stopBtn->setStyleSheet(QStringLiteral(
        "background-color: #95a5a6; color: white; border: none; border-radius: 4px;"));
    connect(stopBtn, &QPushButton::clicked, [this]() {
        emit controlClicked(groupId_, QStringLiteral("stop"));
    });
    buttonRow->addWidget(stopBtn);
    
    QPushButton *fwdBtn = new QPushButton(QStringLiteral("[>] 正转"), this);
    fwdBtn->setMinimumHeight(32);
    fwdBtn->setStyleSheet(QStringLiteral(
        "background-color: #27ae60; color: white; border: none; border-radius: 4px;"));
    connect(fwdBtn, &QPushButton::clicked, [this]() {
        emit controlClicked(groupId_, QStringLiteral("fwd"));
    });
    buttonRow->addWidget(fwdBtn);
    
    QPushButton *revBtn = new QPushButton(QStringLiteral("[<] 反转"), this);
    revBtn->setMinimumHeight(32);
    revBtn->setStyleSheet(QStringLiteral(
        "background-color: #f39c12; color: white; border: none; border-radius: 4px;"));
    connect(revBtn, &QPushButton::clicked, [this]() {
        emit controlClicked(groupId_, QStringLiteral("rev"));
    });
    buttonRow->addWidget(revBtn);
    
    QPushButton *deleteBtn = new QPushButton(QStringLiteral("X"), this);
    deleteBtn->setMinimumHeight(32);
    deleteBtn->setMinimumWidth(40);
    deleteBtn->setStyleSheet(QStringLiteral(
        "background-color: #e74c3c; color: white; border: none; border-radius: 4px;"));
    connect(deleteBtn, &QPushButton::clicked, [this]() {
        emit deleteClicked(groupId_);
    });
    buttonRow->addWidget(deleteBtn);
    
    mainLayout->addLayout(buttonRow);
}

void GroupCard::updateInfo(const QString &name, int deviceCount, int channelCount,
                          const QJsonArray &channels)
{
    name_ = name;
    nameLabel_->setText(QStringLiteral("[G] %1").arg(name));
    deviceCountLabel_->setText(QStringLiteral("%1 设备").arg(deviceCount));
    channelCountLabel_->setText(QStringLiteral("%1 通道").arg(channelCount));
    
    if (channels.isEmpty()) {
        channelsLabel_->setText(QStringLiteral("暂无绑定通道"));
        channelsLabel_->setStyleSheet(QStringLiteral(
            "font-size: 11px; color: #95a5a6; padding: 4px; "
            "background-color: #f8f9fa; border-radius: 4px;"));
    } else {
        QStringList channelTexts;
        for (const QJsonValue &v : channels) {
            QJsonObject ch = v.toObject();
            int node = ch.value(QStringLiteral("node")).toInt();
            int channel = ch.value(QStringLiteral("channel")).toInt();
            channelTexts << QStringLiteral("[CH] 节点%1:通道%2").arg(node).arg(channel);
        }
        channelsLabel_->setText(channelTexts.join(QStringLiteral(" | ")));
        channelsLabel_->setStyleSheet(QStringLiteral(
            "font-size: 11px; color: #2c3e50; padding: 4px; "
            "background-color: #e8f5e9; border-radius: 4px;"));
    }
}

void GroupCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit manageClicked(groupId_);
    }
    QFrame::mousePressEvent(event);
}

// ==================== GroupWidget Implementation ====================

GroupWidget::GroupWidget(RpcClient *rpcClient, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpcClient)
    , statusLabel_(nullptr)
    , cardsContainer_(nullptr)
    , cardsLayout_(nullptr)
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

    // 工具栏
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

    QPushButton *manageChannelsBtn = new QPushButton(QStringLiteral("管理通道"), this);
    manageChannelsBtn->setProperty("type", QStringLiteral("warning"));
    manageChannelsBtn->setMinimumHeight(40);
    connect(manageChannelsBtn, &QPushButton::clicked, this, &GroupWidget::onManageChannelsClicked);
    toolbarLayout->addWidget(manageChannelsBtn);

    toolbarLayout->addStretch();

    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 12px;"));
    toolbarLayout->addWidget(statusLabel_);

    mainLayout->addLayout(toolbarLayout);

    // 分组卡片容器 - 使用网格布局（一行两个）
    cardsContainer_ = new QWidget(this);
    cardsLayout_ = new QGridLayout(cardsContainer_);
    cardsLayout_->setContentsMargins(0, 0, 0, 0);
    cardsLayout_->setSpacing(10);
    cardsLayout_->setColumnStretch(0, 1);
    cardsLayout_->setColumnStretch(1, 1);

    mainLayout->addWidget(cardsContainer_, 1);

    // 提示
    QLabel *helpLabel = new QLabel(
        QStringLiteral("提示：点击分组卡片管理通道绑定，分组通过绑定特定通道来控制设备"),
        this);
    helpLabel->setStyleSheet(QStringLiteral(
        "color: #7f8c8d; font-size: 11px; padding: 4px;"));
    helpLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(helpLabel);
}

void GroupWidget::clearGroupCards()
{
    for (GroupCard *card : groupCards_) {
        cardsLayout_->removeWidget(card);
        delete card;
    }
    groupCards_.clear();
}

void GroupWidget::refreshGroupList()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        if (statusLabel_) {
            statusLabel_->setText(QStringLiteral("[警告] 未连接服务器"));
        }
        return;
    }

    if (statusLabel_) {
        statusLabel_->setText(QStringLiteral("正在刷新..."));
    }

    QJsonValue result = rpcClient_->call(QStringLiteral("group.list"));
    
    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.contains(QStringLiteral("groups"))) {
            QJsonArray groups = obj.value(QStringLiteral("groups")).toArray();
            groupsCache_ = groups;
            updateGroupCards(groups);
            if (statusLabel_) {
                statusLabel_->setText(QStringLiteral("共 %1 个分组").arg(groups.size()));
            }
            
            // 获取每个分组的通道信息
            for (const QJsonValue &v : groups) {
                int groupId = v.toObject().value(QStringLiteral("groupId")).toInt();
                fetchGroupChannels(groupId);
            }
            return;
        }
    }
    
    if (statusLabel_) {
        statusLabel_->setText(QStringLiteral("[错误] 获取分组列表失败"));
    }
}

void GroupWidget::fetchGroupChannels(int groupId)
{
    QJsonObject params;
    params[QStringLiteral("groupId")] = groupId;
    
    QJsonValue result = rpcClient_->call(QStringLiteral("group.getChannels"), params);
    
    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.value(QStringLiteral("ok")).toBool()) {
            QJsonArray channels = obj.value(QStringLiteral("channels")).toArray();
            
            // 更新对应卡片的通道信息
            for (GroupCard *card : groupCards_) {
                if (card->groupId() == groupId) {
                    // 从缓存中获取分组名称和设备数
                    QString name;
                    int deviceCount = 0;
                    for (const QJsonValue &v : groupsCache_) {
                        QJsonObject g = v.toObject();
                        if (g.value(QStringLiteral("groupId")).toInt() == groupId) {
                            name = g.value(QStringLiteral("name")).toString();
                            deviceCount = g.value(QStringLiteral("deviceCount")).toInt();
                            break;
                        }
                    }
                    card->updateInfo(name, deviceCount, channels.size(), channels);
                    break;
                }
            }
        }
    }
}

void GroupWidget::updateGroupCards(const QJsonArray &groups)
{
    clearGroupCards();

    int row = 0;
    int col = 0;
    
    for (const QJsonValue &value : groups) {
        QJsonObject group = value.toObject();
        int groupId = group.value(QStringLiteral("groupId")).toInt();
        QString name = group.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            name = QStringLiteral("分组-%1").arg(groupId);
        }

        GroupCard *card = new GroupCard(groupId, name, this);
        
        // 连接信号
        connect(card, &GroupCard::controlClicked, this, &GroupWidget::onGroupControlClicked);
        connect(card, &GroupCard::manageClicked, this, &GroupWidget::onManageGroupClicked);
        connect(card, &GroupCard::deleteClicked, this, &GroupWidget::onDeleteGroupFromCard);

        // 设置初始信息
        int deviceCount = group.value(QStringLiteral("deviceCount")).toInt();
        card->updateInfo(name, deviceCount, 0, QJsonArray());

        // 网格布局：一行两个
        cardsLayout_->addWidget(card, row, col);
        groupCards_.append(card);

        col++;
        if (col >= 2) {
            col = 0;
            row++;
        }
    }
    
    // 添加弹性空间
    cardsLayout_->setRowStretch(row + 1, 1);
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
    
    if (name.isEmpty()) {
        name = QStringLiteral("分组-%1").arg(groupId);
    }

    QJsonObject params;
    params[QStringLiteral("groupId")] = groupId;
    params[QStringLiteral("name")] = name;

    QJsonValue result = rpcClient_->call(QStringLiteral("group.create"), params);
    
    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        if (statusLabel_) {
            statusLabel_->setText(QStringLiteral("分组 %1 创建成功").arg(groupId));
        }
        emit logMessage(QStringLiteral("创建分组成功: %1").arg(name));
        refreshGroupList();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), 
            QStringLiteral("创建分组失败: %1").arg(error));
    }
}

void GroupWidget::onDeleteGroupClicked()
{
    // 通过对话框选择要删除的分组
    QMessageBox::information(this, QStringLiteral("提示"), 
        QStringLiteral("请点击分组卡片右下角的删除按钮来删除分组"));
}

void GroupWidget::onDeleteGroupFromCard(int groupId)
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, 
        QStringLiteral("确认删除"),
        QStringLiteral("确定要删除分组 %1 吗？").arg(groupId),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QJsonObject params;
    params[QStringLiteral("groupId")] = groupId;

    QJsonValue result = rpcClient_->call(QStringLiteral("group.delete"), params);
    
    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        if (statusLabel_) {
            statusLabel_->setText(QStringLiteral("分组 %1 删除成功").arg(groupId));
        }
        emit logMessage(QStringLiteral("删除分组成功: %1").arg(groupId));
        refreshGroupList();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), 
            QStringLiteral("删除分组失败: %1").arg(error));
    }
}

void GroupWidget::onManageChannelsClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }
    
    // 创建通道管理弹窗
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("管理分组通道"));
    dialog.setMinimumWidth(400);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    QLabel *infoLabel = new QLabel(
        QStringLiteral("分组通过绑定特定设备的特定通道来工作\n策略执行时只控制已绑定的通道"), &dialog);
    infoLabel->setStyleSheet(QStringLiteral(
        "color: #666; font-size: 12px; padding: 8px; "
        "background-color: #e3f2fd; border-radius: 6px;"));
    layout->addWidget(infoLabel);
    
    QFormLayout *formLayout = new QFormLayout();
    
    QSpinBox *groupIdSpinBox = new QSpinBox(&dialog);
    groupIdSpinBox->setRange(1, 999);
    groupIdSpinBox->setValue(selectedGroupId_);
    groupIdSpinBox->setMinimumHeight(40);
    formLayout->addRow(QStringLiteral("分组ID:"), groupIdSpinBox);
    
    QSpinBox *nodeIdSpinBox = new QSpinBox(&dialog);
    nodeIdSpinBox->setRange(1, 255);
    nodeIdSpinBox->setValue(1);
    nodeIdSpinBox->setMinimumHeight(40);
    formLayout->addRow(QStringLiteral("设备节点ID:"), nodeIdSpinBox);
    
    QSpinBox *channelSpinBox = new QSpinBox(&dialog);
    channelSpinBox->setRange(0, 3);
    channelSpinBox->setValue(0);
    channelSpinBox->setMinimumHeight(40);
    formLayout->addRow(QStringLiteral("通道号:"), channelSpinBox);
    
    layout->addLayout(formLayout);
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    
    QPushButton *addBtn = new QPushButton(QStringLiteral("添加通道"), &dialog);
    addBtn->setProperty("type", QStringLiteral("success"));
    addBtn->setMinimumHeight(44);
    btnLayout->addWidget(addBtn);
    
    QPushButton *removeBtn = new QPushButton(QStringLiteral("移除通道"), &dialog);
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
    
    // 添加通道
    connect(addBtn, &QPushButton::clicked, [&]() {
        int groupId = groupIdSpinBox->value();
        int nodeId = nodeIdSpinBox->value();
        int channel = channelSpinBox->value();
        
        QJsonObject params;
        params[QStringLiteral("groupId")] = groupId;
        params[QStringLiteral("node")] = nodeId;
        params[QStringLiteral("channel")] = channel;
        
        QJsonValue result = rpcClient_->call(QStringLiteral("group.addChannel"), params);
        
        if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
            resultLabel->setText(QStringLiteral("节点%1:通道%2 已添加到分组 %3")
                .arg(nodeId).arg(channel).arg(groupId));
            resultLabel->setStyleSheet(QStringLiteral("color: #27ae60; font-weight: bold;"));
            emit logMessage(QStringLiteral("添加通道成功"));
        } else {
            QString error = result.toObject().value(QStringLiteral("error")).toString();
            resultLabel->setText(QStringLiteral("添加失败: %1").arg(error));
            resultLabel->setStyleSheet(QStringLiteral("color: #e74c3c; font-weight: bold;"));
        }
    });
    
    // 移除通道
    connect(removeBtn, &QPushButton::clicked, [&]() {
        int groupId = groupIdSpinBox->value();
        int nodeId = nodeIdSpinBox->value();
        int channel = channelSpinBox->value();
        
        QJsonObject params;
        params[QStringLiteral("groupId")] = groupId;
        params[QStringLiteral("node")] = nodeId;
        params[QStringLiteral("channel")] = channel;
        
        QJsonValue result = rpcClient_->call(QStringLiteral("group.removeChannel"), params);
        
        if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
            resultLabel->setText(QStringLiteral("节点%1:通道%2 已从分组 %3 移除")
                .arg(nodeId).arg(channel).arg(groupId));
            resultLabel->setStyleSheet(QStringLiteral("color: #27ae60; font-weight: bold;"));
            emit logMessage(QStringLiteral("移除通道成功"));
        } else {
            QString error = result.toObject().value(QStringLiteral("error")).toString();
            resultLabel->setText(QStringLiteral("移除失败: %1").arg(error));
            resultLabel->setStyleSheet(QStringLiteral("color: #e74c3c; font-weight: bold;"));
        }
    });
    
    dialog.exec();
    refreshGroupList();
}

void GroupWidget::onGroupControlClicked(int groupId, const QString &action)
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }
    
    // 控制分组绑定的所有通道
    QJsonObject params;
    params[QStringLiteral("groupId")] = groupId;
    params[QStringLiteral("ch")] = 0;  // 会通过分组通道绑定来控制
    params[QStringLiteral("action")] = action;
    
    QJsonValue result = rpcClient_->call(QStringLiteral("group.control"), params);
    
    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        if (statusLabel_) {
            statusLabel_->setText(QStringLiteral("分组 %1 执行 %2 成功").arg(groupId).arg(action));
        }
        emit logMessage(QStringLiteral("分组控制: %1 -> %2").arg(groupId).arg(action));
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), 
            QStringLiteral("控制失败: %1").arg(error));
    }
}

void GroupWidget::onManageGroupClicked(int groupId)
{
    selectedGroupId_ = groupId;
    onManageChannelsClicked();
}

int GroupWidget::getSelectedGroupId()
{
    return selectedGroupId_;
}
