/**
 * @file views/alarm_page.cpp
 * @brief 报警看板页面实现
 */

#include "views/alarm_page.h"
#include "rpc_client.h"
#include "style_constants.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static const char *levelColor(const QString &level)
{
    if (level == "critical") return Style::kColorDanger;
    if (level == "warning")  return Style::kColorWarning;
    return Style::kColorInfo;
}

static QString levelText(const QString &level)
{
    if (level == "critical") return QString::fromUtf8("紧急");
    if (level == "warning")  return QString::fromUtf8("警告");
    return QString::fromUtf8("提示");
}

static QPushButton *makeTabButton(const QString &text, const char *color)
{
    QPushButton *btn = new QPushButton(text);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; font-size:%2px;"
                "  font-weight:bold; border:none; padding:6px 12px;"
                "  border-bottom:2px solid transparent; }"
                "QPushButton:checked { border-bottom:2px solid %1; color:white; }")
            .arg(color)
            .arg(Style::kFontNormal));
    btn->setCheckable(true);
    return btn;
}

// ---------------------------------------------------------------------------
// AlarmPage
// ---------------------------------------------------------------------------

AlarmPage::AlarmPage(RpcClient *rpc, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpc)
    , currentFilter_("all")
{
    // Demo data matching index3.html
    {
        Models::AlarmInfo a;
        a.id = 1; a.level = "critical";
        a.title = QString::fromUtf8("设备故障");
        a.device = QString::fromUtf8("北侧卷帘-2");
        a.desc = QString::fromUtf8("电机过载保护触发");
        a.time = "2024-01-15 14:23:15";
        a.duration = QString::fromUtf8("45分钟");
        a.confirmed = false;
        alarms_.append(a);
    }
    {
        Models::AlarmInfo a;
        a.id = 2; a.level = "critical";
        a.title = QString::fromUtf8("通讯中断");
        a.device = QString::fromUtf8("风机-4");
        a.desc = QString::fromUtf8("控制柜通讯超时 (>30s)");
        a.time = "2024-01-15 14:20:03";
        a.duration = QString::fromUtf8("48分钟");
        a.confirmed = false;
        alarms_.append(a);
    }
    {
        Models::AlarmInfo a;
        a.id = 3; a.level = "warning";
        a.title = QString::fromUtf8("压力异常");
        a.device = QString::fromUtf8("滴灌区-A");
        a.desc = QString::fromUtf8("压力低于设定值 (0.15 < 0.20 MPa)");
        a.time = "2024-01-15 14:10:22";
        a.duration = QString::fromUtf8("58分钟");
        a.confirmed = false;
        alarms_.append(a);
    }
    {
        Models::AlarmInfo a;
        a.id = 4; a.level = "warning";
        a.title = QString::fromUtf8("能耗预警");
        a.device = QString::fromUtf8("系统");
        a.desc = QString::fromUtf8("今日用电量超过昨日同期 15%");
        a.time = "2024-01-15 13:45:00";
        a.duration = "--";
        a.confirmed = false;
        alarms_.append(a);
    }
    {
        Models::AlarmInfo a;
        a.id = 5; a.level = "info";
        a.title = QString::fromUtf8("维护提醒");
        a.device = QString::fromUtf8("喷雾系统");
        a.desc = QString::fromUtf8("滤芯建议更换 (已运行 500 小时)");
        a.time = "2024-01-15 09:00:00";
        a.duration = "--";
        a.confirmed = true;
        alarms_.append(a);
    }

    setupUi();
    renderAlarms();
    updateStats();
}

// ---------------------------------------------------------------------------
// setupUi
// ---------------------------------------------------------------------------

void AlarmPage::setupUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Top tab bar ──────────────────────────────
    QWidget *tabBar = new QWidget;
    tabBar->setStyleSheet(
        QString("background:rgba(30,41,59,0.3); border-bottom:1px solid %1;")
            .arg(Style::kColorBorder));

    QHBoxLayout *tabLayout = new QHBoxLayout(tabBar);
    tabLayout->setContentsMargins(Style::kPageMargin, 4, Style::kPageMargin, 0);
    tabLayout->setSpacing(2);

    tabAll_      = makeTabButton(QString::fromUtf8("全部 (5)"),  Style::kColorTextPrimary);
    tabCritical_ = makeTabButton(QString::fromUtf8("紧急 (2)"), Style::kColorDanger);
    tabWarning_  = makeTabButton(QString::fromUtf8("警告 (2)"), Style::kColorWarning);
    tabInfo_     = makeTabButton(QString::fromUtf8("提示 (1)"),  Style::kColorInfo);
    tabAll_->setChecked(true);

    tabLayout->addWidget(tabAll_);
    tabLayout->addWidget(tabCritical_);
    tabLayout->addWidget(tabWarning_);
    tabLayout->addWidget(tabInfo_);
    tabLayout->addStretch();

    confirmAllBtn_ = new QPushButton(QString::fromUtf8("一键确认"));
    confirmAllBtn_->setCursor(Qt::PointingHandCursor);
    confirmAllBtn_->setFixedHeight(Style::kBtnHeightSmall);
    confirmAllBtn_->setStyleSheet(
        QString("QPushButton { background:%1; color:white; font-size:%2px;"
                "  border:none; border-radius:6px; padding:0 12px; }"
                "QPushButton:hover { background:%3; }")
            .arg(Style::kColorBgCard)
            .arg(Style::kFontSmall)
            .arg(Style::kColorBorderLight));
    tabLayout->addWidget(confirmAllBtn_);

    root->addWidget(tabBar);

    // ── Scroll area ──────────────────────────────
    scrollArea_ = new QScrollArea;
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setFrameShape(QFrame::NoFrame);
    scrollArea_->setStyleSheet(
        QString("QScrollArea { background:transparent; }"
                "QScrollBar:vertical { width:6px; background:transparent; }"
                "QScrollBar::handle:vertical { background:%1; border-radius:3px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }")
            .arg(Style::kColorBorderLight));

    listContainer_ = new QWidget;
    listLayout_ = new QVBoxLayout(listContainer_);
    listLayout_->setContentsMargins(Style::kPageMargin, Style::kPageMargin,
                                    Style::kPageMargin, Style::kPageMargin);
    listLayout_->setSpacing(6);
    listLayout_->addStretch();

    scrollArea_->setWidget(listContainer_);
    root->addWidget(scrollArea_, 1);

    // ── Bottom stats bar ─────────────────────────
    QWidget *statsBar = new QWidget;
    statsBar->setStyleSheet(
        QString("background:rgba(30,41,59,0.3); border-top:1px solid %1;")
            .arg(Style::kColorBorder));

    QHBoxLayout *statsLayout = new QHBoxLayout(statsBar);
    statsLayout->setContentsMargins(Style::kPageMargin, 6, Style::kPageMargin, 6);
    statsLayout->setSpacing(24);

    // Critical dot + label
    QLabel *critDot = new QLabel(QString::fromUtf8("●"));
    critDot->setStyleSheet(QString("color:%1; font-size:8px;").arg(Style::kColorDanger));
    QLabel *critText = new QLabel(QString::fromUtf8("未确认紧急:"));
    critText->setStyleSheet(
        QString("color:%1; font-size:%2px;")
            .arg(Style::kColorTextSecondary).arg(Style::kFontSmall));
    criticalCountLabel_ = new QLabel("2");
    criticalCountLabel_->setStyleSheet(
        QString("color:%1; font-size:%2px; font-weight:bold;")
            .arg(Style::kColorDanger).arg(Style::kFontSmall));

    // Warning dot + label
    QLabel *warnDot = new QLabel(QString::fromUtf8("●"));
    warnDot->setStyleSheet(QString("color:%1; font-size:8px;").arg(Style::kColorWarning));
    QLabel *warnText = new QLabel(QString::fromUtf8("未确认警告:"));
    warnText->setStyleSheet(
        QString("color:%1; font-size:%2px;")
            .arg(Style::kColorTextSecondary).arg(Style::kFontSmall));
    warningCountLabel_ = new QLabel("2");
    warningCountLabel_->setStyleSheet(
        QString("color:%1; font-size:%2px; font-weight:bold;")
            .arg(Style::kColorWarning).arg(Style::kFontSmall));

    statsLayout->addWidget(critDot);
    statsLayout->addWidget(critText);
    statsLayout->addWidget(criticalCountLabel_);
    statsLayout->addSpacing(12);
    statsLayout->addWidget(warnDot);
    statsLayout->addWidget(warnText);
    statsLayout->addWidget(warningCountLabel_);
    statsLayout->addStretch();

    root->addWidget(statsBar);

    // ── Connections ──────────────────────────────
    auto switchTab = [this](const QString &filter, QPushButton *active) {
        currentFilter_ = filter;
        tabAll_->setChecked(false);
        tabCritical_->setChecked(false);
        tabWarning_->setChecked(false);
        tabInfo_->setChecked(false);
        active->setChecked(true);
        renderAlarms();
    };

    connect(tabAll_,      &QPushButton::clicked, [=]() { switchTab("all",      tabAll_);      });
    connect(tabCritical_, &QPushButton::clicked, [=]() { switchTab("critical", tabCritical_); });
    connect(tabWarning_,  &QPushButton::clicked, [=]() { switchTab("warning",  tabWarning_);  });
    connect(tabInfo_,     &QPushButton::clicked, [=]() { switchTab("info",     tabInfo_);     });

    connect(confirmAllBtn_, &QPushButton::clicked, [this]() {
        for (int i = 0; i < alarms_.size(); ++i)
            alarms_[i].confirmed = true;
        renderAlarms();
        updateStats();
    });
}

// ---------------------------------------------------------------------------
// renderAlarms
// ---------------------------------------------------------------------------

void AlarmPage::renderAlarms()
{
    // Remove old items (keep trailing stretch)
    while (listLayout_->count() > 1) {
        QLayoutItem *item = listLayout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    for (int i = 0; i < alarms_.size(); ++i) {
        const Models::AlarmInfo &a = alarms_.at(i);
        if (currentFilter_ != "all" && a.level != currentFilter_)
            continue;
        listLayout_->insertWidget(listLayout_->count() - 1, createAlarmItem(a));
    }
}

// ---------------------------------------------------------------------------
// createAlarmItem
// ---------------------------------------------------------------------------

QWidget *AlarmPage::createAlarmItem(const Models::AlarmInfo &alarm)
{
    const char *color = levelColor(alarm.level);

    QFrame *card = new QFrame;
    card->setStyleSheet(
        QString("QFrame { background:rgba(30,41,59,0.7); border:1px solid %1;"
                "  border-left:4px solid %2; border-radius:8px; }")
            .arg(Style::kColorBorder)
            .arg(color));

    QHBoxLayout *row = new QHBoxLayout(card);
    row->setContentsMargins(10, 8, 10, 8);
    row->setSpacing(10);

    // ── Left icon circle ─────────────────────────
    QLabel *icon = new QLabel(QString::fromUtf8("⚠"));
    icon->setFixedSize(36, 36);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(
        QString("background:rgba(%1,0.15); color:%2; font-size:16px;"
                " border-radius:18px; border:none;")
            .arg(alarm.level == "critical" ? "239,68,68"
                 : alarm.level == "warning" ? "245,158,11" : "59,130,246")
            .arg(color));
    row->addWidget(icon);

    // ── Center info ──────────────────────────────
    QVBoxLayout *info = new QVBoxLayout;
    info->setSpacing(2);

    // Title row: title + level badge + status
    QHBoxLayout *titleRow = new QHBoxLayout;
    titleRow->setSpacing(6);

    QLabel *titleLbl = new QLabel(alarm.title);
    titleLbl->setStyleSheet(
        QString("color:white; font-size:%1px; font-weight:bold; border:none;")
            .arg(Style::kFontNormal));

    QLabel *badge = new QLabel(levelText(alarm.level));
    badge->setStyleSheet(
        QString("background:rgba(%1,0.2); color:%2; font-size:%3px;"
                " padding:1px 6px; border-radius:4px; border:none;")
            .arg(alarm.level == "critical" ? "239,68,68"
                 : alarm.level == "warning" ? "245,158,11" : "59,130,246")
            .arg(color)
            .arg(Style::kFontTiny));

    QLabel *statusLbl = new QLabel(alarm.confirmed
        ? QString::fromUtf8("已处理") : QString::fromUtf8("未处理"));
    statusLbl->setStyleSheet(
        QString("color:%1; font-size:%2px; border:none;")
            .arg(alarm.confirmed ? Style::kColorEmerald : Style::kColorTextMuted)
            .arg(Style::kFontTiny));

    titleRow->addWidget(titleLbl);
    titleRow->addWidget(badge);
    titleRow->addWidget(statusLbl);
    titleRow->addStretch();
    info->addLayout(titleRow);

    // Device + desc
    QLabel *descLbl = new QLabel(alarm.device + " - " + alarm.desc);
    descLbl->setStyleSheet(
        QString("color:%1; font-size:%2px; border:none;")
            .arg(Style::kColorTextPrimary).arg(Style::kFontSmall));
    info->addWidget(descLbl);

    // Time + duration
    QLabel *timeLbl = new QLabel(
        alarm.time + QString::fromUtf8(" • 持续 ") + alarm.duration);
    timeLbl->setStyleSheet(
        QString("color:%1; font-size:%2px; border:none;")
            .arg(Style::kColorTextMuted).arg(Style::kFontTiny));
    info->addWidget(timeLbl);

    row->addLayout(info, 1);

    // ── Right: confirm button or label ───────────
    if (!alarm.confirmed) {
        QPushButton *btn = new QPushButton(QString::fromUtf8("确认"));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedSize(52, 28);
        const char *btnColor = (alarm.level == "critical")
            ? Style::kColorDanger : Style::kColorWarning;
        btn->setStyleSheet(
            QString("QPushButton { background:%1; color:white; font-size:%2px;"
                    "  border:none; border-radius:4px; }"
                    "QPushButton:hover { opacity:0.85; }")
                .arg(btnColor)
                .arg(Style::kFontSmall));

        int alarmId = alarm.id;
        connect(btn, &QPushButton::clicked, [this, alarmId]() {
            for (int i = 0; i < alarms_.size(); ++i) {
                if (alarms_[i].id == alarmId) {
                    alarms_[i].confirmed = true;
                    break;
                }
            }
            renderAlarms();
            updateStats();
        });
        row->addWidget(btn);
    } else {
        QLabel *lbl = new QLabel(QString::fromUtf8("已确认"));
        lbl->setStyleSheet(
            QString("background:%1; color:%2; font-size:%3px;"
                    " padding:4px 10px; border-radius:4px; border:none;")
                .arg(Style::kColorBgCard)
                .arg(Style::kColorTextMuted)
                .arg(Style::kFontSmall));
        row->addWidget(lbl);
    }

    return card;
}

// ---------------------------------------------------------------------------
// updateStats
// ---------------------------------------------------------------------------

void AlarmPage::updateStats()
{
    int uncCritical = 0;
    int uncWarning  = 0;
    int totalUnconfirmed = 0;

    for (int i = 0; i < alarms_.size(); ++i) {
        if (!alarms_.at(i).confirmed) {
            ++totalUnconfirmed;
            if (alarms_.at(i).level == "critical") ++uncCritical;
            else if (alarms_.at(i).level == "warning") ++uncWarning;
        }
    }

    criticalCountLabel_->setText(QString::number(uncCritical));
    warningCountLabel_->setText(QString::number(uncWarning));

    // Update tab text with counts
    int allCount  = alarms_.size();
    int critCount = 0, warnCount = 0, infoCount = 0;
    for (int i = 0; i < alarms_.size(); ++i) {
        if (alarms_.at(i).level == "critical") ++critCount;
        else if (alarms_.at(i).level == "warning") ++warnCount;
        else ++infoCount;
    }
    tabAll_->setText(QString::fromUtf8("全部 (%1)").arg(allCount));
    tabCritical_->setText(QString::fromUtf8("紧急 (%1)").arg(critCount));
    tabWarning_->setText(QString::fromUtf8("警告 (%1)").arg(warnCount));
    tabInfo_->setText(QString::fromUtf8("提示 (%1)").arg(infoCount));

    emit alarmCountChanged(totalUnconfirmed);
}

// ---------------------------------------------------------------------------
// refreshData (stub)
// ---------------------------------------------------------------------------

void AlarmPage::refreshData()
{
    // TODO: fetch alarm data via rpcClient_ and update alarms_
}
