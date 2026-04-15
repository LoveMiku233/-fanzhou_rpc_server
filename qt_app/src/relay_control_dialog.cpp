/**
 * @file relay_control_dialog.cpp
 * @brief 继电器控制对话框实现（使用按钮控制，带状态同步）
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

namespace {
// Delay between stopping each channel during sequential stop (ms)
// This prevents hardware overload and ensures each relay has time to respond
constexpr int kChannelStopIntervalMs = 300;

// Delay before refreshing status after all channels are stopped (ms)
// This allows time for the hardware to report updated status
constexpr int kStatusRefreshDelayMs = 500;

// Delay before refreshing status after a control operation (ms)
// Prevents rapid consecutive RPC calls
constexpr int kControlRefreshDelayMs = 1200;
}

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
    , ch0CurrentLabel_(nullptr)
    , ch1CurrentLabel_(nullptr)
    , ch2CurrentLabel_(nullptr)
    , ch3CurrentLabel_(nullptr)
    , ch0StopBtn_(nullptr)
    , ch0FwdBtn_(nullptr)
    , ch0RevBtn_(nullptr)
    , ch1StopBtn_(nullptr)
    , ch1FwdBtn_(nullptr)
    , ch1RevBtn_(nullptr)
    , ch2StopBtn_(nullptr)
    , ch2FwdBtn_(nullptr)
    , ch2RevBtn_(nullptr)
    , ch3StopBtn_(nullptr)
    , ch3FwdBtn_(nullptr)
    , ch3RevBtn_(nullptr)
    , stopChannelIndex_(0)
    , isControlling_(false)
    , isQuerying_(false)
    , syncTimer_(nullptr)
    , refreshStatusButton_(nullptr)
    , stopAllButton_(nullptr)
{
    setWindowTitle(QStringLiteral("控制: %1 (#%2)").arg(deviceName).arg(nodeId));
    // 使用更大的对话框尺寸以确保按钮完整显示
    setMinimumSize(DIALOG_WIDTH_LARGE, DIALOG_HEIGHT_LARGE);
    setModal(true);
    setupUi();
    
    // 初始查询状态
    onQueryStatusClicked();
    
    // 启动状态同步定时器
    startSyncTimer();
}

RelayControlDialog::~RelayControlDialog()
{
    stopSyncTimer();
}

void RelayControlDialog::setControlBusy(bool busy)
{
    for (QPushButton *btn : controlButtons_) {
        if (btn) {
            btn->setEnabled(!busy);
        }
    }
    if (stopAllButton_) {
        stopAllButton_->setEnabled(!busy);
    }
    if (refreshStatusButton_) {
        refreshStatusButton_->setEnabled(!busy);
    }
}

void RelayControlDialog::startSyncTimer()
{
    if (!syncTimer_) {
        syncTimer_ = new QTimer(this);
        connect(syncTimer_, &QTimer::timeout, this, &RelayControlDialog::onSyncTimerTimeout);
    }
    syncTimer_->start(kSyncIntervalMs);
}

void RelayControlDialog::stopSyncTimer()
{
    if (syncTimer_) {
        syncTimer_->stop();
    }
}

void RelayControlDialog::onSyncTimerTimeout()
{
    // 定时同步设备状态
    // 注意：定时器回调在主线程执行，与用户操作不会有线程冲突
    // 优化：只在非控制、非查询状态时才同步，避免RPC调用堆积
    if (rpcClient_ && rpcClient_->isConnected() && !isControlling_ && !isQuerying_) {
        onQueryStatusClicked();
    }
}

void RelayControlDialog::updateButtonStyles(int channel, int mode)
{
    // 获取对应通道的按钮
    QPushButton *stopBtn = nullptr;
    QPushButton *fwdBtn = nullptr;
    QPushButton *revBtn = nullptr;
    
    switch (channel) {
        case 0: stopBtn = ch0StopBtn_; fwdBtn = ch0FwdBtn_; revBtn = ch0RevBtn_; break;
        case 1: stopBtn = ch1StopBtn_; fwdBtn = ch1FwdBtn_; revBtn = ch1RevBtn_; break;
        case 2: stopBtn = ch2StopBtn_; fwdBtn = ch2FwdBtn_; revBtn = ch2RevBtn_; break;
        case 3: stopBtn = ch3StopBtn_; fwdBtn = ch3FwdBtn_; revBtn = ch3RevBtn_; break;
        default: return;
    }
    
    if (!stopBtn || !fwdBtn || !revBtn) return;
    
    // 普通状态样式 - 淡灰色背景
    QString normalStyle = QStringLiteral(
        "QPushButton { "
        "  background-color: #f0f0f0; "
        "  color: #666666; "
        "  border: 1px solid #d0d0d0; "
        "  border-radius: 6px; "
        "  padding: 8px 12px; "
        "  font-size: 13px; "
        "}"
        "QPushButton:hover { background-color: #e0e0e0; border-color: #c0c0c0; }");
    
    // 停止状态高亮样式 - 深灰色，带阴影效果
    QString activeStopStyle = QStringLiteral(
        "QPushButton { "
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #8e9eab, stop:1 #6c7a89); "
        "  color: white; "
        "  border: 3px solid #5d6d7e; "
        "  border-radius: 6px; "
        "  padding: 8px 12px; "
        "  font-weight: bold; "
        "  font-size: 13px; "
        "}"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #7e8e9b, stop:1 #5c6a79); }");
    
    // 正转状态高亮样式 - 绿色，带渐变和阴影
    QString activeFwdStyle = QStringLiteral(
        "QPushButton { "
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #58d68d, stop:1 #27ae60); "
        "  color: white; "
        "  border: 3px solid #1e8449; "
        "  border-radius: 6px; "
        "  padding: 8px 12px; "
        "  font-weight: bold; "
        "  font-size: 13px; "
        "}"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #48c67d, stop:1 #1f9e50); }");
    
    // 反转状态高亮样式 - 橙色，带渐变和阴影
    QString activeRevStyle = QStringLiteral(
        "QPushButton { "
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f5b041, stop:1 #f39c12); "
        "  color: white; "
        "  border: 3px solid #d68910; "
        "  border-radius: 6px; "
        "  padding: 8px 12px; "
        "  font-weight: bold; "
        "  font-size: 13px; "
        "}"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e5a031, stop:1 #e38c02); }");
    
    // 根据模式设置活动状态样式
    switch (mode) {
        case 0: // 停止
            stopBtn->setStyleSheet(activeStopStyle);
            fwdBtn->setStyleSheet(normalStyle);
            revBtn->setStyleSheet(normalStyle);
            break;
        case 1: // 正转
            stopBtn->setStyleSheet(normalStyle);
            fwdBtn->setStyleSheet(activeFwdStyle);
            revBtn->setStyleSheet(normalStyle);
            break;
        case 2: // 反转
            stopBtn->setStyleSheet(normalStyle);
            fwdBtn->setStyleSheet(normalStyle);
            revBtn->setStyleSheet(activeRevStyle);
            break;
        default:
            stopBtn->setStyleSheet(normalStyle);
            fwdBtn->setStyleSheet(normalStyle);
            revBtn->setStyleSheet(normalStyle);
            break;
    }
}

void RelayControlDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(DIALOG_MARGIN + 4, DIALOG_MARGIN + 4, DIALOG_MARGIN + 4, DIALOG_MARGIN + 4);
    mainLayout->setSpacing(DIALOG_SPACING + 4);

    // 设备信息标题 - 更醒目的样式
    QLabel *titleLabel = new QLabel(QStringLiteral("设备控制: %1 (#%2)").arg(deviceName_).arg(nodeId_), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: %1px; font-weight: bold; color: #2c3e50; "
        "padding: 8px; background-color: #ecf0f1; border-radius: 8px;").arg(FONT_SIZE_CARD_TITLE + 2));
    mainLayout->addWidget(titleLabel);

    // 左右布局：状态在左，控制在右
    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(16);

    // 左侧：设备状态区域
    QGroupBox *statusBox = new QGroupBox(QStringLiteral("设备状态"), this);
    statusBox->setStyleSheet(QStringLiteral(
        "QGroupBox { font-weight: bold; font-size: 14px; border: 2px solid #3498db; border-radius: 10px; margin-top: 14px; padding-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 8px; color: #3498db; }"));
    QVBoxLayout *statusLayout = new QVBoxLayout(statusBox);
    statusLayout->setSpacing(10);
    statusLayout->setContentsMargins(12, 16, 12, 12);

    statusLabel_ = new QLabel(QStringLiteral("在线状态: 查询中..."), this);
    statusLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; color: #7f8c8d;"));
    statusLayout->addWidget(statusLabel_);

    currentLabel_ = new QLabel(QStringLiteral("总电流: -- A"), this);
    currentLabel_->setStyleSheet(QStringLiteral(
        "font-size: 15px; color: #3498db; font-weight: bold; "
        "background-color: #ebf5fb; padding: 4px 8px; border-radius: 6px;"));
    statusLayout->addWidget(currentLabel_);

    // 通道状态 - 垂直排列，更清晰的显示
    QGridLayout *chStatusGrid = new QGridLayout();
    chStatusGrid->setSpacing(8);
    chStatusGrid->setVerticalSpacing(10);

    QLabel *ch0Title = new QLabel(QStringLiteral("通道0:"), this);
    ch0Title->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: bold; color: #34495e;"));
    ch0StatusLabel_ = new QLabel(QStringLiteral("--"), this);
    ch0StatusLabel_->setStyleSheet(QStringLiteral("font-size: 13px; background-color: #f5f5f5; padding: 4px 8px; border-radius: 6px;"));
    chStatusGrid->addWidget(ch0Title, 0, 0);
    chStatusGrid->addWidget(ch0StatusLabel_, 0, 1);

    QLabel *ch1Title = new QLabel(QStringLiteral("通道1:"), this);
    ch1Title->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: bold; color: #34495e;"));
    ch1StatusLabel_ = new QLabel(QStringLiteral("--"), this);
    ch1StatusLabel_->setStyleSheet(QStringLiteral("font-size: 13px; background-color: #f5f5f5; padding: 4px 8px; border-radius: 6px;"));
    chStatusGrid->addWidget(ch1Title, 1, 0);
    chStatusGrid->addWidget(ch1StatusLabel_, 1, 1);

    QLabel *ch2Title = new QLabel(QStringLiteral("通道2:"), this);
    ch2Title->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: bold; color: #34495e;"));
    ch2StatusLabel_ = new QLabel(QStringLiteral("--"), this);
    ch2StatusLabel_->setStyleSheet(QStringLiteral("font-size: 13px; background-color: #f5f5f5; padding: 4px 8px; border-radius: 6px;"));
    chStatusGrid->addWidget(ch2Title, 2, 0);
    chStatusGrid->addWidget(ch2StatusLabel_, 2, 1);

    QLabel *ch3Title = new QLabel(QStringLiteral("通道3:"), this);
    ch3Title->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: bold; color: #34495e;"));
    ch3StatusLabel_ = new QLabel(QStringLiteral("--"), this);
    ch3StatusLabel_->setStyleSheet(QStringLiteral("font-size: 13px; background-color: #f5f5f5; padding: 4px 8px; border-radius: 6px;"));
    chStatusGrid->addWidget(ch3Title, 3, 0);
    chStatusGrid->addWidget(ch3StatusLabel_, 3, 1);

    statusLayout->addLayout(chStatusGrid);
    statusLayout->addStretch();

    refreshStatusButton_ = new QPushButton(QStringLiteral("刷新状态"), this);
    refreshStatusButton_->setMinimumHeight(40);
    refreshStatusButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #5dade2, stop:1 #3498db); "
        "color: white; border-radius: 8px; font-weight: bold; font-size: 13px; }"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #4d9dd2, stop:1 #2488cb); }"));
    connect(refreshStatusButton_, &QPushButton::clicked, this, &RelayControlDialog::onQueryStatusClicked);
    statusLayout->addWidget(refreshStatusButton_);

    contentLayout->addWidget(statusBox);

    // 右侧：通道控制区域（使用按钮）
    QGroupBox *controlBox = new QGroupBox(QStringLiteral("通道控制"), this);
    controlBox->setStyleSheet(QStringLiteral(
        "QGroupBox { font-weight: bold; font-size: 14px; border: 2px solid #27ae60; border-radius: 10px; margin-top: 14px; padding-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 8px; color: #27ae60; }"));
    QVBoxLayout *controlBoxLayout = new QVBoxLayout(controlBox);
    controlBoxLayout->setSpacing(10);
    controlBoxLayout->setContentsMargins(12, 16, 12, 12);

    // 控制说明 - 更醒目
    QLabel *helpLabel = new QLabel(QStringLiteral("🎛️ 点击按钮控制继电器 (高亮 = 当前状态)"), this);
    helpLabel->setStyleSheet(QStringLiteral("color: #34495e; font-size: 12px; font-weight: bold;"));
    helpLabel->setAlignment(Qt::AlignCenter);
    controlBoxLayout->addWidget(helpLabel);

    QGridLayout *controlGrid = new QGridLayout();
    controlGrid->setSpacing(10);
    controlGrid->setVerticalSpacing(12);
    controlGrid->setColumnStretch(1, 1);
    controlGrid->setColumnStretch(2, 1);
    controlGrid->setColumnStretch(3, 1);
    controlGrid->setColumnStretch(4, 0);

    // 存储按钮的数组以便初始化
    QPushButton *stopBtns[4] = {nullptr, nullptr, nullptr, nullptr};
    QPushButton *fwdBtns[4] = {nullptr, nullptr, nullptr, nullptr};
    QPushButton *revBtns[4] = {nullptr, nullptr, nullptr, nullptr};
    QLabel *currentLabels[4] = {nullptr, nullptr, nullptr, nullptr};

    // 创建按钮控制
    for (int ch = 0; ch < 4; ++ch) {
        // 通道标签
        QLabel *chLabel = new QLabel(QStringLiteral("通道%1:").arg(ch), this);
        chLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 13px; color: #2c3e50;"));
        controlGrid->addWidget(chLabel, ch, 0);

        // 初始样式 - 停止状态高亮（默认状态）
        QString initialStopStyle = QStringLiteral(
            "QPushButton { "
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #8e9eab, stop:1 #6c7a89); "
            "  color: white; "
            "  border: 3px solid #5d6d7e; "
            "  border-radius: 6px; "
            "  padding: 8px 12px; "
            "  font-weight: bold; "
            "  font-size: 13px; "
            "}"
            "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #7e8e9b, stop:1 #5c6a79); }");
        
        QString normalStyle = QStringLiteral(
            "QPushButton { "
            "  background-color: #f0f0f0; "
            "  color: #666666; "
            "  border: 1px solid #d0d0d0; "
            "  border-radius: 6px; "
            "  padding: 8px 12px; "
            "  font-size: 13px; "
            "}"
            "QPushButton:hover { background-color: #e0e0e0; border-color: #c0c0c0; }");

        // 停止按钮 - 增大尺寸以确保完整显示，初始为活动状态（默认停止）
        QPushButton *stopBtn = new QPushButton(QStringLiteral("■ 停"), this);
        stopBtn->setProperty("channel", ch);
        stopBtn->setProperty("action", QStringLiteral("stop"));
        stopBtn->setMinimumSize(60, 42);
        stopBtn->setStyleSheet(initialStopStyle);
        connect(stopBtn, &QPushButton::clicked, this, &RelayControlDialog::onChannelControlClicked);
        controlGrid->addWidget(stopBtn, ch, 1);
        stopBtns[ch] = stopBtn;

        // 正转按钮 - 增大尺寸以确保完整显示
        QPushButton *fwdBtn = new QPushButton(QStringLiteral("▶ 正"), this);
        fwdBtn->setProperty("channel", ch);
        fwdBtn->setProperty("action", QStringLiteral("fwd"));
        fwdBtn->setMinimumSize(60, 42);
        fwdBtn->setStyleSheet(normalStyle);
        connect(fwdBtn, &QPushButton::clicked, this, &RelayControlDialog::onChannelControlClicked);
        controlGrid->addWidget(fwdBtn, ch, 2);
        fwdBtns[ch] = fwdBtn;

        // 反转按钮 - 增大尺寸以确保完整显示
        QPushButton *revBtn = new QPushButton(QStringLiteral("◀ 反"), this);
        revBtn->setProperty("channel", ch);
        revBtn->setProperty("action", QStringLiteral("rev"));
        revBtn->setMinimumSize(60, 42);
        revBtn->setStyleSheet(normalStyle);
        connect(revBtn, &QPushButton::clicked, this, &RelayControlDialog::onChannelControlClicked);
        controlGrid->addWidget(revBtn, ch, 3);
        revBtns[ch] = revBtn;

        // 电流显示标签（在按钮旁边）- 更醒目
        QLabel *currentLbl = new QLabel(QStringLiteral("-- A"), this);
        currentLbl->setStyleSheet(QStringLiteral(
            "font-size: 13px; font-weight: bold; color: #95a5a6; "
            "background-color: #f5f5f5; padding: 4px 8px; border-radius: 6px; min-width: 60px;"));
        currentLbl->setAlignment(Qt::AlignCenter);
        controlGrid->addWidget(currentLbl, ch, 4);
        currentLabels[ch] = currentLbl;
    }

    // 保存按钮和电流标签引用
    ch0StopBtn_ = stopBtns[0]; ch0FwdBtn_ = fwdBtns[0]; ch0RevBtn_ = revBtns[0];
    ch1StopBtn_ = stopBtns[1]; ch1FwdBtn_ = fwdBtns[1]; ch1RevBtn_ = revBtns[1];
    ch2StopBtn_ = stopBtns[2]; ch2FwdBtn_ = fwdBtns[2]; ch2RevBtn_ = revBtns[2];
    ch3StopBtn_ = stopBtns[3]; ch3FwdBtn_ = fwdBtns[3]; ch3RevBtn_ = revBtns[3];

    // 收集所有控制按钮，便于批量启用/禁用
    for (int i = 0; i < 4; ++i) {
        controlButtons_ << stopBtns[i] << fwdBtns[i] << revBtns[i];
    }
    
    ch0CurrentLabel_ = currentLabels[0];
    ch1CurrentLabel_ = currentLabels[1];
    ch2CurrentLabel_ = currentLabels[2];
    ch3CurrentLabel_ = currentLabels[3];

    controlBoxLayout->addLayout(controlGrid);
    controlBoxLayout->addStretch();

    // 全部停止按钮 - 更醒目的危险样式
    stopAllButton_ = new QPushButton(QStringLiteral("全部停止"), this);
    stopAllButton_->setProperty("type", QStringLiteral("danger"));
    stopAllButton_->setMinimumHeight(44);
    stopAllButton_->setStyleSheet(QStringLiteral(
        "QPushButton { "
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ec7063, stop:1 #e74c3c); "
        "  color: white; border-radius: 8px; font-weight: bold; font-size: 14px; border: 2px solid #c0392b; "
        "}"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #dc6053, stop:1 #d73c2c); }"
        "QPushButton:pressed { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #cc5043, stop:1 #c72c1c); }"));
    connect(stopAllButton_, &QPushButton::clicked, this, &RelayControlDialog::onStopAllClicked);
    controlBoxLayout->addWidget(stopAllButton_);

    contentLayout->addWidget(controlBox);
    
    mainLayout->addLayout(contentLayout, 1);

    // 关闭按钮
    QPushButton *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    closeBtn->setMinimumHeight(44);
    closeBtn->setStyleSheet(QStringLiteral(
        "QPushButton { "
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #bdc3c7, stop:1 #95a5a6); "
        "  color: white; border-radius: 8px; font-weight: bold; font-size: 14px; "
        "}"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #adb3b7, stop:1 #859596); }"));
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
    if (isControlling_ || isQuerying_) {
        return;
    }

    isControlling_ = true;
    setControlBusy(true);
    statusLabel_->setText(QStringLiteral("在线状态: [执] 正在顺序停止全部通道..."));
    statusLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; color: #f39c12;"));

    // 开始顺序停止所有通道
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
        isControlling_ = false;
        setControlBusy(false);
        QTimer::singleShot(kStatusRefreshDelayMs, this, &RelayControlDialog::onQueryStatusClicked);
        return;
    }

    // 直接异步调用，不使用controlRelay()以避免isControlling_冲突
    int channel = stopChannelIndex_;
    QJsonObject params;
    params[QStringLiteral("node")] = nodeId_;
    params[QStringLiteral("ch")] = channel;
    params[QStringLiteral("action")] = QStringLiteral("stop");

    const int reqId = rpcClient_->callAsync(QStringLiteral("relay.control"), params, this,
        [this, channel](const QJsonValue &result, const QJsonObject &error) {
            if (error.isEmpty() && result.isObject()) {
                QJsonObject obj = result.toObject();
                if (obj.value(QStringLiteral("ok")).toBool()) {
                    emit controlExecuted(QStringLiteral("节点 %1 通道 %2 -> 停止")
                        .arg(nodeId_).arg(channel));
                }
            }
        }, 3000);  // 3秒超时

    if (reqId < 0) {
        isControlling_ = false;
        setControlBusy(false);
        statusLabel_->setText(QStringLiteral("在线状态: [X] 控制请求发送失败"));
        statusLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; color: #e74c3c;"));
        return;
    }

    stopChannelIndex_++;

    // 延迟后停止下一个通道
    QTimer::singleShot(kChannelStopIntervalMs, this, &RelayControlDialog::stopNextChannel);
}

void RelayControlDialog::onQueryStatusClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        statusLabel_->setText(QStringLiteral("在线状态: [X] 未连接服务器"));
        return;
    }

    // 防止重复查询，且控制期间不打断
    if (isQuerying_ || isControlling_) {
        qDebug() << "[RELAY_CONTROL_DIALOG] 查询操作进行中，跳过";
        return;
    }
    isQuerying_ = true;
    if (refreshStatusButton_) {
        refreshStatusButton_->setEnabled(false);
    }

    QJsonObject params;
    params[QStringLiteral("node")] = nodeId_;

    // 使用异步调用避免阻塞UI线程
    int reqId = rpcClient_->callAsync(QStringLiteral("relay.statusAll"), params, this,
        [this](const QJsonValue &result, const QJsonObject &error) {
            isQuerying_ = false;
            if (refreshStatusButton_) {
                refreshStatusButton_->setEnabled(true);
            }

            if (!error.isEmpty()) {
                statusLabel_->setText(QStringLiteral("在线状态: [X] 查询失败"));
                return;
            }

            if (result.isObject()) {
                updateStatusDisplay(result.toObject());
            }
        }, 3000);  // 3秒超时

    // 如果异步调用失败，重置标志
    if (reqId < 0) {
        isQuerying_ = false;
        if (refreshStatusButton_) {
            refreshStatusButton_->setEnabled(true);
        }
    }
}

void RelayControlDialog::updateStatusDisplay(const QJsonObject &status)
{
    bool online = status.value(QStringLiteral("online")).toBool();
    qint64 ageMs = static_cast<qint64>(status.value(QStringLiteral("ageMs")).toDouble(-1));

    if (online) {
        statusLabel_->setText(QStringLiteral("在线状态: ● 在线 (%1ms)").arg(ageMs));
        statusLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; color: #27ae60;"));
    } else if (ageMs < 0) {
        statusLabel_->setText(QStringLiteral("在线状态: ○ 无响应"));
        statusLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; color: #f39c12;"));
    } else {
        statusLabel_->setText(QStringLiteral("在线状态: ✕ 离线 (%1s)").arg(ageMs / 1000));
        statusLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; color: #e74c3c;"));
    }

    // 更新电流（mA转换为A）
    double totalCurrent = status.value(QStringLiteral("totalCurrent")).toDouble(0);
    double totalCurrentInA = totalCurrent / 1000.0;
    currentLabel_->setText(QStringLiteral("总电流: %1 A").arg(totalCurrentInA, 0, 'f', 2));
    currentLabel_->setStyleSheet(QStringLiteral(
        "font-size: 15px; color: #3498db; font-weight: bold; "
        "background-color: #ebf5fb; padding: 4px 8px; border-radius: 6px;"));

    // 更新通道状态
    QJsonObject channels = status.value(QStringLiteral("channels")).toObject();
    QLabel *chLabels[] = {ch0StatusLabel_, ch1StatusLabel_, ch2StatusLabel_, ch3StatusLabel_};
    QLabel *chCurrentLabels[] = {ch0CurrentLabel_, ch1CurrentLabel_, ch2CurrentLabel_, ch3CurrentLabel_};

    for (int ch = 0; ch < 4; ++ch) {
        QString chKey = QString::number(ch);
        if (channels.contains(chKey)) {
            QJsonObject chStatus = channels.value(chKey).toObject();
            int mode = chStatus.value(QStringLiteral("mode")).toInt(0);
            double current = chStatus.value(QStringLiteral("current")).toDouble(0);
            bool phaseLost = chStatus.value(QStringLiteral("phaseLost")).toBool(false);

            QString modeText;
            QString modeIcon;
            QString color;
            QString bgColor;
            
            // 如果缺相，优先显示缺相状态
            if (phaseLost) {
                modeText = QStringLiteral("缺相");
                modeIcon = QStringLiteral("⚠");
                color = QStringLiteral("#dc3545");
                bgColor = QStringLiteral("#f8d7da");
            } else {
                switch (mode) {
                    case 0: 
                        modeText = QStringLiteral("停止"); 
                        modeIcon = QStringLiteral("■");
                        color = QStringLiteral("#6c757d"); 
                        bgColor = QStringLiteral("#e9ecef"); 
                        break;
                    case 1: 
                        modeText = QStringLiteral("正转"); 
                        modeIcon = QStringLiteral("▶");
                        color = QStringLiteral("#28a745"); 
                        bgColor = QStringLiteral("#d4edda"); 
                        break;
                    case 2: 
                        modeText = QStringLiteral("反转"); 
                        modeIcon = QStringLiteral("◀");
                        color = QStringLiteral("#fd7e14"); 
                        bgColor = QStringLiteral("#fff3cd"); 
                        break;
                    default: 
                        modeText = QStringLiteral("未知"); 
                        modeIcon = QStringLiteral("?");
                        color = QStringLiteral("#95a5a6"); 
                        bgColor = QStringLiteral("#f5f5f5"); 
                        break;
                }
            }

            // 将mA转换为A
            double currentInA = current / 1000.0;
            
            // 更新状态标签（在左侧状态区域）- 带图标和背景色
            chLabels[ch]->setText(QStringLiteral("%1 %2 (%3A)")
                .arg(modeIcon, modeText).arg(currentInA, 0, 'f', 2));
            chLabels[ch]->setStyleSheet(QStringLiteral(
                "color: %1; font-weight: bold; font-size: 13px; "
                "background-color: %2; padding: 4px 8px; border-radius: 6px;").arg(color, bgColor));
            
            // 更新控制区域的电流标签（在按钮旁边）- 更醒目
            if (chCurrentLabels[ch]) {
                chCurrentLabels[ch]->setText(QStringLiteral("%1A").arg(currentInA, 0, 'f', 2));
                // 根据缺相状态设置颜色
                if (phaseLost) {
                    chCurrentLabels[ch]->setStyleSheet(QStringLiteral(
                        "font-size: 13px; font-weight: bold; color: #dc3545; "
                        "background-color: #f8d7da; padding: 4px 8px; border-radius: 6px; min-width: 60px;"));
                } else {
                    chCurrentLabels[ch]->setStyleSheet(QStringLiteral(
                        "font-size: 13px; font-weight: bold; color: %1; "
                        "background-color: %2; padding: 4px 8px; border-radius: 6px; min-width: 60px;").arg(color, bgColor));
                }
            }
            
            // 更新按钮样式以反映当前状态
            updateButtonStyles(ch, mode);
        } else {
            chLabels[ch]->setText(QStringLiteral("--"));
            chLabels[ch]->setStyleSheet(QStringLiteral(
                "color: #95a5a6; font-size: 13px; background-color: #f5f5f5; padding: 4px 8px; border-radius: 6px;"));
            
            if (chCurrentLabels[ch]) {
                chCurrentLabels[ch]->setText(QStringLiteral("-- A"));
                chCurrentLabels[ch]->setStyleSheet(QStringLiteral(
                    "font-size: 13px; font-weight: bold; color: #95a5a6; "
                    "background-color: #f5f5f5; padding: 4px 8px; border-radius: 6px; min-width: 60px;"));
            }
        }
    }
}

void RelayControlDialog::controlRelay(int channel, const QString &action)
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        statusLabel_->setText(QStringLiteral("在线状态: [X] 未连接服务器"));
        statusLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; color: #e74c3c;"));
        return;
    }

    // 防止重复控制操作，避免与查询并发
    if (isControlling_ || isQuerying_) {
        qDebug() << "[RELAY_CONTROL_DIALOG] 控制操作进行中，跳过";
        return;
    }
    isControlling_ = true;
    setControlBusy(true);

    QJsonObject params;
    params[QStringLiteral("node")] = nodeId_;
    params[QStringLiteral("ch")] = channel;
    params[QStringLiteral("action")] = action;

    // 使用异步调用避免阻塞UI线程
    int reqId = rpcClient_->callAsync(QStringLiteral("relay.control"), params, this,
        [this, channel, action](const QJsonValue &result, const QJsonObject &error) {
            isControlling_ = false;
            setControlBusy(false);

            if (!error.isEmpty()) {
                // 使用状态标签代替阻塞的QMessageBox，避免阻塞事件循环
                statusLabel_->setText(QStringLiteral("在线状态: [X] 控制失败: %1")
                    .arg(error.value(QStringLiteral("message")).toString()));
                statusLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; color: #e74c3c;"));
                return;
            }

            QJsonObject obj = result.toObject();
            if (obj.value(QStringLiteral("ok")).toBool()) {
                QString actionText;
                if (action == QStringLiteral("stop")) actionText = QStringLiteral("停止");
                else if (action == QStringLiteral("fwd")) actionText = QStringLiteral("正转");
                else if (action == QStringLiteral("rev")) actionText = QStringLiteral("反转");

                emit controlExecuted(QStringLiteral("节点 %1 通道 %2 -> %3")
                    .arg(nodeId_).arg(channel).arg(actionText));

                // 延迟刷新状态，避免频繁RPC调用
                QTimer::singleShot(kControlRefreshDelayMs, this, &RelayControlDialog::onQueryStatusClicked);
            } else {
                QString errorMsg = obj.value(QStringLiteral("error")).toString();
                statusLabel_->setText(QStringLiteral("在线状态: [X] 控制失败: %1").arg(errorMsg));
                statusLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; color: #e74c3c;"));
            }
        }, 3000);  // 3秒超时

    if (reqId < 0) {
        isControlling_ = false;
        setControlBusy(false);
        statusLabel_->setText(QStringLiteral("在线状态: [X] 控制请求发送失败"));
        statusLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; color: #e74c3c;"));
    }
}
