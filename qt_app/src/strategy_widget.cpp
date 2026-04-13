/**
 * @file strategy_widget.cpp
 * @brief 策略管理页面实现 - 卡片式布局（1024x600低分辨率优化版）
 * 
 * 使用卡片式显示策略，参考Web端设计
 */

#include "strategy_widget.h"
#include "rpc_client.h"
#include "strategy_dialog.h"
#include "style_constants.h"

#include <QScroller>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QScrollArea>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMouseEvent>
#include <QListWidget>
#include <QStackedWidget>
#include <QTextEdit>
#include <QDateTimeEdit>
#include <QPainter>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>

using namespace UIConstants;

// ==================== StrategyCard Implementation ====================

StrategyCard::StrategyCard(int strategyId, const QString &name, 
                          const QString &type, QWidget *parent)
    : QFrame(parent)
    , strategyId_(strategyId)
    , name_(name)
    , type_(type)
    , enabled_(false)
    , m_hoverScale(1.0)
    , nameLabel_(nullptr)
    , idLabel_(nullptr)
    , typeLabel_(nullptr)
    , descLabel_(nullptr)
    , statusLabel_(nullptr)
    , toggleBtn_(nullptr)
    , editBtn_(nullptr)
    , deleteBtn_(nullptr)
    , m_hoverAnimation(nullptr)
{
    setupUi();
    
    // 设置悬停动画
    m_hoverAnimation = new QPropertyAnimation(this, "hoverScale", this);
    m_hoverAnimation->setDuration(150);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
    
    setAttribute(Qt::WA_Hover, true);
}

StrategyCard::~StrategyCard()
{
}

void StrategyCard::setHoverScale(qreal scale)
{
    m_hoverScale = scale;
    updateGeometry();
}

void StrategyCard::enterEvent(QEvent *event)
{
    QFrame::enterEvent(event);
    startHoverAnimation(1.02);
    raise();
}

void StrategyCard::leaveEvent(QEvent *event)
{
    QFrame::leaveEvent(event);
    startHoverAnimation(1.0);
}

void StrategyCard::startHoverAnimation(qreal endScale)
{
    if (m_hoverAnimation) {
        m_hoverAnimation->stop();
        m_hoverAnimation->setStartValue(m_hoverScale);
        m_hoverAnimation->setEndValue(endScale);
        m_hoverAnimation->start();
    }
}

void StrategyCard::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制阴影效果
    if (m_hoverScale > 1.0) {
        painter.setPen(Qt::NoPen);
        QColor shadowColor(0, 0, 0, 50);
        painter.setBrush(shadowColor);
        QRect shadowRect = rect().adjusted(4, 6, -4, -2);
        painter.drawRoundedRect(shadowRect, 12, 12);
    }
    
    QFrame::paintEvent(event);
}

void StrategyCard::setupUi()
{
    setObjectName(QStringLiteral("strategyCard"));
    setFrameShape(QFrame::NoFrame);
    setStyleSheet(QStringLiteral(
        "#strategyCard {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #f8f9fa);"
        "  border: 2px solid #e0e0e0;"
        "  border-radius: %1px;"
        "}"
        "#strategyCard:hover {"
        "  border-color: #3498db;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #ebf5fb);"
        "}").arg(BORDER_RADIUS_CARD));
    setMinimumHeight(CARD_MIN_HEIGHT);
    setMaximumWidth(CARD_MAX_WIDTH);
    setMinimumWidth(200);  // 确保卡片最小宽度

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(CARD_MARGIN, CARD_MARGIN, CARD_MARGIN, CARD_MARGIN);
    mainLayout->setSpacing(CARD_SPACING);

    // 顶部行：名称、类型、ID
    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->setSpacing(4);
    
    nameLabel_ = new QLabel(QStringLiteral("[策] %1").arg(name_), this);
    nameLabel_->setStyleSheet(QStringLiteral(
        "font-size: %1px; font-weight: bold; color: #2c3e50;").arg(FONT_SIZE_CARD_TITLE));
    topRow->addWidget(nameLabel_);
    
    topRow->addStretch();
    
    typeLabel_ = new QLabel(type_, this);
    QString typeBg = (type_ == QStringLiteral("AUTO")) ? QStringLiteral("#3498db") : QStringLiteral("#9b59b6");
    QString typeText = (type_ == QStringLiteral("AUTO")) ? QStringLiteral("[自]自动") : QStringLiteral("[手]手动");
    typeLabel_->setStyleSheet(QStringLiteral(
        "font-size: %1px; color: white; background-color: %2; "
        "padding: 2px 6px; border-radius: 6px; font-weight: bold;").arg(FONT_SIZE_SMALL).arg(typeBg));
    typeLabel_->setText(typeText);
    topRow->addWidget(typeLabel_);
    
    idLabel_ = new QLabel(QStringLiteral("ID:%1").arg(strategyId_), this);
    idLabel_->setStyleSheet(QStringLiteral(
        "font-size: %1px; color: #7f8c8d; background-color: #ecf0f1; "
        "padding: 2px 6px; border-radius: 4px;").arg(FONT_SIZE_SMALL));
    topRow->addWidget(idLabel_);
    
    mainLayout->addLayout(topRow);

    // 描述行
    descLabel_ = new QLabel(QStringLiteral("加载中..."), this);
    descLabel_->setStyleSheet(QStringLiteral(
        "font-size: %1px; color: #5d6d7e; padding: 4px; "
        "background-color: #f8f9fa; border-radius: 4px;").arg(FONT_SIZE_SMALL));
    descLabel_->setWordWrap(true);
    descLabel_->setMinimumHeight(30);
    mainLayout->addWidget(descLabel_);

    // 底部分隔线
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QStringLiteral("color: #e8e8e8;"));
    line->setMaximumHeight(1);
    mainLayout->addWidget(line);

    // 状态和操作行
    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(4);
    
    statusLabel_ = new QLabel(QStringLiteral("[X]禁用"), this);
    statusLabel_->setStyleSheet(QStringLiteral(
        "font-size: %1px; font-weight: bold; color: #e74c3c; padding: 4px 8px;"
        "background-color: #fdf2f2; border-radius: 4px;").arg(FONT_SIZE_SMALL));
    bottomRow->addWidget(statusLabel_);
    
    bottomRow->addStretch();
    
    toggleBtn_ = new QPushButton(QStringLiteral("启用"), this);
    toggleBtn_->setFixedHeight(BTN_HEIGHT_SMALL);
    toggleBtn_->setMinimumWidth(BTN_MIN_WIDTH_SMALL);
    toggleBtn_->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #27ae60; color: white; border: none; "
        "border-radius: %1px; font-weight: bold; font-size: %2px; padding: 0 8px; }"
        "QPushButton:hover { background-color: #229954; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_SMALL));
    connect(toggleBtn_, &QPushButton::clicked, [this]() {
        emit toggleClicked(strategyId_, !enabled_);
    });
    bottomRow->addWidget(toggleBtn_);
    
    QPushButton *triggerBtn = new QPushButton(QStringLiteral("触发"), this);
    triggerBtn->setFixedHeight(BTN_HEIGHT_SMALL);
    triggerBtn->setMinimumWidth(BTN_MIN_WIDTH_SMALL);
    triggerBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #e74c3c; color: white; border: none; "
        "border-radius: %1px; font-size: %2px; padding: 0 8px; }"
        "QPushButton:hover { background-color: #c0392b; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_SMALL));
    connect(triggerBtn, &QPushButton::clicked, [this]() {
        emit triggerClicked(strategyId_);
    });
    bottomRow->addWidget(triggerBtn);
    
    editBtn_ = new QPushButton(QStringLiteral("编"), this);
    editBtn_->setFixedHeight(BTN_HEIGHT_SMALL);
    editBtn_->setMinimumWidth(BTN_MIN_WIDTH_SMALL);
    editBtn_->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #f39c12; color: white; border: none; "
        "border-radius: %1px; font-size: %2px; }"
        "QPushButton:hover { background-color: #d68910; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_BODY));
    connect(editBtn_, &QPushButton::clicked, [this]() {
        emit editClicked(strategyId_);
    });
    bottomRow->addWidget(editBtn_);
    
    QPushButton *deleteBtn = new QPushButton(QStringLiteral("删"), this);
    deleteBtn->setFixedHeight(BTN_HEIGHT_SMALL);
    deleteBtn->setMinimumWidth(BTN_MIN_WIDTH_SMALL);
    deleteBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #95a5a6; color: white; border: none; "
        "border-radius: %1px; font-size: %2px; }"
        "QPushButton:hover { background-color: #7f8c8d; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_BODY));
    connect(deleteBtn, &QPushButton::clicked, [this]() {
        emit deleteClicked(strategyId_);
    });
    bottomRow->addWidget(deleteBtn);
    
    mainLayout->addLayout(bottomRow);
}

void StrategyCard::updateInfo(const QString &name, const QString &description, 
                             bool enabled, bool running)
{
    name_ = name;
    enabled_ = enabled;
    
    nameLabel_->setText(QStringLiteral("[策] %1").arg(name));
    descLabel_->setText(description);
    
    if (enabled) {
        if (running) {
            statusLabel_->setText(QStringLiteral("[运]运行中"));
            statusLabel_->setStyleSheet(QStringLiteral(
                "font-size: %1px; font-weight: bold; color: #e74c3c; padding: 4px 8px;"
                "background-color: #fadbd8; border-radius: 4px;").arg(FONT_SIZE_SMALL));
        } else {
            statusLabel_->setText(QStringLiteral("[OK]已启用"));
            statusLabel_->setStyleSheet(QStringLiteral(
                "font-size: %1px; font-weight: bold; color: #27ae60; padding: 4px 8px;"
                "background-color: #d4edda; border-radius: 4px;").arg(FONT_SIZE_SMALL));
        }
        toggleBtn_->setText(QStringLiteral("禁用"));
        toggleBtn_->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #7f8c8d; color: white; border: none; "
            "border-radius: %1px; font-weight: bold; font-size: %2px; padding: 0 8px; }"
            "QPushButton:hover { background-color: #6c7a7d; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_SMALL));
    } else {
        statusLabel_->setText(QStringLiteral("[X]已禁用"));
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: %1px; font-weight: bold; color: #e74c3c; padding: 4px 8px;"
            "background-color: #fdf2f2; border-radius: 4px;").arg(FONT_SIZE_SMALL));
        toggleBtn_->setText(QStringLiteral("启用"));
        toggleBtn_->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #27ae60; color: white; border: none; "
            "border-radius: %1px; font-weight: bold; font-size: %2px; padding: 0 8px; }"
            "QPushButton:hover { background-color: #229954; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_SMALL));
    }
}

// ==================== StrategyWidget Implementation ====================

StrategyWidget::StrategyWidget(RpcClient *rpcClient, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpcClient)
    , statusLabel_(nullptr)
    , cardsContainer_(nullptr)
    , cardsLayout_(nullptr)
{
    setupUi();
    qDebug() << "[STRATEGY_WIDGET] 策略页面初始化完成";
}

void StrategyWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(PAGE_MARGIN, PAGE_MARGIN, PAGE_MARGIN, PAGE_MARGIN);
    mainLayout->setSpacing(PAGE_SPACING);

    // 页面标题
    QLabel *titleLabel = new QLabel(QStringLiteral("[策] 策略管理"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: %1px; font-weight: bold; color: #2c3e50; padding: 2px 0;").arg(FONT_SIZE_TITLE));
    mainLayout->addWidget(titleLabel);

    // 工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(CARD_SPACING);

    QPushButton *refreshBtn = new QPushButton(QStringLiteral("[刷]刷新"), this);
    refreshBtn->setFixedHeight(BTN_HEIGHT);
    refreshBtn->setMinimumWidth(BTN_MIN_WIDTH);
    refreshBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #3498db; color: white; border: none; "
        "border-radius: %1px; padding: 0 12px; font-weight: bold; font-size: %2px; }"
        "QPushButton:hover { background-color: #2980b9; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_BODY));
    connect(refreshBtn, &QPushButton::clicked, this, &StrategyWidget::onRefreshStrategiesClicked);
    toolbarLayout->addWidget(refreshBtn);

    QPushButton *createBtn = new QPushButton(QStringLiteral("[+]创建"), this);
    createBtn->setFixedHeight(BTN_HEIGHT);
    createBtn->setMinimumWidth(BTN_MIN_WIDTH);
    createBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #27ae60; color: white; border: none; "
        "border-radius: %1px; padding: 0 12px; font-weight: bold; font-size: %2px; }"
        "QPushButton:hover { background-color: #229954; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_BODY));
    connect(createBtn, &QPushButton::clicked, this, &StrategyWidget::onCreateStrategyClicked);
    toolbarLayout->addWidget(createBtn);

    toolbarLayout->addStretch();

    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet(QStringLiteral(
        "color: #7f8c8d; font-size: %1px; padding: 4px 8px; background-color: #f8f9fa; border-radius: 4px;").arg(FONT_SIZE_SMALL));
    toolbarLayout->addWidget(statusLabel_);

    mainLayout->addLayout(toolbarLayout);

    // 滚动区域用于卡片
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

    // 卡片容器
    cardsContainer_ = new QWidget();
    cardsContainer_->setStyleSheet(QStringLiteral("background-color: transparent;"));
    cardsLayout_ = new QGridLayout(cardsContainer_);
    cardsLayout_->setContentsMargins(0, 0, 0, 0);
    cardsLayout_->setSpacing(PAGE_SPACING);
    cardsLayout_->setColumnStretch(0, 1);
    cardsLayout_->setColumnStretch(1, 1);

    scrollArea->setWidget(cardsContainer_);
    mainLayout->addWidget(scrollArea, 1);

    // 提示
    QLabel *helpLabel = new QLabel(
        QStringLiteral("[示] 策略包含执行动作和触发条件，点击卡片编辑"),
        this);
    helpLabel->setStyleSheet(QStringLiteral(
        "color: #5d6d7e; font-size: %1px; padding: 6px; background-color: #eaf2f8; border-radius: 4px;").arg(FONT_SIZE_SMALL));
    helpLabel->setAlignment(Qt::AlignCenter);
    helpLabel->setWordWrap(true);
    mainLayout->addWidget(helpLabel);
}

void StrategyWidget::clearStrategyCards()
{
    for (StrategyCard *card : strategyCards_) {
        cardsLayout_->removeWidget(card);
        delete card;
    }
    strategyCards_.clear();
}

void StrategyWidget::refreshAllStrategies()
{
    onRefreshStrategiesClicked();
}

void StrategyWidget::onRefreshStrategiesClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        statusLabel_->setText(QStringLiteral("[X] 未连接"));
        return;
    }

    statusLabel_->setText(QStringLiteral("[载] 加载中..."));
    qDebug() << "[STRATEGY_WIDGET] 刷新策略列表";

    // 使用异步调用避免阻塞UI线程
    rpcClient_->callAsync(QStringLiteral("auto.strategy.list"), QJsonObject(),
        [this](const QJsonValue &result, const QJsonObject &error) {
            QMetaObject::invokeMethod(this, [this, result, error]() {
                if (!error.isEmpty() || !result.isObject()) {
                    statusLabel_->setText(QStringLiteral("[X] 加载失败"));
                    qDebug() << "[STRATEGY_WIDGET] auto.strategy.list 失败";
                    return;
                }

                QJsonObject obj = result.toObject();
                if (obj.contains(QStringLiteral("strategies"))) {
                    QJsonArray strategies = obj.value(QStringLiteral("strategies")).toArray();
                    strategiesCache_ = strategies;
                    updateStrategyCards(strategies);
                    statusLabel_->setText(QStringLiteral("[OK] 共 %1 个策略").arg(strategies.size()));
                    qDebug() << "[STRATEGY_WIDGET] 刷新成功，策略数:" << strategies.size();
                } else {
                    statusLabel_->setText(QStringLiteral("[X] 加载失败"));
                }
            }, Qt::QueuedConnection);
        }, 3000);
}

void StrategyWidget::updateStrategyCards(const QJsonArray &strategies)
{
    clearStrategyCards();

    int row = 0;
    int col = 0;

    for (const QJsonValue &val : strategies) {
        QJsonObject s = val.toObject();
        int id = s.value(QStringLiteral("id")).toInt();
        QString name = s.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            name = QStringLiteral("策略-%1").arg(id);
        }
        QString type = s.value(QStringLiteral("type")).toString();
        if (type.isEmpty()) {
            type = QStringLiteral("auto");
        }

        StrategyCard *card = new StrategyCard(id, name, type.toUpper(), this);
        
        // 连接信号
        connect(card, &StrategyCard::toggleClicked, this, &StrategyWidget::onToggleStrategy);
        connect(card, &StrategyCard::triggerClicked, this, &StrategyWidget::onTriggerStrategy);
        connect(card, &StrategyCard::editClicked, this, &StrategyWidget::onEditStrategyClicked);
        connect(card, &StrategyCard::deleteClicked, this, &StrategyWidget::onDeleteStrategy);

        // 构建描述
        QJsonArray actions = s.value(QStringLiteral("actions")).toArray();
        QJsonArray conditions = s.value(QStringLiteral("conditions")).toArray();
        
        QStringList actionTexts;
        for (const QJsonValue &a : actions) {
            QJsonObject action = a.toObject();
            int node = action.value(QStringLiteral("node")).toInt();
            int channel = action.value(QStringLiteral("channel")).toInt();
            int value = action.value(QStringLiteral("value")).toInt();
            QString actionStr;
            switch (value) {
                case 0: actionStr = QStringLiteral("[停] 停止"); break;
                case 1: actionStr = QStringLiteral("[正] 正转"); break;
                case 2: actionStr = QStringLiteral("[反] 反转"); break;
                default: actionStr = QStringLiteral("[?] 未知"); break;
            }
            actionTexts << QStringLiteral("节点%1:通道%2→%3").arg(node).arg(channel).arg(actionStr);
        }
        
        QStringList conditionTexts;
        for (const QJsonValue &c : conditions) {
            QJsonObject cond = c.toObject();
            QString device = cond.value(QStringLiteral("device")).toString();
            QString identifier = cond.value(QStringLiteral("identifier")).toString();
            QString op = cond.value(QStringLiteral("op")).toString();
            double value = cond.value(QStringLiteral("value")).toDouble();
            
            QString opText;
            if (op == QStringLiteral("gt")) opText = QStringLiteral(">");
            else if (op == QStringLiteral("lt")) opText = QStringLiteral("<");
            else if (op == QStringLiteral("eq")) opText = QStringLiteral("=");
            else if (op == QStringLiteral("egt")) opText = QStringLiteral(">=");
            else if (op == QStringLiteral("elt")) opText = QStringLiteral("<=");
            else opText = QStringLiteral(">");
            
            conditionTexts << QStringLiteral("%1 %2 %3").arg(identifier).arg(opText).arg(value);
        }
        
        QString desc;
        if (!actionTexts.isEmpty()) {
            desc += QStringLiteral("[执] 执行: %1").arg(actionTexts.join(QStringLiteral(" | ")));
        }
        if (!conditionTexts.isEmpty()) {
            if (!desc.isEmpty()) desc += QStringLiteral("\n");
            desc += QStringLiteral("[条] 条件: %1").arg(conditionTexts.join(QStringLiteral(" | ")));
        }
        if (desc.isEmpty()) {
            desc = QStringLiteral("[警] 无配置");
        }

        bool enabled = s.value(QStringLiteral("enabled")).toBool();
        bool running = s.value(QStringLiteral("running")).toBool();
        card->updateInfo(name, desc, enabled, running);

        cardsLayout_->addWidget(card, row, col);
        strategyCards_.append(card);

        col++;
        if (col >= 2) {
            col = 0;
            row++;
        }
    }
    
    cardsLayout_->setRowStretch(row + 1, 1);
}

bool StrategyWidget::showStrategyDialog(QJsonObject &strategy, bool isEdit)
{
    qDebug() << "[STRATEGY_WIDGET] 显示策略对话框 isEdit=" << isEdit;
    
    StrategyDialog dialog(this);
    dialog.setStrategy(strategy, isEdit);
    
    if (dialog.exec() != QDialog::Accepted) {
        qDebug() << "[STRATEGY_WIDGET] 策略对话框取消";
        return false;
    }
    
    strategy = dialog.getStrategy();
    qDebug() << "[STRATEGY_WIDGET] 策略对话框确认，数据:" << QJsonDocument(strategy).toJson(QJsonDocument::Compact);
    return true;
}

void StrategyWidget::onCreateStrategyClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QJsonObject strategy;
    if (!showStrategyDialog(strategy, false)) {
        return;
    }

    qDebug() << "[STRATEGY_WIDGET] 创建策略:" << QJsonDocument(strategy).toJson(QJsonDocument::Compact);
    
    QJsonValue result = rpcClient_->call(QStringLiteral("auto.strategy.create"), strategy);
    
    qDebug() << "[STRATEGY_WIDGET] auto.strategy.create 响应:" << QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        int id = result.toObject().value(QStringLiteral("id")).toInt();
        statusLabel_->setText(QStringLiteral("[OK] 策略 %1 创建成功").arg(id));
        emit logMessage(QStringLiteral("创建策略成功: %1").arg(strategy.value(QStringLiteral("name")).toString()));
        onRefreshStrategiesClicked();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("[X] 创建策略失败: %1").arg(error));
    }
}

void StrategyWidget::onEditStrategyClicked(int strategyId)
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    // 从缓存中查找策略
    QJsonObject strategy;
    bool found = false;
    for (const QJsonValue &v : strategiesCache_) {
        QJsonObject s = v.toObject();
        if (s.value(QStringLiteral("id")).toInt() == strategyId) {
            strategy = s;
            found = true;
            break;
        }
    }
    
    if (!found) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("策略不存在"));
        return;
    }

    qDebug() << "[STRATEGY_WIDGET] 编辑策略 strategyId=" << strategyId;

    if (!showStrategyDialog(strategy, true)) {
        return;
    }

    // 更新策略（使用create接口，服务端会处理更新）
    QJsonValue result = rpcClient_->call(QStringLiteral("auto.strategy.create"), strategy);
    
    qDebug() << "[STRATEGY_WIDGET] 更新策略响应:" << QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        statusLabel_->setText(QStringLiteral("[OK] 策略 %1 更新成功").arg(strategyId));
        emit logMessage(QStringLiteral("更新策略成功"));
        onRefreshStrategiesClicked();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("[X] 更新策略失败: %1").arg(error));
    }
}

void StrategyWidget::onToggleStrategy(int strategyId, bool newState)
{
    if (!rpcClient_ || !rpcClient_->isConnected()) return;

    QJsonObject params;
    params[QStringLiteral("id")] = strategyId;
    params[QStringLiteral("enabled")] = newState;

    qDebug() << "[STRATEGY_WIDGET] 切换策略状态 strategyId=" << strategyId << "enabled=" << newState;
    
    rpcClient_->callAsync(QStringLiteral("auto.strategy.enable"), params,
        [this, strategyId](const QJsonValue &result, const QJsonObject &error) {
            QMetaObject::invokeMethod(this, [this, strategyId, result, error]() {
                if (error.isEmpty() && result.isObject() &&
                    result.toObject().value(QStringLiteral("ok")).toBool()) {
                    statusLabel_->setText(QStringLiteral("[OK] 策略 %1 状态已更新").arg(strategyId));
                    emit logMessage(QStringLiteral("策略状态已更新"));
                    onRefreshStrategiesClicked();
                } else {
                    QString errMsg = result.toObject().value(QStringLiteral("error")).toString();
                    if (errMsg.isEmpty()) errMsg = error.value(QStringLiteral("message")).toString();
                    QMessageBox::warning(this, QStringLiteral("错误"),
                        QStringLiteral("[X] 更新策略失败: %1").arg(errMsg));
                }
            }, Qt::QueuedConnection);
        }, 3000);
}

void StrategyWidget::onTriggerStrategy(int strategyId)
{
    if (!rpcClient_ || !rpcClient_->isConnected()) return;

    QJsonObject params;
    params[QStringLiteral("id")] = strategyId;

    qDebug() << "[STRATEGY_WIDGET] 手动触发策略 strategyId=" << strategyId;
    
    rpcClient_->callAsync(QStringLiteral("auto.strategy.trigger"), params,
        [this, strategyId](const QJsonValue &result, const QJsonObject &error) {
            QMetaObject::invokeMethod(this, [this, strategyId, result, error]() {
                if (error.isEmpty() && result.isObject() &&
                    result.toObject().value(QStringLiteral("ok")).toBool()) {
                    statusLabel_->setText(QStringLiteral("[触] 策略 %1 已触发").arg(strategyId));
                    emit logMessage(QStringLiteral("手动触发策略成功"));
                } else {
                    QString errMsg = result.toObject().value(QStringLiteral("error")).toString();
                    if (errMsg.isEmpty()) errMsg = error.value(QStringLiteral("message")).toString();
                    QMessageBox::warning(this, QStringLiteral("错误"),
                        QStringLiteral("[X] 触发策略失败: %1").arg(errMsg));
                }
            }, Qt::QueuedConnection);
        }, 3000);
}

void StrategyWidget::onDeleteStrategy(int strategyId)
{
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        QStringLiteral("确认删除"),
        QStringLiteral("确定要删除策略 %1 吗？").arg(strategyId),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    QJsonObject params;
    params[QStringLiteral("id")] = strategyId;

    qDebug() << "[STRATEGY_WIDGET] 删除策略 strategyId=" << strategyId;
    
    rpcClient_->callAsync(QStringLiteral("auto.strategy.delete"), params,
        [this, strategyId](const QJsonValue &result, const QJsonObject &error) {
            QMetaObject::invokeMethod(this, [this, strategyId, result, error]() {
                if (error.isEmpty() && result.isObject() &&
                    result.toObject().value(QStringLiteral("ok")).toBool()) {
                    statusLabel_->setText(QStringLiteral("[OK] 策略 %1 删除成功").arg(strategyId));
                    emit logMessage(QStringLiteral("删除策略成功"));
                    onRefreshStrategiesClicked();
                } else {
                    QString errMsg = result.toObject().value(QStringLiteral("error")).toString();
                    if (errMsg.isEmpty()) errMsg = error.value(QStringLiteral("message")).toString();
                    QMessageBox::warning(this, QStringLiteral("错误"),
                        QStringLiteral("[X] 删除策略失败: %1").arg(errMsg));
                }
            }, Qt::QueuedConnection);
        }, 3000);
}
