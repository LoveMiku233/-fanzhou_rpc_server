/**
 * @file strategy_dialog.h
 * @brief 策略创建/编辑对话框
 */

#ifndef STRATEGY_DIALOG_H
#define STRATEGY_DIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>

class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QTableWidget;
class QDoubleSpinBox;

/**
 * @brief 策略编辑对话框 - 带滚动区域和动画效果
 */
class StrategyDialog : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(qreal popupOpacity READ popupOpacity WRITE setPopupOpacity)

public:
    explicit StrategyDialog(QWidget *parent = nullptr);
    
    void setStrategy(const QJsonObject &strategy, bool isEdit = false);
    QJsonObject getStrategy() const;
    
    // 动画相关
    void setPopupOpacity(qreal opacity);
    qreal popupOpacity() const { return m_popupOpacity; }

protected:
    void showEvent(QShowEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onAddAction();
    void onDeleteAction();
    void onAddCondition();
    void onDeleteCondition();
    void onAnimationFinished();

private:
    void setupUi();
    void setupAnimations();
    void loadActions(const QJsonArray &actions);
    void loadConditions(const QJsonArray &conditions);
    
    // 基本信息控件
    QSpinBox *idSpinBox_;
    QLineEdit *nameEdit_;
    QComboBox *typeCombo_;
    QCheckBox *enabledCheck_;
    QSpinBox *groupIdSpin_;
    
    // Actions控件
    QTableWidget *actionsTable_;
    QSpinBox *actionNodeSpin_;
    QSpinBox *actionChSpin_;
    QComboBox *actionValueCombo_;
    
    // Conditions控件
    QTableWidget *conditionsTable_;
    QLineEdit *condDeviceEdit_;
    QLineEdit *condIdEdit_;
    QComboBox *condOpCombo_;
    QDoubleSpinBox *condValueSpin_;
    
    // 动画
    QPropertyAnimation *m_fadeAnimation;
    QPropertyAnimation *m_scaleAnimation;
    QParallelAnimationGroup *m_animationGroup;
    qreal m_popupOpacity;
    bool m_isEdit;
};

#endif // STRATEGY_DIALOG_H
