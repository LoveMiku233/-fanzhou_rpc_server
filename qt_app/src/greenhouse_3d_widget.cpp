/**
 * @file greenhouse_3d_widget.cpp
 * @brief 大棚常用流程页面实现
 */

#include "greenhouse_3d_widget.h"

#include "rpc_client.h"

#include <QAbstractAnimation>
#include <QDateTime>
#include <QDialog>
#include <QEasingCurve>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>

namespace {

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

bool isFanDevice(const QString &deviceName)
{
    return deviceName.startsWith(QStringLiteral("风机"));
}

}  // namespace

Greenhouse3DWidget::Greenhouse3DWidget(RpcClient *rpcClient, QWidget *parent)
    : QWidget(parent)
    , rpcClient_(rpcClient)
    , titleLabel_(nullptr)
    , hintLabel_(nullptr)
    , statusLabel_(nullptr)
    , bindingLabel_(nullptr)
    , greenhouseImageLabel_(nullptr)
    , toastWidget_(nullptr)
    , toastLabel_(nullptr)
    , refreshTimer_(new QTimer(this))
    , toastHideTimer_(new QTimer(this))
    , toastShowAnim_(nullptr)
    , toastHideAnim_(nullptr)
{
    setupUi();
    updateBindingSummary();

    connect(refreshTimer_, &QTimer::timeout, this, &Greenhouse3DWidget::onRefreshGroups);
    refreshTimer_->start(5000);
    QTimer::singleShot(200, this, &Greenhouse3DWidget::onRefreshGroups);

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

    const QStringList deviceNames{
        QStringLiteral("风机1"), QStringLiteral("风机2"),
        QStringLiteral("湿帘1"), QStringLiteral("湿帘2"),
        QStringLiteral("顶帘1"), QStringLiteral("顶帘2"),
        QStringLiteral("端面1"), QStringLiteral("端面2"),
        QStringLiteral("外遮阳1"), QStringLiteral("外遮阳2"),
        QStringLiteral("内遮阳1"), QStringLiteral("内遮阳2")
    };
    deviceStatusMap_.clear();
    for (const QString &name : deviceNames) {
        deviceStatusMap_.insert(name, isFanDevice(name) ? QStringLiteral("关闭") : QStringLiteral("停止"));
    }

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 8, 10, 8);
    mainLayout->setSpacing(8);

    titleLabel_ = new QLabel(QStringLiteral("大棚设备控制"), this);
    titleLabel_->setStyleSheet(QStringLiteral(
        "font-size: 20px; font-weight: 900; color: #17364a; padding: 0 2px;"));
    mainLayout->addWidget(titleLabel_);

    QWidget *visualPanel = new QWidget(this);
    visualPanel->setStyleSheet(QStringLiteral(
        "background: #ffffff; border: 1px solid #d5e3ee; border-radius: 18px;"));

    QHBoxLayout *visualLayout = new QHBoxLayout(visualPanel);
    visualLayout->setContentsMargins(18, 18, 18, 18);
    visualLayout->setSpacing(20);

    QWidget *buttonPanel = new QWidget(visualPanel);
    QGridLayout *buttonLayout = new QGridLayout(buttonPanel);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setHorizontalSpacing(12);
    buttonLayout->setVerticalSpacing(12);

    deviceButtons_.clear();
    for (int i = 0; i < deviceNames.size(); ++i) {
        QPushButton *button = new QPushButton(deviceNames.at(i), buttonPanel);
        button->setProperty("deviceName", deviceNames.at(i));
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumSize(92, 48);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(button, &QPushButton::clicked, this, &Greenhouse3DWidget::onDeviceButtonClicked);
        deviceButtons_.append(button);
        buttonLayout->addWidget(button, i / 2, i % 2);
    }
    buttonLayout->setColumnStretch(0, 1);
    buttonLayout->setColumnStretch(1, 1);
    visualLayout->addWidget(buttonPanel, 0);

    greenhouseImageLabel_ = new QLabel(visualPanel);
    greenhouseImageLabel_->setAlignment(Qt::AlignCenter);
    greenhouseImageLabel_->setMinimumSize(520, 360);
    greenhouseImageLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    greenhouseImageLabel_->setStyleSheet(QStringLiteral(
        "background: #ffffff; border: 1px solid #d4e2eb; border-radius: 14px; padding: 4px;"));
    visualLayout->addWidget(greenhouseImageLabel_, 1);

    mainLayout->addWidget(visualPanel, 1);

    greenhousePixmap_ = QPixmap(QStringLiteral(":/images/greenhouse_page.png"));
    if (!greenhousePixmap_.isNull() && greenhousePixmap_.width() > 800) {
        greenhousePixmap_ = greenhousePixmap_.scaledToWidth(800, Qt::FastTransformation);
    }
    updateGreenhouseImage();
    updateDeviceButtonStyles();

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
    QWidget::paintEvent(event);
}

void Greenhouse3DWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateGreenhouseImage();
    layoutToast();
}

void Greenhouse3DWidget::updateGreenhouseImage()
{
    if (!greenhouseImageLabel_) {
        return;
    }

    if (greenhousePixmap_.isNull()) {
        greenhouseImageLabel_->setText(QStringLiteral("未找到大棚图片资源"));
        greenhouseImageLabel_->setPixmap(QPixmap());
        return;
    }

    const QSize targetSize = greenhouseImageLabel_->contentsRect().size();
    if (targetSize.width() <= 0 || targetSize.height() <= 0) {
        return;
    }
    if (greenhouseDisplaySize_ == targetSize && !greenhouseDisplayPixmap_.isNull()) {
        greenhouseImageLabel_->setText(QString());
        greenhouseImageLabel_->setPixmap(greenhouseDisplayPixmap_);
        return;
    }

    const QSize fitSize(
        static_cast<int>(targetSize.width() * 1.12),
        static_cast<int>(targetSize.height() * 1.12));
    const QPixmap scaledPixmap = greenhousePixmap_.scaled(
        fitSize,
        Qt::KeepAspectRatio,
        Qt::FastTransformation);

    QPixmap composedPixmap(targetSize);
    composedPixmap.fill(Qt::white);
    {
        QPainter painter(&composedPixmap);
        const int offsetX = (targetSize.width() - scaledPixmap.width()) / 2;
        const int offsetY = (targetSize.height() - scaledPixmap.height()) / 2 - 10;
        painter.drawPixmap(offsetX, offsetY, scaledPixmap);
    }

    greenhouseDisplaySize_ = targetSize;
    greenhouseDisplayPixmap_ = composedPixmap;
    greenhouseImageLabel_->setText(QString());
    greenhouseImageLabel_->setPixmap(greenhouseDisplayPixmap_);
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

void Greenhouse3DWidget::updateDeviceButtonStyles()
{
    for (QPushButton *button : deviceButtons_) {
        if (!button) {
            continue;
        }
        const QString deviceName = button->property("deviceName").toString();
        const QString defaultStatus = isFanDevice(deviceName) ? QStringLiteral("关闭") : QStringLiteral("停止");
        const QString status = deviceStatusMap_.value(deviceName, defaultStatus);
        if (isFanDevice(deviceName) &&
            (status == QStringLiteral("打开") || status == QStringLiteral("运行中"))) {
            button->setStyleSheet(QStringLiteral(
                "QPushButton { background: #4f9f69; color: #ffffff; border: 1px solid #2f6f45; "
                "border-radius: 8px; font-size: 14px; font-weight: 700; }"
                "QPushButton:hover { background: #468f5f; }"
                "QPushButton:pressed { background: #3b7a50; }"));
        } else if (status == QStringLiteral("收起")) {
            button->setStyleSheet(QStringLiteral(
                "QPushButton { background: #4f9f69; color: #ffffff; border: 1px solid #2f6f45; "
                "border-radius: 8px; font-size: 14px; font-weight: 700; }"
                "QPushButton:hover { background: #468f5f; }"
                "QPushButton:pressed { background: #3b7a50; }"));
        } else if (status == QStringLiteral("展开")) {
            button->setStyleSheet(QStringLiteral(
                "QPushButton { background: #3f7fd9; color: #ffffff; border: 1px solid #295aa0; "
                "border-radius: 8px; font-size: 14px; font-weight: 700; }"
                "QPushButton:hover { background: #376fc0; }"
                "QPushButton:pressed { background: #2f62ab; }"));
        } else {
            button->setStyleSheet(QStringLiteral(
                "QPushButton { background: #ffffff; color: #30424d; border: 1px solid #aebdc8; "
                "border-radius: 8px; font-size: 14px; font-weight: 700; }"
                "QPushButton:hover { background: #f4f8fb; }"
                "QPushButton:pressed { background: #e8f0f5; }"));
        }
    }
}

void Greenhouse3DWidget::showDeviceStatusDialog(const QString &deviceName)
{
    QDialog dialog(this);
    dialog.setWindowTitle(deviceName + QStringLiteral("状态"));
    dialog.setModal(true);
    dialog.setMinimumSize(420, 250);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background: #f5f8ff; }"
        "QLabel { color: #20343d; }"
        "QPushButton { min-width: 96px; min-height: 42px; border-radius: 8px; font-size: 15px; font-weight: 700; }"));

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(22, 22, 22, 22);
    layout->setSpacing(20);

    QLabel *title = new QLabel(deviceName, &dialog);
    title->setStyleSheet(QStringLiteral("font-size: 30px; font-weight: 800; color: #1a3242;"));
    layout->addWidget(title);

    QHBoxLayout *statusLayout = new QHBoxLayout();
    statusLayout->setSpacing(12);
    QLabel *statusTitle = new QLabel(QStringLiteral("当前状态："), &dialog);
    statusTitle->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: 700;"));
    const QString defaultStatus = isFanDevice(deviceName) ? QStringLiteral("关闭") : QStringLiteral("停止");
    QLabel *statusValue = new QLabel(deviceStatusMap_.value(deviceName, defaultStatus), &dialog);
    statusValue->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: 800; color: #2f9e5b;"));
    statusLayout->addWidget(statusTitle);
    statusLayout->addWidget(statusValue);
    statusLayout->addStretch(1);
    layout->addLayout(statusLayout);

    layout->addStretch(1);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(18);
    buttonLayout->addStretch(1);

    if (isFanDevice(deviceName)) {
        QPushButton *openButton = new QPushButton(QStringLiteral("打开"), &dialog);
        openButton->setStyleSheet(QStringLiteral(
            "QPushButton { background: #eef7f1; color: #17663b; border: 1px solid #90d2a6; }"
            "QPushButton:hover { background: #e0f1e6; }"
            "QPushButton:pressed { background: #d2eadb; }"));
        QPushButton *closeButton = new QPushButton(QStringLiteral("关闭"), &dialog);
        closeButton->setStyleSheet(QStringLiteral(
            "QPushButton { background: #f6f8fa; color: #384852; border: 1px solid #b9c7d0; }"
            "QPushButton:hover { background: #ebf0f4; }"
            "QPushButton:pressed { background: #dee7ed; }"));

        buttonLayout->addWidget(openButton);
        buttonLayout->addWidget(closeButton);

        connect(openButton, &QPushButton::clicked, &dialog, [this, deviceName, statusValue, &dialog]() {
            deviceStatusMap_[deviceName] = QStringLiteral("打开");
            statusValue->setText(QStringLiteral("打开"));
            updateDeviceButtonStyles();
            dialog.accept();
        });
        connect(closeButton, &QPushButton::clicked, &dialog, [this, deviceName, statusValue, &dialog]() {
            deviceStatusMap_[deviceName] = QStringLiteral("关闭");
            statusValue->setText(QStringLiteral("关闭"));
            updateDeviceButtonStyles();
            dialog.accept();
        });
    } else {
        QPushButton *collapseButton = new QPushButton(QStringLiteral("收起"), &dialog);
        collapseButton->setStyleSheet(QStringLiteral(
            "QPushButton { background: #eef7f1; color: #17663b; border: 1px solid #90d2a6; }"
            "QPushButton:hover { background: #e0f1e6; }"
            "QPushButton:pressed { background: #d2eadb; }"));
        QPushButton *stopButton = new QPushButton(QStringLiteral("停止"), &dialog);
        stopButton->setStyleSheet(QStringLiteral(
            "QPushButton { background: #f6f8fa; color: #384852; border: 1px solid #b9c7d0; }"
            "QPushButton:hover { background: #ebf0f4; }"
            "QPushButton:pressed { background: #dee7ed; }"));
        QPushButton *expandButton = new QPushButton(QStringLiteral("展开"), &dialog);
        expandButton->setStyleSheet(QStringLiteral(
            "QPushButton { background: #eef4ff; color: #2559a7; border: 1px solid #9ab8e6; }"
            "QPushButton:hover { background: #e2ecff; }"
            "QPushButton:pressed { background: #d5e3ff; }"));

        buttonLayout->addWidget(collapseButton);
        buttonLayout->addWidget(stopButton);
        buttonLayout->addWidget(expandButton);

        connect(collapseButton, &QPushButton::clicked, &dialog, [this, deviceName, statusValue, &dialog]() {
            deviceStatusMap_[deviceName] = QStringLiteral("收起");
            statusValue->setText(QStringLiteral("收起"));
            updateDeviceButtonStyles();
            dialog.accept();
        });
        connect(stopButton, &QPushButton::clicked, &dialog, [this, deviceName, statusValue, &dialog]() {
            deviceStatusMap_[deviceName] = QStringLiteral("停止");
            statusValue->setText(QStringLiteral("停止"));
            updateDeviceButtonStyles();
            dialog.accept();
        });
        connect(expandButton, &QPushButton::clicked, &dialog, [this, deviceName, statusValue, &dialog]() {
            deviceStatusMap_[deviceName] = QStringLiteral("展开");
            statusValue->setText(QStringLiteral("展开"));
            updateDeviceButtonStyles();
            dialog.accept();
        });
    }

    buttonLayout->addStretch(1);
    layout->addLayout(buttonLayout);

    dialog.exec();
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

void Greenhouse3DWidget::updateBindingSummary()
{
    if (!bindingLabel_) {
        return;
    }
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
}

void Greenhouse3DWidget::onRefreshGroups()
{
    if (!rpcClient_ || !rpcClient_->isConnected()) {
        if (statusLabel_) {
            statusLabel_->setText(QStringLiteral("未连接RPC服务器，无法加载分组。"));
        }
        return;
    }

    rpcClient_->callAsync(QStringLiteral("group.list"), QJsonObject(), this,
        [this](const QJsonValue &result, const QJsonObject &error) {
            if (!error.isEmpty() || !result.isObject()) {
                const QString msg = error.value(QStringLiteral("message")).toString();
                if (statusLabel_) {
                    statusLabel_->setText(QStringLiteral("分组加载失败：%1").arg(msg));
                }
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
            if (statusLabel_) {
                statusLabel_->setText(QStringLiteral("分组绑定已同步：%1")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
            }
        },
        3000);
}

void Greenhouse3DWidget::onDeviceButtonClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    if (!button) {
        return;
    }
    const QString deviceName = button->property("deviceName").toString();
    if (deviceName.isEmpty()) {
        return;
    }
    showDeviceStatusDialog(deviceName);
}
