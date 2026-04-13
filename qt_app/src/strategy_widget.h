/**
 * @file strategy_widget.h
 * @brief 策略管理页面头文件 - 卡片式布局
 *
 * 提供自动化策略的管理界面
 * 使用卡片式显示，参考Web端设计
 */

#ifndef STRATEGY_WIDGET_H
#define STRATEGY_WIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QTabWidget>
#include <QJsonArray>
#include <QJsonObject>
#include <QFrame>
#include <QGridLayout>
#include <QScrollArea>
#include <QPropertyAnimation>

class RpcClient;

/**
 * @brief 策略卡片组件
 * 显示单个策略的信息和操作按钮
 */
class StrategyCard : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(qreal hoverScale READ hoverScale WRITE setHoverScale)

public:
    explicit StrategyCard(int strategyId, const QString &name, 
                         const QString &type, QWidget *parent = nullptr);
    ~StrategyCard();

    int strategyId() const { return strategyId_; }
    void updateInfo(const QString &name, const QString &description, 
                   bool enabled, bool running = false);
    
    void setHoverScale(qreal scale);
    qreal hoverScale() const { return m_hoverScale; }

signals:
    void toggleClicked(int strategyId, bool newState);
    void triggerClicked(int strategyId);
    void editClicked(int strategyId);
    void deleteClicked(int strategyId);

protected:
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void setupUi();
    void startHoverAnimation(qreal endScale);

    int strategyId_;
    QString name_;
    QString type_;
    bool enabled_;
    qreal m_hoverScale;
    
    QLabel *nameLabel_;
    QLabel *idLabel_;
    QLabel *typeLabel_;
    QLabel *descLabel_;
    QLabel *statusLabel_;
    QPushButton *toggleBtn_;
    QPushButton *editBtn_;
    QPushButton *deleteBtn_;
    
    QPropertyAnimation *m_hoverAnimation;
};

/**
 * @brief 策略管理页面 - 卡片式布局
 *
 * 管理所有自动化策略：
 * - 自动策略（auto.strategy）- 基于条件触发的智能控制
 */
class StrategyWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StrategyWidget(RpcClient *rpcClient, QWidget *parent = nullptr);

signals:
    void logMessage(const QString &message, const QString &level = QStringLiteral("INFO"));

public slots:
    void refreshAllStrategies();

private slots:
    void onRefreshStrategiesClicked();
    void onCreateStrategyClicked();
    void onEditStrategyClicked(int strategyId);
    void onToggleStrategy(int strategyId, bool newState);
    void onTriggerStrategy(int strategyId);
    void onDeleteStrategy(int strategyId);

private:
    void setupUi();
    void updateStrategyCards(const QJsonArray &strategies);
    void clearStrategyCards();
    
    // 创建/编辑策略对话框
    bool showStrategyDialog(QJsonObject &strategy, bool isEdit = false);
    QWidget* createActionsEditor(QJsonArray &actions);
    QWidget* createConditionsEditor(QJsonArray &conditions);

    RpcClient *rpcClient_;
    QLabel *statusLabel_;
    
    // 卡片容器
    QWidget *cardsContainer_;
    QGridLayout *cardsLayout_;
    QList<StrategyCard*> strategyCards_;
    
    // 缓存
    QJsonArray strategiesCache_;
};

#endif // STRATEGY_WIDGET_H
