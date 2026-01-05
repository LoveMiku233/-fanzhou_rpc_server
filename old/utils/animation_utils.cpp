#include "animation_utils.h"
#include <QEvent>

QGraphicsOpacityEffect* AnimationUtils::ensureOpacityEffect(QWidget* widget)
{
    if (!widget) return nullptr;
    
    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(widget->graphicsEffect());
    if (!effect) {
        effect = new QGraphicsOpacityEffect(widget);
        effect->setOpacity(1.0);
        widget->setGraphicsEffect(effect);
    }
    return effect;
}

QPropertyAnimation* AnimationUtils::createFadeIn(QWidget* widget, 
                                                  int duration,
                                                  qreal startOpacity,
                                                  qreal endOpacity)
{
    if (!widget) return nullptr;
    
    auto* effect = ensureOpacityEffect(widget);
    effect->setOpacity(startOpacity);
    
    auto* anim = new QPropertyAnimation(effect, "opacity", widget);
    anim->setDuration(duration);
    anim->setStartValue(startOpacity);
    anim->setEndValue(endOpacity);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    
    return anim;
}

QPropertyAnimation* AnimationUtils::createFadeOut(QWidget* widget, int duration)
{
    if (!widget) return nullptr;
    
    auto* effect = ensureOpacityEffect(widget);
    
    auto* anim = new QPropertyAnimation(effect, "opacity", widget);
    anim->setDuration(duration);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::InCubic);
    
    return anim;
}

QPropertyAnimation* AnimationUtils::createSlideIn(QWidget* widget,
                                                   int direction,
                                                   int duration,
                                                   int offset)
{
    if (!widget) return nullptr;
    
    const QPoint originalPos = widget->pos();
    QPoint startPos = originalPos;
    
    switch (direction) {
        case 0: // from left
            startPos.setX(originalPos.x() - offset);
            break;
        case 1: // from right
            startPos.setX(originalPos.x() + offset);
            break;
        case 2: // from top
            startPos.setY(originalPos.y() - offset);
            break;
        case 3: // from bottom
            startPos.setY(originalPos.y() + offset);
            break;
    }
    
    widget->move(startPos);
    
    auto* anim = new QPropertyAnimation(widget, "pos", widget);
    anim->setDuration(duration);
    anim->setStartValue(startPos);
    anim->setEndValue(originalPos);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    
    return anim;
}

QPropertyAnimation* AnimationUtils::createScale(QWidget* widget,
                                                 qreal startScale,
                                                 qreal endScale,
                                                 int duration)
{
    if (!widget) return nullptr;
    
    // We'll use size property for scaling effect
    const QSize originalSize = widget->size();
    const QSize startSize = originalSize * startScale;
    const QSize endSize = originalSize * endScale;
    
    widget->resize(startSize);
    
    auto* anim = new QPropertyAnimation(widget, "size", widget);
    anim->setDuration(duration);
    anim->setStartValue(startSize);
    anim->setEndValue(endSize);
    anim->setEasingCurve(QEasingCurve::OutBack);
    
    return anim;
}

QSequentialAnimationGroup* AnimationUtils::createClickPulse(QWidget* widget, int duration)
{
    if (!widget) return nullptr;
    
    auto* effect = ensureOpacityEffect(widget);
    auto* group = new QSequentialAnimationGroup(widget);
    
    // Dim
    auto* fadeDown = new QPropertyAnimation(effect, "opacity");
    fadeDown->setDuration(duration / 2);
    fadeDown->setStartValue(1.0);
    fadeDown->setEndValue(0.7);
    fadeDown->setEasingCurve(QEasingCurve::InQuad);
    
    // Restore
    auto* fadeUp = new QPropertyAnimation(effect, "opacity");
    fadeUp->setDuration(duration / 2);
    fadeUp->setStartValue(0.7);
    fadeUp->setEndValue(1.0);
    fadeUp->setEasingCurve(QEasingCurve::OutQuad);
    
    group->addAnimation(fadeDown);
    group->addAnimation(fadeUp);
    
    return group;
}

void AnimationUtils::applyHoverGrowEffect(QWidget* widget, int growPixels)
{
    Q_UNUSED(widget);
    Q_UNUSED(growPixels);
    // This function is a placeholder for future implementation.
    // For hover effects, consider using widget-specific implementations
    // as seen in DeviceCardWidget which uses enterEvent/leaveEvent.
}
