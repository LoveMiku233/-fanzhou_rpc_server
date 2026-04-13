/**
 * @file knob_menu_widget.cpp
 * @brief 动画旋钮菜单组件实现
 */

#include "knob_menu_widget.h"
#include <QMouseEvent>
#include <cmath>

KnobMenuWidget::KnobMenuWidget(QWidget *parent)
    : QWidget(parent)
    , centerButton_(nullptr)
    , centerIconLabel_(nullptr)
    , centerTextLabel_(nullptr)
    , currentIndex_(0)
    , rotation_(0.0)
    , expansion_(0.0)
    , isDragging_(false)
    , isExpanded_(false)
    , dragStartRotation_(0.0)
{
    setupUi();

    // 设置透明背景
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);

    // 安装事件过滤器
    centerButton_->installEventFilter(this);
}

KnobMenuWidget::~KnobMenuWidget()
{
}

void KnobMenuWidget::setupUi()
{
    // 设置固定尺寸
    setFixedSize(TOTAL_RADIUS * 2, TOTAL_RADIUS * 2);

    // 创建动画
    rotationAnimation_ = new QPropertyAnimation(this, "rotation", this);
    rotationAnimation_->setDuration(300);
    rotationAnimation_->setEasingCurve(QEasingCurve::OutCubic);

    expansionAnimation_ = new QPropertyAnimation(this, "expansion", this);
    expansionAnimation_->setDuration(350);
    expansionAnimation_->setEasingCurve(QEasingCurve::OutElastic);

    animationGroup_ = new QParallelAnimationGroup(this);
    animationGroup_->addAnimation(rotationAnimation_);
    animationGroup_->addAnimation(expansionAnimation_);

    // 创建中心按钮
    centerButton_ = new QPushButton(this);
    centerButton_->setObjectName(QStringLiteral("knobCenterButton"));
    centerButton_->setFixedSize(KNOB_SIZE, KNOB_SIZE);
    centerButton_->setCursor(Qt::PointingHandCursor);
    centerButton_->setFocusPolicy(Qt::NoFocus);
    centerButton_->move(TOTAL_RADIUS - KNOB_SIZE / 2, TOTAL_RADIUS - KNOB_SIZE / 2);

    // 中心按钮布局
    QVBoxLayout *centerLayout = new QVBoxLayout(centerButton_);
    centerLayout->setContentsMargins(4, 4, 4, 4);
    centerLayout->setSpacing(2);
    centerLayout->setAlignment(Qt::AlignCenter);

    centerIconLabel_ = new QLabel(QStringLiteral("🌾"), centerButton_);
    centerIconLabel_->setAlignment(Qt::AlignCenter);
    centerIconLabel_->setStyleSheet(QStringLiteral("font-size: 28px; background: transparent;"));
    centerIconLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);

    centerTextLabel_ = new QLabel(QStringLiteral("菜单"), centerButton_);
    centerTextLabel_->setAlignment(Qt::AlignCenter);
    centerTextLabel_->setStyleSheet(QStringLiteral(
        "font-size: 11px; font-weight: bold; color: white; background: transparent;"));
    centerTextLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);

    centerLayout->addWidget(centerIconLabel_);
    centerLayout->addWidget(centerTextLabel_);

    connect(centerButton_, &QPushButton::clicked, this, &KnobMenuWidget::onCenterButtonClicked);

    // 添加阴影效果
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 80));
    shadow->setOffset(0, 4);
    centerButton_->setGraphicsEffect(shadow);
}

void KnobMenuWidget::setMenuItems(const QList<MenuItem> &items)
{
    menuItems_ = items;
    createMenuButtons();
}

void KnobMenuWidget::createMenuButtons()
{
    // 清除旧按钮
    qDeleteAll(menuButtons_);
    qDeleteAll(menuButtonEffects_);
    menuButtons_.clear();
    menuButtonEffects_.clear();

    for (const auto &item : menuItems_) {
        QPushButton *btn = new QPushButton(this);
        btn->setObjectName(QStringLiteral("knobMenuItem"));
        btn->setProperty("menuIndex", item.index);
        btn->setFixedSize(MENU_ITEM_SIZE, MENU_ITEM_SIZE);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->hide();

        // 按钮布局
        QVBoxLayout *btnLayout = new QVBoxLayout(btn);
        btnLayout->setContentsMargins(4, 4, 4, 4);
        btnLayout->setSpacing(2);
        btnLayout->setAlignment(Qt::AlignCenter);

        QLabel *iconLabel = new QLabel(item.icon, btn);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setStyleSheet(QStringLiteral("font-size: 22px; background: transparent;"));
        iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

        QLabel *textLabel = new QLabel(item.text, btn);
        textLabel->setAlignment(Qt::AlignCenter);
        textLabel->setStyleSheet(QStringLiteral(
            "font-size: 9px; font-weight: bold; color: white; background: transparent;"));
        textLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

        btnLayout->addWidget(iconLabel);
        btnLayout->addWidget(textLabel);

        // 透明度效果（用于淡入淡出）
        QGraphicsOpacityEffect *opacityEffect = new QGraphicsOpacityEffect(btn);
        opacityEffect->setOpacity(0.0);
        menuButtonEffects_.append(opacityEffect);
        btn->setGraphicsEffect(opacityEffect);

        connect(btn, &QPushButton::clicked, this, &KnobMenuWidget::onMenuItemClicked);

        menuButtons_.append(btn);
    }

    updateMenuPositions();
}

void KnobMenuWidget::setCurrentIndex(int index)
{
    if (index >= 0 && index < menuItems_.size()) {
        currentIndex_ = index;
        if (!menuItems_.isEmpty()) {
            centerIconLabel_->setText(menuItems_[currentIndex_].icon);
            centerTextLabel_->setText(menuItems_[currentIndex_].text);
        }
        animateToIndex(index);
    }
}

void KnobMenuWidget::setRotation(qreal rotation)
{
    if (!qFuzzyCompare(rotation_, rotation)) {
        rotation_ = rotation;
        updateMenuPositions();
        emit rotationChanged(rotation);
        update();
    }
}

void KnobMenuWidget::setExpansion(qreal expansion)
{
    expansion = qBound(0.0, expansion, 1.0);
    if (!qFuzzyCompare(expansion_, expansion)) {
        expansion_ = expansion;
        updateMenuPositions();
        emit expansionChanged(expansion);
        update();
    }
}

void KnobMenuWidget::expand()
{
    if (!isExpanded_) {
        isExpanded_ = true;

        // 显示所有菜单按钮
        for (auto btn : menuButtons_) {
            btn->show();
        }

        expansionAnimation_->stop();
        expansionAnimation_->setStartValue(expansion_);
        expansionAnimation_->setEndValue(1.0);
        expansionAnimation_->start();

        centerTextLabel_->setText(QStringLiteral("收起"));
    }
}

void KnobMenuWidget::collapse()
{
    if (isExpanded_) {
        isExpanded_ = false;

        expansionAnimation_->stop();
        expansionAnimation_->setStartValue(expansion_);
        expansionAnimation_->setEndValue(0.0);
        expansionAnimation_->start();

        if (!menuItems_.isEmpty()) {
            centerTextLabel_->setText(menuItems_[currentIndex_].text);
        }
    }
}

void KnobMenuWidget::toggle()
{
    if (isExpanded_) {
        collapse();
    } else {
        expand();
    }
}

void KnobMenuWidget::onCenterButtonClicked()
{
    toggle();
}

void KnobMenuWidget::onMenuItemClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    int index = btn->property("menuIndex").toInt();
    setCurrentIndex(index);
    collapse();
    emit menuSelected(index);
}

void KnobMenuWidget::updateMenuPositions()
{
    if (menuButtons_.isEmpty()) return;

    const qreal baseRadius = ORBIT_RADIUS_MIN + (ORBIT_RADIUS_MAX - ORBIT_RADIUS_MIN) * expansion_;
    const qreal centerX = TOTAL_RADIUS;
    const qreal centerY = TOTAL_RADIUS;

    for (int i = 0; i < menuButtons_.size(); ++i) {
        qreal angle = angleForIndex(i) - rotation_;
        QPointF pos = positionForAngle(angle, baseRadius);

        // 添加呼吸效果
        qreal breatheOffset = 0.0;
        if (isExpanded_ && i == currentIndex_) {
            breatheOffset = 8.0;
        }
        pos = positionForAngle(angle, baseRadius + breatheOffset);

        int x = qRound(centerX + pos.x() - MENU_ITEM_SIZE / 2.0);
        int y = qRound(centerY + pos.y() - MENU_ITEM_SIZE / 2.0);

        menuButtons_[i]->move(x, y);

        // 设置透明度
        qreal opacity = expansion_;
        if (i < menuButtonEffects_.size()) {
            menuButtonEffects_[i]->setOpacity(opacity);
        }

        // 根据展开状态显示/隐藏按钮
        if (expansion_ > 0.1) {
            menuButtons_[i]->show();
        } else if (expansion_ < 0.05) {
            menuButtons_[i]->hide();
        }

        // 高亮当前选中项
        if (i == currentIndex_ && isExpanded_) {
            menuButtons_[i]->setStyleSheet(QStringLiteral(
                "QPushButton {"
                "  background: qradialgradient(cx:0.5, cy:0.5, rx:0.5, ry:0.5, fx:0.5, fy:0.5,"
                "    stop:0 #f39c12, stop:1 #e67e22);"
                "  border: 3px solid #f1c40f;"
                "  border-radius: %1px;"
                "}"
                "QPushButton:hover {"
                "  background: qradialgradient(cx:0.5, cy:0.5, rx:0.5, ry:0.5, fx:0.5, fy:0.5,"
                "    stop:0 #f7dc6f, stop:1 #f39c12);"
                "}"
            ).arg(MENU_ITEM_SIZE / 2));
        } else {
            menuButtons_[i]->setStyleSheet(QStringLiteral(""));
        }
    }
}

void KnobMenuWidget::animateToIndex(int index)
{
    qreal targetAngle = angleForIndex(index);
    qreal currentAngle = std::fmod(rotation_, 360.0);
    if (currentAngle < 0) currentAngle += 360.0;

    qreal diff = targetAngle - currentAngle;
    if (diff > 180) diff -= 360;
    if (diff < -180) diff += 360;

    rotationAnimation_->stop();
    rotationAnimation_->setStartValue(rotation_);
    rotationAnimation_->setEndValue(rotation_ + diff);
    rotationAnimation_->start();
}

qreal KnobMenuWidget::angleForIndex(int index) const
{
    if (menuItems_.isEmpty()) return 0.0;
    // 从顶部开始，顺时针排列
    return (360.0 / menuItems_.size() * index) - 90.0;
}

int KnobMenuWidget::indexForAngle(qreal angle) const
{
    if (menuItems_.isEmpty()) return 0;
    angle = std::fmod(angle + 90.0 + 180.0 / menuItems_.size(), 360.0);
    if (angle < 0) angle += 360.0;
    return qBound(0, static_cast<int>(std::floor(angle / 360.0 * menuItems_.size())), menuItems_.size() - 1);
}

QPointF KnobMenuWidget::positionForAngle(qreal angle, qreal radius) const
{
    qreal rad = angle * M_PI / 180.0;
    return QPointF(std::cos(rad) * radius, std::sin(rad) * radius);
}

void KnobMenuWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    drawBackground(painter);

    if (isExpanded_ || expansion_ > 0.1) {
        drawOrbit(painter);
    }

    drawKnobBase(painter);
}

void KnobMenuWidget::drawBackground(QPainter &painter)
{
    // 半透明背景圈（仅在展开时显示）
    if (isExpanded_ || expansion_ > 0.1) {
        QPainterPath path;
        path.addEllipse(rect().center(), TOTAL_RADIUS - 5, TOTAL_RADIUS - 5);

        QColor bgColor(20, 30, 40, 100 * expansion_);
        painter.fillPath(path, bgColor);

        // 边框光晕
        QPen pen(QColor(243, 156, 18, 80 * expansion_), 2);
        painter.setPen(pen);
        painter.drawPath(path);
    }
}

void KnobMenuWidget::drawKnobBase(QPainter &painter)
{
    // 中心旋钮底座光晕
    QPointF center = rect().center();
    qreal glowRadius = KNOB_SIZE / 2 + 10;

    QRadialGradient glow(center, glowRadius);
    glow.setColorAt(0, QColor(243, 156, 18, 60));
    glow.setColorAt(1, QColor(243, 156, 18, 0));
    painter.setBrush(glow);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(center, glowRadius, glowRadius);
}

void KnobMenuWidget::drawOrbit(QPainter &painter)
{
    QPointF center = rect().center();
    qreal radius = ORBIT_RADIUS_MIN + (ORBIT_RADIUS_MAX - ORBIT_RADIUS_MIN) * expansion_;

    // 轨道圆环
    QPen orbitPen(QColor(243, 156, 18, 100 * expansion_), 2, Qt::DashLine);
    painter.setPen(orbitPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, radius, radius);

    // 轨道上的装饰点
    if (!menuItems_.isEmpty()) {
        painter.setPen(Qt::NoPen);
        for (int i = 0; i < menuItems_.size(); ++i) {
            qreal angle = angleForIndex(i) - rotation_;
            QPointF pos = positionForAngle(angle, radius);

            QRadialGradient dotGrad(center + pos, 8);
            dotGrad.setColorAt(0, QColor(243, 156, 18, 150 * expansion_));
            dotGrad.setColorAt(1, QColor(243, 156, 18, 0));
            painter.setBrush(dotGrad);
            painter.drawEllipse(center + pos, 6, 6);
        }
    }
}

void KnobMenuWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // 检查是否点击在轨道区域
        QPointF center = rect().center();
        QPointF delta = event->pos() - center;
        qreal dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());

        if (isExpanded_ && dist > KNOB_SIZE / 2 + 10 && dist < ORBIT_RADIUS_MAX + MENU_ITEM_SIZE) {
            isDragging_ = true;
            dragStartPos_ = event->pos();
            dragStartRotation_ = rotation_;
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void KnobMenuWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (isDragging_) {
        QPointF center = rect().center();
        QPointF delta = event->pos() - center;
        QPointF startDelta = dragStartPos_ - center;

        qreal angle = std::atan2(delta.y(), delta.x()) * 180.0 / M_PI;
        qreal startAngle = std::atan2(startDelta.y(), startDelta.x()) * 180.0 / M_PI;

        qreal deltaAngle = angle - startAngle;
        setRotation(dragStartRotation_ - deltaAngle);
        event->accept();
    }
    QWidget::mouseMoveEvent(event);
}

void KnobMenuWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (isDragging_ && event->button() == Qt::LeftButton) {
        isDragging_ = false;

        // 吸附到最近的菜单项
        qreal currentAngle = std::fmod(rotation_, 360.0);
        if (currentAngle < 0) currentAngle += 360.0;
        int snapIndex = indexForAngle(currentAngle);
        setCurrentIndex(snapIndex);
        emit menuSelected(snapIndex);

        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

bool KnobMenuWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == centerButton_) {
        if (event->type() == QEvent::HoverEnter) {
            // 悬停时的轻微放大动画
            QPropertyAnimation *anim = new QPropertyAnimation(centerButton_, "geometry");
            anim->setDuration(200);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            QRect startRect = centerButton_->geometry();
            QRect endRect = startRect.adjusted(-3, -3, 3, 3);
            anim->setStartValue(startRect);
            anim->setEndValue(endRect);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        } else if (event->type() == QEvent::HoverLeave) {
            // 恢复原尺寸
            QPropertyAnimation *anim = new QPropertyAnimation(centerButton_, "geometry");
            anim->setDuration(200);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            QRect startRect = centerButton_->geometry();
            QRect endRect = QRect(
                TOTAL_RADIUS - KNOB_SIZE / 2,
                TOTAL_RADIUS - KNOB_SIZE / 2,
                KNOB_SIZE, KNOB_SIZE
            );
            anim->setStartValue(startRect);
            anim->setEndValue(endRect);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }
    return QWidget::eventFilter(obj, event);
}
