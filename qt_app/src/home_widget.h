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
    /**
     * @brief 当MQTT状态更新时发出此信号
     * @param connected 已连接的通道数
     * @param total 总通道数
     */
    void mqttStatusUpdated(int connected, int total);

private slots:
    void onStopAllClicked();
    void onEmergencyStopClicked();

private:
    void setupUi();
    void updateStats();
    void updateStatsLegacy();  // 兼容旧版本服务器的多RPC调用方式

    RpcClient *rpcClient_;
    // 注意：自动刷新由MainWindow统一管理，不再需要独立的定时器

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
