/**
 * @file sensor_widget.cpp
 * @brief 传感器数据监控页面实现 - 增强版
 */

#include "sensor_widget.h"
#include "rpc_client.h"

#include <QVBoxLayout>
#include <QScroller>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QDateTime>
#include <QTimer>
#include <QPropertyAnimation>

// ==================== SensorCard Implementation ====================

SensorCard::SensorCard(int nodeId, const QString &name, const QString &typeName, QWidget *parent)
    : QFrame(parent)
    , nodeId_(nodeId)
    , name_(name)
    , typeName_(typeName)
    , m_valueOpacity(1.0)
    , m_updating(false)
    , m_lastValue(0.0)
    , nameLabel_(nullptr)
    , typeLabel_(nullptr)
    , valueLabel_(nullptr)
    , unitLabel_(nullptr)
    , statusLabel_(nullptr)
    , detailLabel_(nullptr)
    , paramsLabel_(nullptr)
    , valueContainer_(nullptr)
    , m_valueAnimation(nullptr)
{
    setupUi();
}

SensorCard::~SensorCard()
{
}

void SensorCard::setupUi()
{
    setObjectName(QStringLiteral("sensorCard"));
    setFrameShape(QFrame::NoFrame);
    setMinimumHeight(200);
    setMaximumWidth(400);
    setCursor(Qt::PointingHandCursor);
    
    // 设置卡片样式
    setStyleSheet(QStringLiteral(
        "#sensorCard {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #f8f9fa);"
        "  border: 2px solid #e8e8e8;"
        "  border-radius: 16px;"
        "}"
    ));
    
    // 阴影效果
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(15);
    shadow->setColor(QColor(0, 0, 0, 40));
    shadow->setOffset(0, 4);
    setGraphicsEffect(shadow);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);
    
    // 顶部：名称和类型
    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->setSpacing(8);
    
    nameLabel_ = new QLabel(name_.isEmpty() ? QStringLiteral("传感器-%1").arg(nodeId_) : name_, this);
    nameLabel_->setStyleSheet(QStringLiteral(
        "font-size: 16px; font-weight: bold; color: #2c3e50;"));
    topRow->addWidget(nameLabel_);
    
    topRow->addStretch();
    
    // 类型标签带颜色
    QString typeDisplay = typeName_;
    QString typeBgColor = QStringLiteral("#9b59b6");
    
    if (typeName_.contains(QStringLiteral("temp"), Qt::CaseInsensitive)) {
        typeBgColor = QStringLiteral("#e74c3c");
        typeDisplay = QStringLiteral("[温] 温度");
    } else if (typeName_.contains(QStringLiteral("humid"), Qt::CaseInsensitive)) {
        typeBgColor = QStringLiteral("#3498db");
        typeDisplay = QStringLiteral("[湿] 湿度");
    } else if (typeName_.contains(QStringLiteral("light"), Qt::CaseInsensitive)) {
        typeBgColor = QStringLiteral("#f39c12");
        typeDisplay = QStringLiteral("[光] 光照");
    } else if (typeName_.contains(QStringLiteral("soil"), Qt::CaseInsensitive)) {
        typeBgColor = QStringLiteral("#795548");
        typeDisplay = QStringLiteral("[土] 土壤");
    } else if (typeName_.contains(QStringLiteral("co2"), Qt::CaseInsensitive)) {
        typeBgColor = QStringLiteral("#607d8b");
        typeDisplay = QStringLiteral("[CO2] CO2");
    } else if (typeName_.contains(QStringLiteral("ph"), Qt::CaseInsensitive)) {
        typeBgColor = QStringLiteral("#8bc34a");
        typeDisplay = QStringLiteral("[pH] pH");
    }
    
    typeLabel_ = new QLabel(typeDisplay, this);
    typeLabel_->setStyleSheet(QStringLiteral(
        "font-size: 11px; color: white; background-color: %1; "
        "padding: 5px 12px; border-radius: 12px; font-weight: bold;").arg(typeBgColor));
    topRow->addWidget(typeLabel_);
    
    mainLayout->addLayout(topRow);
    
    // 中部：数值显示（带背景容器）
    valueContainer_ = new QWidget(this);
    valueContainer_->setStyleSheet(QStringLiteral(
        "background-color: #f5f7fa; border-radius: 12px;"
    ));
    valueContainer_->setMinimumHeight(70);
    
    QHBoxLayout *valueRow = new QHBoxLayout(valueContainer_);
    valueRow->setSpacing(6);
    valueRow->setContentsMargins(12, 8, 12, 8);
    
    valueLabel_ = new QLabel(QStringLiteral("--.-"), this);
    valueLabel_->setStyleSheet(QStringLiteral(
        "font-size: 42px; font-weight: bold; color: #3498db; background: transparent;"));
    valueRow->addWidget(valueLabel_);
    
    unitLabel_ = new QLabel(QStringLiteral("--"), this);
    unitLabel_->setStyleSheet(QStringLiteral(
        "font-size: 14px; color: #7f8c8d; padding-top: 16px; background: transparent;"));
    valueRow->addWidget(unitLabel_);
    valueRow->addStretch();
    
    mainLayout->addWidget(valueContainer_);
    
    // 状态标签
    statusLabel_ = new QLabel(QStringLiteral("[等] 等待数据..."), this);
    statusLabel_->setStyleSheet(QStringLiteral(
        "font-size: 12px; color: #95a5a6; padding: 6px 10px; "
        "background-color: #ecf0f1; border-radius: 6px;"));
    statusLabel_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(statusLabel_);
    
    // 参数信息（可折叠区域）
    paramsLabel_ = new QLabel(QStringLiteral("ID: %1 | 点击刷新查看参数").arg(nodeId_), this);
    paramsLabel_->setStyleSheet(QStringLiteral(
        "font-size: 11px; color: #7f8c8d; padding: 8px; "
        "background-color: #f8f9fa; border-radius: 6px;"));
    paramsLabel_->setWordWrap(true);
    paramsLabel_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(paramsLabel_);
    
    // 初始化数值动画
    m_valueAnimation = new QPropertyAnimation(this, "valueOpacity", this);
    m_valueAnimation->setDuration(300);
    m_valueAnimation->setStartValue(0.5);
    m_valueAnimation->setEndValue(1.0);
    m_valueAnimation->setEasingCurve(QEasingCurve::OutCubic);
}

void SensorCard::updateValue(double value, const QString &unit, bool valid, const QString &status)
{
    if (valid) {
        // 数值变化动画效果
        if (m_valueAnimation && value != m_lastValue) {
            m_valueAnimation->stop();
            m_valueAnimation->start();
        }
        m_lastValue = value;
        
        // 格式化数值显示
        QString valueStr;
        if (qAbs(value) >= 1000) {
            valueStr = QString::number(value, 'f', 1);
        } else if (qAbs(value) >= 100) {
            valueStr = QString::number(value, 'f', 1);
        } else {
            valueStr = QString::number(value, 'f', 2);
        }
        
        valueLabel_->setText(valueStr);
        unitLabel_->setText(unit);
        
        // 根据数值范围设置颜色
        QString valueColor = QStringLiteral("#27ae60");  // 默认绿色
        if (typeName_.contains(QStringLiteral("temp"), Qt::CaseInsensitive)) {
            if (value > 35) valueColor = QStringLiteral("#e74c3c");  // 红色-高温
            else if (value > 28) valueColor = QStringLiteral("#f39c12");  // 橙色-较热
            else if (value < 10) valueColor = QStringLiteral("#3498db");  // 蓝色-低温
        } else if (typeName_.contains(QStringLiteral("humid"), Qt::CaseInsensitive)) {
            if (value > 80) valueColor = QStringLiteral("#3498db");  // 蓝色-高湿
            else if (value < 30) valueColor = QStringLiteral("#f39c12");  // 橙色-低湿
        }
        
        valueLabel_->setStyleSheet(QStringLiteral(
            "font-size: 42px; font-weight: bold; color: %1; background: transparent;").arg(valueColor));
        
        // 状态文本
        QString statusText = status.isEmpty() ? QStringLiteral("[OK] 数据正常") : status;
        statusLabel_->setText(statusText);
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: 12px; color: #27ae60; padding: 6px 10px; "
            "background-color: #d4edda; border-radius: 6px; font-weight: bold;"));
        
        // 数值容器背景色
        valueContainer_->setStyleSheet(QStringLiteral(
            "background-color: #eafaf1; border-radius: 12px;"));
        
    } else {
        valueLabel_->setText(QStringLiteral("--.-"));
        unitLabel_->setText(unit);
        valueLabel_->setStyleSheet(QStringLiteral(
            "font-size: 42px; font-weight: bold; color: #e74c3c; background: transparent;"));
        
        QString statusText = status.isEmpty() ? QStringLiteral("[X] 无数据") : status;
        statusLabel_->setText(statusText);
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: 12px; color: #e74c3c; padding: 6px 10px; "
            "background-color: #f8d7da; border-radius: 6px; font-weight: bold;"));
        
        valueContainer_->setStyleSheet(QStringLiteral(
            "background-color: #fdf2f2; border-radius: 12px;"));
    }
}

void SensorCard::updateData(const QJsonObject &data)
{
    QString commType = data.value(QStringLiteral("commTypeName")).toString();
    QString bus = data.value(QStringLiteral("bus")).toString();
    QString addr = data.value(QStringLiteral("addr")).toString();
    
    QString detailText = QStringLiteral("[ID] ID:%1").arg(nodeId_);
    if (!commType.isEmpty()) detailText += QStringLiteral(" | [通] %1").arg(commType);
    if (!bus.isEmpty()) detailText += QStringLiteral(" | [线] %1").arg(bus);
    if (!addr.isEmpty()) detailText += QStringLiteral(" | [址] %1").arg(addr);
    
    detailLabel_->setText(detailText);
    
    qDebug() << "[SENSOR_CARD] 更新传感器" << nodeId_ << "数据:" << detailText;
}

void SensorCard::updateParams(const QJsonObject &params)
{
    QStringList paramList;
    
    // 提取常见传感器参数
    if (params.contains(QStringLiteral("rangeMin")) && params.contains(QStringLiteral("rangeMax"))) {
        double min = params.value(QStringLiteral("rangeMin")).toDouble();
        double max = params.value(QStringLiteral("rangeMax")).toDouble();
        paramList << QStringLiteral("[程] 量程: %1~%2").arg(min).arg(max);
    }
    
    if (params.contains(QStringLiteral("precision"))) {
        double prec = params.value(QStringLiteral("precision")).toDouble();
        paramList << QStringLiteral("[精] 精度: ±%1").arg(prec);
    }
    
    if (params.contains(QStringLiteral("resolution"))) {
        double res = params.value(QStringLiteral("resolution")).toDouble();
        paramList << QStringLiteral("[辨] 分辨率: %1").arg(res);
    }
    
    if (params.contains(QStringLiteral("samplingRate"))) {
        int rate = params.value(QStringLiteral("samplingRate")).toInt();
        paramList << QStringLiteral("[采] 采样率: %1Hz").arg(rate);
    }
    
    if (params.contains(QStringLiteral("calibrationDate"))) {
        QString calDate = params.value(QStringLiteral("calibrationDate")).toString();
        paramList << QStringLiteral("[校] 校准: %1").arg(calDate);
    }
    
    if (paramList.isEmpty()) {
        paramsLabel_->setText(QStringLiteral("[表] 暂无参数信息"));
    } else {
        paramsLabel_->setText(paramList.join(QStringLiteral(" | ")));
    }
    
    paramsLabel_->setStyleSheet(QStringLiteral(
        "font-size: 11px; color: #5d6d7e; padding: 10px; "
        "background-color: #eaf2f8; border-radius: 8px;"));
}

void SensorCard::setUpdating(bool updating)
{
    m_updating = updating;
    if (updating) {
        statusLabel_->setText(QStringLiteral("[新] 更新中..."));
        statusLabel_->setStyleSheet(QStringLiteral(
            "font-size: 12px; color: #3498db; padding: 6px 10px; "
            "background-color: #d6eaf8; border-radius: 6px; font-weight: bold;"));
    }
    update();
}

void SensorCard::setValueOpacity(qreal opacity)
{
    m_valueOpacity = opacity;
    update();
}

void SensorCard::updateValueStyle()
{
    // 数值闪烁效果
}

void SensorCard::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制悬停效果
    if (underMouse()) {
        painter.setPen(QPen(QColor(52, 152, 219, 100), 2));
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 16, 16);
    }
    
    // 绘制更新指示器
    if (m_updating) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(52, 152, 219, 30));
        painter.drawRoundedRect(rect().adjusted(4, 4, -4, -4), 12, 12);
    }
    
    QFrame::paintEvent(event);
}

void SensorCard::enterEvent(QEvent *event)
{
    QFrame::enterEvent(event);
    // 悬停时提升效果
    QGraphicsEffect *effect = this->graphicsEffect();
    QGraphicsDropShadowEffect *shadow = qobject_cast<QGraphicsDropShadowEffect*>(effect);
    if (shadow) {
        shadow->setBlurRadius(25);
        shadow->setColor(QColor(0, 0, 0, 60));
        shadow->setOffset(0, 6);
    }
}

void SensorCard::leaveEvent(QEvent *event)
{
    QFrame::leaveEvent(event);
    // 恢复阴影
    QGraphicsEffect *effect = this->graphicsEffect();
    QGraphicsDropShadowEffect *shadow = qobject_cast<QGraphicsDropShadowEffect*>(effect);
    if (shadow) {
        shadow->setBlurRadius(15);
        shadow->setColor(QColor(0, 0, 0, 40));
        shadow->setOffset(0, 4);
    }
}

// ==================== SensorWidget Implementation ====================

SensorWidget::SensorWidget(RpcClient *rpcClient, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpcClient)
    , statusLabel_(nullptr)
    , autoRefreshBtn_(nullptr)
    , refreshBtn_(nullptr)
    , lastUpdateLabel_(nullptr)
    , refreshTimer_(new QTimer(this))
    , cardsContainer_(nullptr)
    , cardsLayout_(nullptr)
    , autoRefresh_(false)
    , refreshCount_(0)
{
    setupUi();
    
    connect(refreshTimer_, &QTimer::timeout, this, &SensorWidget::onAutoRefreshTimeout);
    
    qDebug() << "[SENSOR_WIDGET] 传感器页面初始化完成";
}

void SensorWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(16);
    
    // 标题栏 - 美化
    QHBoxLayout *titleLayout = new QHBoxLayout();
    
    QLabel *titleLabel = new QLabel(QStringLiteral("[感] 传感器监控"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: 26px; font-weight: bold; color: #2c3e50;"));
    titleLayout->addWidget(titleLabel);
    
    titleLayout->addStretch();
    
    // 自动刷新按钮
    autoRefreshBtn_ = new QPushButton(QStringLiteral("[自] 自动刷新: 关"), this);
    autoRefreshBtn_->setCheckable(true);
    autoRefreshBtn_->setFixedHeight(44);
    autoRefreshBtn_->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #95a5a6; color: white; border: none; "
        "border-radius: 10px; padding: 0 24px; font-weight: bold; font-size: 13px; }"
        "QPushButton:checked { background-color: #27ae60; }"
        "QPushButton:hover { opacity: 0.9; }"));
    connect(autoRefreshBtn_, &QPushButton::toggled, this, &SensorWidget::onAutoRefreshToggled);
    titleLayout->addWidget(autoRefreshBtn_);
    
    // 刷新按钮
    refreshBtn_ = new QPushButton(QStringLiteral("[刷] 刷新"), this);
    refreshBtn_->setFixedHeight(44);
    refreshBtn_->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #3498db; color: white; border: none; "
        "border-radius: 10px; padding: 0 28px; font-weight: bold; font-size: 13px; }"
        "QPushButton:hover { background-color: #2980b9; }"
        "QPushButton:pressed { background-color: #1c5a8a; }"));
    connect(refreshBtn_, &QPushButton::clicked, this, &SensorWidget::onRefreshClicked);
    titleLayout->addWidget(refreshBtn_);
    
    mainLayout->addLayout(titleLayout);
    
    // 状态栏 - 美化
    QHBoxLayout *statusLayout = new QHBoxLayout();
    
    statusLabel_ = new QLabel(QStringLiteral("[等] 准备就绪，等待连接..."), this);
    statusLabel_->setStyleSheet(QStringLiteral(
        "color: #5d6d7e; font-size: 14px; padding: 10px 16px; "
        "background-color: #f8f9fa; border-radius: 8px; font-weight: 500;"));
    statusLayout->addWidget(statusLabel_);
    
    lastUpdateLabel_ = new QLabel(QStringLiteral("--:--:--"), this);
    lastUpdateLabel_->setStyleSheet(QStringLiteral(
        "color: #7f8c8d; font-size: 13px; padding: 10px 16px;"));
    statusLayout->addWidget(lastUpdateLabel_);
    
    mainLayout->addLayout(statusLayout);
    
    // 滚动区域 - 美化滚动条
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { width: 10px; background: #f0f0f0; border-radius: 5px; margin: 4px; }"
        "QScrollBar::handle:vertical { background: #c0c0c0; border-radius: 5px; min-height: 40px; }"
        "QScrollBar::handle:vertical:hover { background: #a0a0a0; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ));
    
    // 启用触控滚动
    QScroller::grabGesture(scrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    
    // 卡片容器
    cardsContainer_ = new QWidget();
    cardsContainer_->setStyleSheet(QStringLiteral("background: transparent;"));
    cardsLayout_ = new QGridLayout(cardsContainer_);
    cardsLayout_->setContentsMargins(0, 0, 0, 0);
    cardsLayout_->setSpacing(20);
    cardsLayout_->setColumnStretch(0, 1);
    cardsLayout_->setColumnStretch(1, 1);
    
    scrollArea->setWidget(cardsContainer_);
    mainLayout->addWidget(scrollArea, 1);
    
    // 底部提示 - 美化
    QLabel *helpLabel = new QLabel(
        QStringLiteral("[示] 提示：绿色表示数据正常，红色表示无数据，橙色/蓝色表示数值偏高或偏低"),
        this);
    helpLabel->setStyleSheet(QStringLiteral(
        "color: #5d6d7e; font-size: 13px; padding: 12px; "
        "background-color: #eaf2f8; border-radius: 10px; font-weight: 500;"));
    helpLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(helpLabel);
}

void SensorWidget::clearSensorCards()
{
    while (!sensorCards_.isEmpty()) {
        SensorCard *card = sensorCards_.takeFirst();
        cardsLayout_->removeWidget(static_cast<QWidget*>(card));
        card->deleteLater();
    }
}

void SensorWidget::refreshSensorList()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        statusLabel_->setText(QStringLiteral("[X] 未连接服务器"));
        statusLabel_->setStyleSheet(QStringLiteral(
            "color: #e74c3c; font-size: 14px; padding: 10px 16px; "
            "background-color: #f8d7da; border-radius: 8px; font-weight: 500;"));
        qDebug() << "[SENSOR_WIDGET] 刷新失败：未连接服务器";
        return;
    }
    
    statusLabel_->setText(QStringLiteral("[载] 正在加载传感器列表..."));
    statusLabel_->setStyleSheet(QStringLiteral(
        "color: #3498db; font-size: 14px; padding: 10px 16px; "
        "background-color: #d6eaf8; border-radius: 8px; font-weight: 500;"));
    
    qDebug() << "[SENSOR_WIDGET] 正在请求传感器列表...";
    
    QJsonValue result = rpcClient_->call(QStringLiteral("sensor.list"));
    
    qDebug() << "[SENSOR_WIDGET] 收到sensor.list响应";
    
    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.value(QStringLiteral("ok")).toBool()) {
            QJsonArray sensors = obj.value(QStringLiteral("sensors")).toArray();
            sensorsCache_ = sensors;
            updateSensorCards(sensors);
            
            statusLabel_->setText(QStringLiteral("[OK] 共 %1 个传感器").arg(sensors.size()));
            statusLabel_->setStyleSheet(QStringLiteral(
                "color: #27ae60; font-size: 14px; padding: 10px 16px; "
                "background-color: #d4edda; border-radius: 8px; font-weight: 500;"));
            
            lastUpdateLabel_->setText(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")));
            
            emit logMessage(QStringLiteral("传感器列表刷新成功，共 %1 个").arg(sensors.size()));
            
            qDebug() << "[SENSOR_WIDGET] 传感器列表加载成功，数量:" << sensors.size();
            
            // 获取每个传感器的数据和参数
            for (const QJsonValue &v : sensors) {
                int nodeId = v.toObject().value(QStringLiteral("nodeId")).toInt();
                fetchSensorData(nodeId);
                fetchSensorParams(nodeId);
            }
            return;
        } else {
            QString error = obj.value(QStringLiteral("error")).toString();
            qDebug() << "[SENSOR_WIDGET] sensor.list返回错误:" << error;
        }
    }
    
    statusLabel_->setText(QStringLiteral("[X] 加载失败"));
    statusLabel_->setStyleSheet(QStringLiteral(
        "color: #e74c3c; font-size: 14px; padding: 10px 16px; "
        "background-color: #f8d7da; border-radius: 8px; font-weight: 500;"));
    emit logMessage(QStringLiteral("传感器列表加载失败"), QStringLiteral("ERROR"));
    qDebug() << "[SENSOR_WIDGET] 加载失败";
}

void SensorWidget::updateSensorCards(const QJsonArray &sensors)
{
    clearSensorCards();
    
    int row = 0;
    int col = 0;
    
    for (const QJsonValue &val : sensors) {
        QJsonObject s = val.toObject();
        int nodeId = s.value(QStringLiteral("nodeId")).toInt();
        QString name = s.value(QStringLiteral("name")).toString();
        QString typeName = s.value(QStringLiteral("typeName")).toString();
        
        SensorCard *card = new SensorCard(nodeId, name, typeName, cardsContainer_);
        card->updateData(s);
        
        // 设置默认单位
        QString unit = getSensorUnit(typeName);
        card->updateValue(0, unit, false);
        
        cardsLayout_->addWidget(card, row, col, Qt::AlignTop);
        sensorCards_.append(card);
        
        col++;
        if (col >= 2) {
            col = 0;
            row++;
        }
    }
    
    cardsLayout_->setRowStretch(row + 1, 1);
}

void SensorWidget::fetchSensorData(int nodeId)
{
    QJsonObject params;
    params[QStringLiteral("nodeId")] = nodeId;
    
    qDebug() << "[SENSOR_WIDGET] 请求传感器数据 nodeId=" << nodeId;
    
    // 查找对应的卡片并设置更新状态
    for (SensorCard *card : sensorCards_) {
        if (card->nodeId() == nodeId) {
            card->setUpdating(true);
            break;
        }
    }
    
    QJsonValue result = rpcClient_->call(QStringLiteral("sensor.read"), params);
    
    if (result.isObject()) {
        updateSensorCardData(nodeId, result.toObject());
    } else {
        qDebug() << "[SENSOR_WIDGET] 传感器" << nodeId << "数据获取失败";
    }
}

void SensorWidget::fetchSensorParams(int nodeId)
{
    QJsonObject params;
    params[QStringLiteral("nodeId")] = nodeId;
    
    qDebug() << "[SENSOR_WIDGET] 请求传感器参数 nodeId=" << nodeId;
    
    QJsonValue result = rpcClient_->call(QStringLiteral("sensor.getParams"), params);
    
    if (result.isObject()) {
        updateSensorCardParams(nodeId, result.toObject());
    }
}

void SensorWidget::updateSensorCardData(int nodeId, const QJsonObject &data)
{
    for (SensorCard *card : sensorCards_) {
        if (card->nodeId() == nodeId) {
            card->setUpdating(false);
            
            QString typeName = data.value(QStringLiteral("typeName")).toString();
            QString unit = getSensorUnit(typeName);
            
            // 获取传感器数值
            double value = 0;
            bool valid = false;
            QString status;
            
            if (data.contains(QStringLiteral("value"))) {
                value = data.value(QStringLiteral("value")).toDouble();
                valid = true;
            } else if (data.contains(QStringLiteral("temperature"))) {
                value = data.value(QStringLiteral("temperature")).toDouble();
                unit = QStringLiteral("°C");
                valid = true;
            } else if (data.contains(QStringLiteral("humidity"))) {
                value = data.value(QStringLiteral("humidity")).toDouble();
                unit = QStringLiteral("%");
                valid = true;
            } else if (data.contains(QStringLiteral("light"))) {
                value = data.value(QStringLiteral("light")).toDouble();
                unit = QStringLiteral("lux");
                valid = true;
            }
            
            // 获取状态信息
            if (data.contains(QStringLiteral("status"))) {
                int statusCode = data.value(QStringLiteral("status")).toInt();
                switch (statusCode) {
                    case 0: status = QStringLiteral("[OK] 正常"); break;
                    case 1: status = QStringLiteral("[警] 警告"); break;
                    case 2: status = QStringLiteral("[X] 错误"); break;
                    default: status = QStringLiteral("? 未知"); break;
                }
            }
            
            card->updateValue(value, unit, valid, status);
            card->updateData(data);
            
            qDebug() << "[SENSOR_WIDGET] 传感器" << nodeId << "数值更新:" << value << unit << "valid=" << valid;
            break;
        }
    }
}

void SensorWidget::updateSensorCardParams(int nodeId, const QJsonObject &params)
{
    for (SensorCard *card : sensorCards_) {
        if (card->nodeId() == nodeId) {
            card->updateParams(params);
            qDebug() << "[SENSOR_WIDGET] 传感器" << nodeId << "参数更新";
            break;
        }
    }
}

QString SensorWidget::getSensorTypeIcon(const QString &typeName) const
{
    if (typeName.contains(QStringLiteral("temp"), Qt::CaseInsensitive)) return QStringLiteral("[温]");
    if (typeName.contains(QStringLiteral("humid"), Qt::CaseInsensitive)) return QStringLiteral("[湿]");
    if (typeName.contains(QStringLiteral("light"), Qt::CaseInsensitive)) return QStringLiteral("[光]");
    if (typeName.contains(QStringLiteral("soil"), Qt::CaseInsensitive)) return QStringLiteral("[土]");
    if (typeName.contains(QStringLiteral("co2"), Qt::CaseInsensitive)) return QStringLiteral("[CO2]");
    if (typeName.contains(QStringLiteral("ph"), Qt::CaseInsensitive)) return QStringLiteral("[pH]");
    return QStringLiteral("[感]");
}

QString SensorWidget::getSensorUnit(const QString &typeName) const
{
    if (typeName.contains(QStringLiteral("temp"), Qt::CaseInsensitive)) return QStringLiteral("°C");
    if (typeName.contains(QStringLiteral("humid"), Qt::CaseInsensitive)) return QStringLiteral("%");
    if (typeName.contains(QStringLiteral("light"), Qt::CaseInsensitive)) return QStringLiteral("lux");
    if (typeName.contains(QStringLiteral("soil"), Qt::CaseInsensitive)) return QStringLiteral("%");
    if (typeName.contains(QStringLiteral("co2"), Qt::CaseInsensitive)) return QStringLiteral("ppm");
    if (typeName.contains(QStringLiteral("ph"), Qt::CaseInsensitive)) return QStringLiteral("pH");
    return QStringLiteral("");
}

QColor SensorWidget::getSensorColor(const QString &typeName) const
{
    if (typeName.contains(QStringLiteral("temp"), Qt::CaseInsensitive)) return QColor("#e74c3c");
    if (typeName.contains(QStringLiteral("humid"), Qt::CaseInsensitive)) return QColor("#3498db");
    if (typeName.contains(QStringLiteral("light"), Qt::CaseInsensitive)) return QColor("#f39c12");
    if (typeName.contains(QStringLiteral("soil"), Qt::CaseInsensitive)) return QColor("#795548");
    if (typeName.contains(QStringLiteral("co2"), Qt::CaseInsensitive)) return QColor("#607d8b");
    if (typeName.contains(QStringLiteral("ph"), Qt::CaseInsensitive)) return QColor("#8bc34a");
    return QColor("#9b59b6");
}

void SensorWidget::onRefreshClicked()
{
    qDebug() << "[SENSOR_WIDGET] 手动刷新按钮点击";
    refreshSensorList();
}

void SensorWidget::onAutoRefreshToggled(bool checked)
{
    autoRefresh_ = checked;
    autoRefreshBtn_->setText(checked ? QStringLiteral("[开] 自动刷新: 开") : QStringLiteral("[自] 自动刷新: 关"));
    
    if (checked) {
        refreshTimer_->start(5000); // 5秒刷新
        refreshSensorList();
        emit logMessage(QStringLiteral("传感器自动刷新已开启（5秒间隔）"));
        qDebug() << "[SENSOR_WIDGET] 自动刷新已开启";
    } else {
        refreshTimer_->stop();
        emit logMessage(QStringLiteral("传感器自动刷新已关闭"));
        qDebug() << "[SENSOR_WIDGET] 自动刷新已关闭";
    }
}

void SensorWidget::onAutoRefreshTimeout()
{
    if (autoRefresh_ && rpcClient_->isConnected()) {
        refreshCount_++;
        qDebug() << "[SENSOR_WIDGET] 自动刷新触发 #" << refreshCount_;
        refreshSensorList();
    }
}

void SensorWidget::onClearAllClicked()
{
    clearSensorCards();
    statusLabel_->setText(QStringLiteral("[清] 已清空显示"));
    emit logMessage(QStringLiteral("传感器显示已清空"));
}
