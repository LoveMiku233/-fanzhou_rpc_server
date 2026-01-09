/**
 * @file relay_control_dialog.cpp
 * @brief 继电器控制对话框实现
 */

#include "relay_control_dialog.h"
#include "rpc_client.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QFrame>

RelayControlDialog::RelayControlDialog(RpcClient *rpcClient, int nodeId,
                                         const QString &deviceName, QWidget *parent)
    : QDialog(parent)
    , rpcClient_(rpcClient)
    , nodeId_(nodeId)
    , deviceName_(deviceName)
    , statusLabel_(nullptr)
    , ch0StatusLabel_(nullptr)
    , ch1StatusLabel_(nullptr)
    , ch2StatusLabel_(nullptr)
    , ch3StatusLabel_(nullptr)
    , currentLabel_(nullptr)
{
    setWindowTitle(QStringLiteral("控制: %1 (#%2)").arg(deviceName).arg(nodeId));
    setMinimumSize(360, 420);
    setModal(true);
    setupUi();
    
    // 初始查询状态
    onQueryStatusClicked();
}

void RelayControlDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // 设备信息 - 使用纯文本
    QLabel *titleLabel = new QLabel(QStringLiteral("[R] %1").arg(deviceName_), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: 14px; font-weight: bold; color: #2c3e50;"));
    mainLayout->addWidget(titleLabel);

    QLabel *nodeLabel = new QLabel(QStringLiteral("节点ID: %1").arg(nodeId_), this);
    nodeLabel->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 11px;"));
    mainLayout->addWidget(nodeLabel);

    // 状态显示区域
    QGroupBox *statusBox = new QGroupBox(QStringLiteral("设备状态"), this);
    QVBoxLayout *statusLayout = new QVBoxLayout(statusBox);
    statusLayout->setSpacing(6);
    statusLayout->setContentsMargins(8, 12, 8, 8);

    statusLabel_ = new QLabel(QStringLiteral("在线状态: 未知"), this);
    statusLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12px;"));
    statusLayout->addWidget(statusLabel_);

    currentLabel_ = new QLabel(QStringLiteral("总电流: -- mA"), this);
    currentLabel_->setStyleSheet(QStringLiteral(
        "font-size: 13px; color: #3498db; font-weight: bold;"));
    statusLayout->addWidget(currentLabel_);

    // 通道状态
    QGridLayout *chStatusGrid = new QGridLayout();
    chStatusGrid->setSpacing(4);

    ch0StatusLabel_ = new QLabel(QStringLiteral("CH0: --"), this);
    ch1StatusLabel_ = new QLabel(QStringLiteral("CH1: --"), this);
    ch2StatusLabel_ = new QLabel(QStringLiteral("CH2: --"), this);
    ch3StatusLabel_ = new QLabel(QStringLiteral("CH3: --"), this);

    chStatusGrid->addWidget(ch0StatusLabel_, 0, 0);
    chStatusGrid->addWidget(ch1StatusLabel_, 0, 1);
    chStatusGrid->addWidget(ch2StatusLabel_, 1, 0);
    chStatusGrid->addWidget(ch3StatusLabel_, 1, 1);

    statusLayout->addLayout(chStatusGrid);

    QPushButton *refreshBtn = new QPushButton(QStringLiteral("刷新状态"), this);
    refreshBtn->setMinimumHeight(32);
    connect(refreshBtn, &QPushButton::clicked, this, &RelayControlDialog::onQueryStatusClicked);
    statusLayout->addWidget(refreshBtn);

    mainLayout->addWidget(statusBox);

    // 通道控制区域
    QGroupBox *controlBox = new QGroupBox(QStringLiteral("通道控制"), this);
    QGridLayout *controlGrid = new QGridLayout(controlBox);
    controlGrid->setSpacing(4);
    controlGrid->setContentsMargins(8, 12, 8, 8);

    for (int ch = 0; ch < 4; ++ch) {
        QLabel *chLabel = new QLabel(QStringLiteral("CH%1:").arg(ch), this);
        chLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11px;"));
        controlGrid->addWidget(chLabel, ch, 0);

        QPushButton *stopBtn = new QPushButton(QStringLiteral("停"), this);
        stopBtn->setProperty("channel", ch);
        stopBtn->setProperty("action", QStringLiteral("stop"));
        stopBtn->setMinimumSize(50, 32);
        connect(stopBtn, &QPushButton::clicked, this, &RelayControlDialog::onChannelControlClicked);
        controlGrid->addWidget(stopBtn, ch, 1);

        QPushButton *fwdBtn = new QPushButton(QStringLiteral("正"), this);
        fwdBtn->setProperty("channel", ch);
        fwdBtn->setProperty("action", QStringLiteral("fwd"));
        fwdBtn->setProperty("type", QStringLiteral("success"));
        fwdBtn->setMinimumSize(50, 32);
        connect(fwdBtn, &QPushButton::clicked, this, &RelayControlDialog::onChannelControlClicked);
        controlGrid->addWidget(fwdBtn, ch, 2);

        QPushButton *revBtn = new QPushButton(QStringLiteral("反"), this);
        revBtn->setProperty("channel", ch);
        revBtn->setProperty("action", QStringLiteral("rev"));
        revBtn->setProperty("type", QStringLiteral("warning"));
        revBtn->setMinimumSize(50, 32);
        connect(revBtn, &QPushButton::clicked, this, &RelayControlDialog::onChannelControlClicked);
        controlGrid->addWidget(revBtn, ch, 3);
    }

    mainLayout->addWidget(controlBox);

    // 全部停止按钮
    QPushButton *stopAllBtn = new QPushButton(QStringLiteral("全部停止"), this);
    stopAllBtn->setProperty("type", QStringLiteral("danger"));
    stopAllBtn->setMinimumHeight(40);
    connect(stopAllBtn, &QPushButton::clicked, this, &RelayControlDialog::onStopAllClicked);
    mainLayout->addWidget(stopAllBtn);

    // 关闭按钮
    QPushButton *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    closeBtn->setMinimumHeight(36);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    mainLayout->addWidget(closeBtn);
}

void RelayControlDialog::onChannelControlClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    int channel = btn->property("channel").toInt();
    QString action = btn->property("action").toString();

    controlRelay(channel, action);
}

void RelayControlDialog::onStopAllClicked()
{
    for (int ch = 0; ch < 4; ++ch) {
        controlRelay(ch, QStringLiteral("stop"));
    }
    
    // 刷新状态
    onQueryStatusClicked();
}

void RelayControlDialog::onQueryStatusClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        statusLabel_->setText(QStringLiteral("在线状态: [X] 未连接服务器"));
        return;
    }

    QJsonObject params;
    params[QStringLiteral("node")] = nodeId_;

    QJsonValue result = rpcClient_->call(QStringLiteral("relay.statusAll"), params);

    if (result.isObject()) {
        updateStatusDisplay(result.toObject());
    }
}

void RelayControlDialog::updateStatusDisplay(const QJsonObject &status)
{
    bool online = status.value(QStringLiteral("online")).toBool();
    qint64 ageMs = static_cast<qint64>(status.value(QStringLiteral("ageMs")).toDouble(-1));

    if (online) {
        statusLabel_->setText(QStringLiteral("在线状态: [OK] 在线 (%1ms)").arg(ageMs));
        statusLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12px; color: #27ae60;"));
    } else if (ageMs < 0) {
        statusLabel_->setText(QStringLiteral("在线状态: [!] 无响应"));
        statusLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12px; color: #f39c12;"));
    } else {
        statusLabel_->setText(QStringLiteral("在线状态: [X] 离线 (%1s)").arg(ageMs / 1000));
        statusLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12px; color: #e74c3c;"));
    }

    // 更新电流
    double totalCurrent = status.value(QStringLiteral("totalCurrent")).toDouble(0);
    currentLabel_->setText(QStringLiteral("总电流: %1 mA").arg(totalCurrent, 0, 'f', 1));

    // 更新通道状态
    QJsonObject channels = status.value(QStringLiteral("channels")).toObject();
    QLabel *chLabels[] = {ch0StatusLabel_, ch1StatusLabel_, ch2StatusLabel_, ch3StatusLabel_};

    for (int ch = 0; ch < 4; ++ch) {
        QString chKey = QString::number(ch);
        if (channels.contains(chKey)) {
            QJsonObject chStatus = channels.value(chKey).toObject();
            int mode = chStatus.value(QStringLiteral("mode")).toInt(0);
            double current = chStatus.value(QStringLiteral("current")).toDouble(0);

            QString modeText;
            QString color;
            switch (mode) {
                case 0: modeText = QStringLiteral("停止"); color = QStringLiteral("#7f8c8d"); break;
                case 1: modeText = QStringLiteral("正转"); color = QStringLiteral("#27ae60"); break;
                case 2: modeText = QStringLiteral("反转"); color = QStringLiteral("#f39c12"); break;
                default: modeText = QStringLiteral("未知"); color = QStringLiteral("#95a5a6"); break;
            }

            chLabels[ch]->setText(QStringLiteral("CH%1: %2 (%3mA)")
                .arg(ch).arg(modeText).arg(current, 0, 'f', 1));
            chLabels[ch]->setStyleSheet(QStringLiteral("color: %1; font-weight: bold; font-size: 11px;").arg(color));
        } else {
            chLabels[ch]->setText(QStringLiteral("CH%1: --").arg(ch));
            chLabels[ch]->setStyleSheet(QStringLiteral("color: #95a5a6; font-size: 11px;"));
        }
    }
}

void RelayControlDialog::controlRelay(int channel, const QString &action)
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QJsonObject params;
    params[QStringLiteral("node")] = nodeId_;
    params[QStringLiteral("ch")] = channel;
    params[QStringLiteral("action")] = action;

    QJsonValue result = rpcClient_->call(QStringLiteral("relay.control"), params);

    QJsonObject obj = result.toObject();
    if (obj.value(QStringLiteral("ok")).toBool()) {
        QString actionText;
        if (action == QStringLiteral("stop")) actionText = QStringLiteral("停止");
        else if (action == QStringLiteral("fwd")) actionText = QStringLiteral("正转");
        else if (action == QStringLiteral("rev")) actionText = QStringLiteral("反转");

        emit controlExecuted(QStringLiteral("节点 %1 通道 %2 -> %3")
            .arg(nodeId_).arg(channel).arg(actionText));

        // 刷新状态
        onQueryStatusClicked();
    } else {
        QString error = obj.value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"),
            QStringLiteral("控制失败: %1").arg(error));
    }
}
