/**
 * @file device_widget.cpp
 * @brief 设备管理页面实现 - 卡片式布局
 */

#include "device_widget.h"
#include "rpc_client.h"
#include "relay_control_dialog.h"

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
    setStyleSheet(QStringLiteral(
        "#deviceCard {"
        "  background-color: white;"
        "  border: 1px solid #d0d5dd;"
        "  border-radius: 8px;"
        "  padding: 8px;"
        "}"
        "#deviceCard:hover {"
        "  border-color: #3498db;"
        "  background-color: #f8f9fa;"
        "}"));
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(90);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(4);

    // 顶部行：名称和节点ID
    QHBoxLayout *topRow = new QHBoxLayout();
    
    nameLabel_ = new QLabel(name_, this);
    nameLabel_->setStyleSheet(QStringLiteral(
        "font-size: 13px; font-weight: bold; color: #2c3e50;"));
    topRow->addWidget(nameLabel_);
    
    topRow->addStretch();
    
    nodeIdLabel_ = new QLabel(QStringLiteral("#%1").arg(nodeId_), this);
    nodeIdLabel_->setStyleSheet(QStringLiteral(
        "font-size: 10px; color: #7f8c8d; background-color: #ecf0f1; "
        "padding: 2px 6px; border-radius: 3px;"));
    topRow->addWidget(nodeIdLabel_);
    
    mainLayout->addLayout(topRow);

    // 中间行：状态和电流
    QHBoxLayout *middleRow = new QHBoxLayout();
    
    statusLabel_ = new QLabel(QStringLiteral("[...] 等待中"), this);
    statusLabel_->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold;"));
    middleRow->addWidget(statusLabel_);
    
    middleRow->addStretch();
    
    currentLabel_ = new QLabel(QStringLiteral("电流: -- mA"), this);
    currentLabel_->setStyleSheet(QStringLiteral(
        "font-size: 11px; color: #3498db; font-weight: bold;"));
    middleRow->addWidget(currentLabel_);
    
    mainLayout->addLayout(middleRow);

    // 底部行：通道状态
    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(4);
    
    ch0Label_ = new QLabel(QStringLiteral("0:--"), this);
    ch0Label_->setStyleSheet(QStringLiteral(
        "font-size: 10px; padding: 2px 4px; background-color: #f5f5f5; border-radius: 3px;"));
    bottomRow->addWidget(ch0Label_);
    
    ch1Label_ = new QLabel(QStringLiteral("1:--"), this);
    ch1Label_->setStyleSheet(QStringLiteral(
        "font-size: 10px; padding: 2px 4px; background-color: #f5f5f5; border-radius: 3px;"));
    bottomRow->addWidget(ch1Label_);
    
    ch2Label_ = new QLabel(QStringLiteral("2:--"), this);
    ch2Label_->setStyleSheet(QStringLiteral(
        "font-size: 10px; padding: 2px 4px; background-color: #f5f5f5; border-radius: 3px;"));
    bottomRow->addWidget(ch2Label_);
    
    ch3Label_ = new QLabel(QStringLiteral("3:--"), this);
    ch3Label_->setStyleSheet(QStringLiteral(
        "font-size: 10px; padding: 2px 4px; background-color: #f5f5f5; border-radius: 3px;"));
    bottomRow->addWidget(ch3Label_);
    
    bottomRow->addStretch();
    
    mainLayout->addLayout(bottomRow);
}

void DeviceCard::updateStatus(bool online, qint64 ageMs, double totalCurrent, const QJsonObject &channels)
{
    // 更新在线状态 - 使用纯文本
    if (online) {
        statusLabel_->setText(QStringLiteral("[OK] %1ms").arg(ageMs));
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: 11px; font-weight: bold; color: #27ae60;"));
    } else if (ageMs < 0) {
        statusLabel_->setText(QStringLiteral("[!] 无响应"));
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: 11px; font-weight: bold; color: #f39c12;"));
    } else {
        statusLabel_->setText(QStringLiteral("[X] %1s").arg(ageMs / 1000));
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: 11px; font-weight: bold; color: #e74c3c;"));
    }

    // 更新总电流
    currentLabel_->setText(QStringLiteral("%1mA").arg(totalCurrent, 0, 'f', 1));

    // 更新通道状态
    QLabel *chLabels[] = {ch0Label_, ch1Label_, ch2Label_, ch3Label_};
    for (int ch = 0; ch < 4; ++ch) {
        QString chKey = QString::number(ch);
        if (channels.contains(chKey)) {
            QJsonObject chStatus = channels.value(chKey).toObject();
            int mode = chStatus.value(QStringLiteral("mode")).toInt(0);

            QString modeText;
            QString bgColor;
            switch (mode) {
                case 0: modeText = QStringLiteral("停"); bgColor = QStringLiteral("#f5f5f5"); break;
                case 1: modeText = QStringLiteral("正"); bgColor = QStringLiteral("#d4edda"); break;
                case 2: modeText = QStringLiteral("反"); bgColor = QStringLiteral("#fff3cd"); break;
                default: modeText = QStringLiteral("?"); bgColor = QStringLiteral("#f5f5f5"); break;
            }

            chLabels[ch]->setText(QStringLiteral("%1:%2").arg(ch).arg(modeText));
            chLabels[ch]->setStyleSheet(QStringLiteral(
                "font-size: 10px; padding: 2px 4px; background-color: %1; border-radius: 3px;")
                .arg(bgColor));
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
    , cardsLayout_(nullptr)
{
    setupUi();
}

void DeviceWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // 页面标题 - 使用纯文本
    QLabel *titleLabel = new QLabel(QStringLiteral("[D] 设备管理"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: 16px; font-weight: bold; color: #2c3e50; padding: 4px 0;"));
    mainLayout->addWidget(titleLabel);

    // 工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(8);

    refreshButton_ = new QPushButton(QStringLiteral("刷新设备"), this);
    refreshButton_->setMinimumHeight(36);
    connect(refreshButton_, &QPushButton::clicked, this, &DeviceWidget::refreshDeviceList);
    toolbarLayout->addWidget(refreshButton_);

    queryAllButton_ = new QPushButton(QStringLiteral("查询全部"), this);
    queryAllButton_->setProperty("type", QStringLiteral("success"));
    queryAllButton_->setMinimumHeight(36);
    connect(queryAllButton_, &QPushButton::clicked, this, &DeviceWidget::onQueryAllClicked);
    toolbarLayout->addWidget(queryAllButton_);

    toolbarLayout->addStretch();

    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 11px;"));
    toolbarLayout->addWidget(statusLabel_);

    mainLayout->addLayout(toolbarLayout);

    // 设备卡片容器
    QWidget *cardsContainer = new QWidget(this);
    cardsLayout_ = new QVBoxLayout(cardsContainer);
    cardsLayout_->setContentsMargins(0, 0, 0, 0);
    cardsLayout_->setSpacing(6);
    cardsLayout_->addStretch();

    mainLayout->addWidget(cardsContainer, 1);

    // 提示文本
    QLabel *helpLabel = new QLabel(
        QStringLiteral("点击设备卡片可打开控制面板"),
        this);
    helpLabel->setStyleSheet(QStringLiteral(
        "color: #7f8c8d; font-size: 10px; padding: 4px;"));
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
        statusLabel_->setText(QStringLiteral("[!] 未连接"));
        emit logMessage(QStringLiteral("刷新设备失败：未连接服务器"), QStringLiteral("WARN"));
        return;
    }

    statusLabel_->setText(QStringLiteral("正在刷新..."));

    QJsonValue result = rpcClient_->call(QStringLiteral("relay.nodes"));

    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.contains(QStringLiteral("nodes"))) {
            QJsonArray nodes = obj.value(QStringLiteral("nodes")).toArray();
            updateDeviceCards(nodes);
            statusLabel_->setText(QStringLiteral("共 %1 个设备").arg(nodes.size()));
            emit logMessage(QStringLiteral("刷新设备列表成功，共 %1 个设备").arg(nodes.size()));
            return;
        }
    }

    if (result.isArray()) {
        QJsonArray nodes = result.toArray();
        updateDeviceCards(nodes);
        statusLabel_->setText(QStringLiteral("共 %1 个设备").arg(nodes.size()));
        emit logMessage(QStringLiteral("刷新设备列表成功，共 %1 个设备").arg(nodes.size()));
        return;
    }

    statusLabel_->setText(QStringLiteral("[X] 获取失败"));
    emit logMessage(QStringLiteral("获取设备列表失败"), QStringLiteral("ERROR"));
}

void DeviceWidget::refreshDeviceStatus()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        return;
    }

    for (DeviceCard *card : deviceCards_) {
        int nodeId = card->nodeId();

        QJsonObject params;
        params[QStringLiteral("node")] = nodeId;

        QJsonValue result = rpcClient_->call(QStringLiteral("relay.statusAll"), params);
        if (result.isObject()) {
            updateDeviceCardStatus(nodeId, result.toObject());
        }
    }
}

void DeviceWidget::onQueryAllClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    statusLabel_->setText(QStringLiteral("正在查询所有设备..."));

    QJsonValue result = rpcClient_->call(QStringLiteral("relay.queryAll"));

    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.value(QStringLiteral("ok")).toBool()) {
            int queried = obj.value(QStringLiteral("queriedDevices")).toInt();
            statusLabel_->setText(QStringLiteral("已查询 %1 个设备").arg(queried));
            emit logMessage(QStringLiteral("查询所有设备成功，共 %1 个设备").arg(queried));

            // 延迟后刷新状态
            QTimer::singleShot(500, this, &DeviceWidget::refreshDeviceStatus);
            return;
        }
    }

    statusLabel_->setText(QStringLiteral("[X] 查询失败"));
    emit logMessage(QStringLiteral("查询所有设备失败"), QStringLiteral("ERROR"));
}

void DeviceWidget::onDeviceCardClicked(int nodeId, const QString &name)
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    RelayControlDialog *dialog = new RelayControlDialog(rpcClient_, nodeId, name, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &RelayControlDialog::controlExecuted, this, [this](const QString &message) {
        emit logMessage(message);
    });
    connect(dialog, &QDialog::finished, this, [this, nodeId]() {
        // 刷新设备状态
        QJsonObject params;
        params[QStringLiteral("node")] = nodeId;
        QJsonValue result = rpcClient_->call(QStringLiteral("relay.statusAll"), params);
        if (result.isObject()) {
            updateDeviceCardStatus(nodeId, result.toObject());
        }
    });
    dialog->exec();
}

void DeviceWidget::updateDeviceCards(const QJsonArray &devices)
{
    clearDeviceCards();

    for (const QJsonValue &value : devices) {
        QJsonObject device = value.toObject();

        int nodeId = device.value(QStringLiteral("nodeId")).toInt();
        QString name = device.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            name = QStringLiteral("继电器-%1").arg(nodeId);
        }

        DeviceCard *card = new DeviceCard(nodeId, name, this);
        connect(card, &DeviceCard::clicked, this, &DeviceWidget::onDeviceCardClicked);

        // 插入到 stretch 之前
        cardsLayout_->insertWidget(cardsLayout_->count() - 1, card);
        deviceCards_.append(card);

        // 设置初始状态
        bool online = device.value(QStringLiteral("online")).toBool();
        card->updateStatus(online, -1, 0, QJsonObject());
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
