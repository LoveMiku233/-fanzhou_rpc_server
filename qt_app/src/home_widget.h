/**
 * @file home_widget.h
 * @brief 主页头文件 - 大棚控制系统总览
 */

#ifndef HOME_WIDGET_H
#define HOME_WIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>

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

private:
    void setupUi();
    void updateStats();

    RpcClient *rpcClient_;

    // 统计信息标签
    QLabel *totalDevicesLabel_;
    QLabel *onlineDevicesLabel_;
    QLabel *offlineDevicesLabel_;
    QLabel *totalGroupsLabel_;
    QLabel *connectionStatusLabel_;
    QLabel *lastUpdateLabel_;

    // 快捷操作按钮
    QPushButton *refreshButton_;
    QPushButton *stopAllButton_;
    QPushButton *emergencyStopButton_;
};

#endif // HOME_WIDGET_H
