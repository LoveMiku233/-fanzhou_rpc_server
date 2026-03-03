/**
 * @file views/scene_page.cpp
 * @brief 场景管理页面实现
 *
 * 支持从RPC Server获取策略/场景数据并显示。
 */

#include "views/scene_page.h"
#include "rpc_client.h"
#include "style_constants.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QMouseEvent>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static QPushButton *makeTabButton(const QString &text, bool active)
{
    QPushButton *btn = new QPushButton(text);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setCheckable(true);
    btn->setChecked(active);
    btn->setStyleSheet(
        QString(
            "QPushButton {"
            "  background: transparent;"
            "  color: %1;"
            "  font-size: %2px;"
            "  font-weight: 500;"
            "  padding: 6px 12px;"
            "  border: none;"
            "  border-bottom: 2px solid transparent;"
            "}"
            "QPushButton:checked {"
            "  color: white;"
            "  border-bottom: 2px solid %3;"
            "}"
            "QPushButton:hover {"
            "  color: white;"
            "}")
            .arg(Style::kColorTextSecondary)
            .arg(Style::kFontSmall)
            .arg(Style::kColorAccentBlue));
    return btn;
}

// ---------------------------------------------------------------------------
// ScenePage
// ---------------------------------------------------------------------------

ScenePage::ScenePage(RpcClient *rpc, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpc)
    , currentFilter_("all")
    , hasRpcData_(false)
    , tabLayout_(nullptr)
    , newSceneBtn_(nullptr)
    , cardScrollArea_(nullptr)
    , cardContainer_(nullptr)
    , cardGrid_(nullptr)
    , statusLabel_(nullptr)
{
    initDemoData();
    setupUi();
    renderScenes();
}

// ---------------------------------------------------------------------------
// initDemoData
// ---------------------------------------------------------------------------

void ScenePage::initDemoData()
{
    {
        Models::SceneInfo s;
        s.id = 1;
        s.name = QString::fromUtf8("夏季通风模式");
        s.type = "auto";
        s.active = true;
        s.conditions = QStringList() << QString::fromUtf8("温度>30°C")
                                     << QString::fromUtf8("定时09:00");
        s.triggers = 3;
        s.lastRun = QString::fromUtf8("今日 14:32");
        scenes_.append(s);
    }
    {
        Models::SceneInfo s;
        s.id = 2;
        s.name = QString::fromUtf8("夜间保温模式");
        s.type = "auto";
        s.active = false;
        s.conditions = QStringList() << QString::fromUtf8("定时18:00");
        s.triggers = 0;
        s.lastRun = QString::fromUtf8("昨日 18:00");
        scenes_.append(s);
    }
    {
        Models::SceneInfo s;
        s.id = 3;
        s.name = QString::fromUtf8("暴雨保护模式");
        s.type = "auto";
        s.active = false;
        s.conditions = QStringList() << QString::fromUtf8("雨量>5mm")
                                     << QString::fromUtf8("风速>10m/s");
        s.triggers = 0;
        s.lastRun = QString::fromUtf8("从未");
        scenes_.append(s);
    }
    {
        Models::SceneInfo s;
        s.id = 4;
        s.name = QString::fromUtf8("手动全开");
        s.type = "manual";
        s.active = false;
        s.triggers = 12;
        s.lastRun = QString::fromUtf8("今日 10:15");
        scenes_.append(s);
    }
    {
        Models::SceneInfo s;
        s.id = 5;
        s.name = QString::fromUtf8("紧急关闭");
        s.type = "manual";
        s.active = false;
        s.triggers = 2;
        s.lastRun = QString::fromUtf8("昨日 16:20");
        scenes_.append(s);
    }
    {
        Models::SceneInfo s;
        s.id = 6;
        s.name = QString::fromUtf8("施肥灌溉");
        s.type = "manual";
        s.active = false;
        s.triggers = 5;
        s.lastRun = QString::fromUtf8("今日 08:30");
        scenes_.append(s);
    }
}

// ---------------------------------------------------------------------------
// setupUi
// ---------------------------------------------------------------------------

void ScenePage::setupUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(Style::kPageMargin, Style::kPageMargin,
                             Style::kPageMargin, Style::kPageMargin);
    root->setSpacing(Style::kCardSpacing);

    // ── Top sub-tab bar ──────────────────────────────────
    QFrame *tabBar = new QFrame;
    tabBar->setStyleSheet(
        QString("QFrame { background:%1; border-radius:%2px;"
                " border:1px solid %3; }")
            .arg(Style::kColorBgPanel)
            .arg(Style::kCardRadius)
            .arg(Style::kColorBorder));

    QHBoxLayout *tabBarLayout = new QHBoxLayout(tabBar);
    tabBarLayout->setContentsMargins(8, 4, 8, 4);
    tabBarLayout->setSpacing(4);

    struct TabDef { const char *label; const char *filter; };
    TabDef tabs[] = {
        { "全部场景", "all"    },
        { "自动场景", "auto"   },
        { "手动场景", "manual" },
    };

    for (int i = 0; i < 3; ++i) {
        QPushButton *btn = makeTabButton(
            QString::fromUtf8(tabs[i].label), i == 0);
        btn->setProperty("filter", tabs[i].filter);
        tabBarLayout->addWidget(btn);
        tabButtons_.append(btn);

        connect(btn, &QPushButton::clicked, this, [this, btn]() {
            currentFilter_ = btn->property("filter").toString();
            for (QPushButton *b : tabButtons_)
                b->setChecked(b == btn);
            renderScenes();
        });
    }

    tabBarLayout->addStretch();

    newSceneBtn_ = new QPushButton(QString::fromUtf8("＋ 新建场景"));
    newSceneBtn_->setCursor(Qt::PointingHandCursor);
    newSceneBtn_->setStyleSheet(
        QString(
            "QPushButton {"
            "  background:%1; color:white; font-size:%2px;"
            "  padding:4px 12px; border-radius:6px; border:none;"
            "}"
            "QPushButton:hover { background:#059669; }")
            .arg(Style::kColorSuccess)
            .arg(Style::kFontSmall));
    tabBarLayout->addWidget(newSceneBtn_);

    root->addWidget(tabBar);

    // ── Scroll area for scene cards ──────────────────────
    cardScrollArea_ = new QScrollArea;
    cardScrollArea_->setWidgetResizable(true);
    cardScrollArea_->setFrameShape(QFrame::NoFrame);
    cardScrollArea_->setStyleSheet(
        QString("QScrollArea { background:transparent; border:none; }"
                "QScrollBar:vertical {"
                "  background:%1; width:6px; border-radius:3px;"
                "}"
                "QScrollBar::handle:vertical {"
                "  background:%2; border-radius:3px; min-height:20px;"
                "}"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
                "  height:0px;"
                "}")
            .arg(Style::kColorBgDark)
            .arg(Style::kColorBorderLight));

    cardContainer_ = new QWidget;
    cardContainer_->setStyleSheet("background:transparent;");
    cardGrid_ = new QGridLayout(cardContainer_);
    cardGrid_->setContentsMargins(0, 0, 0, 0);
    cardGrid_->setSpacing(Style::kCardSpacing);

    cardScrollArea_->setWidget(cardContainer_);
    root->addWidget(cardScrollArea_, 1);

    // ── Status label at bottom ───────────────────────────
    statusLabel_ = new QLabel(QString::fromUtf8("已加载本地示例数据，连接服务器后自动刷新"));
    statusLabel_->setStyleSheet(
        QString("color:%1; font-size:%2px; padding:4px %3px;")
            .arg(Style::kColorTextMuted)
            .arg(Style::kFontTiny)
            .arg(Style::kPageMargin));
    root->addWidget(statusLabel_);
}

// ---------------------------------------------------------------------------
// renderScenes
// ---------------------------------------------------------------------------

void ScenePage::renderScenes()
{
    // Clear existing cards
    QLayoutItem *child;
    while ((child = cardGrid_->takeAt(0)) != nullptr) {
        if (child->widget())
            child->widget()->deleteLater();
        delete child;
    }

    int col = 0;
    int row = 0;
    const int columns = 4;

    for (int i = 0; i < scenes_.size(); ++i) {
        const Models::SceneInfo &scene = scenes_.at(i);
        if (currentFilter_ != "all" && scene.type != currentFilter_)
            continue;

        QWidget *card = createSceneCard(scene);
        cardGrid_->addWidget(card, row, col);

        if (++col >= columns) {
            col = 0;
            ++row;
        }
    }

    // Fill remaining columns with spacers so cards keep uniform width
    for (int c = 0; c < columns; ++c)
        cardGrid_->setColumnStretch(c, 1);

    // Vertical spacer at bottom
    cardGrid_->setRowStretch(row + 1, 1);

    updateTabCounts();
}

// ---------------------------------------------------------------------------
// createSceneCard
// ---------------------------------------------------------------------------

QWidget *ScenePage::createSceneCard(const Models::SceneInfo &scene)
{
    const bool isAuto = (scene.type == "auto");
    const char *borderColor = isAuto ? Style::kColorSuccess : Style::kColorWarning;
    const char *typeColor   = isAuto ? Style::kColorSuccess : Style::kColorWarning;

    QFrame *card = new QFrame;
    card->setProperty("class", "sceneCard");

    // Active state: brighter border + tinted background
    QString activeStyle;
    if (scene.active && isAuto) {
        activeStyle = QString(
            "border:2px solid %1;"
            "background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
            "  stop:0 rgba(16,185,129,38), stop:1 rgba(5,150,105,64));")
            .arg(Style::kColorSuccess);
    } else {
        activeStyle = QString(
            "border:1px solid %1; background:%2;")
            .arg(Style::kColorBorder)
            .arg(Style::kColorBgPanel);
    }

    card->setStyleSheet(
        QString("QFrame[class=\"sceneCard\"] {"
                "  %1"
                "  border-left:4px solid %2;"
                "  border-radius:%3px;"
                "  padding:10px;"
                "}")
            .arg(activeStyle)
            .arg(borderColor)
            .arg(Style::kCardRadius));

    QVBoxLayout *vl = new QVBoxLayout(card);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    // ── Header row: emoji + name + type label | active dot ───
    QHBoxLayout *headerRow = new QHBoxLayout;
    headerRow->setSpacing(6);

    QLabel *emoji = new QLabel(isAuto ? QString::fromUtf8("🤖")
                                      : QString::fromUtf8("👆"));
    emoji->setStyleSheet(QString("font-size:%1px;").arg(Style::kFontLarge));

    QVBoxLayout *nameCol = new QVBoxLayout;
    nameCol->setSpacing(2);

    QLabel *nameLabel = new QLabel(scene.name);
    nameLabel->setStyleSheet(
        QString("color:%1; font-size:%2px; font-weight:bold;")
            .arg(Style::kColorTextPrimary)
            .arg(Style::kFontSmall));

    QLabel *typeLabel = new QLabel(isAuto ? QString::fromUtf8("自动场景")
                                         : QString::fromUtf8("手动场景"));
    typeLabel->setStyleSheet(
        QString("color:%1; font-size:%2px;")
            .arg(typeColor)
            .arg(Style::kFontTiny));

    nameCol->addWidget(nameLabel);
    nameCol->addWidget(typeLabel);

    headerRow->addWidget(emoji);
    headerRow->addLayout(nameCol, 1);

    if (scene.active && isAuto) {
        QLabel *dot = new QLabel(QString::fromUtf8("●"));
        dot->setStyleSheet(
            QString("color:%1; font-size:10px;").arg(Style::kColorSuccess));
        headerRow->addWidget(dot);
    }

    vl->addLayout(headerRow);

    // ── Conditions / manual hint ─────────────────────────
    if (isAuto && !scene.conditions.isEmpty()) {
        QHBoxLayout *condRow = new QHBoxLayout;
        condRow->setSpacing(4);
        for (const QString &cond : scene.conditions) {
            QLabel *tag = new QLabel(cond);
            tag->setStyleSheet(
                QString("background:%1; color:%2; font-size:%3px;"
                        " padding:2px 6px; border-radius:4px;")
                    .arg(Style::kColorBgCard)
                    .arg(Style::kColorTextSecondary)
                    .arg(Style::kFontTiny));
            condRow->addWidget(tag);
        }
        condRow->addStretch();
        vl->addLayout(condRow);
    } else if (!isAuto) {
        QLabel *hint = new QLabel(QString::fromUtf8("手动点击执行"));
        hint->setStyleSheet(
            QString("color:%1; font-size:%2px;")
                .arg(Style::kColorTextMuted)
                .arg(Style::kFontTiny));
        vl->addWidget(hint);
    }

    vl->addStretch();

    // ── Footer: trigger count + last run ─────────────────
    QHBoxLayout *footerRow = new QHBoxLayout;
    footerRow->setSpacing(4);

    QLabel *triggerLabel = new QLabel(
        QString::fromUtf8("今日: %1次").arg(scene.triggers));
    triggerLabel->setStyleSheet(
        QString("color:%1; font-size:%2px;")
            .arg(Style::kColorTextSecondary)
            .arg(Style::kFontTiny));

    QLabel *lastRunLabel = new QLabel(scene.lastRun);
    lastRunLabel->setStyleSheet(
        QString("color:%1; font-size:%2px;")
            .arg(Style::kColorTextMuted)
            .arg(Style::kFontTiny));

    footerRow->addWidget(triggerLabel);
    footerRow->addWidget(lastRunLabel, 1);
    vl->addLayout(footerRow);

    // ── Manual scene: execute button ─────────────────────
    if (!isAuto) {
        QPushButton *execBtn = new QPushButton(QString::fromUtf8("立即执行"));
        execBtn->setCursor(Qt::PointingHandCursor);
        execBtn->setStyleSheet(
            QString(
                "QPushButton {"
                "  background:%1; color:white; font-size:%2px;"
                "  font-weight:500; padding:4px 0; border-radius:6px; border:none;"
                "}"
                "QPushButton:hover { background:#1d4ed8; }")
                .arg(Style::kColorInfo)
                .arg(Style::kFontTiny));

        const int sceneId = scene.id;
        connect(execBtn, &QPushButton::clicked, this, [this, sceneId]() {
            // Trigger the scene via RPC server if connected
            if (rpcClient_ && rpcClient_->isConnected()) {
                QJsonObject params;
                params["id"] = sceneId;
                rpcClient_->callAsync(
                    QStringLiteral("auto.strategy.trigger"),
                    params);
            }
            for (int i = 0; i < scenes_.size(); ++i) {
                if (scenes_[i].id == sceneId) {
                    scenes_[i].triggers += 1;
                    break;
                }
            }
            renderScenes();
        });

        vl->addWidget(execBtn);
    }

    // ── Auto scene: click card to toggle active ──────────
    if (isAuto) {
        card->setProperty("sceneId", scene.id);
        card->setCursor(Qt::PointingHandCursor);
        card->installEventFilter(this);
    }

    return card;
}

// ---------------------------------------------------------------------------
// eventFilter - handle clicks on auto scene cards
// ---------------------------------------------------------------------------

bool ScenePage::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        QWidget *w = qobject_cast<QWidget *>(obj);
        if (w && w->property("sceneId").isValid()) {
            int sceneId = w->property("sceneId").toInt();
            for (int i = 0; i < scenes_.size(); ++i) {
                if (scenes_[i].id == sceneId) {
                    scenes_[i].active = !scenes_[i].active;

                    // Send enable/disable to RPC server if connected
                    if (rpcClient_ && rpcClient_->isConnected()) {
                        QJsonObject params;
                        params["id"] = sceneId;
                        params["enabled"] = scenes_[i].active;
                        rpcClient_->callAsync(
                            QStringLiteral("auto.strategy.enable"),
                            params);
                    }
                    break;
                }
            }
            renderScenes();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ---------------------------------------------------------------------------
// updateTabCounts
// ---------------------------------------------------------------------------

void ScenePage::updateTabCounts()
{
    int allCount = scenes_.size();
    int autoCount = 0;
    int manualCount = 0;
    for (int i = 0; i < scenes_.size(); ++i) {
        if (scenes_.at(i).type == "auto") ++autoCount;
        else ++manualCount;
    }

    if (tabButtons_.size() >= 3) {
        tabButtons_[0]->setText(QString::fromUtf8("全部场景 (%1)").arg(allCount));
        tabButtons_[1]->setText(QString::fromUtf8("自动场景 (%1)").arg(autoCount));
        tabButtons_[2]->setText(QString::fromUtf8("手动场景 (%1)").arg(manualCount));
    }
}

// ---------------------------------------------------------------------------
// onStrategyListReceived  (RPC callback)
// ---------------------------------------------------------------------------

void ScenePage::onStrategyListReceived(const QJsonValue &result,
                                        const QJsonObject &error)
{
    if (!error.isEmpty() || !result.isObject())
        return;

    QJsonObject obj = result.toObject();
    if (!obj.value("ok").toBool())
        return;

    QJsonArray strategies = obj.value("strategies").toArray();

    scenes_.clear();
    hasRpcData_ = true;

    for (const QJsonValue &sv : strategies) {
        QJsonObject so = sv.toObject();
        Models::SceneInfo scene;
        scene.id = so.value("id").toInt();
        scene.name = so.value("name").toString();

        // Map strategy type to scene type
        QString stype = so.value("type").toString().toLower();
        if (stype == "manual") {
            scene.type = "manual";
        } else {
            scene.type = "auto";
        }

        scene.active = so.value("enabled").toBool();

        // Build conditions from the conditions array
        QJsonArray condArr = so.value("conditions").toArray();
        for (const QJsonValue &cv : condArr) {
            QJsonObject co = cv.toObject();
            QString condStr = co.value("identifier").toString()
                + co.value("op").toString()
                + co.value("value").toString();
            if (!condStr.isEmpty()) {
                scene.conditions << condStr;
            }
        }

        // Use version as trigger count indicator
        scene.triggers = so.value("version").toInt(0);

        // Format last run / update time
        QString updateTime = so.value("updateTime").toString();
        if (!updateTime.isEmpty()) {
            scene.lastRun = updateTime;
        } else {
            scene.lastRun = QString::fromUtf8("--");
        }

        scenes_.append(scene);
    }

    // Additional strategies will be appended via onAdditionalStrategiesReceived
    if (statusLabel_) {
        statusLabel_->setText(
            QString::fromUtf8("已从服务器加载 %1 个场景/策略").arg(scenes_.size()));
    }

    renderScenes();
}

// ---------------------------------------------------------------------------
// Helper to append relay/sensor strategies to scene list
// ---------------------------------------------------------------------------

static void appendRelayStrategies(const QJsonValue &result,
                                   const QJsonObject &error,
                                   QList<Models::SceneInfo> &scenes,
                                   int idOffset)
{
    if (!error.isEmpty() || !result.isObject())
        return;
    QJsonObject obj = result.toObject();
    if (!obj.value("ok").toBool())
        return;

    QJsonArray strategies = obj.value("strategies").toArray();
    for (const QJsonValue &sv : strategies) {
        QJsonObject so = sv.toObject();
        Models::SceneInfo scene;
        scene.id = so.value("id").toInt() + idOffset;
        scene.name = so.value("name").toString();
        scene.type = "auto";
        scene.active = so.value("enabled").toBool();
        scene.triggers = 0;

        // Build a descriptive condition from the strategy fields
        int nodeId = so.value("nodeId").toInt(-1);
        int channel = so.value("channel").toInt(-1);
        QString action = so.value("action").toString();
        if (nodeId >= 0) {
            QString cond = QString::fromUtf8("节点%1").arg(nodeId);
            if (channel >= 0)
                cond += QString::fromUtf8(" 通道%1").arg(channel);
            if (!action.isEmpty())
                cond += QString::fromUtf8(" %1").arg(action);
            scene.conditions << cond;
        }

        int intervalSec = so.value("intervalSec").toInt(0);
        if (intervalSec > 0) {
            scene.conditions << QString::fromUtf8("间隔%1秒").arg(intervalSec);
        }

        // Sensor condition
        QString sensorId = so.value("sensorId").toString();
        if (!sensorId.isEmpty()) {
            QString op = so.value("op").toString();
            double threshold = so.value("threshold").toDouble();
            scene.conditions << QString::fromUtf8("传感器%1 %2 %3")
                                    .arg(sensorId, op)
                                    .arg(threshold);
        }

        scene.lastRun = QString::fromUtf8("--");
        scenes.append(scene);
    }
}

// ---------------------------------------------------------------------------
// refreshData  (fetch from RPC server)
// ---------------------------------------------------------------------------

void ScenePage::refreshData()
{
    if (!rpcClient_ || !rpcClient_->isConnected())
        return;

    // Fetch main strategy list
    rpcClient_->callAsync(
        QStringLiteral("auto.strategy.list"),
        QJsonObject(),
        [this](const QJsonValue &result, const QJsonObject &error) {
            onStrategyListReceived(result, error);

            // Also fetch relay strategies
            if (rpcClient_ && rpcClient_->isConnected()) {
                rpcClient_->callAsync(
                    QStringLiteral("auto.relay.list"),
                    QJsonObject(),
                    [this](const QJsonValue &r, const QJsonObject &e) {
                        appendRelayStrategies(r, e, scenes_, 10000);
                        // Fetch sensor strategies
                        if (rpcClient_ && rpcClient_->isConnected()) {
                            rpcClient_->callAsync(
                                QStringLiteral("auto.sensor.list"),
                                QJsonObject(),
                                [this](const QJsonValue &r2, const QJsonObject &e2) {
                                    appendRelayStrategies(r2, e2, scenes_, 20000);
                                    // Fetch sensor-relay strategies
                                    if (rpcClient_ && rpcClient_->isConnected()) {
                                        rpcClient_->callAsync(
                                            QStringLiteral("auto.sensorRelay.list"),
                                            QJsonObject(),
                                            [this](const QJsonValue &r3, const QJsonObject &e3) {
                                                appendRelayStrategies(r3, e3, scenes_, 30000);
                                                if (statusLabel_) {
                                                    statusLabel_->setText(
                                                        QString::fromUtf8("已从服务器加载 %1 个场景/策略")
                                                            .arg(scenes_.size()));
                                                }
                                                renderScenes();
                                            },
                                            3000);
                                    }
                                },
                                3000);
                        }
                    },
                    3000);
            }
        },
        3000);
}
