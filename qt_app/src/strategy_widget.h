/**
 * @file strategy_widget.h
 * @brief 策略管理页面头文件
 *
 * 提供定时策略、传感器策略、继电器策略的管理界面
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

class RpcClient;

/**
 * @brief 策略管理页面
 *
 * 管理所有自动化策略：
 * - 定时策略（auto.strategy）- 基于时间间隔触发分组控制
 * - 传感器策略（auto.sensor）- 基于传感器数据触发分组控制
 * - 继电器策略（auto.relay）- 基于时间间隔直接控制继电器
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
    void onDeleteTimerStrategyClicked();
    void onToggleTimerStrategyClicked();
    void onTriggerTimerStrategyClicked();
    void onTimerStrategyTableClicked(int row, int column);

    // 传感器策略
    void onRefreshSensorStrategiesClicked();
    void onCreateSensorStrategyClicked();
    void onDeleteSensorStrategyClicked();
    void onToggleSensorStrategyClicked();
    void onSensorStrategyTableClicked(int row, int column);

    // 继电器策略
    void onRefreshRelayStrategiesClicked();
    void onCreateRelayStrategyClicked();
    void onDeleteRelayStrategyClicked();
    void onToggleRelayStrategyClicked();
    void onRelayStrategyTableClicked(int row, int column);

private:
    void setupUi();
    QWidget* createTimerStrategyTab();
    QWidget* createSensorStrategyTab();
    QWidget* createRelayStrategyTab();

    void updateTimerStrategyTable(const QJsonArray &strategies);
    void updateSensorStrategyTable(const QJsonArray &strategies);
    void updateRelayStrategyTable(const QJsonArray &strategies);

    RpcClient *rpcClient_;
    QLabel *statusLabel_;
    QTabWidget *tabWidget_;

    // 定时策略
    QTableWidget *timerStrategyTable_;
    QSpinBox *timerIdSpinBox_;
    QLineEdit *timerNameEdit_;
    QSpinBox *timerGroupIdSpinBox_;
    QSpinBox *timerChannelSpinBox_;
    QComboBox *timerActionCombo_;
    QSpinBox *timerIntervalSpinBox_;
    QCheckBox *timerEnabledCheckBox_;

    // 传感器策略
    QTableWidget *sensorStrategyTable_;
    QSpinBox *sensorIdSpinBox_;
    QLineEdit *sensorNameEdit_;
    QComboBox *sensorTypeCombo_;
    QSpinBox *sensorNodeSpinBox_;
    QComboBox *sensorConditionCombo_;
    QDoubleSpinBox *sensorThresholdSpinBox_;
    QSpinBox *sensorGroupIdSpinBox_;
    QSpinBox *sensorChannelSpinBox_;
    QComboBox *sensorActionCombo_;
    QSpinBox *sensorCooldownSpinBox_;
    QCheckBox *sensorEnabledCheckBox_;

    // 继电器策略
    QTableWidget *relayStrategyTable_;
    QSpinBox *relayIdSpinBox_;
    QLineEdit *relayNameEdit_;
    QSpinBox *relayNodeIdSpinBox_;
    QSpinBox *relayChannelSpinBox_;
    QComboBox *relayActionCombo_;
    QSpinBox *relayIntervalSpinBox_;
    QCheckBox *relayEnabledCheckBox_;
};

#endif // STRATEGY_WIDGET_H
