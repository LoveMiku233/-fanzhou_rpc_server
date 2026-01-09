/**
 * @file strategy_widget.cpp
 * @brief 策略管理页面实现 - 表格上方按钮弹窗方式
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

StrategyWidget::StrategyWidget(RpcClient *rpcClient, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpcClient)
    , statusLabel_(nullptr)
    , tabWidget_(nullptr)
    , timerStrategyTable_(nullptr)
    , timerIdSpinBox_(nullptr)
    , timerNameEdit_(nullptr)
    , timerGroupIdSpinBox_(nullptr)
    , timerChannelSpinBox_(nullptr)
    , timerActionCombo_(nullptr)
    , timerIntervalSpinBox_(nullptr)
    , timerEnabledCheckBox_(nullptr)
    , sensorStrategyTable_(nullptr)
    , sensorIdSpinBox_(nullptr)
    , sensorNameEdit_(nullptr)
    , sensorTypeCombo_(nullptr)
    , sensorNodeSpinBox_(nullptr)
    , sensorConditionCombo_(nullptr)
    , sensorThresholdSpinBox_(nullptr)
    , sensorGroupIdSpinBox_(nullptr)
    , sensorChannelSpinBox_(nullptr)
    , sensorActionCombo_(nullptr)
    , sensorCooldownSpinBox_(nullptr)
    , sensorEnabledCheckBox_(nullptr)
    , relayStrategyTable_(nullptr)
    , relayIdSpinBox_(nullptr)
    , relayNameEdit_(nullptr)
    , relayNodeIdSpinBox_(nullptr)
    , relayChannelSpinBox_(nullptr)
    , relayActionCombo_(nullptr)
    , relayIntervalSpinBox_(nullptr)
    , relayEnabledCheckBox_(nullptr)
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
    tabWidget_->addTab(createRelayStrategyTab(), QStringLiteral("继电器策略"));
    mainLayout->addWidget(tabWidget_, 1);
}

QWidget* StrategyWidget::createTimerStrategyTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 表格上方工具栏按钮
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

    QPushButton *deleteBtn = new QPushButton(QStringLiteral("删除策略"), tab);
    deleteBtn->setProperty("type", QStringLiteral("danger"));
    deleteBtn->setMinimumHeight(40);
    connect(deleteBtn, &QPushButton::clicked, this, &StrategyWidget::onDeleteTimerStrategyClicked);
    toolbarLayout->addWidget(deleteBtn);

    QPushButton *toggleBtn = new QPushButton(QStringLiteral("启用/禁用"), tab);
    toggleBtn->setProperty("type", QStringLiteral("warning"));
    toggleBtn->setMinimumHeight(40);
    connect(toggleBtn, &QPushButton::clicked, this, &StrategyWidget::onToggleTimerStrategyClicked);
    toolbarLayout->addWidget(toggleBtn);

    QPushButton *triggerBtn = new QPushButton(QStringLiteral("立即触发"), tab);
    triggerBtn->setMinimumHeight(40);
    connect(triggerBtn, &QPushButton::clicked, this, &StrategyWidget::onTriggerTimerStrategyClicked);
    toolbarLayout->addWidget(triggerBtn);

    toolbarLayout->addStretch();
    layout->addLayout(toolbarLayout);

    // 策略列表表格
    timerStrategyTable_ = new QTableWidget(tab);
    timerStrategyTable_->setColumnCount(7);
    timerStrategyTable_->setHorizontalHeaderLabels({
        QStringLiteral("ID"), QStringLiteral("名称"), QStringLiteral("分组"),
        QStringLiteral("通道"), QStringLiteral("动作"), QStringLiteral("间隔(秒)"),
        QStringLiteral("状态")
    });
    timerStrategyTable_->horizontalHeader()->setStretchLastSection(true);
    timerStrategyTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    timerStrategyTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    timerStrategyTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    timerStrategyTable_->setAlternatingRowColors(true);
    connect(timerStrategyTable_, &QTableWidget::cellClicked,
            this, &StrategyWidget::onTimerStrategyTableClicked);
    layout->addWidget(timerStrategyTable_, 1);

    // 提示
    QLabel *helpLabel = new QLabel(
        QStringLiteral("提示：点击表格行选中策略，然后点击上方按钮操作"),
        tab);
    helpLabel->setStyleSheet(QStringLiteral(
        "color: #7f8c8d; font-size: 11px; padding: 4px;"));
    helpLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(helpLabel);

    // 隐藏的输入控件（用于弹窗）
    timerIdSpinBox_ = new QSpinBox(tab);
    timerIdSpinBox_->hide();
    timerNameEdit_ = new QLineEdit(tab);
    timerNameEdit_->hide();
    timerGroupIdSpinBox_ = new QSpinBox(tab);
    timerGroupIdSpinBox_->hide();
    timerChannelSpinBox_ = new QSpinBox(tab);
    timerChannelSpinBox_->hide();
    timerActionCombo_ = new QComboBox(tab);
    timerActionCombo_->hide();
    timerIntervalSpinBox_ = new QSpinBox(tab);
    timerIntervalSpinBox_->hide();
    timerEnabledCheckBox_ = new QCheckBox(tab);
    timerEnabledCheckBox_->hide();

    return tab;
}

QWidget* StrategyWidget::createSensorStrategyTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 表格上方工具栏按钮
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

    QPushButton *deleteBtn = new QPushButton(QStringLiteral("删除策略"), tab);
    deleteBtn->setProperty("type", QStringLiteral("danger"));
    deleteBtn->setMinimumHeight(40);
    connect(deleteBtn, &QPushButton::clicked, this, &StrategyWidget::onDeleteSensorStrategyClicked);
    toolbarLayout->addWidget(deleteBtn);

    QPushButton *toggleBtn = new QPushButton(QStringLiteral("启用/禁用"), tab);
    toggleBtn->setProperty("type", QStringLiteral("warning"));
    toggleBtn->setMinimumHeight(40);
    connect(toggleBtn, &QPushButton::clicked, this, &StrategyWidget::onToggleSensorStrategyClicked);
    toolbarLayout->addWidget(toggleBtn);

    toolbarLayout->addStretch();
    layout->addLayout(toolbarLayout);

    // 策略列表表格
    sensorStrategyTable_ = new QTableWidget(tab);
    sensorStrategyTable_->setColumnCount(8);
    sensorStrategyTable_->setHorizontalHeaderLabels({
        QStringLiteral("ID"), QStringLiteral("名称"), QStringLiteral("传感器"),
        QStringLiteral("条件"), QStringLiteral("阈值"), QStringLiteral("分组"),
        QStringLiteral("动作"), QStringLiteral("状态")
    });
    sensorStrategyTable_->horizontalHeader()->setStretchLastSection(true);
    sensorStrategyTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    sensorStrategyTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    sensorStrategyTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    sensorStrategyTable_->setAlternatingRowColors(true);
    connect(sensorStrategyTable_, &QTableWidget::cellClicked,
            this, &StrategyWidget::onSensorStrategyTableClicked);
    layout->addWidget(sensorStrategyTable_, 1);

    // 提示
    QLabel *helpLabel = new QLabel(
        QStringLiteral("提示：点击表格行选中策略，然后点击上方按钮操作"),
        tab);
    helpLabel->setStyleSheet(QStringLiteral(
        "color: #7f8c8d; font-size: 11px; padding: 4px;"));
    helpLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(helpLabel);

    // 隐藏的输入控件（用于弹窗）
    sensorIdSpinBox_ = new QSpinBox(tab);
    sensorIdSpinBox_->hide();
    sensorNameEdit_ = new QLineEdit(tab);
    sensorNameEdit_->hide();
    sensorTypeCombo_ = new QComboBox(tab);
    sensorTypeCombo_->hide();
    sensorNodeSpinBox_ = new QSpinBox(tab);
    sensorNodeSpinBox_->hide();
    sensorConditionCombo_ = new QComboBox(tab);
    sensorConditionCombo_->hide();
    sensorThresholdSpinBox_ = new QDoubleSpinBox(tab);
    sensorThresholdSpinBox_->hide();
    sensorGroupIdSpinBox_ = new QSpinBox(tab);
    sensorGroupIdSpinBox_->hide();
    sensorChannelSpinBox_ = new QSpinBox(tab);
    sensorChannelSpinBox_->hide();
    sensorActionCombo_ = new QComboBox(tab);
    sensorActionCombo_->hide();
    sensorCooldownSpinBox_ = new QSpinBox(tab);
    sensorCooldownSpinBox_->hide();
    sensorEnabledCheckBox_ = new QCheckBox(tab);
    sensorEnabledCheckBox_->hide();

    return tab;
}

QWidget* StrategyWidget::createRelayStrategyTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 表格上方工具栏按钮
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(8);

    QPushButton *refreshBtn = new QPushButton(QStringLiteral("刷新"), tab);
    refreshBtn->setMinimumHeight(40);
    connect(refreshBtn, &QPushButton::clicked, this, &StrategyWidget::onRefreshRelayStrategiesClicked);
    toolbarLayout->addWidget(refreshBtn);

    QPushButton *createBtn = new QPushButton(QStringLiteral("创建策略"), tab);
    createBtn->setProperty("type", QStringLiteral("success"));
    createBtn->setMinimumHeight(40);
    connect(createBtn, &QPushButton::clicked, this, &StrategyWidget::onCreateRelayStrategyClicked);
    toolbarLayout->addWidget(createBtn);

    QPushButton *deleteBtn = new QPushButton(QStringLiteral("删除策略"), tab);
    deleteBtn->setProperty("type", QStringLiteral("danger"));
    deleteBtn->setMinimumHeight(40);
    connect(deleteBtn, &QPushButton::clicked, this, &StrategyWidget::onDeleteRelayStrategyClicked);
    toolbarLayout->addWidget(deleteBtn);

    QPushButton *toggleBtn = new QPushButton(QStringLiteral("启用/禁用"), tab);
    toggleBtn->setProperty("type", QStringLiteral("warning"));
    toggleBtn->setMinimumHeight(40);
    connect(toggleBtn, &QPushButton::clicked, this, &StrategyWidget::onToggleRelayStrategyClicked);
    toolbarLayout->addWidget(toggleBtn);

    toolbarLayout->addStretch();
    layout->addLayout(toolbarLayout);

    // 策略列表表格
    relayStrategyTable_ = new QTableWidget(tab);
    relayStrategyTable_->setColumnCount(7);
    relayStrategyTable_->setHorizontalHeaderLabels({
        QStringLiteral("ID"), QStringLiteral("名称"), QStringLiteral("节点ID"),
        QStringLiteral("通道"), QStringLiteral("动作"), QStringLiteral("间隔(秒)"),
        QStringLiteral("状态")
    });
    relayStrategyTable_->horizontalHeader()->setStretchLastSection(true);
    relayStrategyTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    relayStrategyTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    relayStrategyTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    relayStrategyTable_->setAlternatingRowColors(true);
    connect(relayStrategyTable_, &QTableWidget::cellClicked,
            this, &StrategyWidget::onRelayStrategyTableClicked);
    layout->addWidget(relayStrategyTable_, 1);

    // 提示
    QLabel *helpLabel = new QLabel(
        QStringLiteral("提示：点击表格行选中策略，然后点击上方按钮操作"),
        tab);
    helpLabel->setStyleSheet(QStringLiteral(
        "color: #7f8c8d; font-size: 11px; padding: 4px;"));
    helpLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(helpLabel);

    // 隐藏的输入控件（用于弹窗）
    relayIdSpinBox_ = new QSpinBox(tab);
    relayIdSpinBox_->hide();
    relayNameEdit_ = new QLineEdit(tab);
    relayNameEdit_->hide();
    relayNodeIdSpinBox_ = new QSpinBox(tab);
    relayNodeIdSpinBox_->hide();
    relayChannelSpinBox_ = new QSpinBox(tab);
    relayChannelSpinBox_->hide();
    relayActionCombo_ = new QComboBox(tab);
    relayActionCombo_->hide();
    relayIntervalSpinBox_ = new QSpinBox(tab);
    relayIntervalSpinBox_->hide();
    relayEnabledCheckBox_ = new QCheckBox(tab);
    relayEnabledCheckBox_->hide();

    return tab;
}

void StrategyWidget::refreshAllStrategies()
{
    onRefreshTimerStrategiesClicked();
    onRefreshSensorStrategiesClicked();
    onRefreshRelayStrategiesClicked();
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
            updateTimerStrategyTable(strategies);
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
    
    formGrid->addWidget(new QLabel(QStringLiteral("通道:"), &dialog), 1, 2);
    QSpinBox *channelSpinBox = new QSpinBox(&dialog);
    channelSpinBox->setRange(-1, 3);
    channelSpinBox->setValue(-1);
    channelSpinBox->setMinimumHeight(40);
    formGrid->addWidget(channelSpinBox, 1, 3);
    
    // 第三行
    formGrid->addWidget(new QLabel(QStringLiteral("动作:"), &dialog), 2, 0);
    QComboBox *actionCombo = new QComboBox(&dialog);
    actionCombo->addItem(QStringLiteral("停止"), QStringLiteral("stop"));
    actionCombo->addItem(QStringLiteral("正转"), QStringLiteral("fwd"));
    actionCombo->addItem(QStringLiteral("反转"), QStringLiteral("rev"));
    actionCombo->setMinimumHeight(40);
    formGrid->addWidget(actionCombo, 2, 1);
    
    formGrid->addWidget(new QLabel(QStringLiteral("间隔(秒):"), &dialog), 2, 2);
    QSpinBox *intervalSpinBox = new QSpinBox(&dialog);
    intervalSpinBox->setRange(1, 86400);
    intervalSpinBox->setValue(60);
    intervalSpinBox->setMinimumHeight(40);
    formGrid->addWidget(intervalSpinBox, 2, 3);
    
    // 第四行
    QCheckBox *enabledCheckBox = new QCheckBox(QStringLiteral("启用策略"), &dialog);
    enabledCheckBox->setChecked(true);
    formGrid->addWidget(enabledCheckBox, 3, 0, 1, 2);
    
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
        name = QStringLiteral("策略-%1").arg(idSpinBox->value());
    }

    QJsonObject params;
    params[QStringLiteral("id")] = idSpinBox->value();
    params[QStringLiteral("name")] = name;
    params[QStringLiteral("groupId")] = groupIdSpinBox->value();
    params[QStringLiteral("channel")] = channelSpinBox->value();
    params[QStringLiteral("action")] = actionCombo->currentData().toString();
    params[QStringLiteral("intervalSec")] = intervalSpinBox->value();
    params[QStringLiteral("enabled")] = enabledCheckBox->isChecked();

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.strategy.create"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        statusLabel_->setText(QStringLiteral("策略创建成功"));
        emit logMessage(QStringLiteral("创建定时策略成功"));
        onRefreshTimerStrategiesClicked();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("创建策略失败: %1").arg(error));
    }
}

void StrategyWidget::onDeleteTimerStrategyClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    // 从表格获取选中的策略ID
    int currentRow = timerStrategyTable_->currentRow();
    if (currentRow < 0) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先选择一个策略"));
        return;
    }
    
    QTableWidgetItem *idItem = timerStrategyTable_->item(currentRow, 0);
    if (!idItem) return;
    
    int id = idItem->text().toInt();
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        QStringLiteral("确认删除"),
        QStringLiteral("确定要删除策略 %1 吗？").arg(id),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    QJsonObject params;
    params[QStringLiteral("id")] = id;

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.strategy.delete"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        statusLabel_->setText(QStringLiteral("策略 %1 删除成功").arg(id));
        emit logMessage(QStringLiteral("删除定时策略成功"));
        onRefreshTimerStrategiesClicked();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("删除策略失败: %1").arg(error));
    }
}

void StrategyWidget::onToggleTimerStrategyClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    // 从表格获取选中的策略ID
    int currentRow = timerStrategyTable_->currentRow();
    if (currentRow < 0) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先选择一个策略"));
        return;
    }
    
    QTableWidgetItem *idItem = timerStrategyTable_->item(currentRow, 0);
    QTableWidgetItem *statusItem = timerStrategyTable_->item(currentRow, 6);
    if (!idItem) return;
    
    int id = idItem->text().toInt();
    bool currentEnabled = statusItem && statusItem->text().contains(QStringLiteral("启用"));

    QJsonObject params;
    params[QStringLiteral("id")] = id;
    params[QStringLiteral("enabled")] = !currentEnabled;  // 切换状态

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.strategy.enable"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        statusLabel_->setText(QStringLiteral("策略 %1 状态已更新").arg(id));
        emit logMessage(QStringLiteral("策略状态已更新"));
        onRefreshTimerStrategiesClicked();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("更新策略失败: %1").arg(error));
    }
}

void StrategyWidget::onTriggerTimerStrategyClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    // 从表格获取选中的策略ID
    int currentRow = timerStrategyTable_->currentRow();
    if (currentRow < 0) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先选择一个策略"));
        return;
    }
    
    QTableWidgetItem *idItem = timerStrategyTable_->item(currentRow, 0);
    if (!idItem) return;
    
    int id = idItem->text().toInt();

    QJsonObject params;
    params[QStringLiteral("id")] = id;

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.strategy.trigger"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        statusLabel_->setText(QStringLiteral("策略 %1 已触发").arg(id));
        emit logMessage(QStringLiteral("手动触发策略成功"));
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("触发策略失败: %1").arg(error));
    }
}

void StrategyWidget::onTimerStrategyTableClicked(int row, int column)
{
    Q_UNUSED(column);
    QTableWidgetItem *idItem = timerStrategyTable_->item(row, 0);
    QTableWidgetItem *nameItem = timerStrategyTable_->item(row, 1);
    if (idItem) {
        timerIdSpinBox_->setValue(idItem->text().toInt());
    }
    if (nameItem) {
        timerNameEdit_->setText(nameItem->text());
    }
}

void StrategyWidget::updateTimerStrategyTable(const QJsonArray &strategies)
{
    timerStrategyTable_->setRowCount(0);

    for (const QJsonValue &val : strategies) {
        QJsonObject s = val.toObject();
        int row = timerStrategyTable_->rowCount();
        timerStrategyTable_->insertRow(row);

        timerStrategyTable_->setItem(row, 0, new QTableWidgetItem(QString::number(s.value(QStringLiteral("id")).toInt())));
        timerStrategyTable_->setItem(row, 1, new QTableWidgetItem(s.value(QStringLiteral("name")).toString()));
        timerStrategyTable_->setItem(row, 2, new QTableWidgetItem(QString::number(s.value(QStringLiteral("groupId")).toInt())));
        timerStrategyTable_->setItem(row, 3, new QTableWidgetItem(QString::number(s.value(QStringLiteral("channel")).toInt())));
        timerStrategyTable_->setItem(row, 4, new QTableWidgetItem(s.value(QStringLiteral("action")).toString()));
        timerStrategyTable_->setItem(row, 5, new QTableWidgetItem(QString::number(s.value(QStringLiteral("intervalSec")).toInt())));
        
        QString status = s.value(QStringLiteral("enabled")).toBool() ? QStringLiteral("启用") : QStringLiteral("禁用");
        if (s.value(QStringLiteral("running")).toBool()) {
            status += QStringLiteral(" [运行中]");
        }
        timerStrategyTable_->setItem(row, 6, new QTableWidgetItem(status));
    }
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
            updateSensorStrategyTable(strategies);
        }
    }
}

void StrategyWidget::onCreateSensorStrategyClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QJsonObject params;
    params[QStringLiteral("id")] = sensorIdSpinBox_->value();
    params[QStringLiteral("name")] = sensorNameEdit_->text().trimmed();
    params[QStringLiteral("sensorType")] = sensorTypeCombo_->currentData().toString();
    params[QStringLiteral("sensorNode")] = sensorNodeSpinBox_->value();
    params[QStringLiteral("condition")] = sensorConditionCombo_->currentData().toString();
    params[QStringLiteral("threshold")] = sensorThresholdSpinBox_->value();
    params[QStringLiteral("groupId")] = sensorGroupIdSpinBox_->value();
    params[QStringLiteral("channel")] = sensorChannelSpinBox_->value();
    params[QStringLiteral("action")] = sensorActionCombo_->currentData().toString();
    params[QStringLiteral("cooldownSec")] = sensorCooldownSpinBox_->value();
    params[QStringLiteral("enabled")] = sensorEnabledCheckBox_->isChecked();

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.sensor.create"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("传感器策略创建成功！"));
        emit logMessage(QStringLiteral("创建传感器策略成功"));
        onRefreshSensorStrategiesClicked();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("创建策略失败: %1").arg(error));
    }
}

void StrategyWidget::onDeleteSensorStrategyClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    int id = sensorIdSpinBox_->value();
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        QStringLiteral("确认删除"),
        QStringLiteral("确定要删除传感器策略 %1 吗？").arg(id),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    QJsonObject params;
    params[QStringLiteral("id")] = id;

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.sensor.delete"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("策略删除成功！"));
        emit logMessage(QStringLiteral("删除传感器策略成功"));
        onRefreshSensorStrategiesClicked();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("删除策略失败: %1").arg(error));
    }
}

void StrategyWidget::onToggleSensorStrategyClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QJsonObject params;
    params[QStringLiteral("id")] = sensorIdSpinBox_->value();
    params[QStringLiteral("enabled")] = sensorEnabledCheckBox_->isChecked();

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.sensor.enable"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        emit logMessage(QStringLiteral("传感器策略状态已更新"));
        onRefreshSensorStrategiesClicked();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("更新策略失败: %1").arg(error));
    }
}

void StrategyWidget::onSensorStrategyTableClicked(int row, int column)
{
    Q_UNUSED(column);
    QTableWidgetItem *idItem = sensorStrategyTable_->item(row, 0);
    QTableWidgetItem *nameItem = sensorStrategyTable_->item(row, 1);
    if (idItem) {
        sensorIdSpinBox_->setValue(idItem->text().toInt());
    }
    if (nameItem) {
        sensorNameEdit_->setText(nameItem->text());
    }
}

void StrategyWidget::updateSensorStrategyTable(const QJsonArray &strategies)
{
    sensorStrategyTable_->setRowCount(0);

    for (const QJsonValue &val : strategies) {
        QJsonObject s = val.toObject();
        int row = sensorStrategyTable_->rowCount();
        sensorStrategyTable_->insertRow(row);

        sensorStrategyTable_->setItem(row, 0, new QTableWidgetItem(QString::number(s.value(QStringLiteral("id")).toInt())));
        sensorStrategyTable_->setItem(row, 1, new QTableWidgetItem(s.value(QStringLiteral("name")).toString()));
        sensorStrategyTable_->setItem(row, 2, new QTableWidgetItem(QStringLiteral("%1#%2")
            .arg(s.value(QStringLiteral("sensorType")).toString())
            .arg(s.value(QStringLiteral("sensorNode")).toInt())));
        sensorStrategyTable_->setItem(row, 3, new QTableWidgetItem(s.value(QStringLiteral("condition")).toString()));
        sensorStrategyTable_->setItem(row, 4, new QTableWidgetItem(QString::number(s.value(QStringLiteral("threshold")).toDouble(), 'f', 2)));
        sensorStrategyTable_->setItem(row, 5, new QTableWidgetItem(QString::number(s.value(QStringLiteral("groupId")).toInt())));
        sensorStrategyTable_->setItem(row, 6, new QTableWidgetItem(s.value(QStringLiteral("action")).toString()));
        
        QString status = s.value(QStringLiteral("enabled")).toBool() ? QStringLiteral("启用") : QStringLiteral("禁用");
        if (s.value(QStringLiteral("active")).toBool()) {
            status += QStringLiteral(" [活跃]");
        }
        sensorStrategyTable_->setItem(row, 7, new QTableWidgetItem(status));
    }
}

// ==================== 继电器策略槽函数 ====================

void StrategyWidget::onRefreshRelayStrategiesClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        return;
    }

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.relay.list"));
    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.contains(QStringLiteral("strategies"))) {
            QJsonArray strategies = obj.value(QStringLiteral("strategies")).toArray();
            updateRelayStrategyTable(strategies);
        }
    }
}

void StrategyWidget::onCreateRelayStrategyClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QJsonObject params;
    params[QStringLiteral("id")] = relayIdSpinBox_->value();
    params[QStringLiteral("name")] = relayNameEdit_->text().trimmed();
    params[QStringLiteral("nodeId")] = relayNodeIdSpinBox_->value();
    params[QStringLiteral("channel")] = relayChannelSpinBox_->value();
    params[QStringLiteral("action")] = relayActionCombo_->currentData().toString();
    params[QStringLiteral("intervalSec")] = relayIntervalSpinBox_->value();
    params[QStringLiteral("enabled")] = relayEnabledCheckBox_->isChecked();

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.relay.create"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("继电器策略创建成功！"));
        emit logMessage(QStringLiteral("创建继电器策略成功"));
        onRefreshRelayStrategiesClicked();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("创建策略失败: %1").arg(error));
    }
}

void StrategyWidget::onDeleteRelayStrategyClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    int id = relayIdSpinBox_->value();
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        QStringLiteral("确认删除"),
        QStringLiteral("确定要删除继电器策略 %1 吗？").arg(id),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    QJsonObject params;
    params[QStringLiteral("id")] = id;

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.relay.delete"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("策略删除成功！"));
        emit logMessage(QStringLiteral("删除继电器策略成功"));
        onRefreshRelayStrategiesClicked();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("删除策略失败: %1").arg(error));
    }
}

void StrategyWidget::onToggleRelayStrategyClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QJsonObject params;
    params[QStringLiteral("id")] = relayIdSpinBox_->value();
    params[QStringLiteral("enabled")] = relayEnabledCheckBox_->isChecked();

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.relay.enable"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        emit logMessage(QStringLiteral("继电器策略状态已更新"));
        onRefreshRelayStrategiesClicked();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("更新策略失败: %1").arg(error));
    }
}

void StrategyWidget::onRelayStrategyTableClicked(int row, int column)
{
    Q_UNUSED(column);
    QTableWidgetItem *idItem = relayStrategyTable_->item(row, 0);
    QTableWidgetItem *nameItem = relayStrategyTable_->item(row, 1);
    if (idItem) {
        relayIdSpinBox_->setValue(idItem->text().toInt());
    }
    if (nameItem) {
        relayNameEdit_->setText(nameItem->text());
    }
}

void StrategyWidget::updateRelayStrategyTable(const QJsonArray &strategies)
{
    relayStrategyTable_->setRowCount(0);

    for (const QJsonValue &val : strategies) {
        QJsonObject s = val.toObject();
        int row = relayStrategyTable_->rowCount();
        relayStrategyTable_->insertRow(row);

        relayStrategyTable_->setItem(row, 0, new QTableWidgetItem(QString::number(s.value(QStringLiteral("id")).toInt())));
        relayStrategyTable_->setItem(row, 1, new QTableWidgetItem(s.value(QStringLiteral("name")).toString()));
        relayStrategyTable_->setItem(row, 2, new QTableWidgetItem(QString::number(s.value(QStringLiteral("nodeId")).toInt())));
        relayStrategyTable_->setItem(row, 3, new QTableWidgetItem(QString::number(s.value(QStringLiteral("channel")).toInt())));
        relayStrategyTable_->setItem(row, 4, new QTableWidgetItem(s.value(QStringLiteral("action")).toString()));
        relayStrategyTable_->setItem(row, 5, new QTableWidgetItem(QString::number(s.value(QStringLiteral("intervalSec")).toInt())));
        
        QString status = s.value(QStringLiteral("enabled")).toBool() ? QStringLiteral("启用") : QStringLiteral("禁用");
        relayStrategyTable_->setItem(row, 6, new QTableWidgetItem(status));
    }
}
