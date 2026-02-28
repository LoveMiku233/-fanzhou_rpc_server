/**
 * @file views/device_control_page.cpp
 * @brief 设备控制页面实现
 */

#include "views/device_control_page.h"
#include "rpc_client.h"
#include "style_constants.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static const char *statusText(const QString &status)
{
    if (status == "running") return "\xe8\xbf\x90\xe8\xa1\x8c";   // 运行
    if (status == "stopped") return "\xe5\x81\x9c\xe6\xad\xa2";   // 停止
    if (status == "fault")   return "\xe6\x95\x85\xe9\x9a\x9c";   // 故障
    if (status == "manual")  return "\xe6\x89\x8b\xe5\x8a\xa8";   // 手动
    return "";
}

static const char *statusClass(const QString &status)
{
    if (status == "running") return "statusRunning";
    if (status == "stopped") return "statusStopped";
    if (status == "fault")   return "statusFault";
    if (status == "manual")  return "statusManual";
    return "";
}

static QString statusBadgeStyle(const QString &status)
{
    const char *bg = Style::kColorBorder;
    if (status == "running") bg = Style::kColorSuccess;
    else if (status == "stopped") bg = "#64748b";
    else if (status == "fault")   bg = Style::kColorDanger;
    else if (status == "manual")  bg = Style::kColorWarning;

    return QString(
        "color:white; font-size:%1px; font-weight:bold; "
        "padding:1px 6px; border-radius:3px; background:%2;")
        .arg(Style::kFontTiny).arg(bg);
}

static QString groupTabColor(const QString &color)
{
    if (color == "blue")    return Style::kColorInfo;
    if (color == "emerald") return Style::kColorSuccess;
    if (color == "amber")   return Style::kColorWarning;
    if (color == "purple")  return Style::kColorPurple;
    if (color == "red")     return Style::kColorDanger;
    if (color == "cyan")    return Style::kColorAccentCyan;
    return Style::kColorAccentBlue;
}

// ---------------------------------------------------------------------------
// DeviceControlPage
// ---------------------------------------------------------------------------

DeviceControlPage::DeviceControlPage(RpcClient *rpc, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpc)
    , currentGroupIndex_(0)
{
    initDemoData();
    setupUi();
}

// ---------------------------------------------------------------------------
// initDemoData
// ---------------------------------------------------------------------------

void DeviceControlPage::initDemoData()
{
    using Models::DeviceGroup;
    using Models::DeviceInfo;

    // 卷帘组
    {
        DeviceGroup g;
        g.id = "curtain"; g.name = QString::fromUtf8("卷帘组"); g.color = "blue";

        auto mkDC = [](const char *id, const char *name, const char *st,
                       int val, const char *spec, const char *fault = "") {
            DeviceInfo d;
            d.id = id; d.name = QString::fromUtf8(name); d.type = "dc";
            d.status = st; d.value = val; d.spec = spec;
            d.fault = QString::fromUtf8(fault);
            return d;
        };
        g.devices << mkDC("c1", "南侧卷帘-1", "running", 75, "24V")
                  << mkDC("c2", "南侧卷帘-2", "stopped",  0, "24V")
                  << mkDC("c3", "北侧卷帘-1", "manual",  45, "24V")
                  << mkDC("c4", "北侧卷帘-2", "fault",    0, "24V", "电机过载")
                  << mkDC("c5", "顶部卷帘-1", "stopped",  0, "24V")
                  << mkDC("c6", "顶部卷帘-2", "running", 60, "24V");
        groups_ << g;
    }

    // 风机组
    {
        DeviceGroup g;
        g.id = "fan"; g.name = QString::fromUtf8("风机组"); g.color = "emerald";

        auto mkAC = [](const char *id, const char *name, const char *st,
                       const char *spec, const char *rt, const char *cur) {
            DeviceInfo d;
            d.id = id; d.name = QString::fromUtf8(name); d.type = "ac";
            d.status = st; d.spec = spec; d.runtime = rt; d.current = cur;
            return d;
        };
        g.devices << mkAC("f1", "风机-1",   "running", "380V 1.5kW", "04:32:18", "2.8A")
                  << mkAC("f2", "风机-2",   "stopped", "380V 1.5kW", "00:00:00", "0.0A")
                  << mkAC("f3", "环流风机", "manual",  "220V 0.75kW","08:45:33", "3.4A");
        groups_ << g;
    }

    // 遮阳网组
    {
        DeviceGroup g;
        g.id = "shade"; g.name = QString::fromUtf8("遮阳网组"); g.color = "amber";

        DeviceInfo d1; d1.id = "s1"; d1.name = QString::fromUtf8("外遮阳网");
        d1.type = "dc"; d1.status = "running"; d1.value = 60; d1.spec = QString::fromUtf8("推杆驱动");
        DeviceInfo d2; d2.id = "s2"; d2.name = QString::fromUtf8("内遮阳网");
        d2.type = "dc"; d2.status = "stopped"; d2.value = 0; d2.spec = QString::fromUtf8("推杆驱动");

        g.devices << d1 << d2;
        groups_ << g;
    }

    // 灌溉组
    {
        DeviceGroup g;
        g.id = "irrigation"; g.name = QString::fromUtf8("灌溉组"); g.color = "cyan";

        DeviceInfo d1; d1.id = "i1"; d1.name = QString::fromUtf8("滴灌区-A");
        d1.type = "ac"; d1.status = "running"; d1.spec = QString::fromUtf8("电磁阀");
        d1.flow = "1250L"; d1.pressure = "0.25MPa";
        DeviceInfo d2; d2.id = "i2"; d2.name = QString::fromUtf8("喷雾系统");
        d2.type = "ac"; d2.status = "manual"; d2.spec = QString::fromUtf8("高压泵 2.2kW");
        d2.flow = "850L"; d2.pressure = "3.5MPa";

        g.devices << d1 << d2;
        groups_ << g;
    }
}

// ---------------------------------------------------------------------------
// setupUi
// ---------------------------------------------------------------------------

void DeviceControlPage::setupUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Top tab bar (40px) ──────────────────────────────
    QWidget *tabBar = new QWidget;
    tabBar->setFixedHeight(40);
    tabBar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;")
            .arg(Style::kColorBgPanel).arg(Style::kColorBorder));

    QHBoxLayout *tabBarLayout = new QHBoxLayout(tabBar);
    tabBarLayout->setContentsMargins(4, 0, 4, 0);
    tabBarLayout->setSpacing(4);

    // Left scroll button
    scrollLeftBtn_ = new QPushButton(QString::fromUtf8("◀"));
    scrollLeftBtn_->setFixedSize(24, 28);
    scrollLeftBtn_->setCursor(Qt::PointingHandCursor);
    scrollLeftBtn_->setStyleSheet(
        QString("QPushButton { background:%1; color:white; border:none; border-radius:4px; font-size:10px; }"
                "QPushButton:hover { background:%2; }")
            .arg(Style::kColorBgCard).arg(Style::kColorBorderLight));
    tabBarLayout->addWidget(scrollLeftBtn_);

    // Scrollable tab area
    tabScrollArea_ = new QScrollArea;
    tabScrollArea_->setWidgetResizable(true);
    tabScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tabScrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tabScrollArea_->setFixedHeight(34);
    tabScrollArea_->setStyleSheet("QScrollArea { background:transparent; border:none; }");

    QWidget *tabWidget = new QWidget;
    tabLayout_ = new QHBoxLayout(tabWidget);
    tabLayout_->setContentsMargins(0, 0, 0, 0);
    tabLayout_->setSpacing(4);
    tabLayout_->addStretch();
    tabScrollArea_->setWidget(tabWidget);

    tabBarLayout->addWidget(tabScrollArea_, 1);

    // Right scroll button
    scrollRightBtn_ = new QPushButton(QString::fromUtf8("▶"));
    scrollRightBtn_->setFixedSize(24, 28);
    scrollRightBtn_->setCursor(Qt::PointingHandCursor);
    scrollRightBtn_->setStyleSheet(scrollLeftBtn_->styleSheet());
    tabBarLayout->addWidget(scrollRightBtn_);

    // Separator
    QFrame *sep = new QFrame;
    sep->setFrameShape(QFrame::VLine);
    sep->setFixedHeight(24);
    sep->setStyleSheet(QString("color:%1;").arg(Style::kColorBorder));
    tabBarLayout->addWidget(sep);

    // 添加分组
    addGroupBtn_ = new QPushButton(QString::fromUtf8("+ 添加分组"));
    addGroupBtn_->setFixedHeight(28);
    addGroupBtn_->setCursor(Qt::PointingHandCursor);
    addGroupBtn_->setStyleSheet(
        QString("QPushButton { background:%1; color:white; border:none; border-radius:6px;"
                "  font-size:%2px; padding:0 10px; }"
                "QPushButton:hover { background:%3; }")
            .arg(Style::kColorBgCard).arg(Style::kFontSmall).arg(Style::kColorBorderLight));
    tabBarLayout->addWidget(addGroupBtn_);

    // 添加设备
    addDeviceBtn_ = new QPushButton(QString::fromUtf8("+ 添加设备"));
    addDeviceBtn_->setFixedHeight(28);
    addDeviceBtn_->setCursor(Qt::PointingHandCursor);
    addDeviceBtn_->setStyleSheet(
        QString("QPushButton { background:%1; color:white; border:none; border-radius:6px;"
                "  font-size:%2px; padding:0 10px; }"
                "QPushButton:hover { background:%3; }")
            .arg(Style::kColorAccentBlue).arg(Style::kFontSmall).arg(Style::kColorGradientEnd));
    tabBarLayout->addWidget(addDeviceBtn_);

    root->addWidget(tabBar);

    // ── Scroll connections ─────────────────────────────
    connect(scrollLeftBtn_, &QPushButton::clicked, this, [this]() {
        tabScrollArea_->horizontalScrollBar()->setValue(
            tabScrollArea_->horizontalScrollBar()->value() - 120);
    });
    connect(scrollRightBtn_, &QPushButton::clicked, this, [this]() {
        tabScrollArea_->horizontalScrollBar()->setValue(
            tabScrollArea_->horizontalScrollBar()->value() + 120);
    });

    // ── Device card scroll area ─────────────────────────
    cardScrollArea_ = new QScrollArea;
    cardScrollArea_->setWidgetResizable(true);
    cardScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    cardScrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    cardScrollArea_->setStyleSheet(
        QString("QScrollArea { background:%1; border:none; }"
                "QScrollBar:vertical { width:6px; background:%1; }"
                "QScrollBar::handle:vertical { background:%2; border-radius:3px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }")
            .arg(Style::kColorBgDark).arg(Style::kColorBorder));

    cardContainer_ = new QWidget;
    cardScrollArea_->setWidget(cardContainer_);

    root->addWidget(cardScrollArea_, 1);

    // ── Populate ────────────────────────────────────────
    renderGroupTabs();
    renderDevices();
}

// ---------------------------------------------------------------------------
// renderGroupTabs
// ---------------------------------------------------------------------------

void DeviceControlPage::renderGroupTabs()
{
    // Remove old tab buttons (keep the trailing stretch)
    qDeleteAll(tabButtons_);
    tabButtons_.clear();

    // Remove all items except stretch
    QLayoutItem *item;
    while ((item = tabLayout_->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    for (int i = 0; i < groups_.size(); ++i) {
        const Models::DeviceGroup &g = groups_[i];
        QPushButton *btn = new QPushButton(g.name);
        btn->setFixedHeight(28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("class", (i == currentGroupIndex_) ? "groupTabActive" : "groupTab");

        QString accent = groupTabColor(g.color);
        if (i == currentGroupIndex_) {
            btn->setStyleSheet(
                QString("QPushButton { background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
                        "  stop:0 %1, stop:1 %2); color:white; border:none;"
                        "  border-radius:6px; font-size:%3px; font-weight:bold; padding:0 14px; }")
                    .arg(Style::kColorGradientStart).arg(Style::kColorGradientEnd)
                    .arg(Style::kFontSmall));
        } else {
            btn->setStyleSheet(
                QString("QPushButton { background:transparent; color:%1; border:none;"
                        "  border-radius:6px; font-size:%2px; padding:0 14px; }"
                        "QPushButton:hover { color:white; background:%3; }")
                    .arg(Style::kColorTextSecondary).arg(Style::kFontSmall)
                    .arg(Style::kColorBgCard));
        }

        connect(btn, &QPushButton::clicked, this, [this, i]() {
            currentGroupIndex_ = i;
            renderGroupTabs();
            renderDevices();
        });

        tabLayout_->addWidget(btn);
        tabButtons_ << btn;
    }
    tabLayout_->addStretch();
}

// ---------------------------------------------------------------------------
// renderDevices
// ---------------------------------------------------------------------------

void DeviceControlPage::renderDevices()
{
    if (cardContainer_->layout()) {
        QLayout *old = cardContainer_->layout();
        QLayoutItem *child;
        while ((child = old->takeAt(0)) != nullptr) {
            if (child->widget()) child->widget()->deleteLater();
            delete child;
        }
        delete old;
    }

    QGridLayout *grid = new QGridLayout(cardContainer_);
    grid->setContentsMargins(Style::kPageMargin, Style::kPageMargin,
                             Style::kPageMargin, Style::kPageMargin);
    grid->setSpacing(Style::kCardSpacing);

    if (currentGroupIndex_ < 0 || currentGroupIndex_ >= groups_.size())
        return;

    const Models::DeviceGroup &group = groups_[currentGroupIndex_];
    const int cols = 3;

    for (int i = 0; i < group.devices.size(); ++i) {
        const Models::DeviceInfo &dev = group.devices[i];
        QWidget *card = (dev.type == "dc")
                            ? createDCDeviceCard(dev)
                            : createACDeviceCard(dev);
        grid->addWidget(card, i / cols, i % cols);
    }

    // Fill remaining cells in the last row with spacers
    int rem = group.devices.size() % cols;
    if (rem != 0) {
        for (int c = rem; c < cols; ++c)
            grid->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum),
                          (group.devices.size() - 1) / cols, c);
    }
    grid->setRowStretch(group.devices.size() / cols + 1, 1);
}

// ---------------------------------------------------------------------------
// createDCDeviceCard
// ---------------------------------------------------------------------------

QWidget *DeviceControlPage::createDCDeviceCard(const Models::DeviceInfo &dev)
{
    QFrame *card = new QFrame;
    card->setProperty("class", "deviceCard");
    card->setStyleSheet(
        QString("QFrame[class=\"deviceCard\"] {"
                "  background:%1; border-radius:%2px;"
                "  border-left:4px solid %3; }")
            .arg(Style::kColorBgPanel).arg(Style::kCardRadius)
            .arg(Style::kColorInfo));

    QVBoxLayout *vl = new QVBoxLayout(card);
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(6);

    // ── Header: name + status badge ──
    QHBoxLayout *header = new QHBoxLayout;
    header->setSpacing(6);

    QVBoxLayout *nameCol = new QVBoxLayout;
    nameCol->setSpacing(0);

    QLabel *nameLabel = new QLabel(dev.name);
    nameLabel->setStyleSheet(
        QString("color:white; font-size:%1px; font-weight:bold;").arg(Style::kFontSmall));

    QLabel *specLabel = new QLabel(QString("DC | %1").arg(dev.spec));
    specLabel->setStyleSheet(
        QString("color:%1; font-size:%2px;").arg(Style::kColorTextMuted).arg(Style::kFontTiny));

    nameCol->addWidget(nameLabel);
    nameCol->addWidget(specLabel);

    QLabel *badge = new QLabel(QString::fromUtf8(statusText(dev.status)));
    badge->setProperty("class", statusClass(dev.status));
    badge->setStyleSheet(statusBadgeStyle(dev.status));
    badge->setFixedHeight(18);

    header->addLayout(nameCol, 1);
    header->addWidget(badge, 0, Qt::AlignTop);
    vl->addLayout(header);

    // ── Value row ──
    bool isFault = (dev.status == "fault");

    QHBoxLayout *valRow = new QHBoxLayout;
    QLabel *valLabel = new QLabel(isFault
                                     ? QString::fromUtf8("状态")
                                     : QString::fromUtf8("开度"));
    valLabel->setStyleSheet(
        QString("color:%1; font-size:%2px;").arg(Style::kColorTextMuted).arg(Style::kFontSmall));

    QLabel *valNum = new QLabel(isFault ? "--" : QString("%1%").arg(dev.value));
    valNum->setStyleSheet(
        QString("color:%1; font-size:%2px; font-weight:bold;")
            .arg(isFault ? Style::kColorDanger : Style::kColorAccentCyan)
            .arg(Style::kFontSmall));

    valRow->addWidget(valLabel);
    valRow->addStretch();
    valRow->addWidget(valNum);
    vl->addLayout(valRow);

    // ── Slider ──
    QSlider *slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, 100);
    slider->setValue(dev.value);
    slider->setEnabled(!isFault);
    slider->setFixedHeight(16);
    slider->setStyleSheet(
        QString("QSlider::groove:horizontal {"
                "  background:%1; height:6px; border-radius:3px; }"
                "QSlider::handle:horizontal {"
                "  background:%2; width:14px; height:14px; margin:-4px 0;"
                "  border-radius:7px; }"
                "QSlider::sub-page:horizontal {"
                "  background:%2; border-radius:3px; }"
                "QSlider:disabled::handle:horizontal { background:%3; }"
                "QSlider:disabled::sub-page:horizontal { background:%3; }")
            .arg(Style::kColorBgCard).arg(Style::kColorAccentCyan).arg(Style::kColorBorder));
    vl->addWidget(slider);

    // Slider <-> label binding
    QString devId = dev.id;
    connect(slider, &QSlider::valueChanged, this, [this, valNum, devId](int v) {
        valNum->setText(QString("%1%").arg(v));
        emit deviceValueChanged(devId, v);
    });

    // ── Fault info ──
    if (!dev.fault.isEmpty()) {
        QLabel *faultLabel = new QLabel(dev.fault);
        faultLabel->setStyleSheet(
            QString("color:%1; font-size:%2px;").arg(Style::kColorDanger).arg(Style::kFontTiny));
        vl->addWidget(faultLabel);
    }

    return card;
}

// ---------------------------------------------------------------------------
// createACDeviceCard
// ---------------------------------------------------------------------------

QWidget *DeviceControlPage::createACDeviceCard(const Models::DeviceInfo &dev)
{
    QFrame *card = new QFrame;
    card->setProperty("class", "deviceCard");
    card->setStyleSheet(
        QString("QFrame[class=\"deviceCard\"] {"
                "  background:%1; border-radius:%2px;"
                "  border-left:4px solid %3; }")
            .arg(Style::kColorBgPanel).arg(Style::kCardRadius)
            .arg(Style::kColorSuccess));

    QVBoxLayout *vl = new QVBoxLayout(card);
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(4);

    // ── Header ──
    QHBoxLayout *header = new QHBoxLayout;
    header->setSpacing(6);

    QVBoxLayout *nameCol = new QVBoxLayout;
    nameCol->setSpacing(0);

    QLabel *nameLabel = new QLabel(dev.name);
    nameLabel->setStyleSheet(
        QString("color:white; font-size:%1px; font-weight:bold;").arg(Style::kFontSmall));

    QLabel *specLabel = new QLabel(dev.spec);
    specLabel->setStyleSheet(
        QString("color:%1; font-size:%2px;").arg(Style::kColorTextMuted).arg(Style::kFontTiny));

    nameCol->addWidget(nameLabel);
    nameCol->addWidget(specLabel);

    QLabel *badge = new QLabel(QString::fromUtf8(statusText(dev.status)));
    badge->setProperty("class", statusClass(dev.status));
    badge->setStyleSheet(statusBadgeStyle(dev.status));
    badge->setFixedHeight(18);

    header->addLayout(nameCol, 1);
    header->addWidget(badge, 0, Qt::AlignTop);
    vl->addLayout(header);

    // ── Info rows ──
    auto addInfoRow = [&](const QString &label, const QString &value,
                          const char *valueColor) {
        QHBoxLayout *row = new QHBoxLayout;
        QLabel *lbl = new QLabel(label);
        lbl->setStyleSheet(
            QString("color:%1; font-size:%2px;")
                .arg(Style::kColorTextMuted).arg(Style::kFontSmall));
        QLabel *val = new QLabel(value);
        val->setStyleSheet(
            QString("color:%1; font-size:%2px;").arg(valueColor).arg(Style::kFontSmall));
        row->addWidget(lbl);
        row->addStretch();
        row->addWidget(val);
        vl->addLayout(row);
    };

    if (!dev.runtime.isEmpty()) {
        const char *rtColor = (dev.status == "running")
                                  ? Style::kColorEmerald : Style::kColorTextMuted;
        addInfoRow(QString::fromUtf8("运行时间"), dev.runtime, rtColor);
        addInfoRow(QString::fromUtf8("电流"),
                   dev.current.isEmpty() ? "--" : dev.current,
                   Style::kColorAccentCyan);
    }
    if (!dev.flow.isEmpty()) {
        addInfoRow(QString::fromUtf8("流量"), dev.flow, Style::kColorAccentCyan);
        addInfoRow(QString::fromUtf8("压力"), dev.pressure, Style::kColorAccentCyan);
    }

    // ── Start / Stop buttons ──
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    bool canStop  = (dev.status == "running" || dev.status == "manual");
    bool canStart = (dev.status == "stopped");

    QPushButton *stopBtn = new QPushButton(QString::fromUtf8("停止"));
    stopBtn->setFixedHeight(Style::kBtnHeightSmall);
    stopBtn->setCursor(canStop ? Qt::PointingHandCursor : Qt::ForbiddenCursor);
    stopBtn->setEnabled(canStop);
    stopBtn->setStyleSheet(
        canStop
            ? QString("QPushButton { background:%1; color:white; border:none;"
                      "  border-radius:4px; font-size:%2px; font-weight:bold; }"
                      "QPushButton:hover { background:#dc2626; }")
                  .arg(Style::kColorDanger).arg(Style::kFontSmall)
            : QString("QPushButton { background:%1; color:%2; border:none;"
                      "  border-radius:4px; font-size:%3px; }")
                  .arg(Style::kColorBgCard).arg(Style::kColorTextMuted).arg(Style::kFontSmall));

    QPushButton *startBtn = new QPushButton(QString::fromUtf8("启动"));
    startBtn->setFixedHeight(Style::kBtnHeightSmall);
    startBtn->setCursor(canStart ? Qt::PointingHandCursor : Qt::ForbiddenCursor);
    startBtn->setEnabled(canStart);
    startBtn->setStyleSheet(
        canStart
            ? QString("QPushButton { background:%1; color:white; border:none;"
                      "  border-radius:4px; font-size:%2px; font-weight:bold; }"
                      "QPushButton:hover { background:#059669; }")
                  .arg(Style::kColorSuccess).arg(Style::kFontSmall)
            : QString("QPushButton { background:%1; color:%2; border:none;"
                      "  border-radius:4px; font-size:%3px; }")
                  .arg(Style::kColorBgCard).arg(Style::kColorTextMuted).arg(Style::kFontSmall));

    btnRow->addWidget(stopBtn, 1);
    btnRow->addWidget(startBtn, 1);
    vl->addLayout(btnRow);

    return card;
}

// ---------------------------------------------------------------------------
// refreshData (stub)
// ---------------------------------------------------------------------------

void DeviceControlPage::refreshData()
{
    // TODO: fetch data via rpcClient_ and update groups_ / re-render
}
