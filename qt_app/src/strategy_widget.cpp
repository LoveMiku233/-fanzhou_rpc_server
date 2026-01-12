/**
 * @file strategy_widget.cpp
 * @brief 策略管理页面实现 - 卡片式布局
 * 
 * 使用卡片式显示策略，参考Web端设计
 */

#include "strategy_widget.h"
#include "rpc_client.h"

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

// ==================== StrategyCard Implementation ====================

StrategyCard::StrategyCard(int strategyId, const QString &name, 
                          const QString &type, QWidget *parent)
    : QFrame(parent)
    , strategyId_(strategyId)
    , name_(name)
    , type_(type)
    , enabled_(false)
    , nameLabel_(nullptr)
    , idLabel_(nullptr)
    , typeLabel_(nullptr)
    , descLabel_(nullptr)
    , statusLabel_(nullptr)
    , toggleBtn_(nullptr)
{
    setupUi();
}

void StrategyCard::setupUi()
{
    setObjectName(QStringLiteral("strategyCard"));
    setStyleSheet(QStringLiteral(
        "#strategyCard {"
        "  background-color: white;"
        "  border: 1px solid #d0d5dd;"
        "  border-radius: 10px;"
        "  padding: 12px;"
        "}"
        "#strategyCard:hover {"
        "  border-color: #3498db;"
        "  background-color: #f8f9fa;"
        "}"));
    setMinimumHeight(120);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(6);

    // 顶部行：名称、类型、ID
    QHBoxLayout *topRow = new QHBoxLayout();
    
    nameLabel_ = new QLabel(QStringLiteral("[T] %1").arg(name_), this);
    nameLabel_->setStyleSheet(QStringLiteral(
        "font-size: 14px; font-weight: bold; color: #2c3e50;"));
    topRow->addWidget(nameLabel_);
    
    topRow->addStretch();
    
    typeLabel_ = new QLabel(type_, this);
    typeLabel_->setStyleSheet(QStringLiteral(
        "font-size: 11px; color: white; background-color: #3498db; "
        "padding: 2px 6px; border-radius: 4px;"));
    topRow->addWidget(typeLabel_);
    
    idLabel_ = new QLabel(QStringLiteral("ID: %1").arg(strategyId_), this);
    idLabel_->setStyleSheet(QStringLiteral(
        "font-size: 11px; color: #7f8c8d; background-color: #ecf0f1; "
        "padding: 2px 6px; border-radius: 4px; margin-left: 4px;"));
    topRow->addWidget(idLabel_);
    
    mainLayout->addLayout(topRow);

    // 描述行
    descLabel_ = new QLabel(QStringLiteral("加载中..."), this);
    descLabel_->setStyleSheet(QStringLiteral(
        "font-size: 12px; color: #666; padding: 4px; "
        "background-color: #f8f9fa; border-radius: 4px;"));
    descLabel_->setWordWrap(true);
    descLabel_->setMinimumHeight(24);
    mainLayout->addWidget(descLabel_);

    // 状态和操作行
    QHBoxLayout *bottomRow = new QHBoxLayout();
    
    statusLabel_ = new QLabel(QStringLiteral("禁用"), this);
    statusLabel_->setStyleSheet(QStringLiteral(
        "font-size: 12px; font-weight: bold; color: #e74c3c;"));
    bottomRow->addWidget(statusLabel_);
    
    bottomRow->addStretch();
    
    toggleBtn_ = new QPushButton(QStringLiteral("启用"), this);
    toggleBtn_->setMinimumHeight(28);
    toggleBtn_->setMinimumWidth(60);
    toggleBtn_->setStyleSheet(QStringLiteral(
        "background-color: #27ae60; color: white; border: none; border-radius: 4px;"));
    connect(toggleBtn_, &QPushButton::clicked, [this]() {
        emit toggleClicked(strategyId_, !enabled_);
    });
    bottomRow->addWidget(toggleBtn_);
    
    QPushButton *triggerBtn = new QPushButton(QStringLiteral("触发"), this);
    triggerBtn->setMinimumHeight(28);
    triggerBtn->setMinimumWidth(50);
    triggerBtn->setStyleSheet(QStringLiteral(
        "background-color: #3498db; color: white; border: none; border-radius: 4px;"));
    connect(triggerBtn, &QPushButton::clicked, [this]() {
        emit triggerClicked(strategyId_);
    });
    bottomRow->addWidget(triggerBtn);
    
    QPushButton *deleteBtn = new QPushButton(QStringLiteral("X"), this);
    deleteBtn->setMinimumHeight(28);
    deleteBtn->setMinimumWidth(32);
    deleteBtn->setStyleSheet(QStringLiteral(
        "background-color: #e74c3c; color: white; border: none; border-radius: 4px;"));
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
    
    QString icon = type_.contains(QStringLiteral("传感器")) ? 
                   QStringLiteral("[S]") : QStringLiteral("[T]");
    nameLabel_->setText(QStringLiteral("%1 %2").arg(icon, name));
    descLabel_->setText(description);
    
    if (enabled) {
        if (running) {
            statusLabel_->setText(QStringLiteral("运行中"));
            statusLabel_->setStyleSheet(QStringLiteral(
                "font-size: 12px; font-weight: bold; color: #27ae60;"));
        } else {
            statusLabel_->setText(QStringLiteral("已启用"));
            statusLabel_->setStyleSheet(QStringLiteral(
                "font-size: 12px; font-weight: bold; color: #3498db;"));
        }
        toggleBtn_->setText(QStringLiteral("禁用"));
        toggleBtn_->setStyleSheet(QStringLiteral(
            "background-color: #95a5a6; color: white; border: none; border-radius: 4px;"));
    } else {
        statusLabel_->setText(QStringLiteral("已禁用"));
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: 12px; font-weight: bold; color: #e74c3c;"));
        toggleBtn_->setText(QStringLiteral("启用"));
        toggleBtn_->setStyleSheet(QStringLiteral(
            "background-color: #27ae60; color: white; border: none; border-radius: 4px;"));
    }
}

// ==================== StrategyWidget Implementation ====================

StrategyWidget::StrategyWidget(RpcClient *rpcClient, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpcClient)
    , statusLabel_(nullptr)
    , tabWidget_(nullptr)
    , timerCardsContainer_(nullptr)
    , timerCardsLayout_(nullptr)
    , sensorCardsContainer_(nullptr)
    , sensorCardsLayout_(nullptr)
{
    setupUi();
}

void StrategyWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // 页面标题
    QLabel *titleLabel = new QLabel(QStringLiteral("策略管理"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: 18px; font-weight: bold; color: #2c3e50; padding: 4px 0;"));
    mainLayout->addWidget(titleLabel);

    // 工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(8);

    QPushButton *refreshAllBtn = new QPushButton(QStringLiteral("刷新全部"), this);
    refreshAllBtn->setMinimumHeight(36);
    connect(refreshAllBtn, &QPushButton::clicked, this, &StrategyWidget::refreshAllStrategies);
    toolbarLayout->addWidget(refreshAllBtn);

    toolbarLayout->addStretch();

    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 12px;"));
    toolbarLayout->addWidget(statusLabel_);

    mainLayout->addLayout(toolbarLayout);

    // 标签页
    tabWidget_ = new QTabWidget(this);
    tabWidget_->addTab(createTimerStrategyTab(), QStringLiteral("定时策略"));
    tabWidget_->addTab(createSensorStrategyTab(), QStringLiteral("传感器策略"));
    mainLayout->addWidget(tabWidget_, 1);
}

QWidget* StrategyWidget::createTimerStrategyTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(8);

    QPushButton *refreshBtn = new QPushButton(QStringLiteral("刷新"), tab);
    refreshBtn->setMinimumHeight(40);
    connect(refreshBtn, &QPushButton::clicked, this, &StrategyWidget::onRefreshTimerStrategiesClicked);
    toolbarLayout->addWidget(refreshBtn);

    QPushButton *createBtn = new QPushButton(QStringLiteral("创建策略"), tab);
    createBtn->setProperty("type", QStringLiteral("success"));
    createBtn->setMinimumHeight(40);
    connect(createBtn, &QPushButton::clicked, this, &StrategyWidget::onCreateTimerStrategyClicked);
    toolbarLayout->addWidget(createBtn);

    toolbarLayout->addStretch();
    layout->addLayout(toolbarLayout);

    // 卡片容器 - 一行两个
    timerCardsContainer_ = new QWidget(tab);
    timerCardsLayout_ = new QGridLayout(timerCardsContainer_);
    timerCardsLayout_->setContentsMargins(0, 0, 0, 0);
    timerCardsLayout_->setSpacing(10);
    timerCardsLayout_->setColumnStretch(0, 1);
    timerCardsLayout_->setColumnStretch(1, 1);

    layout->addWidget(timerCardsContainer_, 1);

    // 提示
    QLabel *helpLabel = new QLabel(
        QStringLiteral("提示：定时策略按间隔或每日定时执行分组控制"),
        tab);
    helpLabel->setStyleSheet(QStringLiteral(
        "color: #7f8c8d; font-size: 11px; padding: 4px;"));
    helpLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(helpLabel);

    return tab;
}

QWidget* StrategyWidget::createSensorStrategyTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(8);

    QPushButton *refreshBtn = new QPushButton(QStringLiteral("刷新"), tab);
    refreshBtn->setMinimumHeight(40);
    connect(refreshBtn, &QPushButton::clicked, this, &StrategyWidget::onRefreshSensorStrategiesClicked);
    toolbarLayout->addWidget(refreshBtn);

    QPushButton *createBtn = new QPushButton(QStringLiteral("创建策略"), tab);
    createBtn->setProperty("type", QStringLiteral("success"));
    createBtn->setMinimumHeight(40);
    connect(createBtn, &QPushButton::clicked, this, &StrategyWidget::onCreateSensorStrategyClicked);
    toolbarLayout->addWidget(createBtn);

    toolbarLayout->addStretch();
    layout->addLayout(toolbarLayout);

    // 卡片容器
    sensorCardsContainer_ = new QWidget(tab);
    sensorCardsLayout_ = new QGridLayout(sensorCardsContainer_);
    sensorCardsLayout_->setContentsMargins(0, 0, 0, 0);
    sensorCardsLayout_->setSpacing(10);
    sensorCardsLayout_->setColumnStretch(0, 1);
    sensorCardsLayout_->setColumnStretch(1, 1);

    layout->addWidget(sensorCardsContainer_, 1);

    // 提示
    QLabel *helpLabel = new QLabel(
        QStringLiteral("提示：传感器策略当数值达到阈值条件时执行分组控制"),
        tab);
    helpLabel->setStyleSheet(QStringLiteral(
        "color: #7f8c8d; font-size: 11px; padding: 4px;"));
    helpLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(helpLabel);

    return tab;
}

void StrategyWidget::clearTimerStrategyCards()
{
    for (StrategyCard *card : timerStrategyCards_) {
        timerCardsLayout_->removeWidget(card);
        delete card;
    }
    timerStrategyCards_.clear();
}

void StrategyWidget::clearSensorStrategyCards()
{
    for (StrategyCard *card : sensorStrategyCards_) {
        sensorCardsLayout_->removeWidget(card);
        delete card;
    }
    sensorStrategyCards_.clear();
}

void StrategyWidget::refreshAllStrategies()
{
    onRefreshTimerStrategiesClicked();
    onRefreshSensorStrategiesClicked();
}

// ==================== 定时策略槽函数 ====================

void StrategyWidget::onRefreshTimerStrategiesClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        statusLabel_->setText(QStringLiteral("[!] 未连接"));
        return;
    }

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.strategy.list"));
    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.contains(QStringLiteral("strategies"))) {
            QJsonArray strategies = obj.value(QStringLiteral("strategies")).toArray();
            updateTimerStrategyCards(strategies);
            statusLabel_->setText(QStringLiteral("定时策略: %1 个").arg(strategies.size()));
            return;
        }
    }
    statusLabel_->setText(QStringLiteral("[X] 获取策略失败"));
}

void StrategyWidget::onCreateTimerStrategyClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    // 创建弹窗
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("创建定时策略"));
    dialog.setMinimumWidth(400);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    // 说明
    QLabel *infoLabel = new QLabel(
        QStringLiteral("策略将控制分组中已绑定的所有通道"), &dialog);
    infoLabel->setStyleSheet(QStringLiteral(
        "color: #666; font-size: 12px; padding: 8px; "
        "background-color: #e3f2fd; border-radius: 6px;"));
    layout->addWidget(infoLabel);
    
    QGridLayout *formGrid = new QGridLayout();
    formGrid->setSpacing(8);
    
    // 第一行
    formGrid->addWidget(new QLabel(QStringLiteral("策略ID:"), &dialog), 0, 0);
    QSpinBox *idSpinBox = new QSpinBox(&dialog);
    idSpinBox->setRange(1, 999);
    idSpinBox->setMinimumHeight(40);
    formGrid->addWidget(idSpinBox, 0, 1);
    
    formGrid->addWidget(new QLabel(QStringLiteral("名称:"), &dialog), 0, 2);
    QLineEdit *nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText(QStringLiteral("策略名称(可空)"));
    nameEdit->setMinimumHeight(40);
    formGrid->addWidget(nameEdit, 0, 3);
    
    // 第二行
    formGrid->addWidget(new QLabel(QStringLiteral("分组ID:"), &dialog), 1, 0);
    QSpinBox *groupIdSpinBox = new QSpinBox(&dialog);
    groupIdSpinBox->setRange(1, 999);
    groupIdSpinBox->setMinimumHeight(40);
    formGrid->addWidget(groupIdSpinBox, 1, 1);
    
    formGrid->addWidget(new QLabel(QStringLiteral("动作:"), &dialog), 1, 2);
    QComboBox *actionCombo = new QComboBox(&dialog);
    actionCombo->addItem(QStringLiteral("停止"), QStringLiteral("stop"));
    actionCombo->addItem(QStringLiteral("正转"), QStringLiteral("fwd"));
    actionCombo->addItem(QStringLiteral("反转"), QStringLiteral("rev"));
    actionCombo->setMinimumHeight(40);
    formGrid->addWidget(actionCombo, 1, 3);
    
    // 第三行 - 触发类型
    formGrid->addWidget(new QLabel(QStringLiteral("触发方式:"), &dialog), 2, 0);
    QComboBox *triggerTypeCombo = new QComboBox(&dialog);
    triggerTypeCombo->addItem(QStringLiteral("按间隔执行"), QStringLiteral("interval"));
    triggerTypeCombo->addItem(QStringLiteral("每日定时"), QStringLiteral("daily"));
    triggerTypeCombo->setMinimumHeight(40);
    formGrid->addWidget(triggerTypeCombo, 2, 1);
    
    formGrid->addWidget(new QLabel(QStringLiteral("间隔(秒):"), &dialog), 2, 2);
    QSpinBox *intervalSpinBox = new QSpinBox(&dialog);
    intervalSpinBox->setRange(1, 86400);
    intervalSpinBox->setValue(3600);
    intervalSpinBox->setMinimumHeight(40);
    formGrid->addWidget(intervalSpinBox, 2, 3);
    
    // 第四行 - 每日时间
    formGrid->addWidget(new QLabel(QStringLiteral("每日时间:"), &dialog), 3, 0);
    QLineEdit *dailyTimeEdit = new QLineEdit(&dialog);
    dailyTimeEdit->setPlaceholderText(QStringLiteral("HH:MM (如 08:00)"));
    dailyTimeEdit->setMinimumHeight(40);
    dailyTimeEdit->setEnabled(false);
    formGrid->addWidget(dailyTimeEdit, 3, 1);
    
    QCheckBox *enabledCheckBox = new QCheckBox(QStringLiteral("立即启用"), &dialog);
    enabledCheckBox->setChecked(true);
    formGrid->addWidget(enabledCheckBox, 3, 2, 1, 2);
    
    layout->addLayout(formGrid);
    
    // 触发类型切换
    connect(triggerTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [intervalSpinBox, dailyTimeEdit](int index) {
        intervalSpinBox->setEnabled(index == 0);
        dailyTimeEdit->setEnabled(index == 1);
    });
    
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

    QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        name = QStringLiteral("策略-%1").arg(idSpinBox->value());
    }

    QJsonObject params;
    params[QStringLiteral("id")] = idSpinBox->value();
    params[QStringLiteral("name")] = name;
    params[QStringLiteral("groupId")] = groupIdSpinBox->value();
    params[QStringLiteral("channel")] = -1;  // 使用分组绑定的通道
    params[QStringLiteral("action")] = actionCombo->currentData().toString();
    params[QStringLiteral("enabled")] = enabledCheckBox->isChecked();
    params[QStringLiteral("triggerType")] = triggerTypeCombo->currentData().toString();
    
    if (triggerTypeCombo->currentIndex() == 0) {
        params[QStringLiteral("intervalSec")] = intervalSpinBox->value();
    } else {
        params[QStringLiteral("dailyTime")] = dailyTimeEdit->text();
        params[QStringLiteral("intervalSec")] = 86400;  // 默认一天
    }

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.strategy.create"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        statusLabel_->setText(QStringLiteral("策略创建成功"));
        emit logMessage(QStringLiteral("创建定时策略成功: %1").arg(name));
        onRefreshTimerStrategiesClicked();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("创建策略失败: %1").arg(error));
    }
}

void StrategyWidget::onToggleTimerStrategy(int strategyId, bool newState)
{
    if (!rpcClient_ || !rpcClient_->isConnected()) return;

    QJsonObject params;
    params[QStringLiteral("id")] = strategyId;
    params[QStringLiteral("enabled")] = newState;

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.strategy.enable"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        statusLabel_->setText(QStringLiteral("策略 %1 状态已更新").arg(strategyId));
        emit logMessage(QStringLiteral("策略状态已更新"));
        onRefreshTimerStrategiesClicked();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("更新策略失败: %1").arg(error));
    }
}

void StrategyWidget::onTriggerTimerStrategy(int strategyId)
{
    if (!rpcClient_ || !rpcClient_->isConnected()) return;

    QJsonObject params;
    params[QStringLiteral("id")] = strategyId;

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.strategy.trigger"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        statusLabel_->setText(QStringLiteral("策略 %1 已触发").arg(strategyId));
        emit logMessage(QStringLiteral("手动触发策略成功"));
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("触发策略失败: %1").arg(error));
    }
}

void StrategyWidget::onDeleteTimerStrategy(int strategyId)
{
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        QStringLiteral("确认删除"),
        QStringLiteral("确定要删除策略 %1 吗？").arg(strategyId),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    QJsonObject params;
    params[QStringLiteral("id")] = strategyId;

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.strategy.delete"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        statusLabel_->setText(QStringLiteral("策略 %1 删除成功").arg(strategyId));
        emit logMessage(QStringLiteral("删除定时策略成功"));
        onRefreshTimerStrategiesClicked();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("删除策略失败: %1").arg(error));
    }
}

void StrategyWidget::updateTimerStrategyCards(const QJsonArray &strategies)
{
    clearTimerStrategyCards();

    int row = 0;
    int col = 0;

    for (const QJsonValue &val : strategies) {
        QJsonObject s = val.toObject();
        int id = s.value(QStringLiteral("id")).toInt();
        QString name = s.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            name = QStringLiteral("策略-%1").arg(id);
        }

        StrategyCard *card = new StrategyCard(id, name, QStringLiteral("定时"), this);
        
        // 连接信号
        connect(card, &StrategyCard::toggleClicked, this, &StrategyWidget::onToggleTimerStrategy);
        connect(card, &StrategyCard::triggerClicked, this, &StrategyWidget::onTriggerTimerStrategy);
        connect(card, &StrategyCard::deleteClicked, this, &StrategyWidget::onDeleteTimerStrategy);

        // 构建描述
        int groupId = s.value(QStringLiteral("groupId")).toInt();
        QString action = s.value(QStringLiteral("action")).toString();
        QString triggerType = s.value(QStringLiteral("triggerType")).toString();
        QString desc;
        
        if (triggerType == QStringLiteral("daily")) {
            QString dailyTime = s.value(QStringLiteral("dailyTime")).toString();
            desc = QStringLiteral("每日 %1 → 分组%2 → %3")
                .arg(dailyTime).arg(groupId).arg(action);
        } else {
            int interval = s.value(QStringLiteral("intervalSec")).toInt();
            desc = QStringLiteral("每 %1 秒 → 分组%2 → %3")
                .arg(interval).arg(groupId).arg(action);
        }

        bool enabled = s.value(QStringLiteral("enabled")).toBool();
        bool running = s.value(QStringLiteral("running")).toBool();
        card->updateInfo(name, desc, enabled, running);

        timerCardsLayout_->addWidget(card, row, col);
        timerStrategyCards_.append(card);

        col++;
        if (col >= 2) {
            col = 0;
            row++;
        }
    }
    
    timerCardsLayout_->setRowStretch(row + 1, 1);
}

// ==================== 传感器策略槽函数 ====================

void StrategyWidget::onRefreshSensorStrategiesClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        return;
    }

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.sensor.list"));
    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.contains(QStringLiteral("strategies"))) {
            QJsonArray strategies = obj.value(QStringLiteral("strategies")).toArray();
            updateSensorStrategyCards(strategies);
        }
    }
}

void StrategyWidget::onCreateSensorStrategyClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    // 创建弹窗
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("创建传感器策略"));
    dialog.setMinimumWidth(450);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    QLabel *infoLabel = new QLabel(
        QStringLiteral("当传感器数值满足条件时，自动控制分组中的设备"), &dialog);
    infoLabel->setStyleSheet(QStringLiteral(
        "color: #666; font-size: 12px; padding: 8px; "
        "background-color: #e3f2fd; border-radius: 6px;"));
    layout->addWidget(infoLabel);
    
    QGridLayout *formGrid = new QGridLayout();
    formGrid->setSpacing(8);
    
    // 第一行
    formGrid->addWidget(new QLabel(QStringLiteral("策略ID:"), &dialog), 0, 0);
    QSpinBox *idSpinBox = new QSpinBox(&dialog);
    idSpinBox->setRange(1, 999);
    idSpinBox->setMinimumHeight(40);
    formGrid->addWidget(idSpinBox, 0, 1);
    
    formGrid->addWidget(new QLabel(QStringLiteral("名称:"), &dialog), 0, 2);
    QLineEdit *nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText(QStringLiteral("策略名称"));
    nameEdit->setMinimumHeight(40);
    formGrid->addWidget(nameEdit, 0, 3);
    
    // 第二行 - 传感器配置
    formGrid->addWidget(new QLabel(QStringLiteral("传感器类型:"), &dialog), 1, 0);
    QComboBox *sensorTypeCombo = new QComboBox(&dialog);
    sensorTypeCombo->addItem(QStringLiteral("温度"), QStringLiteral("temperature"));
    sensorTypeCombo->addItem(QStringLiteral("湿度"), QStringLiteral("humidity"));
    sensorTypeCombo->addItem(QStringLiteral("光照"), QStringLiteral("light"));
    sensorTypeCombo->addItem(QStringLiteral("土壤湿度"), QStringLiteral("soil_moisture"));
    sensorTypeCombo->setMinimumHeight(40);
    formGrid->addWidget(sensorTypeCombo, 1, 1);
    
    formGrid->addWidget(new QLabel(QStringLiteral("传感器节点:"), &dialog), 1, 2);
    QSpinBox *sensorNodeSpinBox = new QSpinBox(&dialog);
    sensorNodeSpinBox->setRange(1, 255);
    sensorNodeSpinBox->setMinimumHeight(40);
    formGrid->addWidget(sensorNodeSpinBox, 1, 3);
    
    // 第三行 - 条件
    formGrid->addWidget(new QLabel(QStringLiteral("条件:"), &dialog), 2, 0);
    QComboBox *conditionCombo = new QComboBox(&dialog);
    conditionCombo->addItem(QStringLiteral("大于 (>)"), QStringLiteral("gt"));
    conditionCombo->addItem(QStringLiteral("小于 (<)"), QStringLiteral("lt"));
    conditionCombo->addItem(QStringLiteral("等于 (=)"), QStringLiteral("eq"));
    conditionCombo->setMinimumHeight(40);
    formGrid->addWidget(conditionCombo, 2, 1);
    
    formGrid->addWidget(new QLabel(QStringLiteral("阈值:"), &dialog), 2, 2);
    QDoubleSpinBox *thresholdSpinBox = new QDoubleSpinBox(&dialog);
    thresholdSpinBox->setRange(-1000, 1000);
    thresholdSpinBox->setValue(30.0);
    thresholdSpinBox->setMinimumHeight(40);
    formGrid->addWidget(thresholdSpinBox, 2, 3);
    
    // 第四行 - 控制配置
    formGrid->addWidget(new QLabel(QStringLiteral("分组ID:"), &dialog), 3, 0);
    QSpinBox *groupIdSpinBox = new QSpinBox(&dialog);
    groupIdSpinBox->setRange(1, 999);
    groupIdSpinBox->setMinimumHeight(40);
    formGrid->addWidget(groupIdSpinBox, 3, 1);
    
    formGrid->addWidget(new QLabel(QStringLiteral("动作:"), &dialog), 3, 2);
    QComboBox *actionCombo = new QComboBox(&dialog);
    actionCombo->addItem(QStringLiteral("停止"), QStringLiteral("stop"));
    actionCombo->addItem(QStringLiteral("正转"), QStringLiteral("fwd"));
    actionCombo->addItem(QStringLiteral("反转"), QStringLiteral("rev"));
    actionCombo->setMinimumHeight(40);
    formGrid->addWidget(actionCombo, 3, 3);
    
    // 第五行
    formGrid->addWidget(new QLabel(QStringLiteral("冷却时间(秒):"), &dialog), 4, 0);
    QSpinBox *cooldownSpinBox = new QSpinBox(&dialog);
    cooldownSpinBox->setRange(0, 86400);
    cooldownSpinBox->setValue(60);
    cooldownSpinBox->setMinimumHeight(40);
    formGrid->addWidget(cooldownSpinBox, 4, 1);
    
    QCheckBox *enabledCheckBox = new QCheckBox(QStringLiteral("立即启用"), &dialog);
    enabledCheckBox->setChecked(true);
    formGrid->addWidget(enabledCheckBox, 4, 2, 1, 2);
    
    layout->addLayout(formGrid);
    
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

    QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        name = QStringLiteral("传感器策略-%1").arg(idSpinBox->value());
    }

    QJsonObject params;
    params[QStringLiteral("id")] = idSpinBox->value();
    params[QStringLiteral("name")] = name;
    params[QStringLiteral("sensorType")] = sensorTypeCombo->currentData().toString();
    params[QStringLiteral("sensorNode")] = sensorNodeSpinBox->value();
    params[QStringLiteral("condition")] = conditionCombo->currentData().toString();
    params[QStringLiteral("threshold")] = thresholdSpinBox->value();
    params[QStringLiteral("groupId")] = groupIdSpinBox->value();
    params[QStringLiteral("channel")] = -1;  // 使用分组绑定的通道
    params[QStringLiteral("action")] = actionCombo->currentData().toString();
    params[QStringLiteral("cooldownSec")] = cooldownSpinBox->value();
    params[QStringLiteral("enabled")] = enabledCheckBox->isChecked();

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.sensor.create"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        statusLabel_->setText(QStringLiteral("传感器策略创建成功"));
        emit logMessage(QStringLiteral("创建传感器策略成功: %1").arg(name));
        onRefreshSensorStrategiesClicked();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("创建策略失败: %1").arg(error));
    }
}

void StrategyWidget::onToggleSensorStrategy(int strategyId, bool newState)
{
    if (!rpcClient_ || !rpcClient_->isConnected()) return;

    QJsonObject params;
    params[QStringLiteral("id")] = strategyId;
    params[QStringLiteral("enabled")] = newState;

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.sensor.enable"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        emit logMessage(QStringLiteral("传感器策略状态已更新"));
        onRefreshSensorStrategiesClicked();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("更新策略失败: %1").arg(error));
    }
}

void StrategyWidget::onDeleteSensorStrategy(int strategyId)
{
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        QStringLiteral("确认删除"),
        QStringLiteral("确定要删除传感器策略 %1 吗？").arg(strategyId),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    QJsonObject params;
    params[QStringLiteral("id")] = strategyId;

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.sensor.delete"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        emit logMessage(QStringLiteral("删除传感器策略成功"));
        onRefreshSensorStrategiesClicked();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("删除策略失败: %1").arg(error));
    }
}

void StrategyWidget::updateSensorStrategyCards(const QJsonArray &strategies)
{
    clearSensorStrategyCards();

    int row = 0;
    int col = 0;

    for (const QJsonValue &val : strategies) {
        QJsonObject s = val.toObject();
        int id = s.value(QStringLiteral("id")).toInt();
        QString name = s.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            name = QStringLiteral("传感器策略-%1").arg(id);
        }

        StrategyCard *card = new StrategyCard(id, name, QStringLiteral("传感器"), this);
        
        // 连接信号
        connect(card, &StrategyCard::toggleClicked, this, &StrategyWidget::onToggleSensorStrategy);
        connect(card, &StrategyCard::deleteClicked, this, &StrategyWidget::onDeleteSensorStrategy);

        // 构建描述
        QString sensorType = s.value(QStringLiteral("sensorType")).toString();
        int sensorNode = s.value(QStringLiteral("sensorNode")).toInt();
        QString condition = s.value(QStringLiteral("condition")).toString();
        double threshold = s.value(QStringLiteral("threshold")).toDouble();
        int groupId = s.value(QStringLiteral("groupId")).toInt();
        QString action = s.value(QStringLiteral("action")).toString();
        
        QString conditionText = condition == QStringLiteral("gt") ? QStringLiteral(">") :
                               (condition == QStringLiteral("lt") ? QStringLiteral("<") : QStringLiteral("="));
        
        QString desc = QStringLiteral("%1#%2 %3 %4 → 分组%5 → %6")
            .arg(sensorType).arg(sensorNode)
            .arg(conditionText).arg(threshold, 0, 'f', 1)
            .arg(groupId).arg(action);

        bool enabled = s.value(QStringLiteral("enabled")).toBool();
        bool active = s.value(QStringLiteral("active")).toBool();
        card->updateInfo(name, desc, enabled, active);

        sensorCardsLayout_->addWidget(card, row, col);
        sensorStrategyCards_.append(card);

        col++;
        if (col >= 2) {
            col = 0;
            row++;
        }
    }
    
    sensorCardsLayout_->setRowStretch(row + 1, 1);
}
