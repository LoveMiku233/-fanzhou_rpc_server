/**
 * @file relay_control_dialog.cpp
 * @brief 继电器控制对话框实现（1024x600低分辨率优化版）
 */

#include "relay_control_dialog.h"
#include "rpc_client.h"
#include "style_constants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QFrame>
#include <QTimer>

using namespace UIConstants;

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
    , stopChannelIndex_(0)
{
    setWindowTitle(QStringLiteral("控制: %1 (#%2)").arg(deviceName).arg(nodeId));
    setMinimumSize(DIALOG_WIDTH, DIALOG_HEIGHT);
    setModal(true);
    setupUi();
    
    // 初始查询状态
    onQueryStatusClicked();
}

void RelayControlDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(DIALOG_MARGIN, DIALOG_MARGIN, DIALOG_MARGIN, DIALOG_MARGIN);
    mainLayout->setSpacing(DIALOG_SPACING);

    // 设备信息
    QLabel *titleLabel = new QLabel(QStringLiteral("设备: %1 (#%2)").arg(deviceName_).arg(nodeId_), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: %1px; font-weight: bold; color: #2c3e50;").arg(FONT_SIZE_CARD_TITLE));
    mainLayout->addWidget(titleLabel);

    // 左右布局：状态在左，控制在右
    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(12);

    // 左侧：设备状态区域
    QGroupBox *statusBox = new QGroupBox(QStringLiteral("设备状态"), this);
    QVBoxLayout *statusLayout = new QVBoxLayout(statusBox);
    statusLayout->setSpacing(8);
    statusLayout->setContentsMargins(10, 12, 10, 10);

    statusLabel_ = new QLabel(QStringLiteral("在线状态: 未知"), this);
    statusLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 13px;"));
    statusLayout->addWidget(statusLabel_);

    currentLabel_ = new QLabel(QStringLiteral("总电流: -- mA"), this);
    currentLabel_->setStyleSheet(QStringLiteral(
        "font-size: 13px; color: #3498db; font-weight: bold;"));
    statusLayout->addWidget(currentLabel_);

    // 通道状态 - 垂直排列
    QGridLayout *chStatusGrid = new QGridLayout();
    chStatusGrid->setSpacing(6);

    QLabel *ch0Title = new QLabel(QStringLiteral("通道0:"), this);
    ch0Title->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: bold;"));
    ch0StatusLabel_ = new QLabel(QStringLiteral("--"), this);
    ch0StatusLabel_->setStyleSheet(QStringLiteral("font-size: 12px;"));
    chStatusGrid->addWidget(ch0Title, 0, 0);
    chStatusGrid->addWidget(ch0StatusLabel_, 0, 1);

    QLabel *ch1Title = new QLabel(QStringLiteral("通道1:"), this);
    ch1Title->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: bold;"));
    ch1StatusLabel_ = new QLabel(QStringLiteral("--"), this);
    ch1StatusLabel_->setStyleSheet(QStringLiteral("font-size: 12px;"));
    chStatusGrid->addWidget(ch1Title, 1, 0);
    chStatusGrid->addWidget(ch1StatusLabel_, 1, 1);

    QLabel *ch2Title = new QLabel(QStringLiteral("通道2:"), this);
    ch2Title->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: bold;"));
    ch2StatusLabel_ = new QLabel(QStringLiteral("--"), this);
    ch2StatusLabel_->setStyleSheet(QStringLiteral("font-size: 12px;"));
    chStatusGrid->addWidget(ch2Title, 2, 0);
    chStatusGrid->addWidget(ch2StatusLabel_, 2, 1);

    QLabel *ch3Title = new QLabel(QStringLiteral("通道3:"), this);
    ch3Title->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: bold;"));
    ch3StatusLabel_ = new QLabel(QStringLiteral("--"), this);
    ch3StatusLabel_->setStyleSheet(QStringLiteral("font-size: 12px;"));
    chStatusGrid->addWidget(ch3Title, 3, 0);
    chStatusGrid->addWidget(ch3StatusLabel_, 3, 1);

    statusLayout->addLayout(chStatusGrid);
    statusLayout->addStretch();

    QPushButton *refreshBtn = new QPushButton(QStringLiteral("刷新状态"), this);
    refreshBtn->setMinimumHeight(36);
    connect(refreshBtn, &QPushButton::clicked, this, &RelayControlDialog::onQueryStatusClicked);
    statusLayout->addWidget(refreshBtn);

    contentLayout->addWidget(statusBox);

    // 右侧：通道控制区域
    QGroupBox *controlBox = new QGroupBox(QStringLiteral("通道控制"), this);
    QVBoxLayout *controlBoxLayout = new QVBoxLayout(controlBox);
    controlBoxLayout->setSpacing(6);
    controlBoxLayout->setContentsMargins(10, 12, 10, 10);

    // 控制说明
    QLabel *helpLabel = new QLabel(QStringLiteral("停=停止  正=正转  反=反转"), this);
    helpLabel->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 11px;"));
    helpLabel->setAlignment(Qt::AlignCenter);
    controlBoxLayout->addWidget(helpLabel);

    QGridLayout *controlGrid = new QGridLayout();
    controlGrid->setSpacing(6);

    for (int ch = 0; ch < 4; ++ch) {
        QLabel *chLabel = new QLabel(QStringLiteral("通道%1:").arg(ch), this);
        chLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12px;"));
        controlGrid->addWidget(chLabel, ch, 0);

        QPushButton *stopBtn = new QPushButton(QStringLiteral("停"), this);
        stopBtn->setProperty("channel", ch);
        stopBtn->setProperty("action", QStringLiteral("stop"));
        stopBtn->setMinimumSize(50, 36);
        stopBtn->setMaximumWidth(60);
        connect(stopBtn, &QPushButton::clicked, this, &RelayControlDialog::onChannelControlClicked);
        controlGrid->addWidget(stopBtn, ch, 1);

        QPushButton *fwdBtn = new QPushButton(QStringLiteral("正"), this);
        fwdBtn->setProperty("channel", ch);
        fwdBtn->setProperty("action", QStringLiteral("fwd"));
        fwdBtn->setProperty("type", QStringLiteral("success"));
        fwdBtn->setMinimumSize(50, 36);
        fwdBtn->setMaximumWidth(60);
        connect(fwdBtn, &QPushButton::clicked, this, &RelayControlDialog::onChannelControlClicked);
        controlGrid->addWidget(fwdBtn, ch, 2);

        QPushButton *revBtn = new QPushButton(QStringLiteral("反"), this);
        revBtn->setProperty("channel", ch);
        revBtn->setProperty("action", QStringLiteral("rev"));
        revBtn->setProperty("type", QStringLiteral("warning"));
        revBtn->setMinimumSize(50, 36);
        revBtn->setMaximumWidth(60);
        connect(revBtn, &QPushButton::clicked, this, &RelayControlDialog::onChannelControlClicked);
        controlGrid->addWidget(revBtn, ch, 3);
    }

    controlBoxLayout->addLayout(controlGrid);
    controlBoxLayout->addStretch();

    // 全部停止按钮
    QPushButton *stopAllBtn = new QPushButton(QStringLiteral("全部停止"), this);
    stopAllBtn->setProperty("type", QStringLiteral("danger"));
    stopAllBtn->setMinimumHeight(40);
    connect(stopAllBtn, &QPushButton::clicked, this, &RelayControlDialog::onStopAllClicked);
    controlBoxLayout->addWidget(stopAllBtn);

    contentLayout->addWidget(controlBox);
    
    mainLayout->addLayout(contentLayout, 1);

    // 关闭按钮
    QPushButton *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    closeBtn->setMinimumHeight(40);
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
    // 开始顺序停止所有通道，每个通道间隔300ms
    stopChannelIndex_ = 0;
    stopNextChannel();
}

void RelayControlDialog::stopNextChannel()
{
    // 安全检查：确保RPC客户端可用
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        return;
    }
    
    if (stopChannelIndex_ >= 4) {
        // 所有通道已停止，刷新状态
        QTimer::singleShot(200, this, &RelayControlDialog::onQueryStatusClicked);
        return;
    }
    
    controlRelay(stopChannelIndex_, QStringLiteral("stop"));
    stopChannelIndex_++;
    
    // 延迟300ms后停止下一个通道
    QTimer::singleShot(300, this, &RelayControlDialog::stopNextChannel);
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
        statusLabel_->setText(QStringLiteral("在线状态: 在线 (%1ms)").arg(ageMs));
        statusLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; color: #27ae60;"));
    } else if (ageMs < 0) {
        statusLabel_->setText(QStringLiteral("在线状态: 无响应"));
        statusLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; color: #f39c12;"));
    } else {
        statusLabel_->setText(QStringLiteral("在线状态: 离线 (%1s)").arg(ageMs / 1000));
        statusLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; color: #e74c3c;"));
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
            bool phaseLost = chStatus.value(QStringLiteral("phaseLost")).toBool(false);

            QString modeText;
            QString color;
            
            // 如果缺相，优先显示缺相状态
            if (phaseLost) {
                modeText = QStringLiteral("缺相");
                color = QStringLiteral("#dc3545");
            } else {
                switch (mode) {
                    case 0: modeText = QStringLiteral("停止"); color = QStringLiteral("#7f8c8d"); break;
                    case 1: modeText = QStringLiteral("正转"); color = QStringLiteral("#27ae60"); break;
                    case 2: modeText = QStringLiteral("反转"); color = QStringLiteral("#f39c12"); break;
                    default: modeText = QStringLiteral("未知"); color = QStringLiteral("#95a5a6"); break;
                }
            }

            chLabels[ch]->setText(QStringLiteral("%1 (%2mA)")
                .arg(modeText).arg(current, 0, 'f', 1));
            chLabels[ch]->setStyleSheet(QStringLiteral("color: %1; font-weight: bold; font-size: 13px;").arg(color));
        } else {
            chLabels[ch]->setText(QStringLiteral("--"));
            chLabels[ch]->setStyleSheet(QStringLiteral("color: #95a5a6; font-size: 13px;"));
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
