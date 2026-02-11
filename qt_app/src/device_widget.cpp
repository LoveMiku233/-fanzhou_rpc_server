/**
 * @file device_widget.cpp
 * @brief 设备管理页面实现 - 网格卡片布局（1024x600低分辨率优化版）
 */

#include "device_widget.h"
#include "rpc_client.h"
#include "relay_control_dialog.h"
#include "style_constants.h"

#include <QScroller>

#include <algorithm>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QTimer>
#include <QColor>
#include <QMouseEvent>
#include <QScrollArea>
#include <QInputDialog>
#include <QSpinBox>
#include <QLineEdit>
#include <QFormLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGraphicsDropShadowEffect>

using namespace UIConstants;

namespace {
// Minimum current (mA) threshold to display in channel status
// Values below this are considered noise/measurement error and are hidden
constexpr double kMinDisplayCurrentMa = 0.1;

// Delay (ms) before fallback status refresh when device.list doesn't return channel data
// This ensures the UI layout is complete before initiating additional RPC calls
constexpr int kFallbackRefreshDelayMs = 50;
}

// ==================== DeviceCard Implementation ====================

DeviceCard::DeviceCard(int nodeId, const QString &name, QWidget *parent)
    : QFrame(parent)
    , nodeId_(nodeId)
    , name_(name)
    , nameLabel_(nullptr)
    , nodeIdLabel_(nullptr)
    , statusLabel_(nullptr)
    , currentLabel_(nullptr)
    , ch0Label_(nullptr)
    , ch1Label_(nullptr)
    , ch2Label_(nullptr)
    , ch3Label_(nullptr)
{
    setupUi();
}

void DeviceCard::setupUi()
{
    setObjectName(QStringLiteral("deviceCard"));
    setFrameShape(QFrame::NoFrame);
    setStyleSheet(QStringLiteral(
        "#deviceCard {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #f8f9fa);"
        "  border: 2px solid #e0e0e0;"
        "  border-radius: %1px;"
        "}"
        "#deviceCard:hover {"
        "  border-color: #27ae60;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #eafaf1);"
        "}").arg(BORDER_RADIUS_CARD));
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(CARD_MIN_HEIGHT);
    setMinimumWidth(200);  // 确保卡片最小宽度

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(CARD_MARGIN + 2, CARD_MARGIN + 2, CARD_MARGIN + 2, CARD_MARGIN + 2);
    mainLayout->setSpacing(CARD_SPACING + 2);

    // 顶部行：名称和节点ID
    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->setSpacing(6);
    
    nameLabel_ = new QLabel(name_, this);
    nameLabel_->setStyleSheet(QStringLiteral(
        "font-size: %1px; font-weight: bold; color: #2c3e50;").arg(FONT_SIZE_CARD_TITLE));
    topRow->addWidget(nameLabel_);
    
    topRow->addStretch();
    
    nodeIdLabel_ = new QLabel(QStringLiteral("#%1").arg(nodeId_), this);
    nodeIdLabel_->setStyleSheet(QStringLiteral(
        "font-size: %1px; color: #7f8c8d; background-color: #ecf0f1; "
        "padding: 3px 8px; border-radius: 6px;").arg(FONT_SIZE_SMALL));
    topRow->addWidget(nodeIdLabel_);
    
    mainLayout->addLayout(topRow);

    // 中间行：状态和电流
    QHBoxLayout *middleRow = new QHBoxLayout();
    middleRow->setSpacing(8);
    
    statusLabel_ = new QLabel(QStringLiteral("[等]等待..."), this);
    statusLabel_->setStyleSheet(QStringLiteral("font-size: %1px; font-weight: bold; color: #7f8c8d;").arg(FONT_SIZE_BODY));
    middleRow->addWidget(statusLabel_);
    
    middleRow->addStretch();
    
    currentLabel_ = new QLabel(QStringLiteral("--mA"), this);
    currentLabel_->setStyleSheet(QStringLiteral(
        "font-size: %1px; color: #3498db; font-weight: bold;").arg(FONT_SIZE_BODY));
    middleRow->addWidget(currentLabel_);
    
    mainLayout->addLayout(middleRow);

    // 底部分隔线
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QStringLiteral("color: #e0e0e0;"));
    line->setMaximumHeight(1);
    mainLayout->addWidget(line);

    // 底部行：通道状态
    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(6);
    
    auto createChLabel = [this](const QString &text) -> QLabel* {
        QLabel *label = new QLabel(text, this);
        label->setStyleSheet(QStringLiteral(
            "font-size: %1px; padding: 3px 8px; background-color: #f5f5f5; color: #95a5a6; border-radius: 6px;").arg(FONT_SIZE_SMALL));
        return label;
    };
    
    ch0Label_ = createChLabel(QStringLiteral("0:--"));
    bottomRow->addWidget(ch0Label_);
    
    ch1Label_ = createChLabel(QStringLiteral("1:--"));
    bottomRow->addWidget(ch1Label_);
    
    ch2Label_ = createChLabel(QStringLiteral("2:--"));
    bottomRow->addWidget(ch2Label_);
    
    ch3Label_ = createChLabel(QStringLiteral("3:--"));
    bottomRow->addWidget(ch3Label_);
    
    bottomRow->addStretch();
    
    mainLayout->addLayout(bottomRow);
}

void DeviceCard::updateStatus(bool online, qint64 ageMs, double totalCurrent, const QJsonObject &channels)
{
    // 更新在线状态
    if (online) {
        statusLabel_->setText(QStringLiteral("[OK]在线(%1ms)").arg(ageMs));
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: %1px; font-weight: bold; color: #27ae60;").arg(FONT_SIZE_BODY));
    } else if (ageMs < 0) {
        statusLabel_->setText(QStringLiteral("[警]无响应"));
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: %1px; font-weight: bold; color: #f39c12;").arg(FONT_SIZE_BODY));
    } else {
        statusLabel_->setText(QStringLiteral("[X]离线(%1s)").arg(ageMs / 1000));
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: %1px; font-weight: bold; color: #e74c3c;").arg(FONT_SIZE_BODY));
    }

    // 更新总电流 - 当设备从未响应时显示 "--mA"，否则显示实际值
    if (ageMs < 0 && totalCurrent < kMinDisplayCurrentMa) {
        currentLabel_->setText(QStringLiteral("--mA"));
        currentLabel_->setStyleSheet(QStringLiteral(
            "font-size: %1px; color: #95a5a6; font-weight: bold;").arg(FONT_SIZE_BODY));
    } else {
        currentLabel_->setText(QStringLiteral("%1mA").arg(totalCurrent, 0, 'f', 1));
        currentLabel_->setStyleSheet(QStringLiteral(
            "font-size: %1px; color: #3498db; font-weight: bold;").arg(FONT_SIZE_BODY));
    }

    // 更新通道状态 - 显示模式、电流和缺相状态
    QLabel *chLabels[] = {ch0Label_, ch1Label_, ch2Label_, ch3Label_};
    for (int ch = 0; ch < 4; ++ch) {
        QString chKey = QString::number(ch);
        if (channels.contains(chKey)) {
            QJsonObject chStatus = channels.value(chKey).toObject();
            int mode = chStatus.value(QStringLiteral("mode")).toInt(0);
            bool phaseLost = chStatus.value(QStringLiteral("phaseLost")).toBool(false);
            double current = chStatus.value(QStringLiteral("current")).toDouble(0);

            QString modeText;
            QString bgColor;
            QString textColor;
            
            // 如果缺相，优先显示缺相状态
            if (phaseLost) {
                modeText = QStringLiteral("缺");
                bgColor = QStringLiteral("#f8d7da");
                textColor = QStringLiteral("#721c24");
            } else {
                switch (mode) {
                    case 0: modeText = QStringLiteral("停"); bgColor = QStringLiteral("#ecf0f1"); textColor = QStringLiteral("#7f8c8d"); break;
                    case 1: modeText = QStringLiteral("正"); bgColor = QStringLiteral("#d4edda"); textColor = QStringLiteral("#155724"); break;
                    case 2: modeText = QStringLiteral("反"); bgColor = QStringLiteral("#fff3cd"); textColor = QStringLiteral("#856404"); break;
                    default: modeText = QStringLiteral("?"); bgColor = QStringLiteral("#f5f5f5"); textColor = QStringLiteral("#7f8c8d"); break;
                }
            }

            // 显示通道号:状态(电流mA)
            QString displayText;
            if (current > kMinDisplayCurrentMa) {
                displayText = QStringLiteral("%1:%2(%3)").arg(ch).arg(modeText).arg(current, 0, 'f', 0);
            } else {
                displayText = QStringLiteral("%1:%2").arg(ch).arg(modeText);
            }

            chLabels[ch]->setText(displayText);
            chLabels[ch]->setStyleSheet(QStringLiteral(
                "font-size: %1px; padding: 3px 8px; background-color: %2; color: %3; border-radius: 6px;")
                .arg(FONT_SIZE_SMALL).arg(bgColor, textColor));
        } else if (ageMs < 0) {
            // 设备从未响应时，显示等待状态
            chLabels[ch]->setText(QStringLiteral("%1:--").arg(ch));
            chLabels[ch]->setStyleSheet(QStringLiteral(
                "font-size: %1px; padding: 3px 8px; background-color: #f5f5f5; color: #95a5a6; border-radius: 6px;")
                .arg(FONT_SIZE_SMALL));
        }
    }
}

void DeviceCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(nodeId_, name_);
    }
    QFrame::mousePressEvent(event);
}

// ==================== DeviceWidget Implementation ====================

DeviceWidget::DeviceWidget(RpcClient *rpcClient, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpcClient)
    , statusLabel_(nullptr)
    , refreshButton_(nullptr)
    , queryAllButton_(nullptr)
    , addDeviceButton_(nullptr)
    , cardsContainer_(nullptr)
    , cardsLayout_(nullptr)
    , isRefreshing_(false)
{
    setupUi();
    qDebug() << "[DEVICE_WIDGET] 设备页面初始化完成";
}

void DeviceWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(PAGE_MARGIN, PAGE_MARGIN, PAGE_MARGIN, PAGE_MARGIN);
    mainLayout->setSpacing(PAGE_SPACING);

    // 页面标题
    QLabel *titleLabel = new QLabel(QStringLiteral("[设] 设备管理"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: %1px; font-weight: bold; color: #2c3e50; padding: 2px 0;").arg(FONT_SIZE_TITLE));
    mainLayout->addWidget(titleLabel);

    // 工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(CARD_SPACING);

    refreshButton_ = new QPushButton(QStringLiteral("[刷]刷新"), this);
    refreshButton_->setFixedHeight(BTN_HEIGHT);
    refreshButton_->setMinimumWidth(BTN_MIN_WIDTH);
    refreshButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #3498db; color: white; border: none; "
        "border-radius: %1px; padding: 0 12px; font-weight: bold; font-size: %2px; }"
        "QPushButton:hover { background-color: #2980b9; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_BODY));
    connect(refreshButton_, &QPushButton::clicked, this, &DeviceWidget::refreshDeviceList);
    toolbarLayout->addWidget(refreshButton_);

    queryAllButton_ = new QPushButton(QStringLiteral("[查]查询"), this);
    queryAllButton_->setFixedHeight(BTN_HEIGHT);
    queryAllButton_->setMinimumWidth(BTN_MIN_WIDTH);
    queryAllButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #27ae60; color: white; border: none; "
        "border-radius: %1px; padding: 0 12px; font-weight: bold; font-size: %2px; }"
        "QPushButton:hover { background-color: #229954; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_BODY));
    connect(queryAllButton_, &QPushButton::clicked, this, &DeviceWidget::onQueryAllClicked);
    toolbarLayout->addWidget(queryAllButton_);

    addDeviceButton_ = new QPushButton(QStringLiteral("[+]添加"), this);
    addDeviceButton_->setFixedHeight(BTN_HEIGHT);
    addDeviceButton_->setMinimumWidth(BTN_MIN_WIDTH);
    addDeviceButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #f39c12; color: white; border: none; "
        "border-radius: %1px; padding: 0 12px; font-weight: bold; font-size: %2px; }"
        "QPushButton:hover { background-color: #d68910; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_BODY));
    connect(addDeviceButton_, &QPushButton::clicked, this, &DeviceWidget::onAddDeviceClicked);
    toolbarLayout->addWidget(addDeviceButton_);

    toolbarLayout->addStretch();

    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet(QStringLiteral(
        "color: #7f8c8d; font-size: %1px; padding: 4px 8px; background-color: #f8f9fa; border-radius: 4px;").arg(FONT_SIZE_SMALL));
    toolbarLayout->addWidget(statusLabel_);

    mainLayout->addLayout(toolbarLayout);

    // 滚动区域
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

    // 设备卡片容器
    cardsContainer_ = new QWidget();
    cardsContainer_->setStyleSheet(QStringLiteral("background: transparent;"));
    cardsLayout_ = new QGridLayout(cardsContainer_);
    cardsLayout_->setContentsMargins(0, 0, 0, 0);
    cardsLayout_->setSpacing(PAGE_SPACING);
    cardsLayout_->setColumnStretch(0, 1);
    cardsLayout_->setColumnStretch(1, 1);
    
    scrollArea->setWidget(cardsContainer_);
    mainLayout->addWidget(scrollArea, 1);

    // 提示文本
    QLabel *helpLabel = new QLabel(
        QStringLiteral("[示] 点击卡片控制，绿=正转，黄=反转，灰=停止"),
        this);
    helpLabel->setStyleSheet(QStringLiteral(
        "color: #5d6d7e; font-size: %1px; padding: 6px; background-color: #eaf2f8; border-radius: 4px;").arg(FONT_SIZE_SMALL));
    helpLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(helpLabel);
}

void DeviceWidget::clearDeviceCards()
{
    for (DeviceCard *card : deviceCards_) {
        cardsLayout_->removeWidget(card);
        delete card;
    }
    deviceCards_.clear();
}

void DeviceWidget::refreshDeviceList()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        statusLabel_->setText(QStringLiteral("[X] 未连接"));
        emit logMessage(QStringLiteral("刷新设备失败：未连接服务器"), QStringLiteral("WARN"));
        return;
    }

    // 防止重复刷新
    if (isRefreshing_) {
        qDebug() << "[DEVICE_WIDGET] 刷新操作进行中，跳过";
        return;
    }
    isRefreshing_ = true;

    statusLabel_->setText(QStringLiteral("[刷] 刷新中..."));
    qDebug() << "[DEVICE_WIDGET] 刷新设备列表";

    // 使用异步调用避免阻塞UI线程
    int reqId = rpcClient_->callAsync(QStringLiteral("device.list"), QJsonObject(),
        [this](const QJsonValue &result, const QJsonObject &error) {
            // 在主线程中处理结果
            QMetaObject::invokeMethod(this, [this, result, error]() {
                if (!error.isEmpty()) {
                    // 如果device.list失败，尝试relay.nodes
                    tryRelayNodesAsFallback();
                    return;
                }
                
                qDebug() << "[DEVICE_WIDGET] device.list 响应:" << QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact);

                if (result.isObject()) {
                    QJsonObject obj = result.toObject();
                    if (obj.contains(QStringLiteral("devices"))) {
                        QJsonArray devices = obj.value(QStringLiteral("devices")).toArray();
                        updateDeviceCards(devices);
                        statusLabel_->setText(QStringLiteral("[OK] 共 %1 个设备").arg(devices.size()));
                        emit logMessage(QStringLiteral("刷新设备列表成功，共 %1 个设备").arg(devices.size()));
                        isRefreshing_ = false;
                        return;
                    }
                }
                
                // 回退到relay.nodes
                tryRelayNodesAsFallback();
            }, Qt::QueuedConnection);
        }, 2000);  // 2秒超时
    
    // 如果异步调用失败，重置标志
    if (reqId < 0) {
        isRefreshing_ = false;
        statusLabel_->setText(QStringLiteral("[X] 发送请求失败"));
        emit logMessage(QStringLiteral("刷新设备失败：无法发送请求"), QStringLiteral("ERROR"));
    }
}

void DeviceWidget::tryRelayNodesAsFallback()
{
    // 使用异步调用
    rpcClient_->callAsync(QStringLiteral("relay.nodes"), QJsonObject(),
        [this](const QJsonValue &result, const QJsonObject &error) {
            QMetaObject::invokeMethod(this, [this, result, error]() {
                if (!error.isEmpty()) {
                    statusLabel_->setText(QStringLiteral("[X] 获取失败"));
                    emit logMessage(QStringLiteral("获取设备列表失败"), QStringLiteral("ERROR"));
                    isRefreshing_ = false;
                    return;
                }
                
                qDebug() << "[DEVICE_WIDGET] relay.nodes 响应:" << QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact);

                if (result.isObject()) {
                    QJsonObject obj = result.toObject();
                    if (obj.contains(QStringLiteral("nodes"))) {
                        QJsonArray nodes = obj.value(QStringLiteral("nodes")).toArray();
                        // 转换为device格式，保留在线状态和ageMs
                        QJsonArray devices;
                        for (const QJsonValue &val : nodes) {
                            QJsonObject node = val.toObject();
                            QJsonObject device;
                            device[QStringLiteral("nodeId")] = node.value(QStringLiteral("node")).toInt();
                            device[QStringLiteral("name")] = QStringLiteral("继电器-%1").arg(node.value(QStringLiteral("node")).toInt());
                            device[QStringLiteral("online")] = node.value(QStringLiteral("online")).toBool();
                            // 传递ageMs信息以供显示
                            if (node.contains(QStringLiteral("ageMs"))) {
                                device[QStringLiteral("ageMs")] = node.value(QStringLiteral("ageMs"));
                            }
                            devices.append(device);
                        }
                        updateDeviceCards(devices);
                        statusLabel_->setText(QStringLiteral("[OK] 共 %1 个设备").arg(devices.size()));
                        emit logMessage(QStringLiteral("刷新设备列表成功，共 %1 个设备").arg(devices.size()));
                        isRefreshing_ = false;
                        return;
                    }
                }

                statusLabel_->setText(QStringLiteral("[X] 获取失败"));
                emit logMessage(QStringLiteral("获取设备列表失败"), QStringLiteral("ERROR"));
                isRefreshing_ = false;
            }, Qt::QueuedConnection);
        }, 2000);  // 2秒超时
}

void DeviceWidget::refreshDeviceStatus()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        return;
    }

    qDebug() << "[DEVICE_WIDGET] 刷新设备状态，设备数量:" << deviceCards_.size();

    // 使用异步调用避免阻塞UI线程
    for (DeviceCard *card : deviceCards_) {
        int nodeId = card->nodeId();

        QJsonObject params;
        params[QStringLiteral("node")] = nodeId;

        // 使用异步调用，通过lambda回调更新UI
        rpcClient_->callAsync(QStringLiteral("relay.statusAll"), params,
            [this, nodeId](const QJsonValue &result, const QJsonObject &error) {
                if (error.isEmpty() && result.isObject()) {
                    QJsonObject resultObj = result.toObject();
                    qDebug() << "[DEVICE_WIDGET] relay.statusAll node=" << nodeId 
                             << "online=" << resultObj.value(QStringLiteral("online")).toBool()
                             << "totalCurrent=" << resultObj.value(QStringLiteral("totalCurrent")).toDouble();
                    // 在主线程中更新UI
                    QMetaObject::invokeMethod(this, [this, nodeId, result]() {
                        updateDeviceCardStatus(nodeId, result.toObject());
                    }, Qt::QueuedConnection);
                } else if (!error.isEmpty()) {
                    qDebug() << "[DEVICE_WIDGET] relay.statusAll node=" << nodeId << "错误:" 
                             << error.value(QStringLiteral("message")).toString();
                }
            }, 2000);  // 2秒超时
    }
}

void DeviceWidget::onQueryAllClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    statusLabel_->setText(QStringLiteral("[查] 查询中..."));
    qDebug() << "[DEVICE_WIDGET] 查询所有设备";

    // 使用异步调用避免阻塞UI线程
    rpcClient_->callAsync(QStringLiteral("relay.queryAll"), QJsonObject(),
        [this](const QJsonValue &result, const QJsonObject &error) {
            QMetaObject::invokeMethod(this, [this, result, error]() {
                if (!error.isEmpty()) {
                    statusLabel_->setText(QStringLiteral("[X] 查询失败"));
                    emit logMessage(QStringLiteral("查询所有设备失败"), QStringLiteral("ERROR"));
                    return;
                }
                
                qDebug() << "[DEVICE_WIDGET] relay.queryAll 响应:" << QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact);

                if (result.isObject()) {
                    QJsonObject obj = result.toObject();
                    if (obj.value(QStringLiteral("ok")).toBool()) {
                        int queried = obj.value(QStringLiteral("queriedDevices")).toInt();
                        statusLabel_->setText(QStringLiteral("[OK] 已查询 %1 个设备").arg(queried));
                        emit logMessage(QStringLiteral("查询所有设备成功，共 %1 个设备").arg(queried));

                        // 延迟后刷新状态
                        QTimer::singleShot(500, this, &DeviceWidget::refreshDeviceStatus);
                        return;
                    }
                }

                statusLabel_->setText(QStringLiteral("[X] 查询失败"));
                emit logMessage(QStringLiteral("查询所有设备失败"), QStringLiteral("ERROR"));
            }, Qt::QueuedConnection);
        }, 2000);  // 2秒超时
}

void DeviceWidget::onDeviceCardClicked(int nodeId, const QString &name)
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    qDebug() << "[DEVICE_WIDGET] 打开设备控制对话框 nodeId=" << nodeId;

    RelayControlDialog *dialog = new RelayControlDialog(rpcClient_, nodeId, name, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &RelayControlDialog::controlExecuted, this, [this](const QString &message) {
        emit logMessage(message);
    });
    connect(dialog, &QDialog::finished, this, [this, nodeId]() {
        // 安全检查：确保RPC客户端仍然可用
        if (!rpcClient_ || !rpcClient_->isConnected()) {
            return;
        }
        // 使用异步调用刷新设备状态
        QJsonObject params;
        params[QStringLiteral("node")] = nodeId;
        rpcClient_->callAsync(QStringLiteral("relay.statusAll"), params,
            [this, nodeId](const QJsonValue &result, const QJsonObject &error) {
                if (error.isEmpty() && result.isObject()) {
                    QMetaObject::invokeMethod(this, [this, nodeId, result]() {
                        updateDeviceCardStatus(nodeId, result.toObject());
                    }, Qt::QueuedConnection);
                }
            }, 2000);  // 2秒超时
    });
    dialog->exec();
}

void DeviceWidget::onAddDeviceClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    // 创建添加设备对话框 - 触控友好
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("添加设备"));
    dialog.setMinimumWidth(350);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QFormLayout *formLayout = new QFormLayout();
    
    QSpinBox *nodeIdSpinBox = new QSpinBox(&dialog);
    nodeIdSpinBox->setRange(1, 255);
    nodeIdSpinBox->setValue(1);
    nodeIdSpinBox->setMinimumHeight(44);
    nodeIdSpinBox->setStyleSheet(QStringLiteral(
        "QSpinBox { border: 2px solid #e0e0e0; border-radius: 8px; padding: 6px 12px; font-size: 15px; }"
        "QSpinBox:focus { border-color: #3498db; }"));
    formLayout->addRow(QStringLiteral("节点ID:"), nodeIdSpinBox);
    
    QLineEdit *nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText(QStringLiteral("设备-1"));
    nameEdit->setText(QStringLiteral("设备-1"));
    nameEdit->setMinimumHeight(44);
    nameEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { border: 2px solid #e0e0e0; border-radius: 8px; padding: 6px 12px; font-size: 15px; }"
        "QLineEdit:focus { border-color: #3498db; }"));
    formLayout->addRow(QStringLiteral("名称:"), nameEdit);
    
    QSpinBox *typeSpinBox = new QSpinBox(&dialog);
    typeSpinBox->setRange(1, 100);
    typeSpinBox->setValue(1);
    typeSpinBox->setMinimumHeight(44);
    typeSpinBox->setStyleSheet(nodeIdSpinBox->styleSheet());
    formLayout->addRow(QStringLiteral("设备类型:"), typeSpinBox);
    
    layout->addLayout(formLayout);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("添加"));
    buttonBox->button(QDialogButtonBox::Ok)->setMinimumHeight(44);
    buttonBox->button(QDialogButtonBox::Ok)->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #27ae60; color: white; border: none; "
        "border-radius: 8px; padding: 0 24px; font-weight: bold; font-size: 14px; }"
        "QPushButton:hover { background-color: #229954; }"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    buttonBox->button(QDialogButtonBox::Cancel)->setMinimumHeight(44);
    buttonBox->button(QDialogButtonBox::Cancel)->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #95a5a6; color: white; border: none; "
        "border-radius: 8px; padding: 0 24px; font-weight: bold; font-size: 14px; }"
        "QPushButton:hover { background-color: #7f8c8d; }"));
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    
    int nodeId = nodeIdSpinBox->value();
    QString name = nameEdit->text().trimmed();
    int type = typeSpinBox->value();
    
    if (name.isEmpty()) {
        name = QStringLiteral("设备-%1").arg(nodeId);
    }
    
    QJsonObject params;
    params[QStringLiteral("nodeId")] = nodeId;
    params[QStringLiteral("name")] = name;
    params[QStringLiteral("type")] = type;
    
    qDebug() << "[DEVICE_WIDGET] 添加设备:" << name << "nodeId=" << nodeId << "type=" << type;
    
    // 使用异步调用避免阻塞UI线程
    rpcClient_->callAsync(QStringLiteral("device.add"), params,
        [this, name](const QJsonValue &result, const QJsonObject &error) {
            QMetaObject::invokeMethod(this, [this, name, result, error]() {
                if (!error.isEmpty()) {
                    QMessageBox::warning(this, QStringLiteral("错误"), 
                        QStringLiteral("[X] 添加设备失败: %1").arg(error.value(QStringLiteral("message")).toString()));
                    return;
                }
                
                qDebug() << "[DEVICE_WIDGET] device.add 响应:" << QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact);

                if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
                    QMessageBox::information(this, QStringLiteral("成功"), 
                        QStringLiteral("[OK] 设备 %1 添加成功！").arg(name));
                    emit logMessage(QStringLiteral("添加设备成功: %1").arg(name));
                    refreshDeviceList();
                } else {
                    QString errorMsg = result.toObject().value(QStringLiteral("error")).toString();
                    QMessageBox::warning(this, QStringLiteral("错误"), 
                        QStringLiteral("[X] 添加设备失败: %1").arg(errorMsg));
                }
            }, Qt::QueuedConnection);
        }, 2000);  // 2秒超时
}

void DeviceWidget::updateDeviceCards(const QJsonArray &devices)
{
    clearDeviceCards();

    // 按nodeId排序
    QList<QJsonObject> sortedDevices;
    for (const QJsonValue &value : devices) {
        sortedDevices.append(value.toObject());
    }
    std::sort(sortedDevices.begin(), sortedDevices.end(), [](const QJsonObject &a, const QJsonObject &b) {
        return a.value(QStringLiteral("nodeId")).toInt() < b.value(QStringLiteral("nodeId")).toInt();
    });

    int row = 0;
    int col = 0;
    bool hasChannelData = false;
    
    for (const QJsonObject &device : sortedDevices) {
        int nodeId = device.value(QStringLiteral("nodeId")).toInt();
        QString name = device.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            name = QStringLiteral("继电器-%1").arg(nodeId);
        }

        DeviceCard *card = new DeviceCard(nodeId, name, this);
        connect(card, &DeviceCard::clicked, this, &DeviceWidget::onDeviceCardClicked);

        // 网格布局：一行两个
        cardsLayout_->addWidget(card, row, col);
        deviceCards_.append(card);

        col++;
        if (col >= 2) {
            col = 0;
            row++;
        }

        // 设置初始状态 - 使用device.list返回的状态信息（如果有）
        bool online = device.value(QStringLiteral("online")).toBool();
        // 使用toVariant().toLongLong()以正确处理整数和浮点数类型
        qint64 ageMs = device.value(QStringLiteral("ageMs")).toVariant().toLongLong();
        if (!device.contains(QStringLiteral("ageMs")) || device.value(QStringLiteral("ageMs")).isNull()) {
            ageMs = -1;  // 默认值表示无数据
        }
        double totalCurrent = device.value(QStringLiteral("totalCurrent")).toDouble(0);
        QJsonObject channels = device.value(QStringLiteral("channels")).toObject();
        
        // 检查是否包含通道数据
        if (!channels.isEmpty()) {
            hasChannelData = true;
        }
        
        card->updateStatus(online, ageMs, totalCurrent, channels);
    }
    
    // 添加弹性空间
    cardsLayout_->setRowStretch(row + 1, 1);
    
    // 如果device.list没有返回通道数据，则额外调用relay.statusAll刷新状态
    // 这是为了兼容旧版RPC服务器或使用relay.nodes回退时的情况
    if (!hasChannelData) {
        qDebug() << "[DEVICE_WIDGET] device.list未返回通道数据，执行额外的状态刷新";
        QTimer::singleShot(kFallbackRefreshDelayMs, this, &DeviceWidget::refreshDeviceStatus);
    }
}

void DeviceWidget::updateDeviceCardStatus(int nodeId, const QJsonObject &status)
{
    for (DeviceCard *card : deviceCards_) {
        if (card->nodeId() == nodeId) {
            bool online = status.value(QStringLiteral("online")).toBool();
            qint64 ageMs = static_cast<qint64>(status.value(QStringLiteral("ageMs")).toDouble(-1));
            double totalCurrent = status.value(QStringLiteral("totalCurrent")).toDouble(0);
            QJsonObject channels = status.value(QStringLiteral("channels")).toObject();

            card->updateStatus(online, ageMs, totalCurrent, channels);
            break;
        }
    }
}
