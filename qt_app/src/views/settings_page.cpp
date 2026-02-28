/**
 * @file views/settings_page.cpp
 * @brief 系统设置页面实现
 */

#include "views/settings_page.h"
#include "rpc_client.h"
#include "style_constants.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static const char *kInputStyle =
    "QLineEdit, QSpinBox {"
    "  background:%1; color:%2; border:1px solid %3;"
    "  border-radius:6px; padding:4px 8px; font-size:%4px;"
    "}"
    "QLineEdit:focus, QSpinBox:focus {"
    "  border-color:%5;"
    "}";

static QString inputSS()
{
    return QString(kInputStyle)
        .arg(Style::kColorBgInput)
        .arg(Style::kColorTextPrimary)
        .arg(Style::kColorBorder)
        .arg(Style::kFontNormal)
        .arg(Style::kColorBorderFocus);
}

static QLineEdit *makeInput(const QString &text, bool readOnly = false)
{
    QLineEdit *le = new QLineEdit(text);
    le->setReadOnly(readOnly);
    le->setStyleSheet(inputSS());
    le->setFixedHeight(Style::kBtnHeightNormal);
    if (readOnly)
        le->setEnabled(false);
    return le;
}

static QSpinBox *makeSpinBox(int value, int min, int max)
{
    QSpinBox *sb = new QSpinBox;
    sb->setRange(min, max);
    sb->setValue(value);
    sb->setStyleSheet(inputSS());
    sb->setFixedHeight(Style::kBtnHeightNormal);
    return sb;
}

static QLabel *makeFieldLabel(const QString &text)
{
    QLabel *lbl = new QLabel(text);
    lbl->setStyleSheet(
        QString("color:%1; font-size:%2px;")
            .arg(Style::kColorTextSecondary)
            .arg(Style::kFontNormal));
    lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return lbl;
}

static QPushButton *makePrimaryButton(const QString &text)
{
    QPushButton *btn = new QPushButton(text);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(Style::kBtnHeightNormal);
    btn->setStyleSheet(
        QString(
            "QPushButton {"
            "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "    stop:0 %1, stop:1 %2);"
            "  color:%3; font-size:%4px; font-weight:bold;"
            "  border:none; border-radius:6px; padding:0 16px;"
            "}"
            "QPushButton:pressed { opacity:0.8; }")
            .arg(Style::kColorGradientStart)
            .arg(Style::kColorGradientEnd)
            .arg(Style::kColorTextWhite)
            .arg(Style::kFontNormal));
    return btn;
}

static QPushButton *makeSecondaryButton(const QString &text)
{
    QPushButton *btn = new QPushButton(text);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(Style::kBtnHeightNormal);
    btn->setStyleSheet(
        QString(
            "QPushButton {"
            "  background:%1; color:%2; font-size:%3px;"
            "  border:1px solid %4; border-radius:6px; padding:0 16px;"
            "}"
            "QPushButton:pressed { background:%4; }")
            .arg(Style::kColorBgCard)
            .arg(Style::kColorTextPrimary)
            .arg(Style::kFontNormal)
            .arg(Style::kColorBorder));
    return btn;
}

// ---------------------------------------------------------------------------
// SettingsPage
// ---------------------------------------------------------------------------

SettingsPage::SettingsPage(RpcClient *rpc, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpc)
{
    setupUi();
}

// ---------------------------------------------------------------------------
// setupUi
// ---------------------------------------------------------------------------

void SettingsPage::setupUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(Style::kPageMargin, Style::kPageMargin,
                             Style::kPageMargin, Style::kPageMargin);
    root->setSpacing(Style::kCardSpacing);

    // ── Sub-tab bar ──────────────────────────────────
    QHBoxLayout *tabBar = new QHBoxLayout;
    tabBar->setSpacing(4);

    struct TabDef { const char *label; };
    TabDef tabs[] = {
        { "\xe7\xbd\x91\xe7\xbb\x9c\xe9\x85\x8d\xe7\xbd\xae" },  // 网络配置
        { "MQTT\xe9\x85\x8d\xe7\xbd\xae" },                        // MQTT配置
        { "\xe5\x85\xb3\xe4\xba\x8e\xe7\xb3\xbb\xe7\xbb\x9f" },  // 关于系统
    };

    for (int i = 0; i < 3; ++i) {
        QPushButton *btn = new QPushButton(QString::fromUtf8(tabs[i].label));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(Style::kBtnHeightSmall);
        btn->setProperty("class", "subTabButton");
        btn->setProperty("active", i == 0 ? "true" : "false");
        btn->setStyleSheet(
            QString(
                "QPushButton[active=\"true\"] {"
                "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                "    stop:0 %1, stop:1 %2);"
                "  color:%3; font-weight:bold;"
                "}"
                "QPushButton[active=\"false\"] {"
                "  background:%4; color:%5;"
                "}"
                "QPushButton {"
                "  font-size:%6px; border:none; border-radius:6px;"
                "  padding:0 14px;"
                "}")
                .arg(Style::kColorGradientStart)
                .arg(Style::kColorGradientEnd)
                .arg(Style::kColorTextWhite)
                .arg(Style::kColorBgCard)
                .arg(Style::kColorTextSecondary)
                .arg(Style::kFontSmall));

        connect(btn, &QPushButton::clicked, this, [this, i]() { switchTab(i); });
        tabButtons_.append(btn);
        tabBar->addWidget(btn);
    }
    tabBar->addStretch();
    root->addLayout(tabBar);

    // ── Stacked content ──────────────────────────────
    contentStack_ = new QStackedWidget;
    contentStack_->addWidget(createNetworkPanel());
    contentStack_->addWidget(createMqttPanel());
    contentStack_->addWidget(createAboutPanel());
    contentStack_->setCurrentIndex(0);
    root->addWidget(contentStack_, 1);
}

// ---------------------------------------------------------------------------
// switchTab
// ---------------------------------------------------------------------------

void SettingsPage::switchTab(int index)
{
    for (int i = 0; i < tabButtons_.size(); ++i) {
        tabButtons_[i]->setProperty("active", i == index ? "true" : "false");
        tabButtons_[i]->style()->unpolish(tabButtons_[i]);
        tabButtons_[i]->style()->polish(tabButtons_[i]);
    }
    contentStack_->setCurrentIndex(index);
}

// ---------------------------------------------------------------------------
// Network panel
// ---------------------------------------------------------------------------

QWidget *SettingsPage::createNetworkPanel()
{
    QFrame *panel = new QFrame;
    panel->setProperty("class", "glassPanel");

    QVBoxLayout *vl = new QVBoxLayout(panel);
    vl->setContentsMargins(Style::kPageMargin, Style::kPageMargin,
                           Style::kPageMargin, Style::kPageMargin);
    vl->setSpacing(10);

    // Title
    QHBoxLayout *titleRow = new QHBoxLayout;
    QLabel *icon = new QLabel(QString::fromUtf8("\xf0\x9f\x8c\x90")); // 🌐
    icon->setStyleSheet(QString("font-size:%1px;").arg(Style::kFontLarge));
    QLabel *title = new QLabel(QString::fromUtf8("\xe4\xbb\xa5\xe5\xa4\xaa\xe7\xbd\x91\xe9\x85\x8d\xe7\xbd\xae")); // 以太网配置
    title->setStyleSheet(
        QString("color:%1; font-size:%2px; font-weight:bold;")
            .arg(Style::kColorAccentCyan)
            .arg(Style::kFontMedium));
    titleRow->addWidget(icon);
    titleRow->addWidget(title);
    titleRow->addStretch();
    vl->addLayout(titleRow);

    // 2-column grid
    QGridLayout *grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(8);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);

    int row = 0;

    // IP获取方式
    grid->addWidget(makeFieldLabel(QString::fromUtf8("IP\xe5\x9c\xb0\xe5\x9d\x80\xe8\x8e\xb7\xe5\x8f\x96\xe6\x96\xb9\xe5\xbc\x8f")), row, 0); // IP地址获取方式
    QHBoxLayout *radioRow = new QHBoxLayout;
    radioStaticIp_ = new QRadioButton(QString::fromUtf8("\xe9\x9d\x99\xe6\x80\x81IP")); // 静态IP
    radioDhcp_     = new QRadioButton(QString::fromUtf8("DHCP\xe8\x87\xaa\xe5\x8a\xa8\xe8\x8e\xb7\xe5\x8f\x96")); // DHCP自动获取
    radioStaticIp_->setChecked(true);
    QString radioSS = QString("QRadioButton { color:%1; font-size:%2px; }"
                              "QRadioButton::indicator { width:14px; height:14px; }")
                          .arg(Style::kColorTextPrimary).arg(Style::kFontNormal);
    radioStaticIp_->setStyleSheet(radioSS);
    radioDhcp_->setStyleSheet(radioSS);
    radioRow->addWidget(radioStaticIp_);
    radioRow->addWidget(radioDhcp_);
    radioRow->addStretch();
    QWidget *radioContainer = new QWidget;
    radioContainer->setLayout(radioRow);
    grid->addWidget(radioContainer, row, 1);

    // MAC地址
    grid->addWidget(makeFieldLabel(QString::fromUtf8("MAC\xe5\x9c\xb0\xe5\x9d\x80")), row, 2); // MAC地址
    editMac_ = makeInput("00:1A:2B:3C:4D:5E", true);
    grid->addWidget(editMac_, row, 3);

    ++row;

    // IP地址 / 子网掩码
    grid->addWidget(makeFieldLabel(QString::fromUtf8("IP\xe5\x9c\xb0\xe5\x9d\x80")), row, 0); // IP地址
    editIp_ = makeInput("192.168.1.100");
    grid->addWidget(editIp_, row, 1);

    grid->addWidget(makeFieldLabel(QString::fromUtf8("\xe5\xad\x90\xe7\xbd\x91\xe6\x8e\xa9\xe7\xa0\x81")), row, 2); // 子网掩码
    editSubnet_ = makeInput("255.255.255.0");
    grid->addWidget(editSubnet_, row, 3);

    ++row;

    // 默认网关 / 首选DNS
    grid->addWidget(makeFieldLabel(QString::fromUtf8("\xe9\xbb\x98\xe8\xae\xa4\xe7\xbd\x91\xe5\x85\xb3")), row, 0); // 默认网关
    editGateway_ = makeInput("192.168.1.1");
    grid->addWidget(editGateway_, row, 1);

    grid->addWidget(makeFieldLabel(QString::fromUtf8("\xe9\xa6\x96\xe9\x80\x89""DNS")), row, 2); // 首选DNS
    editDnsPrimary_ = makeInput("8.8.8.8");
    grid->addWidget(editDnsPrimary_, row, 3);

    ++row;

    // 备用DNS / 端口号
    grid->addWidget(makeFieldLabel(QString::fromUtf8("\xe5\xa4\x87\xe7\x94\xa8""DNS")), row, 0); // 备用DNS
    editDnsSecondary_ = makeInput("114.114.114.114");
    grid->addWidget(editDnsSecondary_, row, 1);

    grid->addWidget(makeFieldLabel(QString::fromUtf8("\xe7\xab\xaf\xe5\x8f\xa3\xe5\x8f\xb7")), row, 2); // 端口号
    spinPort_ = makeSpinBox(8080, 1, 65535);
    grid->addWidget(spinPort_, row, 3);

    vl->addLayout(grid);

    // Status banner
    networkStatusLabel_ = new QLabel(
        QString::fromUtf8("  \xe2\x9c\x93  \xe7\xbd\x91\xe7\xbb\x9c\xe8\xbf\x9e\xe6\x8e\xa5\xe6\xad\xa3\xe5\xb8\xb8 \xe2\x80\xa2 \xe5\xbb\xb6\xe8\xbf\x9f 2ms")); // ✓ 网络连接正常 • 延迟 2ms
    networkStatusLabel_->setAlignment(Qt::AlignCenter);
    networkStatusLabel_->setFixedHeight(Style::kBtnHeightSmall);
    networkStatusLabel_->setStyleSheet(
        QString("background:%1; color:%2; font-size:%3px;"
                "border-radius:6px; padding:0 8px;")
            .arg(Style::kColorSuccess)
            .arg(Style::kColorTextWhite)
            .arg(Style::kFontSmall));
    vl->addWidget(networkStatusLabel_);

    // Buttons
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnTestConnection_ = makeSecondaryButton(QString::fromUtf8("\xe6\xb5\x8b\xe8\xaf\x95\xe8\xbf\x9e\xe6\x8e\xa5")); // 测试连接
    btnSaveNetwork_    = makePrimaryButton(QString::fromUtf8("\xe4\xbf\x9d\xe5\xad\x98\xe9\x85\x8d\xe7\xbd\xae"));     // 保存配置
    btnRow->addWidget(btnTestConnection_);
    btnRow->addWidget(btnSaveNetwork_);
    vl->addLayout(btnRow);

    vl->addStretch();
    return panel;
}

// ---------------------------------------------------------------------------
// MQTT panel
// ---------------------------------------------------------------------------

QWidget *SettingsPage::createMqttPanel()
{
    QFrame *panel = new QFrame;
    panel->setProperty("class", "glassPanel");

    QVBoxLayout *vl = new QVBoxLayout(panel);
    vl->setContentsMargins(Style::kPageMargin, Style::kPageMargin,
                           Style::kPageMargin, Style::kPageMargin);
    vl->setSpacing(10);

    // Title
    QLabel *title = new QLabel(QString::fromUtf8("MQTT \xe9\x80\x9a\xe4\xbf\xa1\xe9\x85\x8d\xe7\xbd\xae")); // MQTT 通信配置
    title->setStyleSheet(
        QString("color:%1; font-size:%2px; font-weight:bold;")
            .arg(Style::kColorAccentCyan)
            .arg(Style::kFontMedium));
    vl->addWidget(title);

    // 2-column grid
    QGridLayout *grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(8);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);

    int row = 0;

    // 服务器地址 / 端口号
    grid->addWidget(makeFieldLabel(QString::fromUtf8("\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe5\x9c\xb0\xe5\x9d\x80")), row, 0); // 服务器地址
    editMqttServer_ = makeInput("broker.emqx.io");
    grid->addWidget(editMqttServer_, row, 1);

    grid->addWidget(makeFieldLabel(QString::fromUtf8("\xe7\xab\xaf\xe5\x8f\xa3\xe5\x8f\xb7")), row, 2); // 端口号
    spinMqttPort_ = makeSpinBox(1883, 1, 65535);
    grid->addWidget(spinMqttPort_, row, 3);

    ++row;

    // 客户端ID / 用户名
    grid->addWidget(makeFieldLabel(QString::fromUtf8("\xe5\xae\xa2\xe6\x88\xb7\xe7\xab\xaf""ID")), row, 0); // 客户端ID
    editMqttClientId_ = makeInput("GH-001-A");
    grid->addWidget(editMqttClientId_, row, 1);

    grid->addWidget(makeFieldLabel(QString::fromUtf8("\xe7\x94\xa8\xe6\x88\xb7\xe5\x90\x8d")), row, 2); // 用户名
    editMqttUser_ = makeInput("admin");
    grid->addWidget(editMqttUser_, row, 3);

    ++row;

    // 密码 / KeepAlive
    grid->addWidget(makeFieldLabel(QString::fromUtf8("\xe5\xaf\x86\xe7\xa0\x81")), row, 0); // 密码
    editMqttPassword_ = makeInput("******");
    editMqttPassword_->setEchoMode(QLineEdit::Password);
    grid->addWidget(editMqttPassword_, row, 1);

    grid->addWidget(makeFieldLabel("KeepAlive"), row, 2);
    spinMqttKeepAlive_ = makeSpinBox(60, 1, 3600);
    spinMqttKeepAlive_->setSuffix(QString::fromUtf8(" \xe7\xa7\x92")); // 秒
    grid->addWidget(spinMqttKeepAlive_, row, 3);

    ++row;

    // 数据发布主题 / 命令订阅主题
    grid->addWidget(makeFieldLabel(QString::fromUtf8("\xe6\x95\xb0\xe6\x8d\xae\xe5\x8f\x91\xe5\xb8\x83\xe4\xb8\xbb\xe9\xa2\x98")), row, 0); // 数据发布主题
    editMqttPubTopic_ = makeInput("greenhouse/GH-001-A/data");
    grid->addWidget(editMqttPubTopic_, row, 1);

    grid->addWidget(makeFieldLabel(QString::fromUtf8("\xe5\x91\xbd\xe4\xbb\xa4\xe8\xae\xa2\xe9\x98\x85\xe4\xb8\xbb\xe9\xa2\x98")), row, 2); // 命令订阅主题
    editMqttSubTopic_ = makeInput("greenhouse/GH-001-A/cmd");
    grid->addWidget(editMqttSubTopic_, row, 3);

    ++row;

    // 状态上报间隔
    grid->addWidget(makeFieldLabel(QString::fromUtf8("\xe7\x8a\xb6\xe6\x80\x81\xe4\xb8\x8a\xe6\x8a\xa5\xe9\x97\xb4\xe9\x9a\x94")), row, 0); // 状态上报间隔
    spinMqttReportInterval_ = makeSpinBox(30, 1, 3600);
    spinMqttReportInterval_->setSuffix(QString::fromUtf8(" \xe7\xa7\x92")); // 秒
    grid->addWidget(spinMqttReportInterval_, row, 1);

    vl->addLayout(grid);

    // Status
    QHBoxLayout *statusRow = new QHBoxLayout;
    QLabel *dot = new QLabel(QString::fromUtf8("\xe2\x97\x8f")); // ●
    dot->setStyleSheet(QString("color:%1; font-size:12px;").arg(Style::kColorSuccess));

    mqttStatusLabel_ = new QLabel(
        QString::fromUtf8("MQTT \xe5\xb7\xb2\xe8\xbf\x9e\xe6\x8e\xa5")); // MQTT 已连接
    mqttStatusLabel_->setStyleSheet(
        QString("color:%1; font-size:%2px;")
            .arg(Style::kColorSuccess)
            .arg(Style::kFontNormal));

    QLabel *heartbeat = new QLabel(
        QString::fromUtf8("\xe4\xb8\x8a\xe6\xac\xa1\xe5\xbf\x83\xe8\xb7\xb3: 2\xe7\xa7\x92\xe5\x89\x8d")); // 上次心跳: 2秒前
    heartbeat->setStyleSheet(
        QString("color:%1; font-size:%2px;")
            .arg(Style::kColorTextMuted)
            .arg(Style::kFontSmall));

    statusRow->addWidget(dot);
    statusRow->addWidget(mqttStatusLabel_);
    statusRow->addSpacing(12);
    statusRow->addWidget(heartbeat);
    statusRow->addStretch();
    vl->addLayout(statusRow);

    // Buttons
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnMqttDisconnect_ = makeSecondaryButton(QString::fromUtf8("\xe6\x96\xad\xe5\xbc\x80\xe8\xbf\x9e\xe6\x8e\xa5")); // 断开连接
    btnMqttSave_       = makePrimaryButton(QString::fromUtf8("\xe4\xbf\x9d\xe5\xad\x98\xe5\xb9\xb6\xe9\x87\x8d\xe5\x90\xaf"));       // 保存并重启
    btnRow->addWidget(btnMqttDisconnect_);
    btnRow->addWidget(btnMqttSave_);
    vl->addLayout(btnRow);

    vl->addStretch();
    return panel;
}

// ---------------------------------------------------------------------------
// About panel
// ---------------------------------------------------------------------------

QWidget *SettingsPage::createAboutPanel()
{
    QFrame *panel = new QFrame;
    panel->setProperty("class", "glassPanel");

    QVBoxLayout *vl = new QVBoxLayout(panel);
    vl->setContentsMargins(Style::kPageMargin, Style::kPageMargin,
                           Style::kPageMargin, Style::kPageMargin);
    vl->setAlignment(Qt::AlignCenter);
    vl->setSpacing(8);

    vl->addStretch();

    // Large blue gradient icon
    QLabel *iconLabel = new QLabel(QString::fromUtf8("\xe2\x9a\xa1")); // ⚡
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setFixedSize(72, 72);
    iconLabel->setStyleSheet(
        QString(
            "background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
            "  stop:0 %1, stop:1 %2);"
            "color:white; font-size:32px; border-radius:18px;")
            .arg(Style::kColorGradientStart)
            .arg(Style::kColorGradientEnd));
    vl->addWidget(iconLabel, 0, Qt::AlignCenter);

    vl->addSpacing(8);

    // Title
    QLabel *brandTitle = new QLabel(QString::fromUtf8("\xe6\xb3\x9b\xe8\x88\x9f\xe6\x99\xba\xe8\x83\xbd\xe7\xa7\x91\xe6\x8a\x80")); // 泛舟智能科技
    brandTitle->setAlignment(Qt::AlignCenter);
    brandTitle->setStyleSheet(
        QString("color:%1; font-size:%2px; font-weight:bold;")
            .arg(Style::kColorTextPrimary)
            .arg(Style::kFontTitle));
    vl->addWidget(brandTitle);

    // Subtitle
    QLabel *subtitle = new QLabel(QString::fromUtf8("\xe6\x99\xba\xe8\x83\xbd\xe6\xb8\xa9\xe5\xae\xa4\xe6\x8e\xa7\xe5\x88\xb6\xe7\xb3\xbb\xe7\xbb\x9f")); // 智能温室控制系统
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet(
        QString("color:%1; font-size:%2px;")
            .arg(Style::kColorTextSecondary)
            .arg(Style::kFontMedium));
    vl->addWidget(subtitle);

    vl->addSpacing(16);

    // 2×2 info grid
    QGridLayout *infoGrid = new QGridLayout;
    infoGrid->setHorizontalSpacing(24);
    infoGrid->setVerticalSpacing(10);

    struct InfoItem { const char *label; const char *value; };
    InfoItem items[] = {
        { "\xe8\xbd\xaf\xe4\xbb\xb6\xe7\x89\x88\xe6\x9c\xac", "v2.1.0"      },  // 软件版本
        { "\xe7\xa1\xac\xe4\xbb\xb6\xe7\x89\x88\xe6\x9c\xac", "v1.2.0"      },  // 硬件版本
        { "\xe7\xbc\x96\xe8\xaf\x91\xe6\x97\xa5\xe6\x9c\x9f", "2024-01-15"  },  // 编译日期
        { "\xe8\xae\xbe\xe5\xa4\x87ID",                         "GH-001-A"    },  // 设备ID
    };

    for (int i = 0; i < 4; ++i) {
        int r = i / 2;
        int c = (i % 2) * 2;

        QLabel *lbl = new QLabel(QString::fromUtf8(items[i].label));
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lbl->setStyleSheet(
            QString("color:%1; font-size:%2px;")
                .arg(Style::kColorTextMuted)
                .arg(Style::kFontNormal));

        QLabel *val = new QLabel(QString::fromUtf8(items[i].value));
        val->setStyleSheet(
            QString("color:%1; font-size:%2px; font-weight:bold;")
                .arg(Style::kColorTextPrimary)
                .arg(Style::kFontNormal));

        infoGrid->addWidget(lbl, r, c);
        infoGrid->addWidget(val, r, c + 1);
    }

    vl->addLayout(infoGrid);

    vl->addSpacing(16);

    // Copyright
    QLabel *copyright = new QLabel(
        QString::fromUtf8("\xc2\xa9 2024 \xe6\xb3\x9b\xe8\x88\x9f\xe6\x99\xba\xe8\x83\xbd\xe7\xa7\x91\xe6\x8a\x80 All Rights Reserved")); // © 2024 泛舟智能科技
    copyright->setAlignment(Qt::AlignCenter);
    copyright->setStyleSheet(
        QString("color:%1; font-size:%2px;")
            .arg(Style::kColorTextMuted)
            .arg(Style::kFontTiny));
    vl->addWidget(copyright);

    vl->addStretch();
    return panel;
}
