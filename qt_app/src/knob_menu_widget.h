/**
 * @file knob_menu_widget.h
 * @brief 动画旋钮菜单组件 - 节省空间的圆形旋转菜单
 *
 * 特点：
 * - 圆形旋钮设计，占用空间小
 * - 平滑旋转动画
 * - 触摸友好的操作
 * - 半透明毛玻璃效果
 */

#ifndef KNOB_MENU_WIDGET_H
#define KNOB_MENU_WIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QTimer>
#include <QList>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QGraphicsOpacityEffect>
#include <QGraphicsDropShadowEffect>

class KnobMenuWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(qreal expansion READ expansion WRITE setExpansion NOTIFY expansionChanged)

public:
    explicit KnobMenuWidget(QWidget *parent = nullptr);
    ~KnobMenuWidget();

    struct MenuItem {
        QString text;
        QString icon;
        int index;
    };

    void setMenuItems(const QList<MenuItem> &items);
    void setCurrentIndex(int index);
    int currentIndex() const { return currentIndex_; }

    qreal rotation() const { return rotation_; }
    qreal expansion() const { return expansion_; }

public slots:
    void setRotation(qreal rotation);
    void setExpansion(qreal expansion);
    void expand();
    void collapse();
    void toggle();

signals:
    void menuSelected(int index);
    void rotationChanged(qreal rotation);
    void expansionChanged(qreal expansion);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onCenterButtonClicked();
    void onMenuItemClicked();
    void updateMenuPositions();

private:
    void setupUi();
    void createMenuButtons();
    void animateToIndex(int index);
    qreal angleForIndex(int index) const;
    int indexForAngle(qreal angle) const;
    QPointF positionForAngle(qreal angle, qreal radius) const;
    void drawBackground(QPainter &painter);
    void drawKnobBase(QPainter &painter);
    void drawOrbit(QPainter &painter);

    // 组件
    QPushButton *centerButton_;
    QLabel *centerIconLabel_;
    QLabel *centerTextLabel_;
    QList<QPushButton*> menuButtons_;
    QList<QGraphicsOpacityEffect*> menuButtonEffects_;
    QList<MenuItem> menuItems_;

    // 动画
    QPropertyAnimation *rotationAnimation_;
    QPropertyAnimation *expansionAnimation_;
    QParallelAnimationGroup *animationGroup_;

    // 状态
    int currentIndex_;
    qreal rotation_;
    qreal expansion_;  // 0.0 = 收起, 1.0 = 展开
    bool isDragging_;
    bool isExpanded_;
    QPoint dragStartPos_;
    qreal dragStartRotation_;

    // 尺寸常量
    static constexpr int KNOB_SIZE = 80;
    static constexpr int ORBIT_RADIUS_MIN = 50;
    static constexpr int ORBIT_RADIUS_MAX = 130;
    static constexpr int MENU_ITEM_SIZE = 56;
    static constexpr int TOTAL_RADIUS = ORBIT_RADIUS_MAX + MENU_ITEM_SIZE / 2 + 10;
};

#endif // KNOB_MENU_WIDGET_H
