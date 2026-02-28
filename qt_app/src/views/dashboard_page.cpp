/**
 * @file views/dashboard_page.cpp
 * @brief 驾驶舱页面实现
 */

#include "views/dashboard_page.h"
#include "rpc_client.h"
#include "style_constants.h"
#include "models/data_models.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static QLabel *makeValueLabel(const QString &text, const char *color, int fontSize)
{
    QLabel *lbl = new QLabel(text);
    lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:bold;")
                           .arg(color).arg(fontSize));
    return lbl;
}

static QWidget *makeWeatherRow(const QString &name, QLabel **valueOut,
                               const char *color, const QString &defaultVal)
{
    QWidget *row = new QWidget;
    row->setStyleSheet(QString("background:%1; border-radius:6px;")
                           .arg(Style::kColorBgCard));

    QHBoxLayout *hl = new QHBoxLayout(row);
    hl->setContentsMargins(8, 4, 8, 4);

    QLabel *nameLabel = new QLabel(name);
    nameLabel->setStyleSheet(QString("color:%1; font-size:%2px;")
                                 .arg(Style::kColorTextSecondary)
                                 .arg(Style::kFontSmall));

    *valueOut = makeValueLabel(defaultVal, color, Style::kFontLarge);

    hl->addWidget(nameLabel);
    hl->addStretch();
    hl->addWidget(*valueOut);
    return row;
}

static QProgressBar *makeGaugeBar(const char *barColor)
{
    QProgressBar *bar = new QProgressBar;
    bar->setRange(0, 100);
    bar->setValue(0);
    bar->setTextVisible(false);
    bar->setFixedHeight(8);
    bar->setStyleSheet(
        QString("QProgressBar { background:%1; border-radius:4px; }"
                "QProgressBar::chunk { background:%2; border-radius:4px; }")
            .arg(Style::kColorBgCard)
            .arg(barColor));
    return bar;
}

// ---------------------------------------------------------------------------
// DashboardPage
// ---------------------------------------------------------------------------

DashboardPage::DashboardPage(RpcClient *rpc, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpc)
{
    setupUi();
}

QFrame *DashboardPage::createGlassPanel(const QString &title)
{
    QFrame *panel = new QFrame;
    panel->setProperty("class", "glassPanel");

    QVBoxLayout *vl = new QVBoxLayout(panel);
    vl->setContentsMargins(Style::kPageMargin, 8, Style::kPageMargin, 8);
    vl->setSpacing(6);

    if (!title.isEmpty()) {
        QLabel *titleLabel = new QLabel(title);
        titleLabel->setStyleSheet(
            QString("color:%1; font-size:%2px; font-weight:bold;")
                .arg(Style::kColorAccentCyan)
                .arg(Style::kFontNormal));
        vl->addWidget(titleLabel);
    }
    return panel;
}

// ---------------------------------------------------------------------------
// setupUi
// ---------------------------------------------------------------------------

void DashboardPage::setupUi()
{
    QGridLayout *root = new QGridLayout(this);
    root->setContentsMargins(Style::kPageMargin, Style::kPageMargin,
                             Style::kPageMargin, Style::kPageMargin);
    root->setSpacing(Style::kCardSpacing);

    // ── top row stretch / bottom row stretch ─────────────
    root->setRowStretch(0, 45);
    root->setRowStretch(1, 55);
    root->setColumnStretch(0, 25);
    root->setColumnStretch(1, 50);
    root->setColumnStretch(2, 25);

    // =====================================================================
    // 1. 左面板 - 室外气象站
    // =====================================================================
    QFrame *weatherPanel = createGlassPanel(QString::fromUtf8("室外气象站"));
    QVBoxLayout *wLayout = qobject_cast<QVBoxLayout *>(weatherPanel->layout());

    wLayout->addWidget(makeWeatherRow(QString::fromUtf8("温度"),   &weatherTempValue_,     Style::kColorOrange,  QString::fromUtf8("28.5°C")));
    wLayout->addWidget(makeWeatherRow(QString::fromUtf8("湿度"),   &weatherHumidityValue_, Style::kColorBlue,    "65%"));
    wLayout->addWidget(makeWeatherRow(QString::fromUtf8("风速"),   &weatherWindValue_,     Style::kColorEmerald, "3.2 m/s"));
    wLayout->addWidget(makeWeatherRow(QString::fromUtf8("光照"),   &weatherLightValue_,    Style::kColorYellow,  "45000 lux"));
    wLayout->addWidget(makeWeatherRow(QString::fromUtf8("降雨量"), &weatherRainValue_,     Style::kColorAccentCyan, "0.0 mm"));
    wLayout->addStretch();

    root->addWidget(weatherPanel, 0, 0);

    // =====================================================================
    // 2. 中央面板 - 棚内环境监测
    // =====================================================================
    QFrame *indoorPanel = createGlassPanel(QString::fromUtf8("棚内环境监测"));
    QVBoxLayout *iLayout = qobject_cast<QVBoxLayout *>(indoorPanel->layout());

    QHBoxLayout *gaugeRow = new QHBoxLayout;
    gaugeRow->setSpacing(Style::kCardSpacing);

    struct GaugeSpec {
        const char *name;
        const char *color;
        const char *defaultVal;
        int         defaultPct;
        QLabel       **valueLbl;
        QProgressBar **bar;
    };

    GaugeSpec specs[] = {
        { "空气温度",   Style::kColorOrange, "26.8°C",    54, &indoorTempValue_,     &indoorTempBar_     },
        { "空气湿度",   Style::kColorBlue,   "72%",        72, &indoorHumidityValue_, &indoorHumidityBar_ },
        { "CO₂浓度",   Style::kColorPurple, "680 ppm",    34, &indoorCo2Value_,      &indoorCo2Bar_      },
        { "光照强度",   Style::kColorYellow, "32000 lux",  64, &indoorLightValue_,    &indoorLightBar_    },
    };

    for (auto &s : specs) {
        QVBoxLayout *col = new QVBoxLayout;
        col->setAlignment(Qt::AlignCenter);
        col->setSpacing(4);

        *s.bar = makeGaugeBar(s.color);
        (*s.bar)->setValue(s.defaultPct);

        QLabel *nameLabel = new QLabel(QString::fromUtf8(s.name));
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setStyleSheet(
            QString("color:%1; font-size:%2px;")
                .arg(Style::kColorTextSecondary)
                .arg(Style::kFontSmall));

        *s.valueLbl = new QLabel(QString::fromUtf8(s.defaultVal));
        (*s.valueLbl)->setAlignment(Qt::AlignCenter);
        (*s.valueLbl)->setStyleSheet(
            QString("color:%1; font-size:%2px; font-weight:bold;")
                .arg(s.color)
                .arg(Style::kFontXLarge));

        col->addStretch();
        col->addWidget(*s.bar);
        col->addWidget(nameLabel);
        col->addWidget(*s.valueLbl);
        col->addStretch();

        gaugeRow->addLayout(col);
    }

    iLayout->addLayout(gaugeRow, 1);

    root->addWidget(indoorPanel, 0, 1);

    // =====================================================================
    // 3. 右面板 - 紧急停止
    // =====================================================================
    QFrame *stopPanel = createGlassPanel(QString());
    QVBoxLayout *sLayout = qobject_cast<QVBoxLayout *>(stopPanel->layout());
    sLayout->setAlignment(Qt::AlignCenter);

    emergencyStopBtn_ = new QPushButton(QString::fromUtf8("紧急\n停止"));
    emergencyStopBtn_->setObjectName("emergencyStopBtn");
    emergencyStopBtn_->setFixedSize(110, 110);
    emergencyStopBtn_->setCursor(Qt::PointingHandCursor);
    emergencyStopBtn_->setStyleSheet(
        QString(
            "QPushButton {"
            "  background: qradialgradient(cx:0.5, cy:0.5, radius:0.5,"
            "    fx:0.4, fy:0.4, stop:0 #ff6b6b, stop:1 %1);"
            "  color: white;"
            "  font-size: %2px;"
            "  font-weight: bold;"
            "  border: 3px solid #ff8888;"
            "  border-radius: 55px;"
            "}"
            "QPushButton:pressed {"
            "  background: qradialgradient(cx:0.5, cy:0.5, radius:0.5,"
            "    fx:0.4, fy:0.4, stop:0 %1, stop:1 #991b1b);"
            "  border-color: #ff4444;"
            "}")
            .arg(Style::kColorDanger)
            .arg(Style::kFontLarge));

    QLabel *stopHint = new QLabel(QString::fromUtf8("停止所有设备"));
    stopHint->setAlignment(Qt::AlignCenter);
    stopHint->setStyleSheet(
        QString("color:%1; font-size:%2px;")
            .arg(Style::kColorTextMuted)
            .arg(Style::kFontTiny));

    sLayout->addWidget(emergencyStopBtn_, 0, Qt::AlignCenter);
    sLayout->addWidget(stopHint, 0, Qt::AlignCenter);

    connect(emergencyStopBtn_, &QPushButton::clicked,
            this, &DashboardPage::emergencyStopClicked);

    root->addWidget(stopPanel, 0, 2);

    // =====================================================================
    // 4. 底部 - 24小时环境趋势
    // =====================================================================
    QFrame *trendPanel = createGlassPanel(QString::fromUtf8("24小时环境趋势"));
    QVBoxLayout *tLayout = qobject_cast<QVBoxLayout *>(trendPanel->layout());

    // Legend row
    QHBoxLayout *legendRow = new QHBoxLayout;
    legendRow->setSpacing(16);

    struct LegendItem { const char *name; const char *color; };
    LegendItem legends[] = {
        { "温度", Style::kColorOrange },
        { "湿度", Style::kColorBlue   },
        { "光照", Style::kColorYellow },
    };
    for (auto &lg : legends) {
        QLabel *dot = new QLabel(QString::fromUtf8("●"));
        dot->setStyleSheet(QString("color:%1; font-size:10px;").arg(lg.color));

        QLabel *txt = new QLabel(QString::fromUtf8(lg.name));
        txt->setStyleSheet(
            QString("color:%1; font-size:%2px;")
                .arg(Style::kColorTextSecondary)
                .arg(Style::kFontSmall));

        legendRow->addWidget(dot);
        legendRow->addWidget(txt);
    }
    legendRow->addStretch();
    tLayout->addLayout(legendRow);

    // Placeholder
    trendPlaceholder_ = new QLabel(QString::fromUtf8("24小时环境趋势图表"));
    trendPlaceholder_->setAlignment(Qt::AlignCenter);
    trendPlaceholder_->setStyleSheet(
        QString("color:%1; font-size:%2px; border:1px dashed %3; border-radius:8px;")
            .arg(Style::kColorTextMuted)
            .arg(Style::kFontMedium)
            .arg(Style::kColorBorder));
    tLayout->addWidget(trendPlaceholder_, 1);

    root->addWidget(trendPanel, 1, 0, 1, 3);
}

// ---------------------------------------------------------------------------
// refreshData (stub)
// ---------------------------------------------------------------------------

void DashboardPage::refreshData()
{
    // TODO: fetch data via rpcClient_ and update labels / bars
}
