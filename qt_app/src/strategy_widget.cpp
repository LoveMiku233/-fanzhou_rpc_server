/**
 * @file strategy_widget.cpp
 * @brief 策略管理页面实现
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

    // 策略列表
    QGroupBox *listBox = new QGroupBox(QStringLiteral("定时策略列表"), tab);
    QVBoxLayout *listLayout = new QVBoxLayout(listBox);

    timerStrategyTable_ = new QTableWidget(tab);
    timerStrategyTable_->setColumnCount(7);
    timerStrategyTable_->setHorizontalHeaderLabels({
        QStringLiteral("ID"), QStringLiteral("名称"), QStringLiteral("分组"),
        QStringLiteral("通道"), QStringLiteral("动作"), QStringLiteral("间隔(秒)"),
        QStringLiteral("状态")
    });
    timerStrategyTable_->horizontalHeader()->setStretchLastSection(true);
    timerStrategyTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    timerStrategyTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    timerStrategyTable_->setMaximumHeight(150);
    connect(timerStrategyTable_, &QTableWidget::cellClicked,
            this, &StrategyWidget::onTimerStrategyTableClicked);
    listLayout->addWidget(timerStrategyTable_);

    QPushButton *refreshBtn = new QPushButton(QStringLiteral("刷新"), tab);
    connect(refreshBtn, &QPushButton::clicked, this, &StrategyWidget::onRefreshTimerStrategiesClicked);
    listLayout->addWidget(refreshBtn);

    layout->addWidget(listBox);

    // 创建/编辑策略
    QGroupBox *editBox = new QGroupBox(QStringLiteral("创建/管理策略"), tab);
    QGridLayout *editGrid = new QGridLayout(editBox);
    editGrid->setSpacing(8);

    // 第一行
    editGrid->addWidget(new QLabel(QStringLiteral("策略ID:"), tab), 0, 0);
    timerIdSpinBox_ = new QSpinBox(tab);
    timerIdSpinBox_->setRange(1, 999);
    timerIdSpinBox_->setMinimumHeight(32);
    editGrid->addWidget(timerIdSpinBox_, 0, 1);

    editGrid->addWidget(new QLabel(QStringLiteral("名称:"), tab), 0, 2);
    timerNameEdit_ = new QLineEdit(tab);
    timerNameEdit_->setPlaceholderText(QStringLiteral("策略名称"));
    timerNameEdit_->setMinimumHeight(32);
    editGrid->addWidget(timerNameEdit_, 0, 3);

    // 第二行
    editGrid->addWidget(new QLabel(QStringLiteral("分组ID:"), tab), 1, 0);
    timerGroupIdSpinBox_ = new QSpinBox(tab);
    timerGroupIdSpinBox_->setRange(1, 999);
    timerGroupIdSpinBox_->setMinimumHeight(32);
    editGrid->addWidget(timerGroupIdSpinBox_, 1, 1);

    editGrid->addWidget(new QLabel(QStringLiteral("通道:"), tab), 1, 2);
    timerChannelSpinBox_ = new QSpinBox(tab);
    timerChannelSpinBox_->setRange(-1, 3);
    timerChannelSpinBox_->setValue(-1);  // -1=全部
    timerChannelSpinBox_->setMinimumHeight(32);
    editGrid->addWidget(timerChannelSpinBox_, 1, 3);

    // 第三行
    editGrid->addWidget(new QLabel(QStringLiteral("动作:"), tab), 2, 0);
    timerActionCombo_ = new QComboBox(tab);
    timerActionCombo_->addItem(QStringLiteral("停止"), QStringLiteral("stop"));
    timerActionCombo_->addItem(QStringLiteral("正转"), QStringLiteral("fwd"));
    timerActionCombo_->addItem(QStringLiteral("反转"), QStringLiteral("rev"));
    timerActionCombo_->setMinimumHeight(32);
    editGrid->addWidget(timerActionCombo_, 2, 1);

    editGrid->addWidget(new QLabel(QStringLiteral("间隔(秒):"), tab), 2, 2);
    timerIntervalSpinBox_ = new QSpinBox(tab);
    timerIntervalSpinBox_->setRange(1, 86400);
    timerIntervalSpinBox_->setValue(60);
    timerIntervalSpinBox_->setMinimumHeight(32);
    editGrid->addWidget(timerIntervalSpinBox_, 2, 3);

    // 第四行
    timerEnabledCheckBox_ = new QCheckBox(QStringLiteral("启用"), tab);
    timerEnabledCheckBox_->setChecked(true);
    editGrid->addWidget(timerEnabledCheckBox_, 3, 0, 1, 2);

    layout->addWidget(editBox);

    // 操作按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    QPushButton *createBtn = new QPushButton(QStringLiteral("创建策略"), tab);
    createBtn->setProperty("type", QStringLiteral("success"));
    createBtn->setMinimumHeight(36);
    connect(createBtn, &QPushButton::clicked, this, &StrategyWidget::onCreateTimerStrategyClicked);
    btnLayout->addWidget(createBtn);

    QPushButton *deleteBtn = new QPushButton(QStringLiteral("删除策略"), tab);
    deleteBtn->setProperty("type", QStringLiteral("danger"));
    deleteBtn->setMinimumHeight(36);
    connect(deleteBtn, &QPushButton::clicked, this, &StrategyWidget::onDeleteTimerStrategyClicked);
    btnLayout->addWidget(deleteBtn);

    QPushButton *toggleBtn = new QPushButton(QStringLiteral("启用/禁用"), tab);
    toggleBtn->setProperty("type", QStringLiteral("warning"));
    toggleBtn->setMinimumHeight(36);
    connect(toggleBtn, &QPushButton::clicked, this, &StrategyWidget::onToggleTimerStrategyClicked);
    btnLayout->addWidget(toggleBtn);

    QPushButton *triggerBtn = new QPushButton(QStringLiteral("立即触发"), tab);
    triggerBtn->setMinimumHeight(36);
    connect(triggerBtn, &QPushButton::clicked, this, &StrategyWidget::onTriggerTimerStrategyClicked);
    btnLayout->addWidget(triggerBtn);

    layout->addLayout(btnLayout);
    layout->addStretch();

    return tab;
}

QWidget* StrategyWidget::createSensorStrategyTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 策略列表
    QGroupBox *listBox = new QGroupBox(QStringLiteral("传感器策略列表"), tab);
    QVBoxLayout *listLayout = new QVBoxLayout(listBox);

    sensorStrategyTable_ = new QTableWidget(tab);
    sensorStrategyTable_->setColumnCount(8);
    sensorStrategyTable_->setHorizontalHeaderLabels({
        QStringLiteral("ID"), QStringLiteral("名称"), QStringLiteral("传感器"),
        QStringLiteral("条件"), QStringLiteral("阈值"), QStringLiteral("分组"),
        QStringLiteral("动作"), QStringLiteral("状态")
    });
    sensorStrategyTable_->horizontalHeader()->setStretchLastSection(true);
    sensorStrategyTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    sensorStrategyTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    sensorStrategyTable_->setMaximumHeight(130);
    connect(sensorStrategyTable_, &QTableWidget::cellClicked,
            this, &StrategyWidget::onSensorStrategyTableClicked);
    listLayout->addWidget(sensorStrategyTable_);

    QPushButton *refreshBtn = new QPushButton(QStringLiteral("刷新"), tab);
    connect(refreshBtn, &QPushButton::clicked, this, &StrategyWidget::onRefreshSensorStrategiesClicked);
    listLayout->addWidget(refreshBtn);

    layout->addWidget(listBox);

    // 创建策略
    QGroupBox *editBox = new QGroupBox(QStringLiteral("创建/管理传感器策略"), tab);
    QGridLayout *editGrid = new QGridLayout(editBox);
    editGrid->setSpacing(6);

    // 第一行
    editGrid->addWidget(new QLabel(QStringLiteral("ID:"), tab), 0, 0);
    sensorIdSpinBox_ = new QSpinBox(tab);
    sensorIdSpinBox_->setRange(1, 999);
    sensorIdSpinBox_->setMinimumHeight(32);
    editGrid->addWidget(sensorIdSpinBox_, 0, 1);

    editGrid->addWidget(new QLabel(QStringLiteral("名称:"), tab), 0, 2);
    sensorNameEdit_ = new QLineEdit(tab);
    sensorNameEdit_->setMinimumHeight(32);
    editGrid->addWidget(sensorNameEdit_, 0, 3);

    // 第二行
    editGrid->addWidget(new QLabel(QStringLiteral("传感器类型:"), tab), 1, 0);
    sensorTypeCombo_ = new QComboBox(tab);
    sensorTypeCombo_->addItem(QStringLiteral("温度"), QStringLiteral("temperature"));
    sensorTypeCombo_->addItem(QStringLiteral("湿度"), QStringLiteral("humidity"));
    sensorTypeCombo_->addItem(QStringLiteral("光照"), QStringLiteral("light"));
    sensorTypeCombo_->addItem(QStringLiteral("土壤湿度"), QStringLiteral("soil_moisture"));
    sensorTypeCombo_->setMinimumHeight(32);
    editGrid->addWidget(sensorTypeCombo_, 1, 1);

    editGrid->addWidget(new QLabel(QStringLiteral("传感器节点:"), tab), 1, 2);
    sensorNodeSpinBox_ = new QSpinBox(tab);
    sensorNodeSpinBox_->setRange(1, 255);
    sensorNodeSpinBox_->setMinimumHeight(32);
    editGrid->addWidget(sensorNodeSpinBox_, 1, 3);

    // 第三行
    editGrid->addWidget(new QLabel(QStringLiteral("条件:"), tab), 2, 0);
    sensorConditionCombo_ = new QComboBox(tab);
    sensorConditionCombo_->addItem(QStringLiteral("大于 >"), QStringLiteral(">"));
    sensorConditionCombo_->addItem(QStringLiteral("小于 <"), QStringLiteral("<"));
    sensorConditionCombo_->addItem(QStringLiteral("等于 ="), QStringLiteral("="));
    sensorConditionCombo_->addItem(QStringLiteral("大于等于 >="), QStringLiteral(">="));
    sensorConditionCombo_->addItem(QStringLiteral("小于等于 <="), QStringLiteral("<="));
    sensorConditionCombo_->setMinimumHeight(32);
    editGrid->addWidget(sensorConditionCombo_, 2, 1);

    editGrid->addWidget(new QLabel(QStringLiteral("阈值:"), tab), 2, 2);
    sensorThresholdSpinBox_ = new QDoubleSpinBox(tab);
    sensorThresholdSpinBox_->setRange(-1000, 1000);
    sensorThresholdSpinBox_->setDecimals(2);
    sensorThresholdSpinBox_->setMinimumHeight(32);
    editGrid->addWidget(sensorThresholdSpinBox_, 2, 3);

    // 第四行
    editGrid->addWidget(new QLabel(QStringLiteral("分组ID:"), tab), 3, 0);
    sensorGroupIdSpinBox_ = new QSpinBox(tab);
    sensorGroupIdSpinBox_->setRange(1, 999);
    sensorGroupIdSpinBox_->setMinimumHeight(32);
    editGrid->addWidget(sensorGroupIdSpinBox_, 3, 1);

    editGrid->addWidget(new QLabel(QStringLiteral("通道:"), tab), 3, 2);
    sensorChannelSpinBox_ = new QSpinBox(tab);
    sensorChannelSpinBox_->setRange(-1, 3);
    sensorChannelSpinBox_->setValue(-1);
    sensorChannelSpinBox_->setMinimumHeight(32);
    editGrid->addWidget(sensorChannelSpinBox_, 3, 3);

    // 第五行
    editGrid->addWidget(new QLabel(QStringLiteral("动作:"), tab), 4, 0);
    sensorActionCombo_ = new QComboBox(tab);
    sensorActionCombo_->addItem(QStringLiteral("停止"), QStringLiteral("stop"));
    sensorActionCombo_->addItem(QStringLiteral("正转"), QStringLiteral("fwd"));
    sensorActionCombo_->addItem(QStringLiteral("反转"), QStringLiteral("rev"));
    sensorActionCombo_->setMinimumHeight(32);
    editGrid->addWidget(sensorActionCombo_, 4, 1);

    editGrid->addWidget(new QLabel(QStringLiteral("冷却(秒):"), tab), 4, 2);
    sensorCooldownSpinBox_ = new QSpinBox(tab);
    sensorCooldownSpinBox_->setRange(0, 86400);
    sensorCooldownSpinBox_->setValue(60);
    sensorCooldownSpinBox_->setMinimumHeight(32);
    editGrid->addWidget(sensorCooldownSpinBox_, 4, 3);

    sensorEnabledCheckBox_ = new QCheckBox(QStringLiteral("启用"), tab);
    sensorEnabledCheckBox_->setChecked(true);
    editGrid->addWidget(sensorEnabledCheckBox_, 5, 0, 1, 2);

    layout->addWidget(editBox);

    // 操作按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    QPushButton *createBtn = new QPushButton(QStringLiteral("创建策略"), tab);
    createBtn->setProperty("type", QStringLiteral("success"));
    createBtn->setMinimumHeight(36);
    connect(createBtn, &QPushButton::clicked, this, &StrategyWidget::onCreateSensorStrategyClicked);
    btnLayout->addWidget(createBtn);

    QPushButton *deleteBtn = new QPushButton(QStringLiteral("删除策略"), tab);
    deleteBtn->setProperty("type", QStringLiteral("danger"));
    deleteBtn->setMinimumHeight(36);
    connect(deleteBtn, &QPushButton::clicked, this, &StrategyWidget::onDeleteSensorStrategyClicked);
    btnLayout->addWidget(deleteBtn);

    QPushButton *toggleBtn = new QPushButton(QStringLiteral("启用/禁用"), tab);
    toggleBtn->setProperty("type", QStringLiteral("warning"));
    toggleBtn->setMinimumHeight(36);
    connect(toggleBtn, &QPushButton::clicked, this, &StrategyWidget::onToggleSensorStrategyClicked);
    btnLayout->addWidget(toggleBtn);

    layout->addLayout(btnLayout);
    layout->addStretch();

    return tab;
}

QWidget* StrategyWidget::createRelayStrategyTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 策略列表
    QGroupBox *listBox = new QGroupBox(QStringLiteral("继电器定时策略列表"), tab);
    QVBoxLayout *listLayout = new QVBoxLayout(listBox);

    relayStrategyTable_ = new QTableWidget(tab);
    relayStrategyTable_->setColumnCount(7);
    relayStrategyTable_->setHorizontalHeaderLabels({
        QStringLiteral("ID"), QStringLiteral("名称"), QStringLiteral("节点ID"),
        QStringLiteral("通道"), QStringLiteral("动作"), QStringLiteral("间隔(秒)"),
        QStringLiteral("状态")
    });
    relayStrategyTable_->horizontalHeader()->setStretchLastSection(true);
    relayStrategyTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    relayStrategyTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    relayStrategyTable_->setMaximumHeight(150);
    connect(relayStrategyTable_, &QTableWidget::cellClicked,
            this, &StrategyWidget::onRelayStrategyTableClicked);
    listLayout->addWidget(relayStrategyTable_);

    QPushButton *refreshBtn = new QPushButton(QStringLiteral("刷新"), tab);
    connect(refreshBtn, &QPushButton::clicked, this, &StrategyWidget::onRefreshRelayStrategiesClicked);
    listLayout->addWidget(refreshBtn);

    layout->addWidget(listBox);

    // 创建策略
    QGroupBox *editBox = new QGroupBox(QStringLiteral("创建/管理继电器策略"), tab);
    QGridLayout *editGrid = new QGridLayout(editBox);
    editGrid->setSpacing(8);

    // 第一行
    editGrid->addWidget(new QLabel(QStringLiteral("策略ID:"), tab), 0, 0);
    relayIdSpinBox_ = new QSpinBox(tab);
    relayIdSpinBox_->setRange(1, 999);
    relayIdSpinBox_->setMinimumHeight(32);
    editGrid->addWidget(relayIdSpinBox_, 0, 1);

    editGrid->addWidget(new QLabel(QStringLiteral("名称:"), tab), 0, 2);
    relayNameEdit_ = new QLineEdit(tab);
    relayNameEdit_->setPlaceholderText(QStringLiteral("策略名称"));
    relayNameEdit_->setMinimumHeight(32);
    editGrid->addWidget(relayNameEdit_, 0, 3);

    // 第二行
    editGrid->addWidget(new QLabel(QStringLiteral("节点ID:"), tab), 1, 0);
    relayNodeIdSpinBox_ = new QSpinBox(tab);
    relayNodeIdSpinBox_->setRange(1, 255);
    relayNodeIdSpinBox_->setMinimumHeight(32);
    editGrid->addWidget(relayNodeIdSpinBox_, 1, 1);

    editGrid->addWidget(new QLabel(QStringLiteral("通道:"), tab), 1, 2);
    relayChannelSpinBox_ = new QSpinBox(tab);
    relayChannelSpinBox_->setRange(-1, 3);
    relayChannelSpinBox_->setValue(-1);
    relayChannelSpinBox_->setMinimumHeight(32);
    editGrid->addWidget(relayChannelSpinBox_, 1, 3);

    // 第三行
    editGrid->addWidget(new QLabel(QStringLiteral("动作:"), tab), 2, 0);
    relayActionCombo_ = new QComboBox(tab);
    relayActionCombo_->addItem(QStringLiteral("停止"), QStringLiteral("stop"));
    relayActionCombo_->addItem(QStringLiteral("正转"), QStringLiteral("fwd"));
    relayActionCombo_->addItem(QStringLiteral("反转"), QStringLiteral("rev"));
    relayActionCombo_->setMinimumHeight(32);
    editGrid->addWidget(relayActionCombo_, 2, 1);

    editGrid->addWidget(new QLabel(QStringLiteral("间隔(秒):"), tab), 2, 2);
    relayIntervalSpinBox_ = new QSpinBox(tab);
    relayIntervalSpinBox_->setRange(1, 86400);
    relayIntervalSpinBox_->setValue(60);
    relayIntervalSpinBox_->setMinimumHeight(32);
    editGrid->addWidget(relayIntervalSpinBox_, 2, 3);

    // 第四行
    relayEnabledCheckBox_ = new QCheckBox(QStringLiteral("启用"), tab);
    relayEnabledCheckBox_->setChecked(true);
    editGrid->addWidget(relayEnabledCheckBox_, 3, 0, 1, 2);

    layout->addWidget(editBox);

    // 操作按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    QPushButton *createBtn = new QPushButton(QStringLiteral("创建策略"), tab);
    createBtn->setProperty("type", QStringLiteral("success"));
    createBtn->setMinimumHeight(36);
    connect(createBtn, &QPushButton::clicked, this, &StrategyWidget::onCreateRelayStrategyClicked);
    btnLayout->addWidget(createBtn);

    QPushButton *deleteBtn = new QPushButton(QStringLiteral("删除策略"), tab);
    deleteBtn->setProperty("type", QStringLiteral("danger"));
    deleteBtn->setMinimumHeight(36);
    connect(deleteBtn, &QPushButton::clicked, this, &StrategyWidget::onDeleteRelayStrategyClicked);
    btnLayout->addWidget(deleteBtn);

    QPushButton *toggleBtn = new QPushButton(QStringLiteral("启用/禁用"), tab);
    toggleBtn->setProperty("type", QStringLiteral("warning"));
    toggleBtn->setMinimumHeight(36);
    connect(toggleBtn, &QPushButton::clicked, this, &StrategyWidget::onToggleRelayStrategyClicked);
    btnLayout->addWidget(toggleBtn);

    layout->addLayout(btnLayout);
    layout->addStretch();

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

    QJsonObject params;
    params[QStringLiteral("id")] = timerIdSpinBox_->value();
    params[QStringLiteral("name")] = timerNameEdit_->text().trimmed();
    params[QStringLiteral("groupId")] = timerGroupIdSpinBox_->value();
    params[QStringLiteral("channel")] = timerChannelSpinBox_->value();
    params[QStringLiteral("action")] = timerActionCombo_->currentData().toString();
    params[QStringLiteral("intervalSec")] = timerIntervalSpinBox_->value();
    params[QStringLiteral("enabled")] = timerEnabledCheckBox_->isChecked();

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.strategy.create"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("策略创建成功！"));
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

    int id = timerIdSpinBox_->value();
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        QStringLiteral("确认删除"),
        QStringLiteral("确定要删除策略 %1 吗？").arg(id),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    QJsonObject params;
    params[QStringLiteral("id")] = id;

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.strategy.delete"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("策略删除成功！"));
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

    QJsonObject params;
    params[QStringLiteral("id")] = timerIdSpinBox_->value();
    params[QStringLiteral("enabled")] = timerEnabledCheckBox_->isChecked();

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.strategy.enable"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
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

    QJsonObject params;
    params[QStringLiteral("id")] = timerIdSpinBox_->value();

    QJsonValue result = rpcClient_->call(QStringLiteral("auto.strategy.trigger"), params);

    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("策略已触发！"));
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
