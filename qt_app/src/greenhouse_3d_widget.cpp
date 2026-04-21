/**
 * @file greenhouse_3d_widget.cpp
 * @brief 大棚常用流程页面实现
 */

#include "greenhouse_3d_widget.h"

#include "rpc_client.h"

#include <QAbstractAnimation>
#include <QDateTime>
#include <QEasingCurve>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <memory>

namespace {

QString actionDisplay(const QString &action)
{
    return action == QStringLiteral("stop") ? QStringLiteral("停止") : QStringLiteral("启动");
}

bool groupNameMatchesRole(const QString &groupName, const QStringList &keywords)
{
    const QString lower = groupName.toLower();
    for (const QString &kw : keywords) {
        if (lower.contains(kw.toLower())) {
            return true;
        }
    }
    return false;
}

}  // namespace

Greenhouse3DWidget::Greenhouse3DWidget(RpcClient *rpcClient, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpcClient)
    , titleLabel_(nullptr)
    , hintLabel_(nullptr)
    , statusLabel_(nullptr)
    , bindingLabel_(nullptr)
    , toastWidget_(nullptr)
    , toastLabel_(nullptr)
    , refreshTimer_(new QTimer(this))
    , toastHideTimer_(new QTimer(this))
    , toastShowAnim_(nullptr)
    , toastHideAnim_(nullptr)
{
    setupWorkflows();
    setupUi();
    updateBindingSummary();

    connect(refreshTimer_, &QTimer::timeout, this, &Greenhouse3DWidget::onRefreshGroups);
    refreshTimer_->start(5000);
    QTimer::singleShot(200, this, &Greenhouse3DWidget::onRefreshGroups);
    QTimer::singleShot(120, this, &Greenhouse3DWidget::animateWorkflowButtons);

    toastHideTimer_->setSingleShot(true);
    connect(toastHideTimer_, &QTimer::timeout, this, &Greenhouse3DWidget::hideToastAnimated);
}

void Greenhouse3DWidget::setupUi()
{
    roleBindings_.clear();
    auto addRole = [this](const QString &role, const QStringList &keywords) {
        RoleBinding binding;
        binding.role = role;
        binding.keywords = keywords;
        roleBindings_.append(binding);
    };
    addRole(QStringLiteral("fan"), QStringList{QStringLiteral("风机"), QStringLiteral("fan")});
    addRole(QStringLiteral("top_roll"), QStringList{QStringLiteral("顶卷"), QStringLiteral("顶膜"), QStringLiteral("天窗")});
    addRole(QStringLiteral("end_roll"), QStringList{QStringLiteral("端面"), QStringLiteral("端卷"), QStringLiteral("端膜")});
    addRole(QStringLiteral("side_roll"), QStringList{QStringLiteral("侧卷"), QStringLiteral("侧膜")});
    addRole(QStringLiteral("wet_pad"), QStringList{QStringLiteral("湿帘")});
    addRole(QStringLiteral("pump"), QStringList{QStringLiteral("水泵"), QStringLiteral("泵")});

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 14, 18, 14);
    mainLayout->setSpacing(10);

    titleLabel_ = new QLabel(QStringLiteral("大棚常用流程"), this);
    titleLabel_->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: 800; color: #17364a;"));
    mainLayout->addWidget(titleLabel_);

    hintLabel_ = new QLabel(
        QStringLiteral("本页面仅显示流程。设备绑定由分组完成：请在分组名称中包含“风机/顶卷/端面/侧卷/湿帘/水泵”等关键词。"), this);
    hintLabel_->setWordWrap(true);
    hintLabel_->setStyleSheet(QStringLiteral(
        "font-size: 13px; color: #22465d; background: #e5f3ff;"
        "padding: 8px 10px; border: 1px solid #bcd9f1; border-radius: 8px;"));
    mainLayout->addWidget(hintLabel_);

    statusLabel_ = new QLabel(QStringLiteral("正在加载分组绑定..."), this);
    statusLabel_->setStyleSheet(QStringLiteral(
        "font-size: 12px; color: #1f4d2c; background: #d8f3e3;"
        "padding: 6px 10px; border: 1px solid #b8e3c8; border-radius: 8px;"));
    auto *statusShadow = new QGraphicsDropShadowEffect(statusLabel_);
    statusShadow->setBlurRadius(14);
    statusShadow->setOffset(0, 3);
    statusShadow->setColor(QColor(28, 62, 48, 25));
    statusLabel_->setGraphicsEffect(statusShadow);
    mainLayout->addWidget(statusLabel_);

    bindingLabel_ = new QLabel(this);
    bindingLabel_->setWordWrap(true);
    bindingLabel_->setStyleSheet(QStringLiteral(
        "font-size: 12px; color: #41545f; background: #f2f7fb;"
        "padding: 6px 10px; border: 1px solid #d5e2ea; border-radius: 8px;"));
    auto *bindingShadow = new QGraphicsDropShadowEffect(bindingLabel_);
    bindingShadow->setBlurRadius(12);
    bindingShadow->setOffset(0, 3);
    bindingShadow->setColor(QColor(36, 52, 65, 22));
    bindingLabel_->setGraphicsEffect(bindingShadow);
    mainLayout->addWidget(bindingLabel_);

    QWidget *workflowPanel = new QWidget(this);
    workflowPanel->setStyleSheet(QStringLiteral(
        "background: rgba(255,255,255,0.94); border: 1px solid #d5e3ee; border-radius: 14px;"));
    QVBoxLayout *workflowLayout = new QVBoxLayout(workflowPanel);
    workflowLayout->setContentsMargins(10, 10, 10, 10);
    workflowLayout->setSpacing(8);

    auto *panelShadow = new QGraphicsDropShadowEffect(workflowPanel);
    panelShadow->setBlurRadius(28);
    panelShadow->setOffset(0, 8);
    panelShadow->setColor(QColor(24, 54, 74, 45));
    workflowPanel->setGraphicsEffect(panelShadow);

    QLabel *workflowTitle = new QLabel(QStringLiteral("流程列表"), workflowPanel);
    workflowTitle->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 700; color: #16384d;"));
    workflowLayout->addWidget(workflowTitle);

    QWidget *buttonsBox = new QWidget(workflowPanel);
    QGridLayout *buttonsLayout = new QGridLayout(buttonsBox);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->setHorizontalSpacing(8);
    buttonsLayout->setVerticalSpacing(8);

    workflowButtonIndexMap_.clear();
    for (int i = 0; i < workflows_.size(); ++i) {
        Workflow &flow = workflows_[i];
        flow.button = new QPushButton(buttonsBox);
        flow.button->setMinimumHeight(132);
        flow.button->setStyleSheet(flow.style + QStringLiteral(
            "QPushButton { border-radius: 12px; }"));
        flow.button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto *btnShadow = new QGraphicsDropShadowEffect(flow.button);
        btnShadow->setBlurRadius(14);
        btnShadow->setOffset(0, 5);
        btnShadow->setColor(QColor(30, 40, 55, 35));
        flow.button->setGraphicsEffect(btnShadow);
        connect(flow.button, &QPushButton::clicked, this, &Greenhouse3DWidget::onWorkflowClicked);
        workflowButtonIndexMap_.insert(flow.button, i);
        buttonsLayout->addWidget(flow.button, i / 2, i % 2);
        updateWorkflowButtonText(i);
    }
    buttonsLayout->setColumnStretch(0, 1);
    buttonsLayout->setColumnStretch(1, 1);
    workflowLayout->addWidget(buttonsBox);

    mainLayout->addWidget(workflowPanel, 1);

    toastWidget_ = new QWidget(this);
    toastWidget_->setObjectName(QStringLiteral("workflowToast"));
    toastWidget_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    toastWidget_->setFixedSize(360, 62);
    toastWidget_->hide();

    QHBoxLayout *toastLayout = new QHBoxLayout(toastWidget_);
    toastLayout->setContentsMargins(12, 8, 12, 8);
    toastLayout->setSpacing(6);

    toastLabel_ = new QLabel(toastWidget_);
    toastLabel_->setWordWrap(true);
    toastLabel_->setStyleSheet(QStringLiteral("color: #ffffff; font-size: 12px; font-weight: 700;"));
    toastLayout->addWidget(toastLabel_, 1);

    auto *toastShadow = new QGraphicsDropShadowEffect(toastWidget_);
    toastShadow->setBlurRadius(20);
    toastShadow->setOffset(0, 6);
    toastShadow->setColor(QColor(20, 25, 35, 65));
    toastWidget_->setGraphicsEffect(toastShadow);

    toastShowAnim_ = new QPropertyAnimation(toastWidget_, "geometry", this);
    toastShowAnim_->setDuration(220);
    toastShowAnim_->setEasingCurve(QEasingCurve::OutCubic);

    toastHideAnim_ = new QPropertyAnimation(toastWidget_, "geometry", this);
    toastHideAnim_->setDuration(220);
    toastHideAnim_->setEasingCurve(QEasingCurve::InCubic);
    connect(toastHideAnim_, &QPropertyAnimation::finished, this, [this]() {
        if (toastWidget_) {
            toastWidget_->hide();
        }
    });

    layoutToast();
}

void Greenhouse3DWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, QColor("#f4fbff"));
    bg.setColorAt(0.5, QColor("#edf6ff"));
    bg.setColorAt(1.0, QColor("#f6f9ff"));
    p.fillRect(rect(), bg);

    const qreal w = width();
    const qreal h = height();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(111, 187, 255, 38));
    p.drawEllipse(QPointF(0.22 * w, 0.18 * h),
                  150, 90);

    p.setBrush(QColor(126, 224, 185, 34));
    p.drawEllipse(QPointF(0.78 * w, 0.30 * h),
                  170, 102);

    p.setBrush(QColor(255, 197, 122, 28));
    p.drawEllipse(QPointF(0.48 * w, 0.82 * h),
                  190, 100);
}

void Greenhouse3DWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutToast();
}

void Greenhouse3DWidget::layoutToast()
{
    if (!toastWidget_) {
        return;
    }
    const int margin = 14;
    const int x = width() - toastWidget_->width() - margin;
    const int y = margin;
    const QRect targetRect(std::max(0, x), std::max(0, y), toastWidget_->width(), toastWidget_->height());
    toastVisibleRect_ = targetRect;
    if (!toastWidget_->isVisible()) {
        toastWidget_->setGeometry(targetRect);
    } else if ((!toastShowAnim_ || toastShowAnim_->state() != QAbstractAnimation::Running) &&
               (!toastHideAnim_ || toastHideAnim_->state() != QAbstractAnimation::Running)) {
        toastWidget_->setGeometry(targetRect);
    }
}

void Greenhouse3DWidget::showToast(const QString &message, const QString &level)
{
    if (!toastWidget_ || !toastLabel_) {
        return;
    }

    QString bg = QStringLiteral("rgba(39, 174, 96, 0.95)");
    if (level == QStringLiteral("WARN")) {
        bg = QStringLiteral("rgba(243, 156, 18, 0.95)");
    } else if (level == QStringLiteral("ERROR")) {
        bg = QStringLiteral("rgba(231, 76, 60, 0.96)");
    } else if (level == QStringLiteral("INFO")) {
        bg = QStringLiteral("rgba(52, 152, 219, 0.95)");
    }

    toastWidget_->setStyleSheet(QStringLiteral(
        "#workflowToast { background: %1; border-radius: 12px; border: 1px solid rgba(255,255,255,0.22); }").arg(bg));
    toastLabel_->setText(message);
    layoutToast();

    if (toastShowAnim_) toastShowAnim_->stop();
    if (toastHideAnim_) toastHideAnim_->stop();

    const QRect startRect(
        toastVisibleRect_.x() + 26,
        toastVisibleRect_.y(),
        toastVisibleRect_.width(),
        toastVisibleRect_.height());

    toastWidget_->setGeometry(startRect);
    toastWidget_->show();
    toastWidget_->raise();
    if (toastShowAnim_) {
        toastShowAnim_->setStartValue(startRect);
        toastShowAnim_->setEndValue(toastVisibleRect_);
        toastShowAnim_->start();
    }
    toastHideTimer_->start(2600);
}

void Greenhouse3DWidget::hideToastAnimated()
{
    if (!toastWidget_ || !toastWidget_->isVisible()) {
        return;
    }
    if (toastShowAnim_) toastShowAnim_->stop();
    if (!toastHideAnim_) {
        toastWidget_->hide();
        return;
    }

    const QRect startRect = toastWidget_->geometry();
    const QRect endRect(startRect.x() + 26, startRect.y(), startRect.width(), startRect.height());
    toastHideAnim_->setStartValue(startRect);
    toastHideAnim_->setEndValue(endRect);
    toastHideAnim_->start();
}

void Greenhouse3DWidget::animateWorkflowButtons()
{
    int index = 0;
    for (Workflow &flow : workflows_) {
        if (!flow.button) {
            continue;
        }

        const QRect endRect = flow.button->geometry();
        const QRect startRect(endRect.x(), endRect.y() + 16, endRect.width(), endRect.height());
        flow.button->setGeometry(startRect);

        auto *slide = new QPropertyAnimation(flow.button, "geometry", flow.button);
        slide->setDuration(320);
        slide->setStartValue(startRect);
        slide->setEndValue(endRect);
        slide->setEasingCurve(QEasingCurve::OutCubic);

        QTimer::singleShot(index * 70, this, [slide]() {
            slide->start(QAbstractAnimation::DeleteWhenStopped);
        });
        ++index;
    }
}

void Greenhouse3DWidget::setupWorkflows()
{
    workflows_.clear();

    auto appendStep = [](Workflow &flow, const QString &role, const QString &action) {
        WorkflowStep step;
        step.role = role;
        step.action = action;
        flow.steps.append(step);
    };

    Workflow w1;
    w1.name = QStringLiteral("晨间通风");
    w1.description = QStringLiteral("风机+卷膜开启，快速换气");
    w1.trigger = QStringLiteral("日出后/棚内湿度偏高/CO2偏高");
    w1.exitCondition = QStringLiteral("换气完成或进入降温/保温流程");
    w1.protection = QStringLiteral("雨天、大风时禁止开卷膜");
    w1.style = QStringLiteral(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #ffd36e,stop:1 #ff9f43);"
        "color:#4a2a00; border:1px solid #e88b1d; border-radius:10px; font-size:12px; font-weight:800; text-align:left; padding:6px 10px; }"
        "QPushButton:hover { background: #ffbe57; } QPushButton:pressed { background: #ffa63f; }");
    appendStep(w1, QStringLiteral("fan"), QStringLiteral("fwd"));
    appendStep(w1, QStringLiteral("top_roll"), QStringLiteral("fwd"));
    appendStep(w1, QStringLiteral("end_roll"), QStringLiteral("fwd"));
    appendStep(w1, QStringLiteral("wet_pad"), QStringLiteral("stop"));
    appendStep(w1, QStringLiteral("pump"), QStringLiteral("stop"));
    workflows_.append(w1);

    Workflow w2;
    w2.name = QStringLiteral("午间降温");
    w2.description = QStringLiteral("风机/卷膜/湿帘/水泵联动");
    w2.trigger = QStringLiteral("温度高于上限或持续升温");
    w2.exitCondition = QStringLiteral("温度回落到目标区间");
    w2.protection = QStringLiteral("缺水禁止水泵，雨天/大风限制卷膜");
    w2.style = QStringLiteral(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #72d6ff,stop:1 #3ba3ff);"
        "color:#042a52; border:1px solid #2f8add; border-radius:10px; font-size:12px; font-weight:800; text-align:left; padding:6px 10px; }"
        "QPushButton:hover { background: #61c8ff; } QPushButton:pressed { background: #48b1ff; }");
    appendStep(w2, QStringLiteral("fan"), QStringLiteral("fwd"));
    appendStep(w2, QStringLiteral("top_roll"), QStringLiteral("fwd"));
    appendStep(w2, QStringLiteral("end_roll"), QStringLiteral("fwd"));
    appendStep(w2, QStringLiteral("side_roll"), QStringLiteral("fwd"));
    appendStep(w2, QStringLiteral("wet_pad"), QStringLiteral("fwd"));
    appendStep(w2, QStringLiteral("pump"), QStringLiteral("fwd"));
    workflows_.append(w2);

    Workflow w3;
    w3.name = QStringLiteral("灌溉补水");
    w3.description = QStringLiteral("水泵开启，湿帘关闭");
    w3.trigger = QStringLiteral("基质水分低于下限或定时灌溉");
    w3.exitCondition = QStringLiteral("达到灌溉时长或水分恢复");
    w3.protection = QStringLiteral("缺水、泵故障、手动暂停时禁止启动");
    w3.style = QStringLiteral(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #8de7b3,stop:1 #48c774);"
        "color:#0b4723; border:1px solid #2ea85a; border-radius:10px; font-size:12px; font-weight:800; text-align:left; padding:6px 10px; }"
        "QPushButton:hover { background: #73dc9b; } QPushButton:pressed { background: #5cce86; }");
    appendStep(w3, QStringLiteral("pump"), QStringLiteral("fwd"));
    appendStep(w3, QStringLiteral("wet_pad"), QStringLiteral("stop"));
    workflows_.append(w3);

    Workflow w4;
    w4.name = QStringLiteral("保温闭棚");
    w4.description = QStringLiteral("卷膜回收，减少散热");
    w4.trigger = QStringLiteral("夜间/低温/保温时段");
    w4.exitCondition = QStringLiteral("温度恢复或进入通风时段");
    w4.protection = QStringLiteral("卷膜限位异常时停止并报警");
    w4.style = QStringLiteral(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #d7b8ff,stop:1 #9d7bff);"
        "color:#2d1463; border:1px solid #7e5fd6; border-radius:10px; font-size:12px; font-weight:800; text-align:left; padding:6px 10px; }"
        "QPushButton:hover { background: #c9a8ff; } QPushButton:pressed { background: #b898ff; }");
    appendStep(w4, QStringLiteral("top_roll"), QStringLiteral("stop"));
    appendStep(w4, QStringLiteral("end_roll"), QStringLiteral("stop"));
    appendStep(w4, QStringLiteral("side_roll"), QStringLiteral("stop"));
    appendStep(w4, QStringLiteral("fan"), QStringLiteral("stop"));
    workflows_.append(w4);

    Workflow w5;
    w5.name = QStringLiteral("夜间全停");
    w5.description = QStringLiteral("所有执行器停止");
    w5.trigger = QStringLiteral("夜间结束作业或人工全停");
    w5.exitCondition = QStringLiteral("所有绑定分组停止完成");
    w5.protection = QStringLiteral("保留急停优先级，失败设备需人工确认");
    w5.style = QStringLiteral(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #ff9ba5,stop:1 #ff5d6e);"
        "color:#5a0711; border:1px solid #df3f52; border-radius:10px; font-size:12px; font-weight:800; text-align:left; padding:6px 10px; }"
        "QPushButton:hover { background: #ff8592; } QPushButton:pressed { background: #ff6f80; }");
    appendStep(w5, QStringLiteral("fan"), QStringLiteral("stop"));
    appendStep(w5, QStringLiteral("top_roll"), QStringLiteral("stop"));
    appendStep(w5, QStringLiteral("end_roll"), QStringLiteral("stop"));
    appendStep(w5, QStringLiteral("side_roll"), QStringLiteral("stop"));
    appendStep(w5, QStringLiteral("wet_pad"), QStringLiteral("stop"));
    appendStep(w5, QStringLiteral("pump"), QStringLiteral("stop"));
    workflows_.append(w5);

    Workflow w6;
    w6.name = QStringLiteral("快速换气");
    w6.description = QStringLiteral("风机+端面卷膜开启");
    w6.trigger = QStringLiteral("棚内闷棚、CO2偏高或短时除湿");
    w6.exitCondition = QStringLiteral("换气完成或人工停止");
    w6.protection = QStringLiteral("大风/雨天限制端面卷膜");
    w6.style = QStringLiteral(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #7ce6de,stop:1 #29c6b8);"
        "color:#064840; border:1px solid #22a79b; border-radius:10px; font-size:12px; font-weight:800; text-align:left; padding:6px 10px; }"
        "QPushButton:hover { background: #63ddd4; } QPushButton:pressed { background: #4fd3c8; }");
    appendStep(w6, QStringLiteral("fan"), QStringLiteral("fwd"));
    appendStep(w6, QStringLiteral("end_roll"), QStringLiteral("fwd"));
    appendStep(w6, QStringLiteral("wet_pad"), QStringLiteral("stop"));
    workflows_.append(w6);
}

void Greenhouse3DWidget::updateBindingSummary()
{
    QStringList rows;
    for (const RoleBinding &binding : roleBindings_) {
        QString roleName;
        if (binding.role == QStringLiteral("fan")) roleName = QStringLiteral("风机");
        else if (binding.role == QStringLiteral("top_roll")) roleName = QStringLiteral("顶卷");
        else if (binding.role == QStringLiteral("end_roll")) roleName = QStringLiteral("端面卷膜");
        else if (binding.role == QStringLiteral("side_roll")) roleName = QStringLiteral("侧卷膜");
        else if (binding.role == QStringLiteral("wet_pad")) roleName = QStringLiteral("湿帘");
        else if (binding.role == QStringLiteral("pump")) roleName = QStringLiteral("水泵");
        else roleName = binding.role;

        if (binding.groupId > 0) {
            rows << QStringLiteral("%1 -> G%2(%3)").arg(roleName).arg(binding.groupId).arg(binding.groupName);
        } else {
            rows << QStringLiteral("%1 -> 未匹配到分组").arg(roleName);
        }
    }
    bindingLabel_->setText(rows.join(QStringLiteral("  |  ")));
    updateAllWorkflowButtonText();
}

QString Greenhouse3DWidget::roleDisplayName(const QString &role) const
{
    if (role == QStringLiteral("fan")) return QStringLiteral("风机");
    if (role == QStringLiteral("top_roll")) return QStringLiteral("顶卷");
    if (role == QStringLiteral("end_roll")) return QStringLiteral("端面卷膜");
    if (role == QStringLiteral("side_roll")) return QStringLiteral("侧卷膜");
    if (role == QStringLiteral("wet_pad")) return QStringLiteral("湿帘");
    if (role == QStringLiteral("pump")) return QStringLiteral("水泵");
    return role;
}

QString Greenhouse3DWidget::workflowStepsText(const Workflow &workflow) const
{
    QStringList parts;
    for (const WorkflowStep &step : workflow.steps) {
        parts << QStringLiteral("%1%2")
            .arg(actionDisplay(step.action))
            .arg(roleDisplayName(step.role));
    }
    return parts.join(QStringLiteral(" -> "));
}

QString Greenhouse3DWidget::workflowBindingText(const Workflow &workflow) const
{
    int bound = 0;
    QStringList missing;
    for (const WorkflowStep &step : workflow.steps) {
        bool found = false;
        for (const RoleBinding &binding : roleBindings_) {
            if (binding.role == step.role) {
                found = binding.groupId > 0;
                break;
            }
        }
        if (found) {
            ++bound;
        } else {
            const QString name = roleDisplayName(step.role);
            if (!missing.contains(name)) {
                missing << name;
            }
        }
    }

    if (missing.isEmpty()) {
        return QStringLiteral("绑定：%1/%2 已就绪").arg(bound).arg(workflow.steps.size());
    }
    return QStringLiteral("绑定：%1/%2，缺 %3")
        .arg(bound)
        .arg(workflow.steps.size())
        .arg(missing.join(QStringLiteral("/")));
}

void Greenhouse3DWidget::updateWorkflowButtonText(int workflowIndex)
{
    if (workflowIndex < 0 || workflowIndex >= workflows_.size()) {
        return;
    }

    Workflow &flow = workflows_[workflowIndex];
    if (!flow.button) {
        return;
    }

    const QString status = flow.runtimeStatus.isEmpty()
        ? QStringLiteral("状态：空闲")
        : flow.runtimeStatus;
    flow.button->setText(QStringLiteral("%1  |  %2\n%3\n触发：%4\n退出：%5\n保护：%6\n步骤：%7")
        .arg(flow.name)
        .arg(status)
        .arg(workflowBindingText(flow))
        .arg(flow.trigger)
        .arg(flow.exitCondition)
        .arg(flow.protection)
        .arg(workflowStepsText(flow)));
}

void Greenhouse3DWidget::updateAllWorkflowButtonText()
{
    for (int i = 0; i < workflows_.size(); ++i) {
        updateWorkflowButtonText(i);
    }
}

void Greenhouse3DWidget::setWorkflowButtonsEnabled(bool enabled)
{
    for (Workflow &flow : workflows_) {
        if (flow.button) {
            flow.button->setEnabled(enabled);
        }
    }
}

void Greenhouse3DWidget::onRefreshGroups()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        statusLabel_->setText(QStringLiteral("未连接RPC服务器，无法加载分组。"));
        return;
    }

    rpcClient_->callAsync(QStringLiteral("group.list"), QJsonObject(),
        [this](const QJsonValue &result, const QJsonObject &error) {
            if (!error.isEmpty() || !result.isObject()) {
                const QString msg = error.value(QStringLiteral("message")).toString();
                statusLabel_->setText(QStringLiteral("分组加载失败：%1").arg(msg));
                emit logMessage(QStringLiteral("流程页分组加载失败：%1").arg(msg), QStringLiteral("WARN"));
                return;
            }

            const QJsonArray groups = result.toObject().value(QStringLiteral("groups")).toArray();

            for (RoleBinding &binding : roleBindings_) {
                binding.groupId = -1;
                binding.groupName.clear();
                for (const QJsonValue &v : groups) {
                    const QJsonObject g = v.toObject();
                    const QString name = g.value(QStringLiteral("name")).toString();
                    const int groupId = g.value(QStringLiteral("groupId")).toInt(-1);
                    if (groupId <= 0) {
                        continue;
                    }
                    if (groupNameMatchesRole(name, binding.keywords)) {
                        binding.groupId = groupId;
                        binding.groupName = name;
                        break;
                    }
                }
            }

            updateBindingSummary();
            statusLabel_->setText(QStringLiteral("分组绑定已同步：%1")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
        },
        3000);
}

void Greenhouse3DWidget::onWorkflowClicked()
{
    auto *btn = qobject_cast<QPushButton *>(sender());
    if (!btn || !workflowButtonIndexMap_.contains(btn)) {
        return;
    }
    runWorkflow(workflowButtonIndexMap_.value(btn));
}

void Greenhouse3DWidget::executeGroupControl(
    int groupId,
    const QString &groupName,
    const QString &action,
    std::function<void(bool, const QString &)> callback)
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        callback(false, QStringLiteral("未连接服务器"));
        return;
    }

    QJsonObject params{
        {QStringLiteral("groupId"), groupId},
        {QStringLiteral("ch"), -1},
        {QStringLiteral("action"), action}
    };

    rpcClient_->callAsync(QStringLiteral("group.controlOptimized"), params,
        [this, params, groupName, action, callback](const QJsonValue &result, const QJsonObject &error) {
            if (!error.isEmpty()) {
                rpcClient_->callAsync(QStringLiteral("group.control"), params,
                    [groupName, action, callback](const QJsonValue &fbResult, const QJsonObject &fbError) {
                        if (!fbError.isEmpty()) {
                            callback(false, fbError.value(QStringLiteral("message")).toString());
                            return;
                        }
                        const QJsonObject fbObj = fbResult.toObject();
                        if (!fbObj.value(QStringLiteral("ok")).toBool()) {
                            callback(false, fbObj.value(QStringLiteral("error")).toString());
                            return;
                        }
                        callback(true, QStringLiteral("%1 %2").arg(groupName, actionDisplay(action)));
                    },
                    3000);
                return;
            }

            const QJsonObject obj = result.toObject();
            if (!obj.value(QStringLiteral("ok")).toBool()) {
                callback(false, obj.value(QStringLiteral("error")).toString());
                return;
            }

            callback(true, QStringLiteral("%1 %2").arg(groupName, actionDisplay(action)));
        },
        3000);
}

void Greenhouse3DWidget::runWorkflow(int workflowIndex)
{
    if (workflowIndex < 0 || workflowIndex >= workflows_.size()) {
        return;
    }
    if (workflowRunning_) {
        statusLabel_->setText(QStringLiteral("已有流程正在执行，请稍后。"));
        emit logMessage(QStringLiteral("已有流程正在执行"), QStringLiteral("WARN"));
        return;
    }
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        statusLabel_->setText(QStringLiteral("未连接服务器，无法执行流程。"));
        emit logMessage(QStringLiteral("未连接服务器"), QStringLiteral("ERROR"));
        return;
    }

    const Workflow workflow = workflows_[workflowIndex];
    workflowRunning_ = true;
    setWorkflowButtonsEnabled(false);
    workflows_[workflowIndex].runtimeStatus = QStringLiteral("状态：运行中 0/%1").arg(workflow.steps.size());
    updateWorkflowButtonText(workflowIndex);
    emit logMessage(QStringLiteral("开始执行流程：%1").arg(workflow.name), QStringLiteral("INFO"));

    auto indexPtr = std::make_shared<int>(0);
    auto failMessages = std::make_shared<QStringList>();
    auto runner = std::make_shared<std::function<void()>>();

    *runner = [this, workflow, workflowIndex, indexPtr, failMessages, runner]() mutable {
        if (*indexPtr >= workflow.steps.size()) {
            workflowRunning_ = false;
            setWorkflowButtonsEnabled(true);

            if (failMessages->isEmpty()) {
                statusLabel_->setText(QStringLiteral("流程[%1]执行完成").arg(workflow.name));
                workflows_[workflowIndex].runtimeStatus = QStringLiteral("状态：刚完成");
                emit logMessage(QStringLiteral("流程执行完成：%1").arg(workflow.name));
            } else {
                statusLabel_->setText(QStringLiteral("流程[%1]完成，%2步失败")
                    .arg(workflow.name)
                    .arg(failMessages->size()));
                workflows_[workflowIndex].runtimeStatus = QStringLiteral("状态：部分失败 %1步").arg(failMessages->size());
                emit logMessage(QStringLiteral("流程部分失败：%1").arg(failMessages->join(QStringLiteral("; "))),
                                QStringLiteral("WARN"));
            }
            updateWorkflowButtonText(workflowIndex);
            QTimer::singleShot(3500, this, [this, workflowIndex]() {
                if (workflowIndex >= 0 && workflowIndex < workflows_.size() && !workflowRunning_) {
                    workflows_[workflowIndex].runtimeStatus.clear();
                    updateWorkflowButtonText(workflowIndex);
                }
            });

            QTimer::singleShot(700, this, &Greenhouse3DWidget::onRefreshGroups);
            return;
        }

        const WorkflowStep step = workflow.steps[*indexPtr];
        *indexPtr += 1;

        int groupId = -1;
        QString groupName;
        for (const RoleBinding &binding : roleBindings_) {
            if (binding.role == step.role) {
                groupId = binding.groupId;
                groupName = binding.groupName;
                break;
            }
        }

        if (groupId <= 0) {
            failMessages->append(QStringLiteral("角色[%1]未匹配分组").arg(step.role));
            workflows_[workflowIndex].runtimeStatus = QStringLiteral("状态：跳过 %1，未绑定分组")
                .arg(roleDisplayName(step.role));
            updateWorkflowButtonText(workflowIndex);
            QTimer::singleShot(60, this, [runner]() { (*runner)(); });
            return;
        }

        statusLabel_->setText(QStringLiteral("流程[%1]: G%2(%3) -> %4")
            .arg(workflow.name)
            .arg(groupId)
            .arg(groupName)
            .arg(actionDisplay(step.action)));
        workflows_[workflowIndex].runtimeStatus = QStringLiteral("状态：运行中 %1/%2，%3%4")
            .arg(*indexPtr)
            .arg(workflow.steps.size())
            .arg(actionDisplay(step.action))
            .arg(roleDisplayName(step.role));
        updateWorkflowButtonText(workflowIndex);

        executeGroupControl(groupId, groupName, step.action,
            [this, groupName, failMessages, runner](bool ok, const QString &msg) {
                if (!ok) {
                    failMessages->append(QStringLiteral("%1: %2").arg(groupName, msg));
                }
                QTimer::singleShot(100, this, [runner]() { (*runner)(); });
            });
    };

    (*runner)();
}
