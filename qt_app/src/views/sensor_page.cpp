/**
 * @file views/sensor_page.cpp
 * @brief 传感器数据监测页面实现
 */

#include "views/sensor_page.h"
#include "rpc_client.h"
#include "style_constants.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

const char *SensorPage::getTypeColor(const QString &type)
{
    if (type == "temp")     return Style::kColorOrange;
    if (type == "humidity") return Style::kColorBlue;
    if (type == "light")    return Style::kColorYellow;
    if (type == "co2")      return Style::kColorPurple;
    if (type == "soil")     return Style::kColorEmerald;
    return Style::kColorTextPrimary;
}

QString SensorPage::getTypeName(const QString &type)
{
    if (type == "temp")     return QString::fromUtf8("温度");
    if (type == "humidity") return QString::fromUtf8("湿度");
    if (type == "light")    return QString::fromUtf8("光照");
    if (type == "co2")      return QString::fromUtf8("CO\xe2\x82\x82");
    if (type == "soil")     return QString::fromUtf8("土壤");
    return type;
}

// ---------------------------------------------------------------------------
// SensorPage
// ---------------------------------------------------------------------------

SensorPage::SensorPage(RpcClient *rpc, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpc)
{
    // Demo data matching index3.html
    auto add = [this](const char *id, const char *name, const char *type,
                      double val, const char *unit, const char *loc,
                      const char *upd) {
        Models::SensorInfo s;
        s.id = id;
        s.name = QString::fromUtf8(name);
        s.type = type;
        s.value = val;
        s.unit = QString::fromUtf8(unit);
        s.location = QString::fromUtf8(loc);
        s.lastUpdate = upd;
        sensors_.append(s);
    };

    // temp sensors
    add("temp-1", "棚内温度-东", "temp", 26.5, "°C", "东侧区域", "2024-01-15 14:32:18");
    add("temp-2", "棚内温度-西", "temp", 27.2, "°C", "西侧区域", "2024-01-15 14:32:15");
    add("temp-3", "棚内温度-中", "temp", 26.8, "°C", "中央区域", "2024-01-15 14:32:20");
    add("temp-4", "土壤温度-1",  "temp", 22.3, "°C", "种植区A",  "2024-01-15 14:32:10");
    add("temp-5", "土壤温度-2",  "temp", 22.1, "°C", "种植区B",  "2024-01-15 14:32:12");

    // humidity sensors
    add("hum-1", "空气湿度-东", "humidity", 78.5, "%", "东侧区域", "2024-01-15 14:32:18");
    add("hum-2", "空气湿度-西", "humidity", 76.2, "%", "西侧区域", "2024-01-15 14:32:15");
    add("hum-3", "土壤湿度-1",  "humidity", 65.3, "%", "种植区A",  "2024-01-15 14:32:08");
    add("hum-4", "土壤湿度-2",  "humidity", 58.7, "%", "种植区B",  "2024-01-15 14:32:10");

    // light sensors
    add("light-1", "光照强度-东",   "light", 35200, "Lx",     "东侧顶部", "2024-01-15 14:32:22");
    add("light-2", "光照强度-西",   "light", 34800, "Lx",     "西侧顶部", "2024-01-15 14:32:20");
    add("light-3", "光合有效辐射",  "light", 850,   "μmol",   "中央区域", "2024-01-15 14:32:18");

    // CO2 sensors
    add("co2-1", "CO₂浓度-东", "co2", 425, "ppm", "东侧区域", "2024-01-15 14:32:16");
    add("co2-2", "CO₂浓度-西", "co2", 418, "ppm", "西侧区域", "2024-01-15 14:32:14");

    // soil sensors
    add("soil-1", "土壤EC值-1", "soil", 1.25, "mS/cm", "种植区A", "2024-01-15 14:32:05");
    add("soil-2", "土壤EC值-2", "soil", 1.32, "mS/cm", "种植区B", "2024-01-15 14:32:07");
    add("soil-3", "土壤pH值-1", "soil", 6.8,  "pH",    "种植区A", "2024-01-15 14:32:03");
    add("soil-4", "土壤pH值-2", "soil", 6.5,  "pH",    "种植区B", "2024-01-15 14:32:06");

    setupUi();
    renderSensors();
}

// ---------------------------------------------------------------------------
// setupUi
// ---------------------------------------------------------------------------

void SensorPage::setupUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Top title bar ────────────────────────────
    QWidget *titleBar = new QWidget;
    titleBar->setStyleSheet(
        QString("background:rgba(30,41,59,0.3); border-bottom:1px solid %1;")
            .arg(Style::kColorBorder));

    QHBoxLayout *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(Style::kPageMargin, 8, Style::kPageMargin, 8);

    QLabel *titleIcon = new QLabel(QString::fromUtf8("📊"));
    titleIcon->setStyleSheet(
        QString("color:%1; font-size:%2px; border:none;")
            .arg(Style::kColorAccentCyan).arg(Style::kFontMedium));

    QLabel *titleText = new QLabel(QString::fromUtf8("传感器数据监测"));
    titleText->setStyleSheet(
        QString("color:white; font-size:%1px; font-weight:bold; border:none;")
            .arg(Style::kFontNormal));

    countLabel_ = new QLabel(
        QString::fromUtf8("共 %1 个传感器").arg(sensors_.size()));
    countLabel_->setStyleSheet(
        QString("color:%1; font-size:%2px; border:none;")
            .arg(Style::kColorTextSecondary).arg(Style::kFontSmall));

    titleLayout->addWidget(titleIcon);
    titleLayout->addWidget(titleText);
    titleLayout->addStretch();
    titleLayout->addWidget(countLabel_);

    root->addWidget(titleBar);

    // ── Scroll area with sensor grid ─────────────
    scrollArea_ = new QScrollArea;
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setFrameShape(QFrame::NoFrame);
    scrollArea_->setStyleSheet(
        QString("QScrollArea { background:transparent; }"
                "QScrollBar:vertical { width:6px; background:transparent; }"
                "QScrollBar::handle:vertical { background:%1; border-radius:3px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }")
            .arg(Style::kColorBorderLight));

    gridContainer_ = new QWidget;
    gridLayout_ = new QGridLayout(gridContainer_);
    gridLayout_->setContentsMargins(Style::kPageMargin, Style::kPageMargin,
                                    Style::kPageMargin, Style::kPageMargin);
    gridLayout_->setSpacing(Style::kCardSpacing);

    scrollArea_->setWidget(gridContainer_);
    root->addWidget(scrollArea_, 1);
}

// ---------------------------------------------------------------------------
// renderSensors
// ---------------------------------------------------------------------------

void SensorPage::renderSensors()
{
    // Clear existing items
    while (QLayoutItem *item = gridLayout_->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    const int columns = 4;
    for (int i = 0; i < sensors_.size(); ++i) {
        QWidget *card = createSensorCard(sensors_.at(i));
        gridLayout_->addWidget(card, i / columns, i % columns);
    }

    // Spacer to push cards to top
    gridLayout_->setRowStretch(sensors_.size() / columns + 1, 1);

    countLabel_->setText(
        QString::fromUtf8("共 %1 个传感器").arg(sensors_.size()));
}

// ---------------------------------------------------------------------------
// createSensorCard
// ---------------------------------------------------------------------------

QWidget *SensorPage::createSensorCard(const Models::SensorInfo &sensor)
{
    const char *typeColor = getTypeColor(sensor.type);

    QFrame *card = new QFrame;
    card->setProperty("class", "sensorCard");
    card->setStyleSheet(
        QString("QFrame { background:rgba(30,41,59,0.7); border:1px solid %1;"
                "  border-radius:%2px; padding:10px; }")
            .arg(Style::kColorBorder)
            .arg(Style::kCardRadius));

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // ── Top row: location + type badge ───────────
    QHBoxLayout *topRow = new QHBoxLayout;
    topRow->setSpacing(4);

    QLabel *locLabel = new QLabel(sensor.location);
    locLabel->setStyleSheet(
        QString("color:%1; font-size:%2px; border:none;")
            .arg(Style::kColorTextSecondary).arg(Style::kFontSmall));

    QLabel *typeBadge = new QLabel(getTypeName(sensor.type));
    typeBadge->setStyleSheet(
        QString("background:%1; color:%2; font-size:%3px;"
                " padding:1px 6px; border-radius:4px; border:none;")
            .arg(Style::kColorBgCard)
            .arg(Style::kColorTextSecondary)
            .arg(Style::kFontTiny));

    topRow->addWidget(locLabel);
    topRow->addStretch();
    topRow->addWidget(typeBadge);
    layout->addLayout(topRow);

    // ── Sensor name ──────────────────────────────
    QLabel *nameLabel = new QLabel(sensor.name);
    nameLabel->setStyleSheet(
        QString("color:white; font-size:%1px; font-weight:500; border:none;")
            .arg(Style::kFontNormal));
    nameLabel->setWordWrap(false);
    layout->addWidget(nameLabel);

    // ── Value + unit ─────────────────────────────
    QHBoxLayout *valueRow = new QHBoxLayout;
    valueRow->setSpacing(4);
    valueRow->setAlignment(Qt::AlignBaseline | Qt::AlignLeft);

    // Format: integers for large values, decimals for small
    QString valueStr = (sensor.value >= 1000)
        ? QString::number(static_cast<int>(sensor.value))
        : QString::number(sensor.value, 'f',
                          (sensor.value == static_cast<int>(sensor.value)) ? 0 : 1);

    QLabel *valueLbl = new QLabel(valueStr);
    valueLbl->setStyleSheet(
        QString("color:%1; font-size:%2px; font-weight:bold; border:none;")
            .arg(typeColor).arg(Style::kFontTitle));

    QLabel *unitLbl = new QLabel(sensor.unit);
    unitLbl->setStyleSheet(
        QString("color:%1; font-size:%2px; border:none;")
            .arg(Style::kColorTextSecondary).arg(Style::kFontSmall));

    valueRow->addWidget(valueLbl);
    valueRow->addWidget(unitLbl);
    valueRow->addStretch();
    layout->addLayout(valueRow);

    // ── Bottom: clock icon + last update ─────────
    QHBoxLayout *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(4);

    QLabel *clockIcon = new QLabel(QString::fromUtf8("🕐"));
    clockIcon->setStyleSheet(
        QString("color:%1; font-size:8px; border:none;")
            .arg(Style::kColorTextMuted));

    QLabel *timeLbl = new QLabel(sensor.lastUpdate);
    timeLbl->setStyleSheet(
        QString("color:%1; font-size:%2px; border:none;")
            .arg(Style::kColorTextMuted).arg(Style::kFontTiny));

    bottomRow->addWidget(clockIcon);
    bottomRow->addWidget(timeLbl);
    bottomRow->addStretch();
    layout->addLayout(bottomRow);

    return card;
}

// ---------------------------------------------------------------------------
// refreshData (stub)
// ---------------------------------------------------------------------------

void SensorPage::refreshData()
{
    // TODO: fetch sensor data via rpcClient_ and update sensors_
}
