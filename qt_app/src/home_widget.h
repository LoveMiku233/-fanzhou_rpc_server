/**
 * @file home_widget.h
 * @brief 主页头文件 - 大棚控制系统总览（增强版）
 */

#ifndef HOME_WIDGET_H
#define HOME_WIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QGridLayout>

class RpcClient;

/**
 * @brief 主页 - 显示大棚控制系统总体信息
 */
class HomeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HomeWidget(RpcClient *rpcClient, QWidget *parent = nullptr);

public slots:
    void refreshData();

signals:
    void navigateToDevices();
    void navigateToGroups();
    void navigateToSettings();

private slots:
    void onStopAllClicked();
    void onEmergencyStopClicked();
    void onAutoRefreshTimeout();

private:
    void setupUi();
    void updateStats();

    RpcClient *rpcClient_;
    QTimer *autoRefreshTimer_;

    // 统计信息标签
    QLabel *totalDevicesLabel_;
    QLabel *onlineDevicesLabel_;
    QLabel *offlineDevicesLabel_;
    QLabel *totalGroupsLabel_;
    QLabel *totalStrategiesLabel_;
    QLabel *totalSensorsLabel_;
    QLabel *canStatusLabel_;
    QLabel *mqttStatusLabel_;
    QLabel *connectionStatusLabel_;
    QLabel *systemUptimeLabel_;
    QLabel *lastUpdateLabel_;

    // 快捷操作按钮
    QPushButton *refreshButton_;
    QPushButton *stopAllButton_;
    QPushButton *emergencyStopButton_;
};

#endif // HOME_WIDGET_H
