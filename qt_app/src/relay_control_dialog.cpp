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

namespace {
// Delay between stopping each channel during sequential stop (ms)
// This prevents hardware overload and ensures each relay has time to respond
constexpr int kChannelStopIntervalMs = 300;

// Delay before refreshing status after all channels are stopped (ms)
// This allows time for the hardware to report updated status
constexpr int kStatusRefreshDelayMs = 200;
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
    , ch0Slider_(nullptr)
    , ch1Slider_(nullptr)
    , ch2Slider_(nullptr)
    , ch3Slider_(nullptr)
    , stopChannelIndex_(0)
    , isControlling_(false)
{
    setWindowTitle(QStringLiteral("控制: %1 (#%2)").arg(deviceName).arg(nodeId));
    setMinimumSize(DIALOG_WIDTH, DIALOG_HEIGHT);
    setModal(true);
    setupUi();
    
    // 初始查询状态
    onQueryStatusClicked();
}

void RelayControlDialog::updateSliderStyle(QSlider *slider, int value)
{
    QString bgColor;
    QString handleColor;
    
    switch (value) {
        case 0: // 停止 - 灰色
            bgColor = QStringLiteral("#ecf0f1");
            handleColor = QStringLiteral("#7f8c8d");
            break;
        case 1: // 正转 - 绿色
            bgColor = QStringLiteral("#d4edda");
            handleColor = QStringLiteral("#27ae60");
            break;
        case 2: // 反转 - 橙色
            bgColor = QStringLiteral("#fff3cd");
            handleColor = QStringLiteral("#f39c12");
            break;
        default:
            bgColor = QStringLiteral("#f5f5f5");
            handleColor = QStringLiteral("#95a5a6");
            break;
    }
    
    slider->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal {"
        "  border: 1px solid #bdc3c7;"
        "  height: 20px;"
        "  background: %1;"
        "  border-radius: 10px;"
        "}"
        "QSlider::handle:horizontal {"
        "  background: %2;"
        "  border: 2px solid #ffffff;"
        "  width: 28px;"
        "  margin: -2px 0;"
        "  border-radius: 14px;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "  background: %2;"
        "  border: 2px solid #3498db;"
        "}").arg(bgColor, handleColor));
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

    currentLabel_ = new QLabel(QStringLiteral("总电流: -- A"), this);
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

    // 右侧：通道控制区域（使用滑块）
    QGroupBox *controlBox = new QGroupBox(QStringLiteral("通道控制"), this);
    QVBoxLayout *controlBoxLayout = new QVBoxLayout(controlBox);
    controlBoxLayout->setSpacing(8);
    controlBoxLayout->setContentsMargins(10, 12, 10, 10);

    // 控制说明
    QLabel *helpLabel = new QLabel(QStringLiteral("左=反转  中=停止  右=正转"), this);
    helpLabel->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 11px;"));
    helpLabel->setAlignment(Qt::AlignCenter);
    controlBoxLayout->addWidget(helpLabel);

    QGridLayout *controlGrid = new QGridLayout();
    controlGrid->setSpacing(8);
    controlGrid->setColumnStretch(1, 1);

    // 创建滑块控制
    QSlider *sliders[] = {nullptr, nullptr, nullptr, nullptr};
    QLabel *currentLabels[] = {nullptr, nullptr, nullptr, nullptr};
    
    for (int ch = 0; ch < 4; ++ch) {
        // 通道标签
        QLabel *chLabel = new QLabel(QStringLiteral("通道%1:").arg(ch), this);
        chLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12px;"));
        controlGrid->addWidget(chLabel, ch, 0);

        // 滑块控制 (0=停止, 1=正转, 2=反转)
        // 但为了直观性，我们使用 -1=反转, 0=停止, 1=正转
        QSlider *slider = new QSlider(Qt::Horizontal, this);
        slider->setMinimum(-1);  // 反转
        slider->setMaximum(1);   // 正转
        slider->setValue(0);     // 停止
        slider->setTickPosition(QSlider::TicksBelow);
        slider->setTickInterval(1);
        slider->setMinimumWidth(120);
        slider->setMinimumHeight(30);
        slider->setProperty("channel", ch);
        
        updateSliderStyle(slider, 0);
        
        connect(slider, &QSlider::valueChanged, this, &RelayControlDialog::onSliderValueChanged);
        controlGrid->addWidget(slider, ch, 1);
        sliders[ch] = slider;

        // 电流显示标签（在滑块旁边）
        QLabel *currentLbl = new QLabel(QStringLiteral("-- A"), this);
        currentLbl->setStyleSheet(QStringLiteral(
            "font-size: 12px; font-weight: bold; color: #3498db; min-width: 50px;"));
        currentLbl->setAlignment(Qt::AlignCenter);
        controlGrid->addWidget(currentLbl, ch, 2);
        currentLabels[ch] = currentLbl;
    }

    // 保存滑块和电流标签引用
    ch0Slider_ = sliders[0];
    ch1Slider_ = sliders[1];
    ch2Slider_ = sliders[2];
    ch3Slider_ = sliders[3];
    
    ch0CurrentLabel_ = currentLabels[0];
    ch1CurrentLabel_ = currentLabels[1];
    ch2CurrentLabel_ = currentLabels[2];
    ch3CurrentLabel_ = currentLabels[3];

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

void RelayControlDialog::onSliderValueChanged(int value)
{
    QSlider *slider = qobject_cast<QSlider*>(sender());
    if (!slider) return;

    int channel = slider->property("channel").toInt();
    
    // 更新滑块样式
    // 将滑块值转换为显示模式: -1=反转(2), 0=停止(0), 1=正转(1)
    int displayMode = (value == -1) ? 2 : value;
    updateSliderStyle(slider, displayMode);
    
    // 转换滑块值为动作
    QString action;
    if (value == 0) {
        action = QStringLiteral("stop");
    } else if (value == 1) {
        action = QStringLiteral("fwd");
    } else if (value == -1) {
        action = QStringLiteral("rev");
    }

    controlRelay(channel, action);
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
    // 开始顺序停止所有通道
    stopChannelIndex_ = 0;
    stopNextChannel();
    
    // 同时将所有滑块设置为停止位置
    QSlider *sliders[] = {ch0Slider_, ch1Slider_, ch2Slider_, ch3Slider_};
    for (int ch = 0; ch < 4; ++ch) {
        if (sliders[ch]) {
            sliders[ch]->blockSignals(true);
            sliders[ch]->setValue(0);
            updateSliderStyle(sliders[ch], 0);
            sliders[ch]->blockSignals(false);
        }
    }
}

void RelayControlDialog::stopNextChannel()
{
    // 安全检查：确保RPC客户端可用
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        return;
    }
    
    if (stopChannelIndex_ >= 4) {
        // 所有通道已停止，刷新状态
        QTimer::singleShot(kStatusRefreshDelayMs, this, &RelayControlDialog::onQueryStatusClicked);
        return;
    }
    
    // 直接异步调用，不使用controlRelay()以避免isControlling_冲突
    int channel = stopChannelIndex_;
    QJsonObject params;
    params[QStringLiteral("node")] = nodeId_;
    params[QStringLiteral("ch")] = channel;
    params[QStringLiteral("action")] = QStringLiteral("stop");
    
    rpcClient_->callAsync(QStringLiteral("relay.control"), params,
        [this, channel](const QJsonValue &result, const QJsonObject &error) {
            QMetaObject::invokeMethod(this, [this, channel, result, error]() {
                if (error.isEmpty() && result.isObject()) {
                    QJsonObject obj = result.toObject();
                    if (obj.value(QStringLiteral("ok")).toBool()) {
                        emit controlExecuted(QStringLiteral("节点 %1 通道 %2 -> 停止")
                            .arg(nodeId_).arg(channel));
                    }
                }
            }, Qt::QueuedConnection);
        }, 2000);  // 2秒超时
    
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

    QJsonObject params;
    params[QStringLiteral("node")] = nodeId_;

    // 使用异步调用避免阻塞UI线程
    rpcClient_->callAsync(QStringLiteral("relay.statusAll"), params,
        [this](const QJsonValue &result, const QJsonObject &error) {
            QMetaObject::invokeMethod(this, [this, result, error]() {
                if (!error.isEmpty()) {
                    statusLabel_->setText(QStringLiteral("在线状态: [X] 查询失败"));
                    return;
                }
                
                if (result.isObject()) {
                    updateStatusDisplay(result.toObject());
                }
            }, Qt::QueuedConnection);
        }, 2000);  // 2秒超时
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

    // 更新电流（mA转换为A）
    double totalCurrent = status.value(QStringLiteral("totalCurrent")).toDouble(0);
    double totalCurrentInA = totalCurrent / 1000.0;
    currentLabel_->setText(QStringLiteral("总电流: %1 A").arg(totalCurrentInA, 0, 'f', 2));

    // 更新通道状态
    QJsonObject channels = status.value(QStringLiteral("channels")).toObject();
    QLabel *chLabels[] = {ch0StatusLabel_, ch1StatusLabel_, ch2StatusLabel_, ch3StatusLabel_};
    QLabel *chCurrentLabels[] = {ch0CurrentLabel_, ch1CurrentLabel_, ch2CurrentLabel_, ch3CurrentLabel_};
    QSlider *chSliders[] = {ch0Slider_, ch1Slider_, ch2Slider_, ch3Slider_};

    for (int ch = 0; ch < 4; ++ch) {
        QString chKey = QString::number(ch);
        if (channels.contains(chKey)) {
            QJsonObject chStatus = channels.value(chKey).toObject();
            int mode = chStatus.value(QStringLiteral("mode")).toInt(0);
            double current = chStatus.value(QStringLiteral("current")).toDouble(0);
            bool phaseLost = chStatus.value(QStringLiteral("phaseLost")).toBool(false);

            QString modeText;
            QString color;
            QString bgColor;
            
            // 如果缺相，优先显示缺相状态
            if (phaseLost) {
                modeText = QStringLiteral("缺相");
                color = QStringLiteral("#dc3545");
                bgColor = QStringLiteral("#f8d7da");
            } else {
                switch (mode) {
                    case 0: modeText = QStringLiteral("停止"); color = QStringLiteral("#7f8c8d"); bgColor = QStringLiteral("#ecf0f1"); break;
                    case 1: modeText = QStringLiteral("正转"); color = QStringLiteral("#27ae60"); bgColor = QStringLiteral("#d4edda"); break;
                    case 2: modeText = QStringLiteral("反转"); color = QStringLiteral("#f39c12"); bgColor = QStringLiteral("#fff3cd"); break;
                    default: modeText = QStringLiteral("未知"); color = QStringLiteral("#95a5a6"); bgColor = QStringLiteral("#f5f5f5"); break;
                }
            }

            // 将mA转换为A
            double currentInA = current / 1000.0;
            
            // 更新状态标签（在左侧状态区域）
            chLabels[ch]->setText(QStringLiteral("%1 (%2A)")
                .arg(modeText).arg(currentInA, 0, 'f', 2));
            chLabels[ch]->setStyleSheet(QStringLiteral("color: %1; font-weight: bold; font-size: 13px;").arg(color));
            
            // 更新控制区域的电流标签（在滑块旁边）
            if (chCurrentLabels[ch]) {
                chCurrentLabels[ch]->setText(QStringLiteral("%1A").arg(currentInA, 0, 'f', 2));
                // 根据缺相状态设置颜色
                if (phaseLost) {
                    chCurrentLabels[ch]->setStyleSheet(QStringLiteral(
                        "font-size: 12px; font-weight: bold; color: #dc3545; "
                        "background-color: #f8d7da; padding: 2px 4px; border-radius: 4px; min-width: 50px;"));
                } else {
                    chCurrentLabels[ch]->setStyleSheet(QStringLiteral(
                        "font-size: 12px; font-weight: bold; color: %1; "
                        "background-color: %2; padding: 2px 4px; border-radius: 4px; min-width: 50px;").arg(color, bgColor));
                }
            }
            
            // 更新滑块位置（不触发信号）
            if (chSliders[ch]) {
                chSliders[ch]->blockSignals(true);
                int sliderValue = 0;
                switch (mode) {
                    case 0: sliderValue = 0; break;   // 停止
                    case 1: sliderValue = 1; break;   // 正转
                    case 2: sliderValue = -1; break;  // 反转
                }
                chSliders[ch]->setValue(sliderValue);
                // 将mode转换为显示模式
                int displayMode = mode;
                updateSliderStyle(chSliders[ch], displayMode);
                chSliders[ch]->blockSignals(false);
            }
        } else {
            chLabels[ch]->setText(QStringLiteral("--"));
            chLabels[ch]->setStyleSheet(QStringLiteral("color: #95a5a6; font-size: 13px;"));
            
            if (chCurrentLabels[ch]) {
                chCurrentLabels[ch]->setText(QStringLiteral("-- A"));
                chCurrentLabels[ch]->setStyleSheet(QStringLiteral(
                    "font-size: 12px; font-weight: bold; color: #95a5a6; min-width: 50px;"));
            }
        }
    }
}

void RelayControlDialog::controlRelay(int channel, const QString &action)
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }
    
    // 防止重复控制操作
    if (isControlling_) {
        qDebug() << "[RELAY_CONTROL_DIALOG] 控制操作进行中，跳过";
        return;
    }
    isControlling_ = true;

    QJsonObject params;
    params[QStringLiteral("node")] = nodeId_;
    params[QStringLiteral("ch")] = channel;
    params[QStringLiteral("action")] = action;

    // 使用异步调用避免阻塞UI线程
    rpcClient_->callAsync(QStringLiteral("relay.control"), params,
        [this, channel, action](const QJsonValue &result, const QJsonObject &error) {
            QMetaObject::invokeMethod(this, [this, channel, action, result, error]() {
                isControlling_ = false;
                
                if (!error.isEmpty()) {
                    QMessageBox::warning(this, QStringLiteral("错误"),
                        QStringLiteral("控制失败: %1").arg(error.value(QStringLiteral("message")).toString()));
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

                    // 刷新状态
                    onQueryStatusClicked();
                } else {
                    QString errorMsg = obj.value(QStringLiteral("error")).toString();
                    QMessageBox::warning(this, QStringLiteral("错误"),
                        QStringLiteral("控制失败: %1").arg(errorMsg));
                }
            }, Qt::QueuedConnection);
        }, 2000);  // 2秒超时
}
