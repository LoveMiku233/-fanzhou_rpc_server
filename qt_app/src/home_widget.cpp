/**
 * @file home_widget.cpp
 * @brief 主页实现 - 大棚控制系统总览（1024x600低分辨率优化版）
 */

#include "home_widget.h"
#include "rpc_client.h"
#include "style_constants.h"

#include <QVBoxLayout>
#include <QDebug>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QDateTime>
#include <QFrame>
#include <QMessageBox>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QScrollArea>
#include <QSettings>
#include <QtMath>

using namespace UIConstants;

HomeWidget::HomeWidget(RpcClient *rpcClient, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpcClient)
    , totalDevicesLabel_(nullptr)
    , onlineDevicesLabel_(nullptr)
    , offlineDevicesLabel_(nullptr)
    , totalGroupsLabel_(nullptr)
    , totalStrategiesLabel_(nullptr)
    , totalSensorsLabel_(nullptr)
    , canStatusLabel_(nullptr)
    , mqttStatusLabel_(nullptr)
    , connectionStatusLabel_(nullptr)
    , systemUptimeLabel_(nullptr)
    , lastUpdateLabel_(nullptr)
    , modeLabel_(nullptr)
    , alertSummaryLabel_(nullptr)
    , sensorTrustLabel_(nullptr)
    , workflowSummaryLabel_(nullptr)
    , deviceSummaryLabel_(nullptr)
    , tempValueLabel_(nullptr)
    , humidityValueLabel_(nullptr)
    , co2ValueLabel_(nullptr)
    , lightValueLabel_(nullptr)
    , soilValueLabel_(nullptr)
    , refreshButton_(nullptr)
    , stopAllButton_(nullptr)
    , emergencyStopButton_(nullptr)
{
    QSettings settings;
    lowPerformanceMode_ = settings.value(QStringLiteral("ui/lowPerformanceMode"), true).toBool();
    setupUi();

    // 注意：自动刷新由MainWindow统一管理，HomeWidget不再有独立的刷新定时器
    // 这样避免了重复的RPC调用

    qDebug() << "[HOME_WIDGET] 主页初始化完成 lowPerformanceMode=" << lowPerformanceMode_;
}

void HomeWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 8, 10, 8);
    mainLayout->setSpacing(8);

    QLabel *titleLabel = new QLabel(QStringLiteral("大棚值班总览"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: 20px; font-weight: 900; color: #17364a; padding: 0 2px;"));
    mainLayout->addWidget(titleLabel);

    const bool useShadow = !lowPerformanceMode_;
    auto addShadow = [useShadow](QWidget *widget, const QColor &color = QColor(30, 50, 65, 32)) {
        if (!useShadow) {
            return;
        }
        auto *shadow = new QGraphicsDropShadowEffect(widget);
        shadow->setBlurRadius(18);
        shadow->setOffset(0, 5);
        shadow->setColor(color);
        widget->setGraphicsEffect(shadow);
    };

    auto createStatusPill = [this](const QString &title, const QString &value, const QString &bg, const QString &fg) -> QLabel* {
        QLabel *label = new QLabel(QStringLiteral("%1  %2").arg(title, value), this);
        label->setAlignment(Qt::AlignCenter);
        label->setMinimumHeight(34);
        label->setStyleSheet(QStringLiteral(
            "QLabel { background: %1; color: %2; border-radius: 8px; "
            "padding: 6px 10px; font-size: 13px; font-weight: 800; }").arg(bg, fg));
        return label;
    };

    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->setSpacing(8);

    connectionStatusLabel_ = createStatusPill(QStringLiteral("服务器"), QStringLiteral("未连接"), QStringLiteral("#c44337"), QStringLiteral("#fff5f2"));
    modeLabel_ = createStatusPill(QStringLiteral("模式"), QStringLiteral("手动值守"), QStringLiteral("#1e688e"), QStringLiteral("#eaf7ff"));
    alertSummaryLabel_ = createStatusPill(QStringLiteral("告警"), QStringLiteral("待同步"), QStringLiteral("#ffc15e"), QStringLiteral("#4a2f00"));
    systemUptimeLabel_ = createStatusPill(QStringLiteral("运行"), QStringLiteral("--"), QStringLiteral("#3d9760"), QStringLiteral("#f4fff7"));

    topLayout->addWidget(connectionStatusLabel_, 2);
    topLayout->addWidget(modeLabel_, 1);
    topLayout->addWidget(alertSummaryLabel_, 1);
    topLayout->addWidget(systemUptimeLabel_, 1);
    mainLayout->addLayout(topLayout);

    auto createCard = [this, addShadow](const QString &objectName, const QString &style) -> QFrame* {
        QFrame *card = new QFrame(this);
        card->setObjectName(objectName);
        card->setStyleSheet(QStringLiteral(
            "#%1 { %2 border-radius: 12px; border: 1px solid #b8cfdd; }")
            .arg(objectName, style));
        addShadow(card);
        return card;
    };

    auto createEnvCard = [this](const QString &title, const QString &unit, const QString &bgColor) -> QPair<QFrame*, QLabel*> {
        QFrame *card = new QFrame(this);
        card->setMinimumHeight(74);
        card->setStyleSheet(QStringLiteral(
            "QFrame { background: %1; border-radius: 10px; border: none; }")
            .arg(bgColor));

        QVBoxLayout *layout = new QVBoxLayout(card);
        layout->setContentsMargins(10, 8, 10, 8);
        layout->setSpacing(2);

        QLabel *titleLabel = new QLabel(title, card);
        titleLabel->setStyleSheet(QStringLiteral(
            "color: rgba(255,255,255,0.88); font-size: 12px; font-weight: 700;"));
        layout->addWidget(titleLabel);

        QLabel *valueLabel = new QLabel(QStringLiteral("--"), card);
        valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        valueLabel->setStyleSheet(QStringLiteral(
            "color: white; font-size: 24px; font-weight: 900;"));
        layout->addWidget(valueLabel);

        QLabel *unitLabel = new QLabel(unit, card);
        unitLabel->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.74); font-size: 11px;"));
        layout->addWidget(unitLabel);

        return qMakePair(card, valueLabel);
    };

    QHBoxLayout *middleLayout = new QHBoxLayout();
    middleLayout->setSpacing(10);

    QFrame *envPanel = createCard(QStringLiteral("envPanel"),
        QStringLiteral("background: #f5f9fc; border: 1px solid #bfd7e5;"));
    QVBoxLayout *envLayout = new QVBoxLayout(envPanel);
    envLayout->setContentsMargins(10, 10, 10, 10);
    envLayout->setSpacing(8);
    QLabel *envTitle = new QLabel(QStringLiteral("环境关键值"), envPanel);
    envTitle->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 900; color: #17364a;"));
    envLayout->addWidget(envTitle);

    QGridLayout *envGrid = new QGridLayout();
    envGrid->setSpacing(8);
    auto tempCard = createEnvCard(QStringLiteral("温度"), QStringLiteral("缺少传感器实时值"), QStringLiteral("#e85d4f"));
    tempValueLabel_ = tempCard.second;
    envGrid->addWidget(tempCard.first, 0, 0);
    auto humCard = createEnvCard(QStringLiteral("空气湿度"), QStringLiteral("缺少传感器实时值"), QStringLiteral("#248bd2"));
    humidityValueLabel_ = humCard.second;
    envGrid->addWidget(humCard.first, 0, 1);
    auto co2Card = createEnvCard(QStringLiteral("CO2"), QStringLiteral("缺少传感器实时值"), QStringLiteral("#28a36a"));
    co2ValueLabel_ = co2Card.second;
    envGrid->addWidget(co2Card.first, 1, 0);
    auto lightCard = createEnvCard(QStringLiteral("光照"), QStringLiteral("缺少传感器实时值"), QStringLiteral("#f2a51f"));
    lightValueLabel_ = lightCard.second;
    envGrid->addWidget(lightCard.first, 1, 1);
    auto soilCard = createEnvCard(QStringLiteral("基质水分"), QStringLiteral("缺少传感器实时值"), QStringLiteral("#7d5f43"));
    soilValueLabel_ = soilCard.second;
    envGrid->addWidget(soilCard.first, 2, 0, 1, 2);
    envLayout->addLayout(envGrid);
    middleLayout->addWidget(envPanel, 3);

    QFrame *flowPanel = createCard(QStringLiteral("flowPanel"),
        QStringLiteral("background: #f7fbff; border: 1px solid #bfd7e5;"));
    QVBoxLayout *flowLayout = new QVBoxLayout(flowPanel);
    flowLayout->setContentsMargins(10, 10, 10, 10);
    flowLayout->setSpacing(7);
    QLabel *flowTitle = new QLabel(QStringLiteral("值守建议"), flowPanel);
    flowTitle->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 900; color: #17364a;"));
    flowLayout->addWidget(flowTitle);

    auto createFlowLabel = [](const QString &text, const QString &color) -> QLabel* {
        QLabel *label = new QLabel(text);
        label->setWordWrap(true);
        label->setMinimumHeight(44);
        label->setStyleSheet(QStringLiteral(
            "QLabel { background: %1; color: #17364a; border-radius: 8px; "
            "padding: 7px 9px; font-size: 12px; font-weight: 700; }").arg(color));
        return label;
    };
    flowLayout->addWidget(createFlowLabel(QStringLiteral("默认模式：手动值守。策略配置仍保留，不会因首页模式文案而被清除。"), QStringLiteral("#dff4ff")));
    flowLayout->addWidget(createFlowLabel(QStringLiteral("大棚一键动作建议从“大棚”页执行，减少误操作。"), QStringLiteral("#e2f7e8")));
    flowLayout->addWidget(createFlowLabel(QStringLiteral("新增“建议”页可按作物给出固定种植建议，并预留 GPT 分析入口。"), QStringLiteral("#fff0cf")));
    flowLayout->addWidget(createFlowLabel(QStringLiteral("遇到通讯异常请先检查 CAN/MQTT，再执行控制动作。"), QStringLiteral("#ffe4e0")));
    middleLayout->addWidget(flowPanel, 3);

    QFrame *opsPanel = createCard(QStringLiteral("opsPanel"),
        QStringLiteral("background: #f7fbff; border: 1px solid #bfd7e5;"));
    QVBoxLayout *opsLayout = new QVBoxLayout(opsPanel);
    opsLayout->setContentsMargins(10, 10, 10, 10);
    opsLayout->setSpacing(8);
    QLabel *opsTitle = new QLabel(QStringLiteral("设备与可靠性"), opsPanel);
    opsTitle->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 900; color: #17364a;"));
    opsLayout->addWidget(opsTitle);

    deviceSummaryLabel_ = new QLabel(QStringLiteral("设备：--"), opsPanel);
    workflowSummaryLabel_ = new QLabel(QStringLiteral("策略：--"), opsPanel);
    sensorTrustLabel_ = new QLabel(QStringLiteral("传感器可信度：待同步"), opsPanel);
    for (QLabel *label : {deviceSummaryLabel_, workflowSummaryLabel_, sensorTrustLabel_}) {
        label->setWordWrap(true);
        label->setStyleSheet(QStringLiteral(
            "QLabel { background: #f2f7fb; color: #2b414d; border: 1px solid #d7e6ee; "
            "border-radius: 8px; padding: 8px 9px; font-size: 12px; font-weight: 700; }"));
        opsLayout->addWidget(label);
    }

    totalDevicesLabel_ = new QLabel(QStringLiteral("--"), opsPanel);
    onlineDevicesLabel_ = new QLabel(QStringLiteral("--"), opsPanel);
    offlineDevicesLabel_ = new QLabel(QStringLiteral("--"), opsPanel);
    totalGroupsLabel_ = new QLabel(QStringLiteral("--"), opsPanel);
    totalStrategiesLabel_ = new QLabel(QStringLiteral("--"), opsPanel);
    totalSensorsLabel_ = new QLabel(QStringLiteral("--"), opsPanel);
    canStatusLabel_ = new QLabel(QStringLiteral("--"), opsPanel);
    mqttStatusLabel_ = new QLabel(QStringLiteral("--"), opsPanel);

    opsLayout->addStretch();
    middleLayout->addWidget(opsPanel, 2);
    mainLayout->addLayout(middleLayout, 1);

    QHBoxLayout *actionsLayout = new QHBoxLayout();
    actionsLayout->setSpacing(8);
    refreshButton_ = new QPushButton(QStringLiteral("刷新"), this);
    refreshButton_->setMinimumHeight(BTN_HEIGHT);
    refreshButton_->setMinimumWidth(BTN_MIN_WIDTH);
    refreshButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #1e88e5; color: white; border: none; "
        "border-radius: %1px; padding: 0 16px; font-weight: bold; font-size: %2px; }"
        "QPushButton:hover { background-color: #1565c0; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_BODY));
    connect(refreshButton_, &QPushButton::clicked, this, &HomeWidget::refreshData);
    actionsLayout->addWidget(refreshButton_, 1);

    stopAllButton_ = new QPushButton(QStringLiteral("全停"), this);
    stopAllButton_->setMinimumHeight(BTN_HEIGHT);
    stopAllButton_->setMinimumWidth(BTN_MIN_WIDTH);
    stopAllButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #fb8c00; color: white; border: none; "
        "border-radius: %1px; padding: 0 16px; font-weight: bold; font-size: %2px; }"
        "QPushButton:hover { background-color: #ef6c00; }").arg(BORDER_RADIUS_BTN).arg(FONT_SIZE_BODY));
    connect(stopAllButton_, &QPushButton::clicked, this, &HomeWidget::onStopAllClicked);
    actionsLayout->addWidget(stopAllButton_, 1);

    emergencyStopButton_ = new QPushButton(QStringLiteral("紧急停止"), this);
    emergencyStopButton_->setFixedHeight(BTN_HEIGHT_EMERGENCY);
    emergencyStopButton_->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e53935, stop:1 #c62828);"
        "  color: white;"
        "  font-size: %1px;"
        "  font-weight: bold;"
        "  border: 2px solid #b71c1c;"
        "  border-radius: %2px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #c62828, stop:1 #b71c1c);"
        "}"
    ).arg(FONT_SIZE_TITLE).arg(BORDER_RADIUS_BTN));
    connect(emergencyStopButton_, &QPushButton::clicked, this, &HomeWidget::onEmergencyStopClicked);
    actionsLayout->addWidget(emergencyStopButton_, 2);
    mainLayout->addLayout(actionsLayout);

    lastUpdateLabel_ = new QLabel(QStringLiteral("更新: --"), this);
    lastUpdateLabel_->setStyleSheet(QStringLiteral(
        "color: #78909c; font-size: %1px; padding: 4px;").arg(FONT_SIZE_SMALL));
    lastUpdateLabel_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(lastUpdateLabel_);
}

void HomeWidget::onStopAllClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        QStringLiteral("确认"),
        QStringLiteral("确定要停止所有设备吗？"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    qDebug() << "[HOME_WIDGET] 执行全部停止";
    stopAllButton_->setEnabled(false);
    rpcClient_->callAsync(QStringLiteral("relay.emergencyStop"), QJsonObject(), this,
        [this](const QJsonValue &result, const QJsonObject &error) {
            stopAllButton_->setEnabled(true);
            if (!error.isEmpty()) {
                QMessageBox::warning(this, QStringLiteral("全停失败"),
                    QStringLiteral("请求失败: %1").arg(error.value(QStringLiteral("message")).toString()));
                return;
            }
            qDebug() << "[HOME_WIDGET] 全部停止结果:" << QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact);
            QMessageBox::information(this, QStringLiteral("全停完成"), QStringLiteral("已发送全停命令"));
        }, 3000);
}

void HomeWidget::onEmergencyStopClicked()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先连接服务器"));
        return;
    }

    qDebug() << "[HOME_WIDGET] 执行紧急停止";
    emergencyStopButton_->setEnabled(false);
    rpcClient_->callAsync(QStringLiteral("relay.emergencyStop"), QJsonObject(), this,
        [this](const QJsonValue &result, const QJsonObject &error) {
            emergencyStopButton_->setEnabled(true);
            if (!error.isEmpty()) {
                QMessageBox::warning(this, QStringLiteral("急停执行失败"),
                    QStringLiteral("请求失败: %1").arg(error.value(QStringLiteral("message")).toString()));
                return;
            }

            qDebug() << "[HOME_WIDGET] 紧急停止结果:" << QJsonDocument(result.toObject()).toJson(QJsonDocument::Compact);

            if (result.isObject()) {
                const QJsonObject obj = result.toObject();
                if (obj.value(QStringLiteral("ok")).toBool()) {
                    const int stopped = obj.value(QStringLiteral("stoppedChannels")).toInt();
                    const int devices = obj.value(QStringLiteral("deviceCount")).toInt();
                    QMessageBox::information(this, QStringLiteral("急停执行完成"),
                        QStringLiteral("已停止 %1 个设备的 %2 个通道").arg(devices).arg(stopped));
                } else {
                    QMessageBox::warning(this, QStringLiteral("急停执行失败"),
                        QStringLiteral("执行急停命令时发生错误"));
                }
            }
        }, 3000);
}

void HomeWidget::updateDutyOverview(int totalDevices, int onlineDevices, int offlineDevices,
                                    int totalGroups, int totalStrategies, int totalSensors,
                                    bool canOpened, bool canValid, int mqttConnected,
                                    int mqttTotal, bool mqttValid, const QString &uptime)
{
    if (modeLabel_) {
        modeLabel_->setText(QStringLiteral("模式  手动值守"));
    }

    if (systemUptimeLabel_) {
        systemUptimeLabel_->setText(uptime.isEmpty()
            ? QStringLiteral("运行  --")
            : QStringLiteral("运行  %1").arg(uptime));
    }

    const bool mqttBad = mqttValid && mqttTotal > 0 && mqttConnected < mqttTotal;
    const bool canBad = canValid && !canOpened;
    if (alertSummaryLabel_) {
        if (offlineDevices > 0 || mqttBad || canBad) {
            alertSummaryLabel_->setText(QStringLiteral("告警  %1项待处理")
                .arg((offlineDevices > 0 ? 1 : 0) + (mqttBad ? 1 : 0) + (canBad ? 1 : 0)));
            alertSummaryLabel_->setStyleSheet(QStringLiteral(
                "QLabel { background: #ffc15e; color: #4a2f00; border-radius: 8px; "
                "padding: 6px 10px; font-size: 13px; font-weight: 800; }"));
        } else {
            alertSummaryLabel_->setText(QStringLiteral("告警  正常"));
            alertSummaryLabel_->setStyleSheet(QStringLiteral(
                "QLabel { background: #3d9760; color: #f4fff7; border-radius: 8px; "
                "padding: 6px 10px; font-size: 13px; font-weight: 800; }"));
        }
    }

    if (deviceSummaryLabel_) {
        deviceSummaryLabel_->setText(QStringLiteral("设备：在线 %1/%2，离线 %3；分组 %4 个。关键动作请在大棚控制页确认绑定关系后执行。")
            .arg(onlineDevices).arg(totalDevices).arg(offlineDevices).arg(totalGroups));
    }
    if (workflowSummaryLabel_) {
        workflowSummaryLabel_->setText(QStringLiteral("策略：已配置 %1 个；默认手动值守，不影响策略配置与启停。")
            .arg(totalStrategies));
    }
    if (sensorTrustLabel_) {
        sensorTrustLabel_->setText(totalSensors > 0
            ? QStringLiteral("传感器可信度：%1 个传感器已登记，需查看更新时间和异常值后再让策略长期自动运行。").arg(totalSensors)
            : QStringLiteral("传感器可信度：未发现传感器，建议保持手动值守并按固定策略执行。"));
    }

}

void HomeWidget::refreshData()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (statsRefreshInFlight_) {
        // 避免低性能终端上异步请求堆积
        if (now - lastRefreshRequestMs_ < 4000) {
            return;
        }
    }
    lastRefreshRequestMs_ = now;
    updateStats();
}

void HomeWidget::refreshSensorOverview()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        return;
    }

    rpcClient_->callAsync(QStringLiteral("sensor.values"), QJsonObject(), this,
        [this](const QJsonValue &result, const QJsonObject &error) {
            QMetaObject::invokeMethod(this, [this, result, error]() {
                if (!error.isEmpty() || !result.isObject()) {
                    return;
                }

                QString temp = QStringLiteral("--");
                QString humidity = QStringLiteral("--");
                QString co2 = QStringLiteral("--");
                QString light = QStringLiteral("--");
                QString soil = QStringLiteral("--");

                const auto formatDisplay = [](const QJsonValue &value, const QString &unit) -> QString {
                    QString text;
                    if (value.isDouble()) {
                        const double v = value.toDouble();
                        text = qFabs(v) >= 100.0 ? QString::number(v, 'f', 0) : QString::number(v, 'f', 1);
                    } else if (value.isBool()) {
                        text = value.toBool() ? QStringLiteral("开") : QStringLiteral("关");
                    } else {
                        text = value.toVariant().toString();
                    }
                    if (!unit.isEmpty()) {
                        text += unit;
                    }
                    return text;
                };

                const QJsonArray sensors = result.toObject().value(QStringLiteral("sensors")).toArray();
                for (const QJsonValue &v : sensors) {
                    if (!v.isObject()) {
                        continue;
                    }
                    const QJsonObject obj = v.toObject();
                    if (!obj.value(QStringLiteral("hasValue")).toBool(false)) {
                        continue;
                    }

                    const QString sensorId = obj.value(QStringLiteral("sensorId")).toString().toLower();
                    const QString name = obj.value(QStringLiteral("name")).toString().toLower();
                    const QString unit = obj.value(QStringLiteral("unit")).toString();
                    const QJsonValue value = obj.value(QStringLiteral("value"));
                    if (value.isUndefined() || value.isNull()) {
                        continue;
                    }

                    const QString display = formatDisplay(value, unit);
                    if (sensorId == QStringLiteral("temperature") ||
                        sensorId.contains(QStringLiteral("temp")) ||
                        name.contains(QStringLiteral("温"))) {
                        temp = display;
                        continue;
                    }
                    if (sensorId.contains(QStringLiteral("soil")) ||
                        name.contains(QStringLiteral("土壤")) ||
                        name.contains(QStringLiteral("基质"))) {
                        if (soil == QStringLiteral("--")) {
                            soil = display;
                        }
                        continue;
                    }
                    if (sensorId == QStringLiteral("humidity") ||
                        (sensorId.contains(QStringLiteral("humid")) && !sensorId.contains(QStringLiteral("soil"))) ||
                        name.contains(QStringLiteral("湿度"))) {
                        humidity = display;
                        continue;
                    }
                    if (sensorId.contains(QStringLiteral("co2")) || name.contains(QStringLiteral("co2"))) {
                        co2 = display;
                        continue;
                    }
                    if (sensorId.contains(QStringLiteral("light")) ||
                        sensorId.contains(QStringLiteral("illuminance")) ||
                        name.contains(QStringLiteral("光"))) {
                        light = display;
                        continue;
                    }
                }

                if (tempValueLabel_) tempValueLabel_->setText(temp);
                if (humidityValueLabel_) humidityValueLabel_->setText(humidity);
                if (co2ValueLabel_) co2ValueLabel_->setText(co2);
                if (lightValueLabel_) lightValueLabel_->setText(light);
                if (soilValueLabel_) soilValueLabel_->setText(soil);
            }, Qt::QueuedConnection);
        }, 2500);
}

void HomeWidget::updateStats()
{
    statsRefreshInFlight_ = true;

    if (!rpcClient_ || !rpcClient_->isConnected()) {
        statsRefreshInFlight_ = false;
        connectionStatusLabel_->setText(QStringLiteral("未连接"));
        connectionStatusLabel_->setStyleSheet(QStringLiteral(
            "QLabel { background: #c44337; color: #fff5f2; border-radius: 8px; "
            "padding: 6px 10px; font-size: 13px; font-weight: 800; }"));
        totalDevicesLabel_->setText(QStringLiteral("--"));
        onlineDevicesLabel_->setText(QStringLiteral("--"));
        offlineDevicesLabel_->setText(QStringLiteral("--"));
        totalGroupsLabel_->setText(QStringLiteral("--"));
        totalStrategiesLabel_->setText(QStringLiteral("--"));
        totalSensorsLabel_->setText(QStringLiteral("--"));
        canStatusLabel_->setText(QStringLiteral("--"));
        mqttStatusLabel_->setText(QStringLiteral("--"));
        systemUptimeLabel_->setText(QStringLiteral("运行时间: --"));
        updateDutyOverview(0, 0, 0, 0, 0, 0, false, false, 0, 0, false, QString());
        return;
    }

    connectionStatusLabel_->setText(QStringLiteral("已连接 %1:%2")
        .arg(rpcClient_->host()).arg(rpcClient_->port()));
    connectionStatusLabel_->setStyleSheet(QStringLiteral(
        "QLabel { background: #3d9760; color: #f4fff7; border-radius: 8px; "
        "padding: 6px 10px; font-size: 13px; font-weight: 800; }"));

    // 使用异步调用避免阻塞UI线程
    rpcClient_->callAsync(QStringLiteral("sys.dashboard"), QJsonObject(), this,
        [this](const QJsonValue &result, const QJsonObject &error) {
            QMetaObject::invokeMethod(this, [this, result, error]() {
                if (!error.isEmpty() || !result.isObject()) {
                    // 异步调用失败，尝试兼容模式
                    if (!lowPerformanceMode_) {
                        qDebug() << "[HOME_WIDGET] sys.dashboard异步调用失败，使用兼容模式";
                    }
                    updateStatsLegacy();
                    return;
                }

                QJsonObject obj = result.toObject();

                if (!obj.value(QStringLiteral("ok")).toBool()) {
                    if (!lowPerformanceMode_) {
                        qDebug() << "[HOME_WIDGET] Dashboard调用返回失败";
                    }
                    updateStatsLegacy();
                    return;
                }

                // 设备统计
                int totalDevices = obj.value(QStringLiteral("totalDevices")).toInt();
                int onlineDevices = obj.value(QStringLiteral("onlineDevices")).toInt();
                int offlineDevices = obj.value(QStringLiteral("offlineDevices")).toInt();

                totalDevicesLabel_->setText(QString::number(totalDevices));
                onlineDevicesLabel_->setText(QString::number(onlineDevices));
                offlineDevicesLabel_->setText(QString::number(offlineDevices));

                // 分组
                int totalGroups = obj.value(QStringLiteral("totalGroups")).toInt();
                totalGroupsLabel_->setText(QString::number(totalGroups));

                // 策略
                int totalStrategies = obj.value(QStringLiteral("totalStrategies")).toInt();
                totalStrategiesLabel_->setText(QString::number(totalStrategies));

                // 传感器
                int totalSensors = obj.value(QStringLiteral("totalSensors")).toInt();
                totalSensorsLabel_->setText(QString::number(totalSensors));

                // CAN状态
                bool canOpened = obj.value(QStringLiteral("canOpened")).toBool();
                if (canOpened) {
                    canStatusLabel_->setText(QStringLiteral("正常"));
                    canStatusLabel_->parentWidget()->setStyleSheet(
                        canStatusLabel_->parentWidget()->styleSheet().replace(QStringLiteral("#546e7a"), QStringLiteral("#43a047")));
                } else {
                    canStatusLabel_->setText(QStringLiteral("关闭"));
                }

                // MQTT状态
                int mqttConnected = obj.value(QStringLiteral("mqttConnected")).toInt();
                int mqttTotal = obj.value(QStringLiteral("mqttTotal")).toInt();
                if (mqttTotal > 0) {
                    mqttStatusLabel_->setText(QStringLiteral("%1/%2").arg(mqttConnected).arg(mqttTotal));
                } else {
                    mqttStatusLabel_->setText(QStringLiteral("未配置"));
                }

                // 通知MainWindow更新状态栏的云状态，避免重复RPC调用
                emit mqttStatusUpdated(mqttConnected, mqttTotal);

                // 系统运行时间
                QString uptime = obj.value(QStringLiteral("uptime")).toString();
                if (!uptime.isEmpty()) {
                    systemUptimeLabel_->setText(QStringLiteral("运行时间: %1").arg(uptime));
                }

                updateDutyOverview(totalDevices, onlineDevices, offlineDevices,
                                   totalGroups, totalStrategies, totalSensors,
                                   canOpened, true, mqttConnected, mqttTotal, true, uptime);
                refreshSensorOverview();
                statsRefreshInFlight_ = false;
                if (!lowPerformanceMode_) {
                    qDebug() << "[HOME_WIDGET] Dashboard数据更新成功（异步RPC）";
                }
            }, Qt::QueuedConnection);
        }, 3000);

    // 更新时间
    lastUpdateLabel_->setText(QStringLiteral("最后更新: %1")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));

    if (!lowPerformanceMode_) {
        qDebug() << "[HOME_WIDGET] 统计数据更新请求已发送";
    }
}

void HomeWidget::updateStatsLegacy()
{
    // 兼容旧版本服务器的多RPC调用方式
    // 使用异步调用避免阻塞UI线程
    if (!lowPerformanceMode_) {
        qDebug() << "[HOME_WIDGET] 使用兼容模式更新统计数据（异步）";
    }

    // 使用共享指针跟踪请求状态
    auto statsData = std::make_shared<StatsData>();

    // 1. 获取设备列表
    rpcClient_->callAsync(QStringLiteral("relay.nodes"), QJsonObject(), this,
        [this, statsData](const QJsonValue &result, const QJsonObject &error) {
            if (error.isEmpty() && result.isObject()) {
                QJsonObject obj = result.toObject();
                if (obj.contains(QStringLiteral("nodes"))) {
                    QJsonArray nodes = obj.value(QStringLiteral("nodes")).toArray();
                    statsData->totalDevices = nodes.size();
                    for (const QJsonValue &node : nodes) {
                        if (node.toObject().value(QStringLiteral("online")).toBool()) {
                            statsData->onlineDevices++;
                        } else {
                            statsData->offlineDevices++;
                        }
                    }
                }
            }
            statsData->devicesDone = true;
            checkAndUpdateStats(statsData);
        }, 2000);

    // 2. 获取分组列表
    rpcClient_->callAsync(QStringLiteral("group.list"), QJsonObject(), this,
        [this, statsData](const QJsonValue &result, const QJsonObject &error) {
            if (error.isEmpty() && result.isObject()) {
                QJsonObject obj = result.toObject();
                if (obj.contains(QStringLiteral("groups"))) {
                    statsData->totalGroups = obj.value(QStringLiteral("groups")).toArray().size();
                }
            }
            statsData->groupsDone = true;
            checkAndUpdateStats(statsData);
        }, 2000);

    // 3. 获取策略列表
    rpcClient_->callAsync(QStringLiteral("auto.strategy.list"), QJsonObject(), this,
        [this, statsData](const QJsonValue &result, const QJsonObject &error) {
            if (error.isEmpty() && result.isObject()) {
                QJsonObject obj = result.toObject();
                if (obj.contains(QStringLiteral("strategies"))) {
                    statsData->totalStrategies = obj.value(QStringLiteral("strategies")).toArray().size();
                }
            }
            statsData->strategiesDone = true;
            checkAndUpdateStats(statsData);
        }, 2000);

    // 4. 获取传感器列表
    rpcClient_->callAsync(QStringLiteral("sensor.list"), QJsonObject(), this,
        [this, statsData](const QJsonValue &result, const QJsonObject &error) {
            if (error.isEmpty() && result.isObject()) {
                QJsonObject obj = result.toObject();
                if (obj.contains(QStringLiteral("sensors"))) {
                    statsData->totalSensors = obj.value(QStringLiteral("sensors")).toArray().size();
                }
            }
            statsData->sensorsDone = true;
            checkAndUpdateStats(statsData);
        }, 2000);

    // 5. 获取CAN状态
    rpcClient_->callAsync(QStringLiteral("can.status"), QJsonObject(), this,
        [this, statsData](const QJsonValue &result, const QJsonObject &error) {
            if (error.isEmpty() && result.isObject()) {
                QJsonObject obj = result.toObject();
                statsData->canOpened = obj.value(QStringLiteral("isOpen")).toBool();
                statsData->canValid = true;
            }
            statsData->canDone = true;
            checkAndUpdateStats(statsData);
        }, 2000);

    // 6. 获取MQTT状态
    rpcClient_->callAsync(QStringLiteral("mqtt.channels.list"), QJsonObject(), this,
        [this, statsData](const QJsonValue &result, const QJsonObject &error) {
            if (error.isEmpty() && result.isObject()) {
                QJsonObject obj = result.toObject();
                if (obj.value(QStringLiteral("ok")).toBool()) {
                    QJsonArray channels = obj.value(QStringLiteral("channels")).toArray();
                    statsData->mqttTotal = channels.size();
                    for (const QJsonValue &ch : channels) {
                        if (ch.toObject().value(QStringLiteral("connected")).toBool()) {
                            statsData->mqttConnected++;
                        }
                    }
                }
                statsData->mqttValid = true;
            }
            statsData->mqttDone = true;
            checkAndUpdateStats(statsData);
        }, 2000);

    // 7. 获取系统信息
    rpcClient_->callAsync(QStringLiteral("sys.info"), QJsonObject(), this,
        [this, statsData](const QJsonValue &result, const QJsonObject &error) {
            if (error.isEmpty() && result.isObject()) {
                QJsonObject obj = result.toObject();
                statsData->uptime = obj.value(QStringLiteral("uptime")).toString();
            }
            statsData->sysInfoDone = true;
            checkAndUpdateStats(statsData);
        }, 2000);
}

void HomeWidget::checkAndUpdateStats(std::shared_ptr<StatsData> statsData)
{
    // 检查是否所有请求都完成
    if (!statsData->devicesDone || !statsData->groupsDone || !statsData->strategiesDone ||
        !statsData->sensorsDone || !statsData->canDone || !statsData->mqttDone ||
        !statsData->sysInfoDone) {
        return;
    }

    // 在主线程更新UI
    QMetaObject::invokeMethod(this, [this, statsData]() {
        // 更新设备统计
        totalDevicesLabel_->setText(QString::number(statsData->totalDevices));
        onlineDevicesLabel_->setText(QString::number(statsData->onlineDevices));
        offlineDevicesLabel_->setText(QString::number(statsData->offlineDevices));

        // 更新分组
        totalGroupsLabel_->setText(QString::number(statsData->totalGroups));

        // 更新策略
        totalStrategiesLabel_->setText(QString::number(statsData->totalStrategies));

        // 更新传感器
        totalSensorsLabel_->setText(QString::number(statsData->totalSensors));

        // 更新CAN状态
        if (statsData->canValid) {
            if (statsData->canOpened) {
                canStatusLabel_->setText(QStringLiteral("正常"));
                canStatusLabel_->parentWidget()->setStyleSheet(
                    canStatusLabel_->parentWidget()->styleSheet().replace(QStringLiteral("#546e7a"), QStringLiteral("#43a047")));
            } else {
                canStatusLabel_->setText(QStringLiteral("关闭"));
            }
        } else {
            canStatusLabel_->setText(QStringLiteral("未知"));
        }

        // 更新MQTT状态
        if (statsData->mqttValid) {
            if (statsData->mqttTotal > 0) {
                mqttStatusLabel_->setText(QStringLiteral("%1/%2").arg(statsData->mqttConnected).arg(statsData->mqttTotal));
            } else {
                mqttStatusLabel_->setText(QStringLiteral("未配置"));
            }
        } else {
            mqttStatusLabel_->setText(QStringLiteral("未知"));
        }

        // 通知MainWindow更新状态栏的云状态
        emit mqttStatusUpdated(statsData->mqttConnected, statsData->mqttTotal);

        // 更新系统运行时间
        if (!statsData->uptime.isEmpty()) {
            systemUptimeLabel_->setText(QStringLiteral("运行时间: %1").arg(statsData->uptime));
        }

        updateDutyOverview(statsData->totalDevices, statsData->onlineDevices, statsData->offlineDevices,
                           statsData->totalGroups, statsData->totalStrategies, statsData->totalSensors,
                           statsData->canOpened, statsData->canValid,
                           statsData->mqttConnected, statsData->mqttTotal, statsData->mqttValid,
                           statsData->uptime);
        refreshSensorOverview();
        statsRefreshInFlight_ = false;
        if (!lowPerformanceMode_) {
            qDebug() << "[HOME_WIDGET] 兼容模式统计数据更新完成";
        }
    }, Qt::QueuedConnection);
}
