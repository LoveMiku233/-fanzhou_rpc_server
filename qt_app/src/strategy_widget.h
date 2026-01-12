/**
 * @file strategy_widget.h
 * @brief 策略管理页面头文件 - 卡片式布局
 *
 * 提供定时策略、传感器策略、继电器策略的管理界面
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

class RpcClient;

/**
 * @brief 策略卡片组件
 * 显示单个策略的信息和操作按钮
 */
class StrategyCard : public QFrame
{
    Q_OBJECT

public:
    explicit StrategyCard(int strategyId, const QString &name, 
                         const QString &type, QWidget *parent = nullptr);

    int strategyId() const { return strategyId_; }
    void updateInfo(const QString &name, const QString &description, 
                   bool enabled, bool running = false);

signals:
    void toggleClicked(int strategyId, bool newState);
    void triggerClicked(int strategyId);
    void deleteClicked(int strategyId);

private:
    void setupUi();

    int strategyId_;
    QString name_;
    QString type_;
    bool enabled_;
    
    QLabel *nameLabel_;
    QLabel *idLabel_;
    QLabel *typeLabel_;
    QLabel *descLabel_;
    QLabel *statusLabel_;
    QPushButton *toggleBtn_;
};

/**
 * @brief 策略管理页面 - 卡片式布局
 *
 * 管理所有自动化策略：
 * - 定时策略（auto.strategy）- 基于时间间隔或每日定时触发分组控制
 * - 传感器策略（auto.sensor）- 基于传感器数据触发分组控制
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
    // 定时策略
    void onRefreshTimerStrategiesClicked();
    void onCreateTimerStrategyClicked();
    void onToggleTimerStrategy(int strategyId, bool newState);
    void onTriggerTimerStrategy(int strategyId);
    void onDeleteTimerStrategy(int strategyId);

    // 传感器策略
    void onRefreshSensorStrategiesClicked();
    void onCreateSensorStrategyClicked();
    void onToggleSensorStrategy(int strategyId, bool newState);
    void onDeleteSensorStrategy(int strategyId);

private:
    void setupUi();
    QWidget* createTimerStrategyTab();
    QWidget* createSensorStrategyTab();

    void updateTimerStrategyCards(const QJsonArray &strategies);
    void updateSensorStrategyCards(const QJsonArray &strategies);
    void clearTimerStrategyCards();
    void clearSensorStrategyCards();

    RpcClient *rpcClient_;
    QLabel *statusLabel_;
    QTabWidget *tabWidget_;

    // 定时策略卡片
    QWidget *timerCardsContainer_;
    QGridLayout *timerCardsLayout_;
    QList<StrategyCard*> timerStrategyCards_;

    // 传感器策略卡片
    QWidget *sensorCardsContainer_;
    QGridLayout *sensorCardsLayout_;
    QList<StrategyCard*> sensorStrategyCards_;
};

#endif // STRATEGY_WIDGET_H
