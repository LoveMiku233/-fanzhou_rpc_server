/**
 * @file relay_control_widget.cpp
 * @brief 继电器控制页面实现
 */

#include "relay_control_widget.h"
#include "rpc_client.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QDateTime>

RelayControlWidget::RelayControlWidget(RpcClient *rpcClient, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpcClient)
    , nodeSpinBox_(nullptr)
    , channelCombo_(nullptr)
    , actionCombo_(nullptr)
    , resultTextEdit_(nullptr)
    , statusLabel_(nullptr)
{
    setupUi();
}

void RelayControlWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    QHBoxLayout *topLayout = new QHBoxLayout();

    // 控制面板
    QGroupBox *controlGroupBox = new QGroupBox(QStringLiteral("🎛️ 继电器控制"), this);
    QFormLayout *controlLayout = new QFormLayout(controlGroupBox);

    nodeSpinBox_ = new QSpinBox(this);
    nodeSpinBox_->setRange(1, 255);
    nodeSpinBox_->setValue(1);
    controlLayout->addRow(QStringLiteral("节点ID:"), nodeSpinBox_);

    channelCombo_ = new QComboBox(this);
    channelCombo_->addItem(QStringLiteral("通道 0"), 0);
    channelCombo_->addItem(QStringLiteral("通道 1"), 1);
    channelCombo_->addItem(QStringLiteral("通道 2"), 2);
    channelCombo_->addItem(QStringLiteral("通道 3"), 3);
    controlLayout->addRow(QStringLiteral("通道:"), channelCombo_);

    actionCombo_ = new QComboBox(this);
    actionCombo_->addItem(QStringLiteral("⏹️ 停止"), QStringLiteral("stop"));
    actionCombo_->addItem(QStringLiteral("▶️ 正转"), QStringLiteral("fwd"));
    actionCombo_->addItem(QStringLiteral("◀️ 反转"), QStringLiteral("rev"));
    controlLayout->addRow(QStringLiteral("动作:"), actionCombo_);

    // 控制按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    
    QPushButton *controlButton = new QPushButton(QStringLiteral("⚡ 执行控制"), this);
    controlButton->setProperty("type", QStringLiteral("success"));
    connect(controlButton, &QPushButton::clicked, this, &RelayControlWidget::onControlClicked);
    btnLayout->addWidget(controlButton);

    QPushButton *queryButton = new QPushButton(QStringLiteral("🔍 查询状态"), this);
    connect(queryButton, &QPushButton::clicked, this, &RelayControlWidget::onQueryClicked);
    btnLayout->addWidget(queryButton);

    controlLayout->addRow(btnLayout);

    QHBoxLayout *btn2Layout = new QHBoxLayout();
    
    QPushButton *queryAllButton = new QPushButton(QStringLiteral("📊 查询全部通道"), this);
    connect(queryAllButton, &QPushButton::clicked, this, &RelayControlWidget::onQueryAllClicked);
    btn2Layout->addWidget(queryAllButton);

    QPushButton *stopAllButton = new QPushButton(QStringLiteral("🛑 停止全部"), this);
    stopAllButton->setProperty("type", QStringLiteral("danger"));
    connect(stopAllButton, &QPushButton::clicked, this, &RelayControlWidget::onStopAllClicked);
    btn2Layout->addWidget(stopAllButton);

    controlLayout->addRow(btn2Layout);

    topLayout->addWidget(controlGroupBox);

    // 快捷控制面板
    QGroupBox *quickGroupBox = new QGroupBox(QStringLiteral("🚀 快捷控制"), this);
    QGridLayout *quickLayout = new QGridLayout(quickGroupBox);

    // 为每个通道创建快捷按钮
    for (int ch = 0; ch < 4; ++ch) {
        QLabel *chLabel = new QLabel(QStringLiteral("通道 %1:").arg(ch), this);
        quickLayout->addWidget(chLabel, ch, 0);

        QPushButton *stopBtn = new QPushButton(QStringLiteral("⏹"), this);
        stopBtn->setProperty("channel", ch);
        stopBtn->setProperty("action", QStringLiteral("stop"));
        stopBtn->setMaximumWidth(50);
        connect(stopBtn, &QPushButton::clicked, this, &RelayControlWidget::onQuickControlClicked);
        quickLayout->addWidget(stopBtn, ch, 1);

        QPushButton *fwdBtn = new QPushButton(QStringLiteral("▶"), this);
        fwdBtn->setProperty("channel", ch);
        fwdBtn->setProperty("action", QStringLiteral("fwd"));
        fwdBtn->setProperty("type", QStringLiteral("success"));
        fwdBtn->setMaximumWidth(50);
        connect(fwdBtn, &QPushButton::clicked, this, &RelayControlWidget::onQuickControlClicked);
        quickLayout->addWidget(fwdBtn, ch, 2);

        QPushButton *revBtn = new QPushButton(QStringLiteral("◀"), this);
        revBtn->setProperty("channel", ch);
        revBtn->setProperty("action", QStringLiteral("rev"));
        revBtn->setProperty("type", QStringLiteral("warning"));
        revBtn->setMaximumWidth(50);
        connect(revBtn, &QPushButton::clicked, this, &RelayControlWidget::onQuickControlClicked);
        quickLayout->addWidget(revBtn, ch, 3);
    }

    topLayout->addWidget(quickGroupBox);
    topLayout->addStretch();

    mainLayout->addLayout(topLayout);

    // 状态标签
    statusLabel_ = new QLabel(this);
    mainLayout->addWidget(statusLabel_);

    // 结果显示
    QGroupBox *resultGroupBox = new QGroupBox(QStringLiteral("📋 操作结果"), this);
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroupBox);

    resultTextEdit_ = new QTextEdit(this);
    resultTextEdit_->setReadOnly(true);
    resultTextEdit_->setMinimumHeight(200);

    QPushButton *clearButton = new QPushButton(QStringLiteral("🗑️ 清空"), this);
    connect(clearButton, &QPushButton::clicked, resultTextEdit_, &QTextEdit::clear);

    resultLayout->addWidget(resultTextEdit_);
    resultLayout->addWidget(clearButton);

    mainLayout->addWidget(resultGroupBox, 1);

    // 说明
    QLabel *helpLabel = new QLabel(
        QStringLiteral("💡 提示：使用快捷控制按钮可以快速操作单个通道。⏹=停止，▶=正转，◀=反转"),
        this);
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet(QStringLiteral("color: #666; font-size: 12px; padding: 5px;"));
    mainLayout->addWidget(helpLabel);
}

void RelayControlWidget::onControlClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    int node = nodeSpinBox_->value();
    int channel = channelCombo_->currentData().toInt();
    QString action = actionCombo_->currentData().toString();

    controlRelay(node, channel, action);
}

void RelayControlWidget::onQueryClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    int node = nodeSpinBox_->value();
    int channel = channelCombo_->currentData().toInt();

    QJsonObject params;
    params[QStringLiteral("node")] = node;
    params[QStringLiteral("ch")] = channel;

    QJsonValue result = rpcClient_->call(QStringLiteral("relay.status"), params);
    
    QString resultStr = QString::fromUtf8(QJsonDocument(result.toObject()).toJson(QJsonDocument::Indented));
    appendResult(QStringLiteral("查询节点 %1 通道 %2:\n%3").arg(node).arg(channel).arg(resultStr));
    
    statusLabel_->setText(QStringLiteral("✅ 查询完成"));
}

void RelayControlWidget::onQueryAllClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    int node = nodeSpinBox_->value();

    QJsonObject params;
    params[QStringLiteral("node")] = node;

    QJsonValue result = rpcClient_->call(QStringLiteral("relay.statusAll"), params);
    
    QString resultStr = QString::fromUtf8(QJsonDocument(result.toObject()).toJson(QJsonDocument::Indented));
    appendResult(QStringLiteral("查询节点 %1 全部通道:\n%2").arg(node).arg(resultStr));
    
    statusLabel_->setText(QStringLiteral("✅ 查询完成"));
}

void RelayControlWidget::onStopAllClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this, 
        QStringLiteral("确认"),
        QStringLiteral("确定要停止节点 %1 的所有通道吗？").arg(nodeSpinBox_->value()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    int node = nodeSpinBox_->value();
    
    for (int ch = 0; ch < 4; ++ch) {
        controlRelay(node, ch, QStringLiteral("stop"));
    }

    statusLabel_->setText(QStringLiteral("✅ 已停止节点 %1 的所有通道").arg(node));
}

void RelayControlWidget::onQuickControlClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    int node = nodeSpinBox_->value();
    int channel = btn->property("channel").toInt();
    QString action = btn->property("action").toString();

    controlRelay(node, channel, action);
}

void RelayControlWidget::controlRelay(int node, int channel, const QString &action)
{
    QJsonObject params;
    params[QStringLiteral("node")] = node;
    params[QStringLiteral("ch")] = channel;
    params[QStringLiteral("action")] = action;

    QJsonValue result = rpcClient_->call(QStringLiteral("relay.control"), params);
    
    QJsonObject obj = result.toObject();
    if (obj.value(QStringLiteral("ok")).toBool()) {
        QString actionText;
        if (action == QStringLiteral("stop")) actionText = QStringLiteral("停止");
        else if (action == QStringLiteral("fwd")) actionText = QStringLiteral("正转");
        else if (action == QStringLiteral("rev")) actionText = QStringLiteral("反转");
        
        appendResult(QStringLiteral("✅ 节点 %1 通道 %2 -> %3")
            .arg(node).arg(channel).arg(actionText));
        statusLabel_->setText(QStringLiteral("✅ 控制成功"));
        
        // 检查警告
        if (obj.contains(QStringLiteral("warning"))) {
            QString warning = obj.value(QStringLiteral("warning")).toString();
            appendResult(QStringLiteral("⚠️ 警告: %1").arg(warning));
        }
    } else {
        QString error = obj.value(QStringLiteral("error")).toString();
        if (obj.contains(QStringLiteral("rpcError"))) {
            QJsonObject rpcError = obj.value(QStringLiteral("rpcError")).toObject();
            error = rpcError.value(QStringLiteral("message")).toString();
        }
        appendResult(QStringLiteral("❌ 控制失败: %1").arg(error));
        statusLabel_->setText(QStringLiteral("❌ 控制失败"));
    }
}

void RelayControlWidget::appendResult(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    resultTextEdit_->append(QStringLiteral("[%1] %2").arg(timestamp, message));
    
    // 滚动到底部
    QTextCursor cursor = resultTextEdit_->textCursor();
    cursor.movePosition(QTextCursor::End);
    resultTextEdit_->setTextCursor(cursor);
}
