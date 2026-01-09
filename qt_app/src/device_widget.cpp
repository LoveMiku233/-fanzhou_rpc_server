/**
 * @file device_widget.cpp
 * @brief 设备管理页面实现 - 网格卡片布局（一行两个）
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
#include <QInputDialog>
#include <QSpinBox>
#include <QLineEdit>
#include <QFormLayout>
#include <QDialog>
#include <QDialogButtonBox>

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
        "  border-radius: 10px;"
        "  padding: 10px;"
        "}"
        "#deviceCard:hover {"
        "  border-color: #27ae60;"
        "  background-color: #f8f9fa;"
        "}"));
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(100);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(6);

    // 顶部行：名称和节点ID
    QHBoxLayout *topRow = new QHBoxLayout();
    
    nameLabel_ = new QLabel(name_, this);
    nameLabel_->setStyleSheet(QStringLiteral(
        "font-size: 15px; font-weight: bold; color: #2c3e50;"));
    topRow->addWidget(nameLabel_);
    
    topRow->addStretch();
    
    nodeIdLabel_ = new QLabel(QStringLiteral("#%1").arg(nodeId_), this);
    nodeIdLabel_->setStyleSheet(QStringLiteral(
        "font-size: 12px; color: #7f8c8d; background-color: #ecf0f1; "
        "padding: 3px 8px; border-radius: 4px;"));
    topRow->addWidget(nodeIdLabel_);
    
    mainLayout->addLayout(topRow);

    // 中间行：状态和电流
    QHBoxLayout *middleRow = new QHBoxLayout();
    
    statusLabel_ = new QLabel(QStringLiteral("等待中..."), this);
    statusLabel_->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: bold;"));
    middleRow->addWidget(statusLabel_);
    
    middleRow->addStretch();
    
    currentLabel_ = new QLabel(QStringLiteral("-- mA"), this);
    currentLabel_->setStyleSheet(QStringLiteral(
        "font-size: 13px; color: #3498db; font-weight: bold;"));
    middleRow->addWidget(currentLabel_);
    
    mainLayout->addLayout(middleRow);

    // 底部行：通道状态
    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(6);
    
    ch0Label_ = new QLabel(QStringLiteral("0:--"), this);
    ch0Label_->setStyleSheet(QStringLiteral(
        "font-size: 12px; padding: 3px 6px; background-color: #f5f5f5; border-radius: 4px;"));
    bottomRow->addWidget(ch0Label_);
    
    ch1Label_ = new QLabel(QStringLiteral("1:--"), this);
    ch1Label_->setStyleSheet(QStringLiteral(
        "font-size: 12px; padding: 3px 6px; background-color: #f5f5f5; border-radius: 4px;"));
    bottomRow->addWidget(ch1Label_);
    
    ch2Label_ = new QLabel(QStringLiteral("2:--"), this);
    ch2Label_->setStyleSheet(QStringLiteral(
        "font-size: 12px; padding: 3px 6px; background-color: #f5f5f5; border-radius: 4px;"));
    bottomRow->addWidget(ch2Label_);
    
    ch3Label_ = new QLabel(QStringLiteral("3:--"), this);
    ch3Label_->setStyleSheet(QStringLiteral(
        "font-size: 12px; padding: 3px 6px; background-color: #f5f5f5; border-radius: 4px;"));
    bottomRow->addWidget(ch3Label_);
    
    bottomRow->addStretch();
    
    mainLayout->addLayout(bottomRow);
}

void DeviceCard::updateStatus(bool online, qint64 ageMs, double totalCurrent, const QJsonObject &channels)
{
    // 更新在线状态
    if (online) {
        statusLabel_->setText(QStringLiteral("在线 (%1ms)").arg(ageMs));
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: 13px; font-weight: bold; color: #27ae60;"));
    } else if (ageMs < 0) {
        statusLabel_->setText(QStringLiteral("无响应"));
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: 13px; font-weight: bold; color: #f39c12;"));
    } else {
        statusLabel_->setText(QStringLiteral("离线 (%1s)").arg(ageMs / 1000));
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: 13px; font-weight: bold; color: #e74c3c;"));
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
                "font-size: 12px; padding: 3px 6px; background-color: %1; border-radius: 4px;")
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
    , addDeviceButton_(nullptr)
    , cardsContainer_(nullptr)
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
        "font-size: 18px; font-weight: bold; color: #2c3e50; padding: 4px 0;"));
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

    addDeviceButton_ = new QPushButton(QStringLiteral("添加设备"), this);
    addDeviceButton_->setProperty("type", QStringLiteral("warning"));
    addDeviceButton_->setMinimumHeight(36);
    connect(addDeviceButton_, &QPushButton::clicked, this, &DeviceWidget::onAddDeviceClicked);
    toolbarLayout->addWidget(addDeviceButton_);

    toolbarLayout->addStretch();

    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 12px;"));
    toolbarLayout->addWidget(statusLabel_);

    mainLayout->addLayout(toolbarLayout);

    // 设备卡片容器 - 使用网格布局（一行两个）
    cardsContainer_ = new QWidget(this);
    cardsLayout_ = new QGridLayout(cardsContainer_);
    cardsLayout_->setContentsMargins(0, 0, 0, 0);
    cardsLayout_->setSpacing(8);
    cardsLayout_->setColumnStretch(0, 1);
    cardsLayout_->setColumnStretch(1, 1);

    mainLayout->addWidget(cardsContainer_, 1);

    // 提示文本
    QLabel *helpLabel = new QLabel(
        QStringLiteral("点击设备卡片可打开控制面板"),
        this);
    helpLabel->setStyleSheet(QStringLiteral(
        "color: #7f8c8d; font-size: 11px; padding: 4px;"));
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

    // 使用device.list获取设备列表（包含设备名称）
    QJsonValue result = rpcClient_->call(QStringLiteral("device.list"));

    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.contains(QStringLiteral("devices"))) {
            QJsonArray devices = obj.value(QStringLiteral("devices")).toArray();
            updateDeviceCards(devices);
            statusLabel_->setText(QStringLiteral("共 %1 个设备").arg(devices.size()));
            emit logMessage(QStringLiteral("刷新设备列表成功，共 %1 个设备").arg(devices.size()));
            return;
        }
    }

    // 如果device.list失败，回退使用relay.nodes
    result = rpcClient_->call(QStringLiteral("relay.nodes"));

    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.contains(QStringLiteral("nodes"))) {
            QJsonArray nodes = obj.value(QStringLiteral("nodes")).toArray();
            // 转换为device格式
            QJsonArray devices;
            for (const QJsonValue &val : nodes) {
                QJsonObject node = val.toObject();
                QJsonObject device;
                device[QStringLiteral("nodeId")] = node.value(QStringLiteral("node")).toInt();
                device[QStringLiteral("name")] = QStringLiteral("继电器-%1").arg(node.value(QStringLiteral("node")).toInt());
                device[QStringLiteral("online")] = node.value(QStringLiteral("online")).toBool();
                devices.append(device);
            }
            updateDeviceCards(devices);
            statusLabel_->setText(QStringLiteral("共 %1 个设备").arg(devices.size()));
            emit logMessage(QStringLiteral("刷新设备列表成功，共 %1 个设备").arg(devices.size()));
            return;
        }
    }

    if (result.isArray()) {
        QJsonArray nodes = result.toArray();
        QJsonArray devices;
        for (const QJsonValue &val : nodes) {
            QJsonObject node = val.toObject();
            QJsonObject device;
            device[QStringLiteral("nodeId")] = node.value(QStringLiteral("node")).toInt();
            device[QStringLiteral("name")] = QStringLiteral("继电器-%1").arg(node.value(QStringLiteral("node")).toInt());
            device[QStringLiteral("online")] = node.value(QStringLiteral("online")).toBool();
            devices.append(device);
        }
        updateDeviceCards(devices);
        statusLabel_->setText(QStringLiteral("共 %1 个设备").arg(devices.size()));
        emit logMessage(QStringLiteral("刷新设备列表成功，共 %1 个设备").arg(devices.size()));
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

void DeviceWidget::onAddDeviceClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    // 创建添加设备对话框
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("添加设备"));
    dialog.setMinimumWidth(300);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QFormLayout *formLayout = new QFormLayout();
    
    QSpinBox *nodeIdSpinBox = new QSpinBox(&dialog);
    nodeIdSpinBox->setRange(1, 255);
    nodeIdSpinBox->setValue(1);
    nodeIdSpinBox->setMinimumHeight(36);
    formLayout->addRow(QStringLiteral("节点ID:"), nodeIdSpinBox);
    
    QLineEdit *nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText(QStringLiteral("设备名称"));
    nameEdit->setMinimumHeight(36);
    formLayout->addRow(QStringLiteral("名称:"), nameEdit);
    
    QSpinBox *typeSpinBox = new QSpinBox(&dialog);
    typeSpinBox->setRange(1, 100);
    typeSpinBox->setValue(1);  // 默认继电器类型
    typeSpinBox->setMinimumHeight(36);
    formLayout->addRow(QStringLiteral("设备类型:"), typeSpinBox);
    
    layout->addLayout(formLayout);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("添加"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
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
    
    QJsonValue result = rpcClient_->call(QStringLiteral("device.add"), params);
    
    if (result.isObject() && result.toObject().value(QStringLiteral("ok")).toBool()) {
        QMessageBox::information(this, QStringLiteral("成功"), 
            QStringLiteral("设备 %1 添加成功！").arg(name));
        emit logMessage(QStringLiteral("添加设备成功: %1").arg(name));
        refreshDeviceList();
    } else {
        QString error = result.toObject().value(QStringLiteral("error")).toString();
        QMessageBox::warning(this, QStringLiteral("错误"), 
            QStringLiteral("添加设备失败: %1").arg(error));
    }
}

void DeviceWidget::updateDeviceCards(const QJsonArray &devices)
{
    clearDeviceCards();

    int row = 0;
    int col = 0;
    
    for (const QJsonValue &value : devices) {
        QJsonObject device = value.toObject();

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

        // 设置初始状态
        bool online = device.value(QStringLiteral("online")).toBool();
        card->updateStatus(online, -1, 0, QJsonObject());
    }
    
    // 添加弹性空间
    cardsLayout_->setRowStretch(row + 1, 1);
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
