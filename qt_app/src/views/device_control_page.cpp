/**
 * @file views/device_control_page.cpp
 * @brief 设备控制页面实现
 *
 * 支持从RPC Server获取分组数据，支持多种控件类型（滑块、双态、正反转），
 * 提供添加/删除分组、添加/删除/编辑设备等操作界面。
 */

#include "views/device_control_page.h"
#include "rpc_client.h"
#include "style_constants.h"

#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
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

// Common card frame style with consistent background
static QString cardFrameStyle(const char *accentColor)
{
    return QString(
        "QFrame[class=\"deviceCard\"] {"
        "  background:%1; border-radius:%2px;"
        "  border:1px solid %3; border-left:3px solid %4; }")
        .arg(Style::kColorBgPanel).arg(Style::kCardRadius)
        .arg(Style::kColorBorder).arg(accentColor);
}

// Small icon-button used in card headers
static QPushButton *createSmallIconBtn(const QString &text, const char *color,
                                       const char *hoverColor)
{
    QPushButton *btn = new QPushButton(text);
    btn->setFixedSize(22, 22);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:none;"
                "  font-size:12px; border-radius:4px; padding:0; }"
                "QPushButton:hover { background:%2; color:white; }")
            .arg(color).arg(hoverColor));
    return btn;
}

// Dialog stylesheet for consistent dark theme
static QString dialogStyle()
{
    return QString(
        "QDialog { background:%1; }"
        "QLabel { color:%2; font-size:%3px; background:transparent; }"
        "QLineEdit { background:%4; color:%2; border:1px solid %5;"
        "  border-radius:4px; padding:4px 8px; font-size:%3px; }"
        "QLineEdit:focus { border-color:%6; }"
        "QComboBox { background:%4; color:%2; border:1px solid %5;"
        "  border-radius:4px; padding:4px 8px; font-size:%3px; }"
        "QComboBox::drop-down { border:none; width:20px; }"
        "QComboBox::down-arrow { border-left:4px solid transparent;"
        "  border-right:4px solid transparent; border-top:5px solid %7; margin-right:6px; }"
        "QComboBox QAbstractItemView { background:%4; color:%2;"
        "  border:1px solid %5; selection-background-color:%6; }"
        "QPushButton { background:%5; color:%2; border:none;"
        "  border-radius:4px; padding:6px 16px; font-size:%3px; min-height:28px; }"
        "QPushButton:hover { background:%8; }")
        .arg(Style::kColorBgPanel)     // %1
        .arg(Style::kColorTextPrimary) // %2
        .arg(Style::kFontSmall)        // %3
        .arg(Style::kColorBgCard)      // %4
        .arg(Style::kColorBorder)      // %5
        .arg(Style::kColorAccentBlue)  // %6
        .arg(Style::kColorTextMuted)   // %7
        .arg(Style::kColorBorderLight);// %8
}

// ---------------------------------------------------------------------------
// DeviceControlPage
// ---------------------------------------------------------------------------

DeviceControlPage::DeviceControlPage(RpcClient *rpc, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpc)
    , currentGroupIndex_(0)
    , deleteGroupBtn_(nullptr)
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

        auto mkDev = [](const char *id, const char *name, const char *st,
                       int val, const char *spec, const char *ct,
                       const char *fault = "") {
            DeviceInfo d;
            d.id = id; d.name = QString::fromUtf8(name); d.type = "dc";
            d.status = st; d.value = val; d.spec = spec;
            d.controlType = ct;
            d.fault = QString::fromUtf8(fault);
            return d;
        };
        g.devices << mkDev("c1", "南侧卷帘-1", "running", 75, "24V", "slider")
                  << mkDev("c2", "南侧卷帘-2", "stopped",  0, "24V", "slider")
                  << mkDev("c3", "北侧卷帘-1", "manual",  45, "24V", "forward_reverse")
                  << mkDev("c4", "北侧卷帘-2", "fault",    0, "24V", "slider", "电机过载")
                  << mkDev("c5", "顶部卷帘-1", "stopped",  0, "24V", "toggle")
                  << mkDev("c6", "顶部卷帘-2", "running", 60, "24V", "slider");
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
            d.controlType = "forward_reverse";
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
        d1.type = "dc"; d1.status = "running"; d1.value = 60;
        d1.spec = QString::fromUtf8("推杆驱动"); d1.controlType = "slider";
        DeviceInfo d2; d2.id = "s2"; d2.name = QString::fromUtf8("内遮阳网");
        d2.type = "dc"; d2.status = "stopped"; d2.value = 0;
        d2.spec = QString::fromUtf8("推杆驱动"); d2.controlType = "toggle";

        g.devices << d1 << d2;
        groups_ << g;
    }

    // 灌溉组
    {
        DeviceGroup g;
        g.id = "irrigation"; g.name = QString::fromUtf8("灌溉组"); g.color = "cyan";

        DeviceInfo d1; d1.id = "i1"; d1.name = QString::fromUtf8("滴灌区-A");
        d1.type = "ac"; d1.status = "running"; d1.spec = QString::fromUtf8("电磁阀");
        d1.flow = "1250L"; d1.pressure = "0.25MPa"; d1.controlType = "toggle";
        DeviceInfo d2; d2.id = "i2"; d2.name = QString::fromUtf8("喷雾系统");
        d2.type = "ac"; d2.status = "manual"; d2.spec = QString::fromUtf8("高压泵 2.2kW");
        d2.flow = "850L"; d2.pressure = "3.5MPa"; d2.controlType = "forward_reverse";

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
        QString("QPushButton { background:%1; color:%2; border:none; border-radius:4px; font-size:10px; }"
                "QPushButton:hover { background:%3; color:white; }")
            .arg(Style::kColorBgCard).arg(Style::kColorTextSecondary).arg(Style::kColorBorderLight));
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
    addGroupBtn_ = new QPushButton(QString::fromUtf8("+ 分组"));
    addGroupBtn_->setFixedHeight(28);
    addGroupBtn_->setCursor(Qt::PointingHandCursor);
    addGroupBtn_->setStyleSheet(
        QString("QPushButton { background:%1; color:%2; border:none; border-radius:6px;"
                "  font-size:%3px; padding:0 10px; }"
                "QPushButton:hover { background:%4; color:white; }")
            .arg(Style::kColorBgCard).arg(Style::kColorTextSecondary)
            .arg(Style::kFontSmall).arg(Style::kColorBorderLight));
    tabBarLayout->addWidget(addGroupBtn_);

    // 删除分组
    deleteGroupBtn_ = new QPushButton(QString::fromUtf8("- 分组"));
    deleteGroupBtn_->setFixedHeight(28);
    deleteGroupBtn_->setCursor(Qt::PointingHandCursor);
    deleteGroupBtn_->setStyleSheet(
        QString("QPushButton { background:%1; color:%2; border:none; border-radius:6px;"
                "  font-size:%3px; padding:0 10px; }"
                "QPushButton:hover { background:%4; color:white; }")
            .arg(Style::kColorBgCard).arg(Style::kColorTextMuted)
            .arg(Style::kFontSmall).arg(Style::kColorDanger));
    tabBarLayout->addWidget(deleteGroupBtn_);

    // 添加设备
    addDeviceBtn_ = new QPushButton(QString::fromUtf8("+ 设备"));
    addDeviceBtn_->setFixedHeight(28);
    addDeviceBtn_->setCursor(Qt::PointingHandCursor);
    addDeviceBtn_->setStyleSheet(
        QString("QPushButton { background:%1; color:white; border:none; border-radius:6px;"
                "  font-size:%2px; padding:0 10px; }"
                "QPushButton:hover { background:%3; }")
            .arg(Style::kColorAccentBlue).arg(Style::kFontSmall).arg(Style::kColorGradientEnd));
    tabBarLayout->addWidget(addDeviceBtn_);

    root->addWidget(tabBar);

    // ── Button connections ─────────────────────────────
    connect(scrollLeftBtn_, &QPushButton::clicked, this, [this]() {
        tabScrollArea_->horizontalScrollBar()->setValue(
            tabScrollArea_->horizontalScrollBar()->value() - 120);
    });
    connect(scrollRightBtn_, &QPushButton::clicked, this, [this]() {
        tabScrollArea_->horizontalScrollBar()->setValue(
            tabScrollArea_->horizontalScrollBar()->value() + 120);
    });
    connect(addGroupBtn_, &QPushButton::clicked, this, &DeviceControlPage::onAddGroup);
    connect(deleteGroupBtn_, &QPushButton::clicked, this, &DeviceControlPage::onDeleteGroup);
    connect(addDeviceBtn_, &QPushButton::clicked, this, &DeviceControlPage::onAddDevice);

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
    cardContainer_->setStyleSheet(
        QString("background:%1;").arg(Style::kColorBgDark));
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

    // Update delete group button state
    if (deleteGroupBtn_) {
        deleteGroupBtn_->setEnabled(!groups_.isEmpty());
    }
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
        QWidget *card = createDeviceCard(dev);
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
// createDeviceCard - dispatch by controlType
// ---------------------------------------------------------------------------

QWidget *DeviceControlPage::createDeviceCard(const Models::DeviceInfo &dev)
{
    if (dev.controlType == "toggle")
        return createToggleCard(dev);
    if (dev.controlType == "forward_reverse")
        return createForwardReverseCard(dev);
    if (dev.type == "ac")
        return createACDeviceCard(dev);
    // Default: slider card
    return createSliderCard(dev);
}

// ---------------------------------------------------------------------------
// createCardHeader - common header for all card types
// ---------------------------------------------------------------------------

QHBoxLayout *DeviceControlPage::createCardHeader(const Models::DeviceInfo &dev,
                                                  QVBoxLayout *parent)
{
    QHBoxLayout *header = new QHBoxLayout;
    header->setSpacing(4);

    QVBoxLayout *nameCol = new QVBoxLayout;
    nameCol->setSpacing(0);

    QLabel *nameLabel = new QLabel(dev.name);
    nameLabel->setStyleSheet(
        QString("color:white; font-size:%1px; font-weight:bold; background:transparent;")
            .arg(Style::kFontSmall));

    QString specStr = dev.spec;
    if (!dev.type.isEmpty())
        specStr = QString("%1 | %2").arg(dev.type.toUpper()).arg(dev.spec);
    QLabel *specLabel = new QLabel(specStr);
    specLabel->setStyleSheet(
        QString("color:%1; font-size:%2px; background:transparent;")
            .arg(Style::kColorTextMuted).arg(Style::kFontTiny));

    nameCol->addWidget(nameLabel);
    nameCol->addWidget(specLabel);

    QLabel *badge = new QLabel(QString::fromUtf8(statusText(dev.status)));
    badge->setProperty("class", statusClass(dev.status));
    badge->setStyleSheet(statusBadgeStyle(dev.status));
    badge->setFixedHeight(18);

    // Edit button
    QPushButton *editBtn = createSmallIconBtn(
        QString::fromUtf8("✎"), Style::kColorTextMuted, Style::kColorAccentBlue);
    QString devId = dev.id;
    connect(editBtn, &QPushButton::clicked, this, [this, devId]() {
        onEditDevice(devId);
    });

    // Delete button
    QPushButton *delBtn = createSmallIconBtn(
        QString::fromUtf8("✕"), Style::kColorTextMuted, Style::kColorDanger);
    connect(delBtn, &QPushButton::clicked, this, [this, devId]() {
        onDeleteDevice(devId);
    });

    header->addLayout(nameCol, 1);
    header->addWidget(badge, 0, Qt::AlignTop);
    header->addWidget(editBtn, 0, Qt::AlignTop);
    header->addWidget(delBtn, 0, Qt::AlignTop);

    parent->addLayout(header);
    return header;
}

// ---------------------------------------------------------------------------
// createSliderCard
// ---------------------------------------------------------------------------

QWidget *DeviceControlPage::createSliderCard(const Models::DeviceInfo &dev)
{
    QFrame *card = new QFrame;
    card->setProperty("class", "deviceCard");
    card->setStyleSheet(cardFrameStyle(Style::kColorInfo));

    QVBoxLayout *vl = new QVBoxLayout(card);
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(6);

    // ── Header ──
    createCardHeader(dev, vl);

    // ── Value row ──
    bool isFault = (dev.status == "fault");

    QHBoxLayout *valRow = new QHBoxLayout;
    QLabel *valLabel = new QLabel(isFault
                                     ? QString::fromUtf8("状态")
                                     : QString::fromUtf8("开度"));
    valLabel->setStyleSheet(
        QString("color:%1; font-size:%2px; background:transparent;")
            .arg(Style::kColorTextMuted).arg(Style::kFontSmall));

    QLabel *valNum = new QLabel(isFault ? "--" : QString("%1%").arg(dev.value));
    valNum->setStyleSheet(
        QString("color:%1; font-size:%2px; font-weight:bold; background:transparent;")
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
            QString("color:%1; font-size:%2px; background:transparent;")
                .arg(Style::kColorDanger).arg(Style::kFontTiny));
        vl->addWidget(faultLabel);
    }

    return card;
}

// ---------------------------------------------------------------------------
// createToggleCard
// ---------------------------------------------------------------------------

QWidget *DeviceControlPage::createToggleCard(const Models::DeviceInfo &dev)
{
    QFrame *card = new QFrame;
    card->setProperty("class", "deviceCard");
    card->setStyleSheet(cardFrameStyle(Style::kColorSuccess));

    QVBoxLayout *vl = new QVBoxLayout(card);
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(6);

    // ── Header ──
    createCardHeader(dev, vl);

    // ── Info rows (if AC device) ──
    if (!dev.flow.isEmpty()) {
        auto addInfoRow = [&](const QString &label, const QString &value, const char *vc) {
            QHBoxLayout *row = new QHBoxLayout;
            QLabel *lbl = new QLabel(label);
            lbl->setStyleSheet(
                QString("color:%1; font-size:%2px; background:transparent;")
                    .arg(Style::kColorTextMuted).arg(Style::kFontSmall));
            QLabel *val = new QLabel(value);
            val->setStyleSheet(
                QString("color:%1; font-size:%2px; background:transparent;")
                    .arg(vc).arg(Style::kFontSmall));
            row->addWidget(lbl);
            row->addStretch();
            row->addWidget(val);
            vl->addLayout(row);
        };
        addInfoRow(QString::fromUtf8("流量"), dev.flow, Style::kColorAccentCyan);
        addInfoRow(QString::fromUtf8("压力"), dev.pressure, Style::kColorAccentCyan);
    }

    // ── Toggle button (开/关) ──
    bool isOn = (dev.status == "running" || dev.status == "manual");
    bool isFault = (dev.status == "fault");

    QPushButton *toggleBtn = new QPushButton(
        isOn ? QString::fromUtf8("● 已开启") : QString::fromUtf8("○ 已关闭"));
    toggleBtn->setFixedHeight(Style::kBtnHeightSmall);
    toggleBtn->setCursor(isFault ? Qt::ForbiddenCursor : Qt::PointingHandCursor);
    toggleBtn->setEnabled(!isFault);

    if (isOn) {
        toggleBtn->setStyleSheet(
            QString("QPushButton { background:%1; color:white; border:none;"
                    "  border-radius:6px; font-size:%2px; font-weight:bold; }"
                    "QPushButton:hover { background:#059669; }")
                .arg(Style::kColorSuccess).arg(Style::kFontSmall));
    } else {
        toggleBtn->setStyleSheet(
            QString("QPushButton { background:%1; color:%2; border:1px solid %3;"
                    "  border-radius:6px; font-size:%4px; }"
                    "QPushButton:hover { background:%3; color:white; }")
                .arg(Style::kColorBgCard).arg(Style::kColorTextSecondary)
                .arg(Style::kColorBorder).arg(Style::kFontSmall));
    }

    QString devId = dev.id;
    int nodeId = dev.nodeId;
    int channel = dev.channel;
    connect(toggleBtn, &QPushButton::clicked, this, [this, devId, isOn, nodeId, channel]() {
        if (rpcClient_ && rpcClient_->isConnected() && nodeId >= 0 && channel >= 0) {
            QJsonObject params;
            params[QStringLiteral("node")] = nodeId;
            params[QStringLiteral("ch")] = channel;
            params[QStringLiteral("action")] = isOn ? QStringLiteral("stop")
                                                     : QStringLiteral("fwd");
            rpcClient_->callAsync(QStringLiteral("relay.control"), params,
                [this](const QJsonValue &, const QJsonObject &) {
                    refreshData();
                }, 3000);
        }
    });
    vl->addWidget(toggleBtn);

    // ── Fault info ──
    if (!dev.fault.isEmpty()) {
        QLabel *faultLabel = new QLabel(dev.fault);
        faultLabel->setStyleSheet(
            QString("color:%1; font-size:%2px; background:transparent;")
                .arg(Style::kColorDanger).arg(Style::kFontTiny));
        vl->addWidget(faultLabel);
    }

    return card;
}

// ---------------------------------------------------------------------------
// createForwardReverseCard
// ---------------------------------------------------------------------------

QWidget *DeviceControlPage::createForwardReverseCard(const Models::DeviceInfo &dev)
{
    QFrame *card = new QFrame;
    card->setProperty("class", "deviceCard");
    card->setStyleSheet(cardFrameStyle(Style::kColorWarning));

    QVBoxLayout *vl = new QVBoxLayout(card);
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(6);

    // ── Header ──
    createCardHeader(dev, vl);

    // ── Info rows (if AC device) ──
    if (!dev.runtime.isEmpty()) {
        auto addInfoRow = [&](const QString &label, const QString &value, const char *vc) {
            QHBoxLayout *row = new QHBoxLayout;
            QLabel *lbl = new QLabel(label);
            lbl->setStyleSheet(
                QString("color:%1; font-size:%2px; background:transparent;")
                    .arg(Style::kColorTextMuted).arg(Style::kFontSmall));
            QLabel *val = new QLabel(value);
            val->setStyleSheet(
                QString("color:%1; font-size:%2px; background:transparent;")
                    .arg(vc).arg(Style::kFontSmall));
            row->addWidget(lbl);
            row->addStretch();
            row->addWidget(val);
            vl->addLayout(row);
        };
        const char *rtColor = (dev.status == "running")
                                  ? Style::kColorEmerald : Style::kColorTextMuted;
        addInfoRow(QString::fromUtf8("运行时间"), dev.runtime, rtColor);
        addInfoRow(QString::fromUtf8("电流"),
                   dev.current.isEmpty() ? "--" : dev.current,
                   Style::kColorAccentCyan);
    }

    // ── Forward / Stop / Reverse buttons ──
    bool isFault = (dev.status == "fault");

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->setSpacing(6);

    QPushButton *revBtn = new QPushButton(QString::fromUtf8("◀ 反转"));
    revBtn->setFixedHeight(Style::kBtnHeightSmall);
    revBtn->setCursor(isFault ? Qt::ForbiddenCursor : Qt::PointingHandCursor);
    revBtn->setEnabled(!isFault);
    revBtn->setStyleSheet(
        !isFault
            ? QString("QPushButton { background:%1; color:white; border:none;"
                      "  border-radius:4px; font-size:%2px; font-weight:bold; }"
                      "QPushButton:hover { background:%3; }")
                  .arg(Style::kColorWarning).arg(Style::kFontSmall).arg("#d97706")
            : QString("QPushButton { background:%1; color:%2; border:none;"
                      "  border-radius:4px; font-size:%3px; }")
                  .arg(Style::kColorBgCard).arg(Style::kColorTextMuted).arg(Style::kFontSmall));

    QPushButton *stopBtn = new QPushButton(QString::fromUtf8("■ 停止"));
    stopBtn->setFixedHeight(Style::kBtnHeightSmall);
    stopBtn->setCursor(isFault ? Qt::ForbiddenCursor : Qt::PointingHandCursor);
    stopBtn->setEnabled(!isFault);
    stopBtn->setStyleSheet(
        !isFault
            ? QString("QPushButton { background:%1; color:white; border:none;"
                      "  border-radius:4px; font-size:%2px; font-weight:bold; }"
                      "QPushButton:hover { background:#dc2626; }")
                  .arg(Style::kColorDanger).arg(Style::kFontSmall)
            : QString("QPushButton { background:%1; color:%2; border:none;"
                      "  border-radius:4px; font-size:%3px; }")
                  .arg(Style::kColorBgCard).arg(Style::kColorTextMuted).arg(Style::kFontSmall));

    QPushButton *fwdBtn = new QPushButton(QString::fromUtf8("正转 ▶"));
    fwdBtn->setFixedHeight(Style::kBtnHeightSmall);
    fwdBtn->setCursor(isFault ? Qt::ForbiddenCursor : Qt::PointingHandCursor);
    fwdBtn->setEnabled(!isFault);
    fwdBtn->setStyleSheet(
        !isFault
            ? QString("QPushButton { background:%1; color:white; border:none;"
                      "  border-radius:4px; font-size:%2px; font-weight:bold; }"
                      "QPushButton:hover { background:#059669; }")
                  .arg(Style::kColorSuccess).arg(Style::kFontSmall)
            : QString("QPushButton { background:%1; color:%2; border:none;"
                      "  border-radius:4px; font-size:%3px; }")
                  .arg(Style::kColorBgCard).arg(Style::kColorTextMuted).arg(Style::kFontSmall));

    QString devId = dev.id;
    int nodeId = dev.nodeId;
    int channel = dev.channel;
    connect(revBtn, &QPushButton::clicked, this, [this, nodeId, channel]() {
        if (rpcClient_ && rpcClient_->isConnected() && nodeId >= 0 && channel >= 0) {
            QJsonObject params;
            params[QStringLiteral("node")] = nodeId;
            params[QStringLiteral("ch")] = channel;
            params[QStringLiteral("action")] = QStringLiteral("rev");
            rpcClient_->callAsync(QStringLiteral("relay.control"), params);
        }
    });
    connect(stopBtn, &QPushButton::clicked, this, [this, nodeId, channel]() {
        if (rpcClient_ && rpcClient_->isConnected() && nodeId >= 0 && channel >= 0) {
            QJsonObject params;
            params[QStringLiteral("node")] = nodeId;
            params[QStringLiteral("ch")] = channel;
            params[QStringLiteral("action")] = QStringLiteral("stop");
            rpcClient_->callAsync(QStringLiteral("relay.control"), params);
        }
    });
    connect(fwdBtn, &QPushButton::clicked, this, [this, nodeId, channel]() {
        if (rpcClient_ && rpcClient_->isConnected() && nodeId >= 0 && channel >= 0) {
            QJsonObject params;
            params[QStringLiteral("node")] = nodeId;
            params[QStringLiteral("ch")] = channel;
            params[QStringLiteral("action")] = QStringLiteral("fwd");
            rpcClient_->callAsync(QStringLiteral("relay.control"), params);
        }
    });

    btnRow->addWidget(revBtn, 1);
    btnRow->addWidget(stopBtn, 1);
    btnRow->addWidget(fwdBtn, 1);
    vl->addLayout(btnRow);

    // ── Fault info ──
    if (!dev.fault.isEmpty()) {
        QLabel *faultLabel = new QLabel(dev.fault);
        faultLabel->setStyleSheet(
            QString("color:%1; font-size:%2px; background:transparent;")
                .arg(Style::kColorDanger).arg(Style::kFontTiny));
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
    card->setStyleSheet(cardFrameStyle(Style::kColorSuccess));

    QVBoxLayout *vl = new QVBoxLayout(card);
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(4);

    // ── Header ──
    createCardHeader(dev, vl);

    // ── Info rows ──
    auto addInfoRow = [&](const QString &label, const QString &value,
                          const char *valueColor) {
        QHBoxLayout *row = new QHBoxLayout;
        QLabel *lbl = new QLabel(label);
        lbl->setStyleSheet(
            QString("color:%1; font-size:%2px; background:transparent;")
                .arg(Style::kColorTextMuted).arg(Style::kFontSmall));
        QLabel *val = new QLabel(value);
        val->setStyleSheet(
            QString("color:%1; font-size:%2px; background:transparent;")
                .arg(valueColor).arg(Style::kFontSmall));
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

    QString devId = dev.id;
    int nodeId = dev.nodeId;
    int channel = dev.channel;
    connect(stopBtn, &QPushButton::clicked, this, [this, nodeId, channel]() {
        if (rpcClient_ && rpcClient_->isConnected() && nodeId >= 0 && channel >= 0) {
            QJsonObject params;
            params[QStringLiteral("node")] = nodeId;
            params[QStringLiteral("ch")] = channel;
            params[QStringLiteral("action")] = QStringLiteral("stop");
            rpcClient_->callAsync(QStringLiteral("relay.control"), params);
        }
    });
    connect(startBtn, &QPushButton::clicked, this, [this, nodeId, channel]() {
        if (rpcClient_ && rpcClient_->isConnected() && nodeId >= 0 && channel >= 0) {
            QJsonObject params;
            params[QStringLiteral("node")] = nodeId;
            params[QStringLiteral("ch")] = channel;
            params[QStringLiteral("action")] = QStringLiteral("fwd");
            rpcClient_->callAsync(QStringLiteral("relay.control"), params);
        }
    });

    btnRow->addWidget(stopBtn, 1);
    btnRow->addWidget(startBtn, 1);
    vl->addLayout(btnRow);

    return card;
}

// ---------------------------------------------------------------------------
// onAddGroup
// ---------------------------------------------------------------------------

void DeviceControlPage::onAddGroup()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("添加分组"));
    dlg.setFixedSize(320, 200);
    dlg.setStyleSheet(dialogStyle());

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->setSpacing(12);
    layout->setContentsMargins(16, 16, 16, 16);

    QLabel *title = new QLabel(QString::fromUtf8("新建设备分组"));
    title->setStyleSheet(
        QString("color:white; font-size:%1px; font-weight:bold; background:transparent;")
            .arg(Style::kFontMedium));
    layout->addWidget(title);

    QFormLayout *form = new QFormLayout;
    form->setSpacing(8);

    QLineEdit *nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText(QString::fromUtf8("输入分组名称"));
    form->addRow(QString::fromUtf8("名称:"), nameEdit);

    QComboBox *colorBox = new QComboBox;
    colorBox->addItems({
        QString::fromUtf8("蓝色 (blue)"),
        QString::fromUtf8("绿色 (emerald)"),
        QString::fromUtf8("琥珀 (amber)"),
        QString::fromUtf8("紫色 (purple)"),
        QString::fromUtf8("红色 (red)"),
        QString::fromUtf8("青色 (cyan)")
    });
    form->addRow(QString::fromUtf8("颜色:"), colorBox);

    layout->addLayout(form);

    QHBoxLayout *btnRow = new QHBoxLayout;
    QPushButton *cancelBtn = new QPushButton(QString::fromUtf8("取消"));
    QPushButton *okBtn = new QPushButton(QString::fromUtf8("确定"));
    okBtn->setStyleSheet(
        QString("QPushButton { background:%1; color:white; border:none;"
                "  border-radius:4px; padding:6px 16px; font-size:%2px; }"
                "QPushButton:hover { background:%3; }")
            .arg(Style::kColorAccentBlue).arg(Style::kFontSmall).arg(Style::kColorGradientEnd));

    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    layout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) return;

    static const char *colorKeys[] = {"blue","emerald","amber","purple","red","cyan"};
    int idx = colorBox->currentIndex();
    QString color = (idx >= 0 && idx < 6) ? colorKeys[idx] : "blue";

    Models::DeviceGroup g;
    g.id = QString("group_%1").arg(groups_.size() + 1);
    g.name = name;
    g.color = color;

    // RPC: group.create
    if (rpcClient_ && rpcClient_->isConnected()) {
        QJsonObject params;
        params[QStringLiteral("name")] = name;
        rpcClient_->callAsync(QStringLiteral("group.create"), params);
    }
    groups_ << g;
    currentGroupIndex_ = groups_.size() - 1;
    renderGroupTabs();
    renderDevices();
}

// ---------------------------------------------------------------------------
// onDeleteGroup
// ---------------------------------------------------------------------------

void DeviceControlPage::onDeleteGroup()
{
    if (groups_.isEmpty() || currentGroupIndex_ < 0 ||
        currentGroupIndex_ >= groups_.size())
        return;

    const Models::DeviceGroup &g = groups_[currentGroupIndex_];

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QString::fromUtf8("删除分组"));
    msgBox.setText(QString::fromUtf8("确定要删除分组「%1」吗？\n该操作不可撤销。").arg(g.name));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setStyleSheet(dialogStyle());

    if (msgBox.exec() != QMessageBox::Yes)
        return;

    // RPC: group.delete
    if (rpcClient_ && rpcClient_->isConnected()) {
        QJsonObject params;
        params[QStringLiteral("groupId")] = g.id.toInt();
        rpcClient_->callAsync(QStringLiteral("group.delete"), params);
    }
    groups_.removeAt(currentGroupIndex_);
    if (currentGroupIndex_ >= groups_.size())
        currentGroupIndex_ = groups_.size() - 1;
    if (currentGroupIndex_ < 0)
        currentGroupIndex_ = 0;

    renderGroupTabs();
    renderDevices();
}

// ---------------------------------------------------------------------------
// onAddDevice
// ---------------------------------------------------------------------------

void DeviceControlPage::onAddDevice()
{
    if (groups_.isEmpty()) return;

    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("添加设备"));
    dlg.setFixedSize(360, 320);
    dlg.setStyleSheet(dialogStyle());

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->setSpacing(12);
    layout->setContentsMargins(16, 16, 16, 16);

    QLabel *title = new QLabel(QString::fromUtf8("添加新设备"));
    title->setStyleSheet(
        QString("color:white; font-size:%1px; font-weight:bold; background:transparent;")
            .arg(Style::kFontMedium));
    layout->addWidget(title);

    QFormLayout *form = new QFormLayout;
    form->setSpacing(8);

    QLineEdit *nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText(QString::fromUtf8("输入设备名称"));
    form->addRow(QString::fromUtf8("名称:"), nameEdit);

    QComboBox *typeBox = new QComboBox;
    typeBox->addItem(QString::fromUtf8("直流设备 (DC)"), "dc");
    typeBox->addItem(QString::fromUtf8("交流设备 (AC)"), "ac");
    form->addRow(QString::fromUtf8("类型:"), typeBox);

    QComboBox *controlBox = new QComboBox;
    controlBox->addItem(QString::fromUtf8("滑块 (Slider)"), "slider");
    controlBox->addItem(QString::fromUtf8("双态按钮 (Toggle)"), "toggle");
    controlBox->addItem(QString::fromUtf8("正反转按钮 (Forward/Reverse)"), "forward_reverse");
    form->addRow(QString::fromUtf8("控件:"), controlBox);

    QLineEdit *specEdit = new QLineEdit;
    specEdit->setPlaceholderText(QString::fromUtf8("如: 24V, 380V 1.5kW"));
    form->addRow(QString::fromUtf8("规格:"), specEdit);

    QLineEdit *nodeEdit = new QLineEdit;
    nodeEdit->setPlaceholderText(QString::fromUtf8("继电器节点ID（可选）"));
    form->addRow(QString::fromUtf8("节点:"), nodeEdit);

    QLineEdit *channelEdit = new QLineEdit;
    channelEdit->setPlaceholderText(QString::fromUtf8("通道号 0-3（可选）"));
    form->addRow(QString::fromUtf8("通道:"), channelEdit);

    layout->addLayout(form);

    QHBoxLayout *btnRow = new QHBoxLayout;
    QPushButton *cancelBtn = new QPushButton(QString::fromUtf8("取消"));
    QPushButton *okBtn = new QPushButton(QString::fromUtf8("确定"));
    okBtn->setStyleSheet(
        QString("QPushButton { background:%1; color:white; border:none;"
                "  border-radius:4px; padding:6px 16px; font-size:%2px; }"
                "QPushButton:hover { background:%3; }")
            .arg(Style::kColorAccentBlue).arg(Style::kFontSmall).arg(Style::kColorGradientEnd));

    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    layout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) return;

    Models::DeviceInfo dev;
    dev.id = QString("dev_%1_%2").arg(groups_[currentGroupIndex_].id)
                 .arg(groups_[currentGroupIndex_].devices.size() + 1);
    dev.name = name;
    dev.type = typeBox->currentData().toString();
    dev.controlType = controlBox->currentData().toString();
    dev.spec = specEdit->text().trimmed();
    dev.status = "stopped";
    dev.value = 0;

    bool nodeOk = false;
    int node = nodeEdit->text().toInt(&nodeOk);
    if (nodeOk && node >= 0) dev.nodeId = node;

    bool chOk = false;
    int ch = channelEdit->text().toInt(&chOk);
    if (chOk && ch >= 0 && ch <= 3) dev.channel = ch;

    // RPC: group.addDevice
    if (rpcClient_ && rpcClient_->isConnected()) {
        QJsonObject params;
        params[QStringLiteral("groupId")] = groups_[currentGroupIndex_].id.toInt();
        params[QStringLiteral("node")] = dev.nodeId;
        rpcClient_->callAsync(QStringLiteral("group.addDevice"), params);
    }
    groups_[currentGroupIndex_].devices << dev;
    renderDevices();
}

// ---------------------------------------------------------------------------
// onDeleteDevice
// ---------------------------------------------------------------------------

void DeviceControlPage::onDeleteDevice(const QString &deviceId)
{
    if (currentGroupIndex_ < 0 || currentGroupIndex_ >= groups_.size())
        return;

    Models::DeviceGroup &group = groups_[currentGroupIndex_];

    // Find device name for confirmation
    QString devName;
    for (const auto &d : group.devices) {
        if (d.id == deviceId) {
            devName = d.name;
            break;
        }
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QString::fromUtf8("删除设备"));
    msgBox.setText(QString::fromUtf8("确定要删除设备「%1」吗？").arg(devName));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setStyleSheet(dialogStyle());

    if (msgBox.exec() != QMessageBox::Yes)
        return;

    // RPC: group.removeDevice
    if (rpcClient_ && rpcClient_->isConnected()) {
        for (const Models::DeviceInfo &d : group.devices) {
            if (d.id == deviceId) {
                QJsonObject params;
                params[QStringLiteral("groupId")] = group.id.toInt();
                params[QStringLiteral("node")] = d.nodeId;
                rpcClient_->callAsync(QStringLiteral("group.removeDevice"), params);
                break;
            }
        }
    }
    for (int i = 0; i < group.devices.size(); ++i) {
        if (group.devices[i].id == deviceId) {
            group.devices.removeAt(i);
            break;
        }
    }
    renderDevices();
}

// ---------------------------------------------------------------------------
// onEditDevice
// ---------------------------------------------------------------------------

void DeviceControlPage::onEditDevice(const QString &deviceId)
{
    if (currentGroupIndex_ < 0 || currentGroupIndex_ >= groups_.size())
        return;

    Models::DeviceGroup &group = groups_[currentGroupIndex_];

    int devIdx = -1;
    for (int i = 0; i < group.devices.size(); ++i) {
        if (group.devices[i].id == deviceId) {
            devIdx = i;
            break;
        }
    }
    if (devIdx < 0) return;

    Models::DeviceInfo &dev = group.devices[devIdx];

    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("编辑设备"));
    dlg.setFixedSize(360, 320);
    dlg.setStyleSheet(dialogStyle());

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->setSpacing(12);
    layout->setContentsMargins(16, 16, 16, 16);

    QLabel *title = new QLabel(QString::fromUtf8("编辑设备控制"));
    title->setStyleSheet(
        QString("color:white; font-size:%1px; font-weight:bold; background:transparent;")
            .arg(Style::kFontMedium));
    layout->addWidget(title);

    QFormLayout *form = new QFormLayout;
    form->setSpacing(8);

    QLineEdit *nameEdit = new QLineEdit(dev.name);
    form->addRow(QString::fromUtf8("名称:"), nameEdit);

    QComboBox *typeBox = new QComboBox;
    typeBox->addItem(QString::fromUtf8("直流设备 (DC)"), "dc");
    typeBox->addItem(QString::fromUtf8("交流设备 (AC)"), "ac");
    typeBox->setCurrentIndex(dev.type == "ac" ? 1 : 0);
    form->addRow(QString::fromUtf8("类型:"), typeBox);

    QComboBox *controlBox = new QComboBox;
    controlBox->addItem(QString::fromUtf8("滑块 (Slider)"), "slider");
    controlBox->addItem(QString::fromUtf8("双态按钮 (Toggle)"), "toggle");
    controlBox->addItem(QString::fromUtf8("正反转按钮 (Forward/Reverse)"), "forward_reverse");
    int ctIdx = 0;
    if (dev.controlType == "toggle") ctIdx = 1;
    else if (dev.controlType == "forward_reverse") ctIdx = 2;
    controlBox->setCurrentIndex(ctIdx);
    form->addRow(QString::fromUtf8("控件:"), controlBox);

    QLineEdit *specEdit = new QLineEdit(dev.spec);
    form->addRow(QString::fromUtf8("规格:"), specEdit);

    QLineEdit *nodeEdit = new QLineEdit(dev.nodeId >= 0 ? QString::number(dev.nodeId) : "");
    nodeEdit->setPlaceholderText(QString::fromUtf8("继电器节点ID（可选）"));
    form->addRow(QString::fromUtf8("节点:"), nodeEdit);

    QLineEdit *channelEdit = new QLineEdit(dev.channel >= 0 ? QString::number(dev.channel) : "");
    channelEdit->setPlaceholderText(QString::fromUtf8("通道号 0-3（可选）"));
    form->addRow(QString::fromUtf8("通道:"), channelEdit);

    layout->addLayout(form);

    QHBoxLayout *btnRow = new QHBoxLayout;
    QPushButton *cancelBtn = new QPushButton(QString::fromUtf8("取消"));
    QPushButton *okBtn = new QPushButton(QString::fromUtf8("保存"));
    okBtn->setStyleSheet(
        QString("QPushButton { background:%1; color:white; border:none;"
                "  border-radius:4px; padding:6px 16px; font-size:%2px; }"
                "QPushButton:hover { background:%3; }")
            .arg(Style::kColorAccentBlue).arg(Style::kFontSmall).arg(Style::kColorGradientEnd));

    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    layout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QString newName = nameEdit->text().trimmed();
    if (!newName.isEmpty()) dev.name = newName;
    dev.type = typeBox->currentData().toString();
    dev.controlType = controlBox->currentData().toString();
    dev.spec = specEdit->text().trimmed();

    bool nodeOk = false;
    int node = nodeEdit->text().toInt(&nodeOk);
    dev.nodeId = nodeOk ? node : -1;

    bool chOk = false;
    int ch = channelEdit->text().toInt(&chOk);
    dev.channel = (chOk && ch >= 0 && ch <= 3) ? ch : -1;

    renderDevices();
}

// ---------------------------------------------------------------------------
// onGroupListReceived  (RPC callback)
// ---------------------------------------------------------------------------

void DeviceControlPage::onGroupListReceived(const QJsonValue &result,
                                             const QJsonObject &error)
{
    if (!error.isEmpty() || !result.isObject())
        return;

    QJsonObject obj = result.toObject();
    if (!obj.value("ok").toBool())
        return;

    QJsonArray groupsArr = obj.value("groups").toArray();

    groups_.clear();
    for (const QJsonValue &gv : groupsArr) {
        QJsonObject go = gv.toObject();
        Models::DeviceGroup g;
        g.id = QString::number(go.value("groupId").toInt());
        g.name = go.value("name").toString();
        g.color = "blue"; // TODO: RPC server does not include color yet; default to blue

        QJsonArray devices = go.value("devices").toArray();
        for (const QJsonValue &dv : devices) {
            Models::DeviceInfo dev;
            dev.nodeId = dv.toInt();
            dev.id = QString("node_%1").arg(dev.nodeId);
            dev.name = QString::fromUtf8("设备 %1").arg(dev.nodeId);
            dev.type = "ac";
            dev.status = "stopped";
            dev.controlType = "forward_reverse";
            g.devices << dev;
        }

        QJsonArray channels = go.value("channels").toArray();
        for (const QJsonValue &cv : channels) {
            QJsonObject co = cv.toObject();
            Models::DeviceInfo dev;
            dev.nodeId = co.value("node").toInt();
            dev.channel = co.value("channel").toInt();
            dev.id = QString("ch_%1_%2").arg(dev.nodeId).arg(dev.channel);
            dev.name = QString::fromUtf8("通道 %1-%2").arg(dev.nodeId).arg(dev.channel);
            dev.type = "dc";
            dev.status = "stopped";
            dev.controlType = "forward_reverse";
            g.devices << dev;
        }

        groups_ << g;
    }

    if (currentGroupIndex_ >= groups_.size())
        currentGroupIndex_ = groups_.isEmpty() ? 0 : groups_.size() - 1;

    renderGroupTabs();
    renderDevices();
}

// ---------------------------------------------------------------------------
// refreshData
// ---------------------------------------------------------------------------

void DeviceControlPage::refreshData()
{
    if (!rpcClient_ || !rpcClient_->isConnected())
        return;

    rpcClient_->callAsync(
        QStringLiteral("group.list"),
        QJsonObject(),
        [this](const QJsonValue &result, const QJsonObject &error) {
            onGroupListReceived(result, error);
        },
        3000);
}
