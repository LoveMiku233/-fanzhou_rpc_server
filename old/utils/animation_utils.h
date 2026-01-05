#ifndef ANIMATION_UTILS_H
#define ANIMATION_UTILS_H

#include <QWidget>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QSequentialAnimationGroup>
#include <QParallelAnimationGroup>
#include <QEasingCurve>

/**
 * @brief Utility class for common UI animations
 * 
 * Provides factory methods for creating smooth, reusable animations
 * for page transitions, hover effects, and click feedback.
 */
class AnimationUtils
{
public:
    /**
     * @brief Create a fade-in animation for a widget
     * @param widget Target widget
     * @param duration Animation duration in milliseconds
     * @param startOpacity Starting opacity (0.0 to 1.0)
     * @param endOpacity Ending opacity (0.0 to 1.0)
     * @return Animation object (caller takes ownership)
     */
    static QPropertyAnimation* createFadeIn(QWidget* widget, 
                                            int duration = 300,
                                            qreal startOpacity = 0.0,
                                            qreal endOpacity = 1.0);
    
    /**
     * @brief Create a fade-out animation for a widget
     * @param widget Target widget
     * @param duration Animation duration in milliseconds
     * @return Animation object (caller takes ownership)
     */
    static QPropertyAnimation* createFadeOut(QWidget* widget, int duration = 300);
    
    /**
     * @brief Create a slide-in animation from a direction
     * @param widget Target widget
     * @param direction 0=left, 1=right, 2=top, 3=bottom
     * @param duration Animation duration in milliseconds
     * @param offset Slide distance in pixels
     * @return Animation object (caller takes ownership)
     */
    static QPropertyAnimation* createSlideIn(QWidget* widget,
                                             int direction = 1,
                                             int duration = 300,
                                             int offset = 50);
    
    /**
     * @brief Create a scale animation (zoom effect)
     * @param widget Target widget
     * @param startScale Starting scale factor
     * @param endScale Ending scale factor
     * @param duration Animation duration in milliseconds
     * @return Animation object (caller takes ownership)
     */
    static QPropertyAnimation* createScale(QWidget* widget,
                                           qreal startScale = 0.95,
                                           qreal endScale = 1.0,
                                           int duration = 200);
    
    /**
     * @brief Apply a hover grow effect to a widget
     * @param widget Target widget
     * @param growPixels How many pixels to grow on hover
     */
    static void applyHoverGrowEffect(QWidget* widget, int growPixels = 4);
    
    /**
     * @brief Create a click pulse animation
     * @param widget Target widget
     * @param duration Total animation duration
     * @return Animation group (caller takes ownership)
     */
    static QSequentialAnimationGroup* createClickPulse(QWidget* widget, int duration = 150);
    
    /**
     * @brief Ensure widget has an opacity effect installed
     * @param widget Target widget
     * @return The opacity effect (may be newly created or existing)
     */
    static QGraphicsOpacityEffect* ensureOpacityEffect(QWidget* widget);
    
private:
    AnimationUtils() = default;
};

#endif // ANIMATION_UTILS_H
