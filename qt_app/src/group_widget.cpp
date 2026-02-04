/**
 * @file group_widget.cpp
 * @brief 分组管理页面实现 - 卡片式布局（1024x600低分辨率优化版）
 */

#include "group_widget.h"
#include "rpc_client.h"
#include "style_constants.h"

#include <QScroller>

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
#include <QGraphicsDropShadowEffect>

using namespace UIConstants;

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
    setFrameShape(QFrame::NoFrame);
    setStyleSheet(QStringLiteral(
        "#groupCard {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #f8f9fa);"
        "  border: 1px solid #e0e0e0;"
        "  border-radius: %1px;"
        "}"
        "#groupCard:hover {"
        "  border-color: #9b59b6;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #f5eef8);"
        "}").arg(BORDER_RADIUS_CARD));
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(CARD_MIN_HEIGHT);
    
    // 阴影效果
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(8);
    shadow->setColor(QColor(0, 0, 0, 25));
    shadow->setOffset(0, 2);
    setGraphicsEffect(shadow);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(CARD_MARGIN, CARD_MARGIN, CARD_MARGIN, CARD_MARGIN);
    mainLayout->setSpacing(CARD_SPACING);

    // 顶部行：名称和ID
    QHBoxLayout *topRow = new QHBoxLayout();
    
    nameLabel_ = new QLabel(QStringLiteral("[组] %1").arg(name_), this);
    nameLabel_->setStyleSheet(QStringLiteral(
        "font-size: %1px; font-weight: bold; color: #2c3e50;").arg(FONT_SIZE_CARD_TITLE));
    topRow->addWidget(nameLabel_);
    
    topRow->addStretch();
    
    idLabel_ = new QLabel(QStringLiteral("ID:%1").arg(groupId_), this);
    idLabel_->setStyleSheet(QStringLiteral(
        "font-size: %1px; color: #7f8c8d; background-color: #ecf0f1; "
        "padding: 2px 6px; border-radius: 4px;").arg(FONT_SIZE_SMALL));
    topRow->addWidget(idLabel_);
    
    mainLayout->addLayout(topRow);

    // 中间行：设备数和通道数
    QHBoxLayout *middleRow = new QHBoxLayout();
    
    deviceCountLabel_ = new QLabel(QStringLiteral("0设备"), this);
    deviceCountLabel_->setStyleSheet(QStringLiteral(
        "font-size: %1px; color: #3498db;").arg(FONT_SIZE_BODY));
    middleRow->addWidget(deviceCountLabel_);
    
    channelCountLabel_ = new QLabel(QStringLiteral("0通道"), this);
    channelCountLabel_->setStyleSheet(QStringLiteral(
        "font-size: %1px; color: #9b59b6;").arg(FONT_SIZE_BODY));
    middleRow->addWidget(channelCountLabel_);
    
    middleRow->addStretch();
    
    mainLayout->addLayout(middleRow);

    // 通道信息行
    channelsLabel_ = new QLabel(QStringLiteral("暂无绑定"), this);
    channelsLabel_->setStyleSheet(QStringLiteral(
        "font-size: %1px; color: #95a5a6; padding: 4px 6px; "
        "background-color: #f8f9fa; border-radius: 4px;").arg(FONT_SIZE_SMALL));
    channelsLabel_->setWordWrap(true);
    channelsLabel_->setMinimumHeight(24);
    mainLayout->addWidget(channelsLabel_);

    // 底部分隔线
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QStringLiteral("color: #e8e8e8;"));
    line->setMaximumHeight(1);
    mainLayout->addWidget(line);

    // 底部行：控制按钮
    QHBoxLayout *buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(4);
    
    QPushButton *stopBtn = new QPushButton(QStringLiteral("停"), this);
    stopBtn->setMinimumHeight(BTN_HEIGHT_SMALL);
    stopBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #7f8c8d; color: white; border: none; "
        "border-radius: %1px; font-weight: bold; font-size: %2px; padding: 0 8px; }"
        "QPushButton:hover { background-color: #6c7a7d; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_SMALL));
    connect(stopBtn, &QPushButton::clicked, [this]() {
        emit controlClicked(groupId_, QStringLiteral("stop"));
    });
    buttonRow->addWidget(stopBtn);
    
    QPushButton *fwdBtn = new QPushButton(QStringLiteral("正"), this);
    fwdBtn->setMinimumHeight(BTN_HEIGHT_SMALL);
    fwdBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #27ae60; color: white; border: none; "
        "border-radius: %1px; font-weight: bold; font-size: %2px; padding: 0 8px; }"
        "QPushButton:hover { background-color: #229954; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_SMALL));
    connect(fwdBtn, &QPushButton::clicked, [this]() {
        emit controlClicked(groupId_, QStringLiteral("fwd"));
    });
    buttonRow->addWidget(fwdBtn);
    
    QPushButton *revBtn = new QPushButton(QStringLiteral("反"), this);
    revBtn->setMinimumHeight(BTN_HEIGHT_SMALL);
    revBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #f39c12; color: white; border: none; "
        "border-radius: %1px; font-weight: bold; font-size: %2px; padding: 0 8px; }"
        "QPushButton:hover { background-color: #d68910; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_SMALL));
    connect(revBtn, &QPushButton::clicked, [this]() {
        emit controlClicked(groupId_, QStringLiteral("rev"));
    });
    buttonRow->addWidget(revBtn);
    
    QPushButton *deleteBtn = new QPushButton(QStringLiteral("删"), this);
    deleteBtn->setMinimumHeight(BTN_HEIGHT_SMALL);
    deleteBtn->setMinimumWidth(BTN_MIN_WIDTH_SMALL);
    deleteBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #e74c3c; color: white; border: none; "
        "border-radius: %1px; font-weight: bold; font-size: %2px; }"
        "QPushButton:hover { background-color: #c0392b; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_BODY));
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
    nameLabel_->setText(QStringLiteral("[组] %1").arg(name));
    deviceCountLabel_->setText(QStringLiteral("%1设备").arg(deviceCount));
    channelCountLabel_->setText(QStringLiteral("%1通道").arg(channelCount));
    
    if (channels.isEmpty()) {
        channelsLabel_->setText(QStringLiteral("暂无绑定"));
        channelsLabel_->setStyleSheet(QStringLiteral(
            "font-size: %1px; color: #95a5a6; padding: 4px 6px; "
            "background-color: #f8f9fa; border-radius: 4px;").arg(FONT_SIZE_SMALL));
    } else {
        QStringList channelTexts;
        for (const QJsonValue &v : channels) {
            QJsonObject ch = v.toObject();
            int node = ch.value(QStringLiteral("node")).toInt();
            int channel = ch.value(QStringLiteral("channel")).toInt();
            channelTexts << QStringLiteral("%1:%2").arg(node).arg(channel);
        }
        channelsLabel_->setText(channelTexts.join(QStringLiteral(",")));
        channelsLabel_->setStyleSheet(QStringLiteral(
            "font-size: %1px; color: #2c3e50; padding: 4px 6px; "
            "background-color: #e8f5e9; border-radius: 4px;").arg(FONT_SIZE_SMALL));
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
    qDebug() << "[GROUP_WIDGET] 分组页面初始化完成";
}

void GroupWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(PAGE_MARGIN, PAGE_MARGIN, PAGE_MARGIN, PAGE_MARGIN);
    mainLayout->setSpacing(PAGE_SPACING);

    // 页面标题
    QLabel *titleLabel = new QLabel(QStringLiteral("[组] 分组管理"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: %1px; font-weight: bold; color: #2c3e50; padding: 2px 0;").arg(FONT_SIZE_TITLE));
    mainLayout->addWidget(titleLabel);

    // 工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(CARD_SPACING);

    QPushButton *refreshButton = new QPushButton(QStringLiteral("[刷]刷新"), this);
    refreshButton->setMinimumHeight(BTN_HEIGHT);
    refreshButton->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #3498db; color: white; border: none; "
        "border-radius: %1px; padding: 0 12px; font-weight: bold; font-size: %2px; }"
        "QPushButton:hover { background-color: #2980b9; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_BODY));
    connect(refreshButton, &QPushButton::clicked, this, &GroupWidget::refreshGroupList);
    toolbarLayout->addWidget(refreshButton);

    QPushButton *createButton = new QPushButton(QStringLiteral("[+]创建"), this);
    createButton->setMinimumHeight(BTN_HEIGHT);
    createButton->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #27ae60; color: white; border: none; "
        "border-radius: %1px; padding: 0 12px; font-weight: bold; font-size: %2px; }"
        "QPushButton:hover { background-color: #229954; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_BODY));
    connect(createButton, &QPushButton::clicked, this, &GroupWidget::onCreateGroupClicked);
    toolbarLayout->addWidget(createButton);

    QPushButton *manageChannelsBtn = new QPushButton(QStringLiteral("[置]管理"), this);
    manageChannelsBtn->setMinimumHeight(BTN_HEIGHT);
    manageChannelsBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #f39c12; color: white; border: none; "
        "border-radius: %1px; padding: 0 12px; font-weight: bold; font-size: %2px; }"
        "QPushButton:hover { background-color: #d68910; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_BODY));
    connect(manageChannelsBtn, &QPushButton::clicked, this, &GroupWidget::onManageChannelsClicked);
    toolbarLayout->addWidget(manageChannelsBtn);

    toolbarLayout->addStretch();

    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet(QStringLiteral(
        "color: #7f8c8d; font-size: %1px; padding: 4px 8px; background-color: #f8f9fa; border-radius: 4px;").arg(FONT_SIZE_SMALL));
    toolbarLayout->addWidget(statusLabel_);

    mainLayout->addLayout(toolbarLayout);

    // 滚动区域
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { width: %1px; background: #f0f0f0; border-radius: %2px; margin: 2px; }"
        "QScrollBar::handle:vertical { background: #c0c0c0; border-radius: %2px; min-height: 30px; }"
    ).arg(SCROLLBAR_WIDTH).arg(SCROLLBAR_WIDTH/2));
    
    // 启用触控滚动
    QScroller::grabGesture(scrollArea->viewport(), QScroller::LeftMouseButtonGesture);

    // 分组卡片容器
    cardsContainer_ = new QWidget();
    cardsContainer_->setStyleSheet(QStringLiteral("background: transparent;"));
    cardsLayout_ = new QGridLayout(cardsContainer_);
    cardsLayout_->setContentsMargins(0, 0, 0, 0);
    cardsLayout_->setSpacing(PAGE_SPACING);
    cardsLayout_->setColumnStretch(0, 1);
    cardsLayout_->setColumnStretch(1, 1);
    
    scrollArea->setWidget(cardsContainer_);
    mainLayout->addWidget(scrollArea, 1);

    // 提示
    QLabel *helpLabel = new QLabel(
        QStringLiteral("[示] 点击卡片管理通道绑定"),
        this);
    helpLabel->setStyleSheet(QStringLiteral(
        "color: #5d6d7e; font-size: %1px; padding: 6px; background-color: #eaf2f8; border-radius: 4px;").arg(FONT_SIZE_SMALL));
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
            statusLabel_->setText(QStringLiteral("[X] 未连接服务器"));
        }
        return;
    }

    if (statusLabel_) {
        statusLabel_->setText(QStringLiteral("[刷] 刷新中..."));
    }

    qDebug() << "[GROUP_WIDGET] 刷新分组列表";
    
    QJsonValue result = rpcClient_->call(QStringLiteral("group.list"));
    
    qDebug() << "[GROUP_WIDGET] group.list 响应:" << QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact);
    
    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.contains(QStringLiteral("groups"))) {
            QJsonArray groups = obj.value(QStringLiteral("groups")).toArray();
            groupsCache_ = groups;
            updateGroupCards(groups);
            if (statusLabel_) {
                statusLabel_->setText(QStringLiteral("[OK] 共 %1 个分组").arg(groups.size()));
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
        statusLabel_->setText(QStringLiteral("[X] 获取失败"));
    }
}

void GroupWidget::fetchGroupChannels(int groupId)
{
    QJsonObject params;
    params[QStringLiteral("groupId")] = groupId;
    
    // 使用 group.get 方法获取分组详情
    QJsonValue result = rpcClient_->call(QStringLiteral("group.get"), params);
    
    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.value(QStringLiteral("ok")).toBool()) {
            QJsonArray channels = obj.value(QStringLiteral("channels")).toArray();
            QString name = obj.value(QStringLiteral("name")).toString();
            int deviceCount = obj.value(QStringLiteral("deviceCount")).toInt();
            
            // 更新对应卡片的通道信息
            for (GroupCard *card : groupCards_) {
                if (card->groupId() == groupId) {
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

    // 创建弹窗 - 触控友好
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("创建分组"));
    dialog.setMinimumWidth(350);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QFormLayout *formLayout = new QFormLayout();
    
    QSpinBox *groupIdSpinBox = new QSpinBox(&dialog);
    groupIdSpinBox->setRange(1, 999);
    groupIdSpinBox->setValue(selectedGroupId_);
    groupIdSpinBox->setMinimumHeight(44);
    groupIdSpinBox->setStyleSheet(QStringLiteral(
        "QSpinBox { border: 2px solid #e0e0e0; border-radius: 8px; padding: 6px 12px; font-size: 15px; }"
        "QSpinBox:focus { border-color: #9b59b6; }"));
    formLayout->addRow(QStringLiteral("分组ID:"), groupIdSpinBox);
    
    QLineEdit *nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText(QStringLiteral("分组-1"));
    nameEdit->setText(QStringLiteral("分组-1"));
    nameEdit->setMinimumHeight(44);
    nameEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { border: 2px solid #e0e0e0; border-radius: 8px; padding: 6px 12px; font-size: 15px; }"
        "QLineEdit:focus { border-color: #9b59b6; }"));
    formLayout->addRow(QStringLiteral("名称:"), nameEdit);
    
    layout->addLayout(formLayout);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("创建"));
    buttonBox->button(QDialogButtonBox::Ok)->setMinimumHeight(44);
    buttonBox->button(QDialogButtonBox::Ok)->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #27ae60; color: white; border: none; "
        "border-radius: 8px; padding: 0 24px; font-weight: bold; font-size: 14px; }"
        "QPushButton:hover { background-color: #229954; }"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    buttonBox->button(QDialogButtonBox::Cancel)->setMinimumHeight(44);
    buttonBox->button(QDialogButtonBox::Cancel)->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #95a5a6; color: white; border: none; "
        "border-radius: 8px; padding: 0 24px; font-weight: bold; font-size: 14px; }"
        "QPushButton:hover { background-color: #7f8c8d; }"));
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

    qDebug() << "[GROUP_WIDGET] 创建分组:" << name << "groupId=" << groupId;
    
    QJsonValue result = rpcClient_->call(QStringLiteral("group.create"), params);
    
    qDebug() << "[GROUP_WIDGET] group.create 响应:" << QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact);
    
    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        if (statusLabel_) {
            statusLabel_->setText(QStringLiteral("[OK] 分组 %1 创建成功").arg(groupId));
        }
        emit logMessage(QStringLiteral("创建分组成功: %1").arg(name));
        refreshGroupList();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), 
            QStringLiteral("[X] 创建分组失败: %1").arg(error));
    }
}

void GroupWidget::onDeleteGroupClicked()
{
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

    qDebug() << "[GROUP_WIDGET] 删除分组 groupId=" << groupId;
    
    QJsonValue result = rpcClient_->call(QStringLiteral("group.delete"), params);
    
    qDebug() << "[GROUP_WIDGET] group.delete 响应:" << QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact);
    
    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        if (statusLabel_) {
            statusLabel_->setText(QStringLiteral("[OK] 分组 %1 删除成功").arg(groupId));
        }
        emit logMessage(QStringLiteral("删除分组成功: %1").arg(groupId));
        refreshGroupList();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), 
            QStringLiteral("[X] 删除分组失败: %1").arg(error));
    }
}

void GroupWidget::onManageChannelsClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }
    
    // 创建通道管理弹窗 - 触控友好
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("管理分组通道"));
    dialog.setMinimumWidth(420);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    QLabel *infoLabel = new QLabel(
        QStringLiteral("[示] 分组通过绑定特定设备的特定通道来工作\n策略执行时只控制已绑定的通道"), &dialog);
    infoLabel->setStyleSheet(QStringLiteral(
        "color: #5d6d7e; font-size: 13px; padding: 12px; "
        "background-color: #eaf2f8; border-radius: 8px;"));
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);
    
    QFormLayout *formLayout = new QFormLayout();
    formLayout->setSpacing(12);
    
    QSpinBox *groupIdSpinBox = new QSpinBox(&dialog);
    groupIdSpinBox->setRange(1, 999);
    groupIdSpinBox->setValue(selectedGroupId_);
    groupIdSpinBox->setMinimumHeight(44);
    groupIdSpinBox->setStyleSheet(QStringLiteral(
        "QSpinBox { border: 2px solid #e0e0e0; border-radius: 8px; padding: 6px 12px; font-size: 15px; }"));
    formLayout->addRow(QStringLiteral("分组ID:"), groupIdSpinBox);
    
    QSpinBox *nodeIdSpinBox = new QSpinBox(&dialog);
    nodeIdSpinBox->setRange(1, 255);
    nodeIdSpinBox->setValue(1);
    nodeIdSpinBox->setMinimumHeight(44);
    nodeIdSpinBox->setStyleSheet(groupIdSpinBox->styleSheet());
    formLayout->addRow(QStringLiteral("设备节点ID:"), nodeIdSpinBox);
    
    QSpinBox *channelSpinBox = new QSpinBox(&dialog);
    channelSpinBox->setRange(0, 3);
    channelSpinBox->setValue(0);
    channelSpinBox->setMinimumHeight(44);
    channelSpinBox->setStyleSheet(groupIdSpinBox->styleSheet());
    formLayout->addRow(QStringLiteral("通道号:"), channelSpinBox);
    
    layout->addLayout(formLayout);
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    
    QPushButton *addBtn = new QPushButton(QStringLiteral("[+] 添加通道"), &dialog);
    addBtn->setMinimumHeight(48);
    addBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #27ae60; color: white; border: none; "
        "border-radius: 10px; padding: 0 24px; font-weight: bold; font-size: 14px; }"
        "QPushButton:hover { background-color: #229954; }"));
    btnLayout->addWidget(addBtn);
    
    QPushButton *removeBtn = new QPushButton(QStringLiteral("[-] 移除通道"), &dialog);
    removeBtn->setMinimumHeight(48);
    removeBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #e74c3c; color: white; border: none; "
        "border-radius: 10px; padding: 0 24px; font-weight: bold; font-size: 14px; }"
        "QPushButton:hover { background-color: #c0392b; }"));
    btnLayout->addWidget(removeBtn);
    
    layout->addLayout(btnLayout);
    
    QLabel *resultLabel = new QLabel(&dialog);
    resultLabel->setAlignment(Qt::AlignCenter);
    resultLabel->setMinimumHeight(36);
    resultLabel->setStyleSheet(QStringLiteral("font-size: 13px; padding: 8px; border-radius: 6px;"));
    layout->addWidget(resultLabel);
    
    QPushButton *closeBtn = new QPushButton(QStringLiteral("关闭"), &dialog);
    closeBtn->setMinimumHeight(44);
    closeBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #95a5a6; color: white; border: none; "
        "border-radius: 10px; padding: 0 24px; font-weight: bold; font-size: 14px; }"
        "QPushButton:hover { background-color: #7f8c8d; }"));
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
        
        qDebug() << "[GROUP_WIDGET] 添加通道 groupId=" << groupId << "node=" << nodeId << "channel=" << channel;
        
        QJsonValue result = rpcClient_->call(QStringLiteral("group.addChannel"), params);
        
        if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
            resultLabel->setText(QStringLiteral("[OK] 节点%1:通道%2 已添加到分组 %3")
                .arg(nodeId).arg(channel).arg(groupId));
            resultLabel->setStyleSheet(QStringLiteral(
                "color: #155724; background-color: #d4edda; font-weight: bold;"));
            emit logMessage(QStringLiteral("添加通道成功"));
        } else {
            QString error = result.toObject().value(QStringLiteral("error")).toString();
            resultLabel->setText(QStringLiteral("[X] 添加失败: %1").arg(error));
            resultLabel->setStyleSheet(QStringLiteral(
                "color: #721c24; background-color: #f8d7da; font-weight: bold;"));
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
        
        qDebug() << "[GROUP_WIDGET] 移除通道 groupId=" << groupId << "node=" << nodeId << "channel=" << channel;
        
        QJsonValue result = rpcClient_->call(QStringLiteral("group.removeChannel"), params);
        
        if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
            resultLabel->setText(QStringLiteral("[OK] 节点%1:通道%2 已从分组 %3 移除")
                .arg(nodeId).arg(channel).arg(groupId));
            resultLabel->setStyleSheet(QStringLiteral(
                "color: #155724; background-color: #d4edda; font-weight: bold;"));
            emit logMessage(QStringLiteral("移除通道成功"));
        } else {
            QString error = result.toObject().value(QStringLiteral("error")).toString();
            resultLabel->setText(QStringLiteral("[X] 移除失败: %1").arg(error));
            resultLabel->setStyleSheet(QStringLiteral(
                "color: #721c24; background-color: #f8d7da; font-weight: bold;"));
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
    params[QStringLiteral("ch")] = -1;  // 会通过分组通道绑定来控制
    params[QStringLiteral("action")] = action;

    qDebug() << "[GROUP_WIDGET] 分组控制 groupId=" << groupId << "action=" << action;
    
    QJsonValue result = rpcClient_->call(QStringLiteral("group.control"), params);
    
    qDebug() << "[GROUP_WIDGET] group.control 响应:" << QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact);
    
    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        if (statusLabel_) {
            statusLabel_->setText(QStringLiteral("[OK] 分组 %1 执行 %2 成功").arg(groupId).arg(action));
        }
        emit logMessage(QStringLiteral("分组控制: %1 -> %2").arg(groupId).arg(action));
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), 
            QStringLiteral("[X] 控制失败: %1").arg(error));
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
