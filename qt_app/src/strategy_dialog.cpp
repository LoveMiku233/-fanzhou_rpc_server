/**
 * @file strategy_dialog.cpp
 * @brief 策略编辑对话框实现 - 带滚动区域和动画效果（1024x600低分辨率优化版）
 */

#include "strategy_dialog.h"
#include "style_constants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QDoubleSpinBox>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>
#include <QScroller>
#include <QScrollBar>
#include <QGestureEvent>

using namespace UIConstants;

StrategyDialog::StrategyDialog(QWidget *parent)
    : QDialog(parent)
    , idSpinBox_(nullptr)
    , nameEdit_(nullptr)
    , typeCombo_(nullptr)
    , enabledCheck_(nullptr)
    , actionsTable_(nullptr)
    , actionNodeSpin_(nullptr)
    , actionChSpin_(nullptr)
    , actionValueCombo_(nullptr)
    , conditionsTable_(nullptr)
    , condDeviceEdit_(nullptr)
    , condIdEdit_(nullptr)
    , condOpCombo_(nullptr)
    , condValueSpin_(nullptr)
    , m_popupOpacity(0.0)
    , m_isEdit(false)
{
    setupUi();
    setupAnimations();
    
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    
    qDebug() << "[STRATEGY_DIALOG] 对话框初始化完成";
}

void StrategyDialog::setupUi()
{
    qDebug() << "[STRATEGY_DIALOG] 设置UI...";
    
    // 主布局 - 减小边距适配低分辨率
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(DIALOG_MARGIN, DIALOG_MARGIN, DIALOG_MARGIN, DIALOG_MARGIN);
    mainLayout->setSpacing(0);
    
    // 内容容器
    QWidget *contentWidget = new QWidget(this);
    contentWidget->setObjectName(QStringLiteral("contentWidget"));
    contentWidget->setStyleSheet(QStringLiteral(
        "#contentWidget {"
        "  background-color: white;"
        "  border-radius: %1px;"
        "}").arg(BORDER_RADIUS_DIALOG));
    
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 80));
    shadow->setOffset(0, 4);
    contentWidget->setGraphicsEffect(shadow);
    
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    
    // 标题栏 - 减小高度
    QWidget *titleBar = new QWidget(contentWidget);
    titleBar->setFixedHeight(BTN_HEIGHT_LARGE);
    titleBar->setStyleSheet(QStringLiteral(
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3498db, stop:1 #2980b9); "
        "border-top-left-radius: %1px; border-top-right-radius: %1px;").arg(BORDER_RADIUS_DIALOG));
    
    QHBoxLayout *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(12, 0, 8, 0);
    
    QLabel *titleLabel = new QLabel(contentWidget);
    titleLabel->setObjectName(QStringLiteral("dialogTitle"));
    titleLabel->setStyleSheet(QStringLiteral(
        "color: white; font-size: %1px; font-weight: bold; background: transparent;").arg(FONT_SIZE_CARD_TITLE));
    titleLayout->addWidget(titleLabel);
    
    QPushButton *closeBtn = new QPushButton(QStringLiteral("[X]"), titleBar);
    closeBtn->setFixedSize(BTN_HEIGHT_SMALL, BTN_HEIGHT_SMALL);
    closeBtn->setStyleSheet(QStringLiteral(
        "QPushButton { color: white; font-size: %1px; border: none; background: transparent; }"
        "QPushButton:hover { background-color: rgba(255,255,255,0.25); border-radius: %2px; }")
        .arg(FONT_SIZE_BODY).arg(BTN_HEIGHT_SMALL/2));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    titleLayout->addWidget(closeBtn);
    
    contentLayout->addWidget(titleBar);
    
    // ===== 滚动区域 - 优化高度 =====
    QScrollArea *scrollArea = new QScrollArea(contentWidget);
    scrollArea->setObjectName(QStringLiteral("strategyScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setMinimumHeight(300);
    scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { width: %1px; background: #f8f9fa; border-radius: %2px; margin: 2px; }"
        "QScrollBar::handle:vertical { background: #bdc3c7; border-radius: %2px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: #95a5a6; }"
        "QScrollBar::handle:vertical:pressed { background: #7f8c8d; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ).arg(SCROLLBAR_WIDTH).arg(SCROLLBAR_WIDTH/2));
    
    // 启用触控手势支持
    QScroller::grabGesture(scrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    scrollArea->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
    
    // 滚动内容
    QWidget *scrollContent = new QWidget();
    scrollContent->setObjectName(QStringLiteral("scrollContent"));
    scrollContent->setStyleSheet(QStringLiteral(
        "#scrollContent { background-color: #f8f9fa; }"
    ));
    
    QVBoxLayout *formLayout = new QVBoxLayout(scrollContent);
    formLayout->setContentsMargins(DIALOG_MARGIN, DIALOG_MARGIN, DIALOG_MARGIN, DIALOG_MARGIN);
    formLayout->setSpacing(DIALOG_SPACING);
    
    // ===== 基本信息组 =====
    QGroupBox *basicGroup = new QGroupBox(QStringLiteral("基本信息"), scrollContent);
    basicGroup->setStyleSheet(QStringLiteral(
        "QGroupBox { font-weight: bold; font-size: %1px; border: 1px solid #e0e0e0; border-radius: %2px; margin-top: 10px; padding-top: 12px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 8px; color: #3498db; background-color: #f8f9fa; font-size: %3px; }"
    ).arg(FONT_SIZE_BODY).arg(BORDER_RADIUS_CARD).arg(FONT_SIZE_SMALL));
    
    QFormLayout *basicLayout = new QFormLayout(basicGroup);
    basicLayout->setSpacing(DIALOG_SPACING);
    basicLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    basicLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    
    // 输入控件通用样式
    QString inputStyle = QStringLiteral(
        "border: 1px solid #e0e0e0; border-radius: %1px; padding: 4px 8px; font-size: %2px;")
        .arg(BORDER_RADIUS_INPUT).arg(FONT_SIZE_BODY);
    QString inputFocusStyle = inputStyle + QStringLiteral(" QSpinBox:focus, QLineEdit:focus, QComboBox:focus { border-color: #3498db; }");
    
    // 策略ID
    idSpinBox_ = new QSpinBox(basicGroup);
    idSpinBox_->setRange(1, 9999);
    idSpinBox_->setValue(1);
    idSpinBox_->setFixedHeight(INPUT_HEIGHT);
    idSpinBox_->setStyleSheet(inputStyle);
    basicLayout->addRow(QStringLiteral("策略ID:"), idSpinBox_);
    
    // 名称
    nameEdit_ = new QLineEdit(basicGroup);
    nameEdit_->setPlaceholderText(QStringLiteral("策略-1"));
    nameEdit_->setText(QStringLiteral("策略-1"));
    nameEdit_->setFixedHeight(INPUT_HEIGHT);
    nameEdit_->setStyleSheet(inputStyle);
    basicLayout->addRow(QStringLiteral("名称:"), nameEdit_);
    
    // 类型
    typeCombo_ = new QComboBox(basicGroup);
    typeCombo_->addItem(QStringLiteral("自动触发"), QStringLiteral("auto"));
    typeCombo_->addItem(QStringLiteral("手动触发"), QStringLiteral("manual"));
    typeCombo_->setCurrentIndex(0);
    typeCombo_->setFixedHeight(INPUT_HEIGHT);
    typeCombo_->setStyleSheet(inputStyle);
    basicLayout->addRow(QStringLiteral("触发类型:"), typeCombo_);
    
    // 状态
    enabledCheck_ = new QCheckBox(QStringLiteral("启用此策略"), basicGroup);
    enabledCheck_->setChecked(true);
    enabledCheck_->setStyleSheet(QStringLiteral(
        "QCheckBox { font-size: %1px; spacing: 8px; }"
        "QCheckBox::indicator { width: 20px; height: 20px; border-radius: 4px; border: 1px solid #d0d5dd; }"
        "QCheckBox::indicator:checked { background-color: #27ae60; border-color: #27ae60; }"
    ).arg(FONT_SIZE_BODY));
    basicLayout->addRow(QStringLiteral("状态:"), enabledCheck_);
    
    formLayout->addWidget(basicGroup);
    
    // ===== 执行动作组 =====
    QGroupBox *actionsGroup = new QGroupBox(QStringLiteral("执行动作"), scrollContent);
    actionsGroup->setStyleSheet(basicGroup->styleSheet());
    
    QVBoxLayout *actionsLayout = new QVBoxLayout(actionsGroup);
    actionsLayout->setSpacing(DIALOG_SPACING);
    
    // 说明标签
    QLabel *actionTip = new QLabel(QStringLiteral("[示] 添加需要控制的设备通道"));
    actionTip->setStyleSheet(QStringLiteral("color: #5d6d7e; font-size: %1px; padding: 4px; background-color: #eaf2f8; border-radius: 4px;").arg(FONT_SIZE_SMALL));
    actionTip->setWordWrap(true);
    actionsLayout->addWidget(actionTip);
    
    // 动作表格 - 优化大小
    actionsTable_ = new QTableWidget(0, 4, actionsGroup);
    actionsTable_->setHorizontalHeaderLabels(QStringList() 
        << QStringLiteral("节点") << QStringLiteral("通道") << QStringLiteral("动作") << QStringLiteral("操作"));
    actionsTable_->horizontalHeader()->setStretchLastSection(true);
    actionsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    actionsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    actionsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    actionsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    actionsTable_->setMaximumHeight(TABLE_MAX_HEIGHT);
    actionsTable_->setMinimumHeight(TABLE_MIN_HEIGHT);
    actionsTable_->verticalHeader()->setDefaultSectionSize(TABLE_ROW_HEIGHT);
    actionsTable_->setStyleSheet(QStringLiteral(
        "QTableWidget { border: 1px solid #e0e0e0; border-radius: %1px; gridline-color: #f0f0f0; font-size: %2px; }"
        "QHeaderView::section { background-color: #ecf0f1; padding: 6px; border: none; border-bottom: 1px solid #d0d5dd; font-weight: bold; font-size: %3px; }"
        "QTableWidget::item { padding: 4px; }"
        "QTableWidget::item:selected { background-color: #d6eaf8; color: #2874a6; }"
    ).arg(BORDER_RADIUS_INPUT).arg(FONT_SIZE_SMALL).arg(FONT_SIZE_SMALL));
    actionsLayout->addWidget(actionsTable_);
    
    // 添加动作行
    QHBoxLayout *actionInputLayout = new QHBoxLayout();
    actionInputLayout->setSpacing(CARD_SPACING);
    
    // 节点
    actionNodeSpin_ = new QSpinBox(actionsGroup);
    actionNodeSpin_->setRange(1, 255);
    actionNodeSpin_->setValue(1);
    actionNodeSpin_->setFixedHeight(BTN_HEIGHT);
    actionNodeSpin_->setStyleSheet(inputStyle);
    
    // 通道
    actionChSpin_ = new QSpinBox(actionsGroup);
    actionChSpin_->setRange(0, 3);
    actionChSpin_->setValue(0);
    actionChSpin_->setFixedHeight(BTN_HEIGHT);
    actionChSpin_->setStyleSheet(inputStyle);
    
    // 动作
    actionValueCombo_ = new QComboBox(actionsGroup);
    actionValueCombo_->addItem(QStringLiteral("停止"), 0);
    actionValueCombo_->addItem(QStringLiteral("正转"), 1);
    actionValueCombo_->addItem(QStringLiteral("反转"), 2);
    actionValueCombo_->setCurrentIndex(1);
    actionValueCombo_->setFixedHeight(BTN_HEIGHT);
    actionValueCombo_->setStyleSheet(inputStyle);
    
    QPushButton *addActionBtn = new QPushButton(QStringLiteral("[+]添加"), actionsGroup);
    addActionBtn->setFixedHeight(BTN_HEIGHT);
    addActionBtn->setCursor(Qt::PointingHandCursor);
    addActionBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #27ae60; color: white; border: none; "
        "border-radius: %1px; padding: 0 12px; font-weight: bold; font-size: %2px; }"
        "QPushButton:hover { background-color: #229954; }"
    ).arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_SMALL));
    connect(addActionBtn, &QPushButton::clicked, this, &StrategyDialog::onAddAction);
    
    actionInputLayout->addWidget(new QLabel(QStringLiteral("节点:")));
    actionInputLayout->addWidget(actionNodeSpin_);
    actionInputLayout->addWidget(new QLabel(QStringLiteral("通道:")));
    actionInputLayout->addWidget(actionChSpin_);
    actionInputLayout->addWidget(new QLabel(QStringLiteral("动作:")));
    actionInputLayout->addWidget(actionValueCombo_);
    actionInputLayout->addWidget(addActionBtn);
    actionInputLayout->addStretch();
    
    actionsLayout->addLayout(actionInputLayout);
    formLayout->addWidget(actionsGroup);
    
    // ===== 触发条件组 =====
    QGroupBox *conditionsGroup = new QGroupBox(QStringLiteral("触发条件(可选)"), scrollContent);
    conditionsGroup->setStyleSheet(basicGroup->styleSheet());
    
    QVBoxLayout *conditionsLayout = new QVBoxLayout(conditionsGroup);
    conditionsLayout->setSpacing(DIALOG_SPACING);
    
    QLabel *condTip = new QLabel(QStringLiteral("[示] 添加传感器条件"));
    condTip->setStyleSheet(QStringLiteral("color: #5d6d7e; font-size: %1px; padding: 4px; background-color: #eaf2f8; border-radius: 4px;").arg(FONT_SIZE_SMALL));
    condTip->setWordWrap(true);
    conditionsLayout->addWidget(condTip);
    
    conditionsTable_ = new QTableWidget(0, 5, conditionsGroup);
    conditionsTable_->setHorizontalHeaderLabels(QStringList() 
        << QStringLiteral("设备") << QStringLiteral("标识") << QStringLiteral("操作") << QStringLiteral("值") << QStringLiteral("删除"));
    conditionsTable_->horizontalHeader()->setStretchLastSection(true);
    conditionsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    conditionsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    conditionsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    conditionsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    conditionsTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    conditionsTable_->setMaximumHeight(TABLE_MAX_HEIGHT);
    conditionsTable_->setMinimumHeight(TABLE_MIN_HEIGHT);
    conditionsTable_->verticalHeader()->setDefaultSectionSize(TABLE_ROW_HEIGHT);
    conditionsTable_->setStyleSheet(actionsTable_->styleSheet());
    conditionsLayout->addWidget(conditionsTable_);
    
    QHBoxLayout *condInputLayout = new QHBoxLayout();
    condInputLayout->setSpacing(CARD_SPACING);
    
    // 设备名
    condDeviceEdit_ = new QLineEdit(conditionsGroup);
    condDeviceEdit_->setPlaceholderText(QStringLiteral("sensor1"));
    condDeviceEdit_->setText(QStringLiteral("sensor1"));
    condDeviceEdit_->setFixedHeight(BTN_HEIGHT);
    condDeviceEdit_->setStyleSheet(inputStyle);
    condDeviceEdit_->setMaximumWidth(100);
    
    // 标识符
    condIdEdit_ = new QLineEdit(conditionsGroup);
    condIdEdit_->setPlaceholderText(QStringLiteral("temp"));
    condIdEdit_->setText(QStringLiteral("temperature"));
    condIdEdit_->setFixedHeight(BTN_HEIGHT);
    condIdEdit_->setStyleSheet(inputStyle);
    condIdEdit_->setMaximumWidth(100);
    
    // 操作符
    condOpCombo_ = new QComboBox(conditionsGroup);
    condOpCombo_->addItem(QStringLiteral(">"), QStringLiteral("gt"));
    condOpCombo_->addItem(QStringLiteral("<"), QStringLiteral("lt"));
    condOpCombo_->addItem(QStringLiteral("="), QStringLiteral("eq"));
    condOpCombo_->addItem(QStringLiteral(">="), QStringLiteral("egt"));
    condOpCombo_->addItem(QStringLiteral("<="), QStringLiteral("elt"));
    condOpCombo_->setCurrentIndex(0);
    condOpCombo_->setFixedHeight(BTN_HEIGHT);
    condOpCombo_->setStyleSheet(inputStyle);
    condOpCombo_->setMaximumWidth(50);
    
    // 阈值
    condValueSpin_ = new QDoubleSpinBox(conditionsGroup);
    condValueSpin_->setRange(-9999, 9999);
    condValueSpin_->setValue(25.0);
    condValueSpin_->setDecimals(1);
    condValueSpin_->setFixedHeight(BTN_HEIGHT);
    condValueSpin_->setStyleSheet(inputStyle);
    condValueSpin_->setMaximumWidth(80);
    
    QPushButton *addCondBtn = new QPushButton(QStringLiteral("[+]添加"), conditionsGroup);
    addCondBtn->setFixedHeight(BTN_HEIGHT);
    addCondBtn->setCursor(Qt::PointingHandCursor);
    addCondBtn->setStyleSheet(addActionBtn->styleSheet());
    connect(addCondBtn, &QPushButton::clicked, this, &StrategyDialog::onAddCondition);
    
    condInputLayout->addWidget(condDeviceEdit_);
    condInputLayout->addWidget(condIdEdit_);
    condInputLayout->addWidget(condOpCombo_);
    condInputLayout->addWidget(condValueSpin_);
    condInputLayout->addWidget(addCondBtn);
    condInputLayout->addStretch();
    
    conditionsLayout->addLayout(condInputLayout);
    formLayout->addWidget(conditionsGroup);
    
    formLayout->addStretch();
    
    // 设置滚动内容
    scrollContent->setMinimumWidth(DIALOG_WIDTH_LARGE - DIALOG_MARGIN * 4);
    scrollArea->setWidget(scrollContent);
    contentLayout->addWidget(scrollArea, 1);
    
    // 底部按钮栏 - 紧凑布局
    QWidget *buttonBar = new QWidget(contentWidget);
    buttonBar->setFixedHeight(BTN_HEIGHT_LARGE + 16);
    buttonBar->setStyleSheet(QStringLiteral(
        "background-color: #f0f0f0; border-bottom-left-radius: %1px; border-bottom-right-radius: %1px;").arg(BORDER_RADIUS_DIALOG));
    
    QHBoxLayout *buttonLayout = new QHBoxLayout(buttonBar);
    buttonLayout->setContentsMargins(12, 8, 12, 8);
    buttonLayout->setSpacing(DIALOG_SPACING);
    
    QPushButton *cancelBtn = new QPushButton(QStringLiteral("取消"), buttonBar);
    cancelBtn->setFixedSize(80, BTN_HEIGHT);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #95a5a6; color: white; border: none; "
        "border-radius: %1px; font-weight: bold; font-size: %2px; }"
        "QPushButton:hover { background-color: #7f8c8d; }"
    ).arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_BODY));
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    
    QPushButton *saveBtn = new QPushButton(QStringLiteral("[存]保存"), buttonBar);
    saveBtn->setObjectName(QStringLiteral("saveBtn"));
    saveBtn->setFixedSize(100, BTN_HEIGHT);
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #3498db; color: white; border: none; "
        "border-radius: %1px; font-weight: bold; font-size: %2px; }"
        "QPushButton:hover { background-color: #2980b9; }"
    ).arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_BODY));
    connect(saveBtn, &QPushButton::clicked, this, &QDialog::accept);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(saveBtn);
    
    contentLayout->addWidget(buttonBar);
    mainLayout->addWidget(contentWidget);
    
    // 设置对话框大小 - 1024x600低分辨率优化
    setMinimumSize(DIALOG_WIDTH_LARGE, DIALOG_HEIGHT_LARGE);
    resize(DIALOG_WIDTH_LARGE, DIALOG_HEIGHT_LARGE);
    
    qDebug() << "[STRATEGY_DIALOG] UI设置完成，尺寸:" << size();
}

void StrategyDialog::setupAnimations()
{
    m_fadeAnimation = new QPropertyAnimation(this, "popupOpacity", this);
    m_fadeAnimation->setDuration(250);
    m_fadeAnimation->setStartValue(0.0);
    m_fadeAnimation->setEndValue(1.0);
    m_fadeAnimation->setEasingCurve(QEasingCurve::OutCubic);
}

void StrategyDialog::setStrategy(const QJsonObject &strategy, bool isEdit)
{
    m_isEdit = isEdit;
    qDebug() << "[STRATEGY_DIALOG] 设置策略数据，编辑模式:" << isEdit;
    qDebug() << "[STRATEGY_DIALOG] 策略数据:" << QJsonDocument(strategy).toJson(QJsonDocument::Compact);
    
    QLabel *titleLabel = findChild<QLabel*>(QStringLiteral("dialogTitle"));
    if (titleLabel) {
        titleLabel->setText(isEdit ? QStringLiteral("编辑策略") : QStringLiteral("创建新策略"));
    }
    
    QPushButton *saveBtn = findChild<QPushButton*>(QStringLiteral("saveBtn"));
    if (saveBtn) {
        saveBtn->setText(isEdit ? QStringLiteral("[存] 保存修改") : QStringLiteral("[存] 创建策略"));
    }
    
    // 加载数据或使用默认值
    idSpinBox_->setValue(strategy.value(QStringLiteral("id")).toInt(1));
    idSpinBox_->setEnabled(!isEdit);
    
    QString name = strategy.value(QStringLiteral("name")).toString();
    nameEdit_->setText(name.isEmpty() ? QStringLiteral("策略-1") : name);
    
    QString type = strategy.value(QStringLiteral("type")).toString(QStringLiteral("auto"));
    for (int i = 0; i < typeCombo_->count(); i++) {
        if (typeCombo_->itemData(i).toString() == type) {
            typeCombo_->setCurrentIndex(i);
            break;
        }
    }
    
    enabledCheck_->setChecked(strategy.value(QStringLiteral("enabled")).toBool(true));
    
    loadActions(strategy.value(QStringLiteral("actions")).toArray());
    loadConditions(strategy.value(QStringLiteral("conditions")).toArray());
    
    qDebug() << "[STRATEGY_DIALOG] 策略数据加载完成";
}

QJsonObject StrategyDialog::getStrategy() const
{
    QJsonObject strategy;
    
    strategy[QStringLiteral("id")] = idSpinBox_->value();
    strategy[QStringLiteral("name")] = nameEdit_->text();
    strategy[QStringLiteral("type")] = typeCombo_->currentData().toString();
    strategy[QStringLiteral("enabled")] = enabledCheck_->isChecked();
    strategy[QStringLiteral("groupId")] = 0;  // 不再使用分组绑定，通过执行动作选择通道
    
    // 收集动作
    QJsonArray actions;
    for (int i = 0; i < actionsTable_->rowCount(); i++) {
        QJsonObject action;
        action[QStringLiteral("node")] = actionsTable_->item(i, 0)->text().toInt();
        action[QStringLiteral("channel")] = actionsTable_->item(i, 1)->text().toInt();
        action[QStringLiteral("value")] = actionsTable_->item(i, 2)->data(Qt::UserRole).toInt();
        actions.append(action);
    }
    strategy[QStringLiteral("actions")] = actions;
    
    // 收集条件
    QJsonArray conditions;
    for (int i = 0; i < conditionsTable_->rowCount(); i++) {
        QJsonObject condition;
        condition[QStringLiteral("device")] = conditionsTable_->item(i, 0)->text();
        condition[QStringLiteral("identifier")] = conditionsTable_->item(i, 1)->text();
        condition[QStringLiteral("op")] = conditionsTable_->item(i, 2)->data(Qt::UserRole).toString();
        condition[QStringLiteral("value")] = conditionsTable_->item(i, 3)->text().toDouble();
        conditions.append(condition);
    }
    strategy[QStringLiteral("conditions")] = conditions;
    
    qDebug() << "[STRATEGY_DIALOG] 收集策略数据:" << QJsonDocument(strategy).toJson(QJsonDocument::Compact);
    
    return strategy;
}

void StrategyDialog::loadActions(const QJsonArray &actions)
{
    actionsTable_->setRowCount(0);
    
    qDebug() << "[STRATEGY_DIALOG] 加载动作列表，数量:" << actions.size();
    
    for (const QJsonValue &v : actions) {
        QJsonObject action = v.toObject();
        int row = actionsTable_->rowCount();
        actionsTable_->insertRow(row);
        
        int node = action.value(QStringLiteral("node")).toInt();
        int channel = action.value(QStringLiteral("channel")).toInt();
        int value = action.value(QStringLiteral("value")).toInt();
        
        QTableWidgetItem *nodeItem = new QTableWidgetItem(QString::number(node));
        nodeItem->setTextAlignment(Qt::AlignCenter);
        actionsTable_->setItem(row, 0, nodeItem);
        
        QTableWidgetItem *chItem = new QTableWidgetItem(QString::number(channel));
        chItem->setTextAlignment(Qt::AlignCenter);
        actionsTable_->setItem(row, 1, chItem);
        
        QString actionText;
        switch (value) {
            case 0: actionText = QStringLiteral("[停] 停止"); break;
            case 1: actionText = QStringLiteral("[正] 正转"); break;
            case 2: actionText = QStringLiteral("[反] 反转"); break;
            default: actionText = QStringLiteral("[?] 未知"); break;
        }
        QTableWidgetItem *valItem = new QTableWidgetItem(actionText);
        valItem->setData(Qt::UserRole, value);
        valItem->setTextAlignment(Qt::AlignCenter);
        actionsTable_->setItem(row, 2, valItem);
        
        QPushButton *delBtn = new QPushButton(QStringLiteral("[删] 删除"));
        delBtn->setProperty("row", row);
        delBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #e74c3c; color: white; border: none; "
            "border-radius: 6px; padding: 8px 16px; font-size: 12px; }"
            "QPushButton:hover { background-color: #c0392b; }"
        ));
        connect(delBtn, &QPushButton::clicked, this, &StrategyDialog::onDeleteAction);
        actionsTable_->setCellWidget(row, 3, delBtn);
        
        qDebug() << "[STRATEGY_DIALOG] 加载动作" << row << ": 节点" << node << "通道" << channel << "动作" << value;
    }
}

void StrategyDialog::loadConditions(const QJsonArray &conditions)
{
    conditionsTable_->setRowCount(0);
    
    qDebug() << "[STRATEGY_DIALOG] 加载条件列表，数量:" << conditions.size();
    
    for (const QJsonValue &v : conditions) {
        QJsonObject cond = v.toObject();
        int row = conditionsTable_->rowCount();
        conditionsTable_->insertRow(row);
        
        QString device = cond.value(QStringLiteral("device")).toString();
        QString identifier = cond.value(QStringLiteral("identifier")).toString();
        QString op = cond.value(QStringLiteral("op")).toString();
        double value = cond.value(QStringLiteral("value")).toDouble();
        
        QTableWidgetItem *devItem = new QTableWidgetItem(device.isEmpty() ? QStringLiteral("sensor1") : device);
        devItem->setTextAlignment(Qt::AlignCenter);
        conditionsTable_->setItem(row, 0, devItem);
        
        QTableWidgetItem *idItem = new QTableWidgetItem(identifier.isEmpty() ? QStringLiteral("temperature") : identifier);
        idItem->setTextAlignment(Qt::AlignCenter);
        conditionsTable_->setItem(row, 1, idItem);
        
        QString opText;
        if (op == QStringLiteral("gt")) opText = QStringLiteral(">");
        else if (op == QStringLiteral("lt")) opText = QStringLiteral("<");
        else if (op == QStringLiteral("eq")) opText = QStringLiteral("=");
        else if (op == QStringLiteral("egt")) opText = QStringLiteral(">=");
        else if (op == QStringLiteral("elt")) opText = QStringLiteral("<=");
        else opText = QStringLiteral(">");
        
        QTableWidgetItem *opItem = new QTableWidgetItem(opText);
        opItem->setData(Qt::UserRole, op.isEmpty() ? QStringLiteral("gt") : op);
        opItem->setTextAlignment(Qt::AlignCenter);
        conditionsTable_->setItem(row, 2, opItem);
        
        QTableWidgetItem *valItem = new QTableWidgetItem(QString::number(value, 'f', 1));
        valItem->setTextAlignment(Qt::AlignCenter);
        conditionsTable_->setItem(row, 3, valItem);
        
        QPushButton *delBtn = new QPushButton(QStringLiteral("[删] 删除"));
        delBtn->setProperty("row", row);
        delBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #e74c3c; color: white; border: none; "
            "border-radius: 6px; padding: 8px 16px; font-size: 12px; }"
            "QPushButton:hover { background-color: #c0392b; }"
        ));
        connect(delBtn, &QPushButton::clicked, this, &StrategyDialog::onDeleteCondition);
        conditionsTable_->setCellWidget(row, 4, delBtn);
        
        qDebug() << "[STRATEGY_DIALOG] 加载条件" << row << ":" << device << identifier << op << value;
    }
}

void StrategyDialog::onAddAction()
{
    int row = actionsTable_->rowCount();
    actionsTable_->insertRow(row);
    
    int node = actionNodeSpin_->value();
    int channel = actionChSpin_->value();
    int value = actionValueCombo_->currentData().toInt();
    QString actionText = actionValueCombo_->currentText();
    
    QTableWidgetItem *nodeItem = new QTableWidgetItem(QString::number(node));
    nodeItem->setTextAlignment(Qt::AlignCenter);
    actionsTable_->setItem(row, 0, nodeItem);
    
    QTableWidgetItem *chItem = new QTableWidgetItem(QString::number(channel));
    chItem->setTextAlignment(Qt::AlignCenter);
    actionsTable_->setItem(row, 1, chItem);
    
    QTableWidgetItem *valItem = new QTableWidgetItem(actionText);
    valItem->setData(Qt::UserRole, value);
    valItem->setTextAlignment(Qt::AlignCenter);
    actionsTable_->setItem(row, 2, valItem);
    
    QPushButton *delBtn = new QPushButton(QStringLiteral("[删] 删除"));
    delBtn->setProperty("row", row);
    delBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #e74c3c; color: white; border: none; "
        "border-radius: 6px; padding: 8px 16px; font-size: 12px; }"
        "QPushButton:hover { background-color: #c0392b; }"
    ));
    connect(delBtn, &QPushButton::clicked, this, &StrategyDialog::onDeleteAction);
    actionsTable_->setCellWidget(row, 3, delBtn);
    
    qDebug() << "[STRATEGY_DIALOG] 添加动作: 节点" << node << "通道" << channel << "动作" << actionText;
}

void StrategyDialog::onDeleteAction()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (btn) {
        int row = btn->property("row").toInt();
        qDebug() << "[STRATEGY_DIALOG] 删除动作行:" << row;
        actionsTable_->removeRow(row);
        // 更新行号
        for (int i = 0; i < actionsTable_->rowCount(); i++) {
            QWidget *widget = actionsTable_->cellWidget(i, 3);
            if (widget) {
                QPushButton *b = qobject_cast<QPushButton*>(widget);
                if (b) b->setProperty("row", i);
            }
        }
    }
}

void StrategyDialog::onAddCondition()
{
    int row = conditionsTable_->rowCount();
    conditionsTable_->insertRow(row);
    
    QString device = condDeviceEdit_->text();
    QString identifier = condIdEdit_->text();
    QString op = condOpCombo_->currentData().toString();
    QString opText = condOpCombo_->currentText();
    double value = condValueSpin_->value();
    
    QTableWidgetItem *devItem = new QTableWidgetItem(device.isEmpty() ? QStringLiteral("sensor1") : device);
    devItem->setTextAlignment(Qt::AlignCenter);
    conditionsTable_->setItem(row, 0, devItem);
    
    QTableWidgetItem *idItem = new QTableWidgetItem(identifier.isEmpty() ? QStringLiteral("temperature") : identifier);
    idItem->setTextAlignment(Qt::AlignCenter);
    conditionsTable_->setItem(row, 1, idItem);
    
    QTableWidgetItem *opItem = new QTableWidgetItem(opText);
    opItem->setData(Qt::UserRole, op);
    opItem->setTextAlignment(Qt::AlignCenter);
    conditionsTable_->setItem(row, 2, opItem);
    
    QTableWidgetItem *valItem = new QTableWidgetItem(QString::number(value, 'f', 1));
    valItem->setTextAlignment(Qt::AlignCenter);
    conditionsTable_->setItem(row, 3, valItem);
    
    QPushButton *delBtn = new QPushButton(QStringLiteral("[删] 删除"));
    delBtn->setProperty("row", row);
    delBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #e74c3c; color: white; border: none; "
        "border-radius: 6px; padding: 8px 16px; font-size: 12px; }"
        "QPushButton:hover { background-color: #c0392b; }"
    ));
    connect(delBtn, &QPushButton::clicked, this, &StrategyDialog::onDeleteCondition);
    conditionsTable_->setCellWidget(row, 4, delBtn);
    
    qDebug() << "[STRATEGY_DIALOG] 添加条件:" << device << identifier << op << value;
}

void StrategyDialog::onDeleteCondition()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (btn) {
        int row = btn->property("row").toInt();
        qDebug() << "[STRATEGY_DIALOG] 删除条件行:" << row;
        conditionsTable_->removeRow(row);
        for (int i = 0; i < conditionsTable_->rowCount(); i++) {
            QWidget *widget = conditionsTable_->cellWidget(i, 4);
            if (widget) {
                QPushButton *b = qobject_cast<QPushButton*>(widget);
                if (b) b->setProperty("row", i);
            }
        }
    }
}

void StrategyDialog::setPopupOpacity(qreal opacity)
{
    m_popupOpacity = opacity;
    update();
}

void StrategyDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    
    if (m_fadeAnimation) {
        m_fadeAnimation->start();
    }
    
    qDebug() << "[STRATEGY_DIALOG] 对话框显示，大小:" << size();
}

void StrategyDialog::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制半透明背景遮罩
    painter.fillRect(rect(), QColor(0, 0, 0, static_cast<int>(70 * m_popupOpacity)));
    
    QDialog::paintEvent(event);
}

void StrategyDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    
    // 确保对话框在父窗口居中
    if (parentWidget()) {
        QPoint parentCenter = parentWidget()->mapToGlobal(parentWidget()->rect().center());
        move(parentCenter.x() - width() / 2, parentCenter.y() - height() / 2);
    }
}

void StrategyDialog::onAnimationFinished()
{
    // 动画完成时的回调
    qDebug() << "[STRATEGY_DIALOG] 动画完成";
}
