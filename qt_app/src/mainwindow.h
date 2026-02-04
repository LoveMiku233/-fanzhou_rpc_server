/**
 * @file mainwindow.h
 * @brief 主窗口头文件 - 大棚控制系统
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QList>
#include <QDateTime>

class RpcClient;
class HomeWidget;
class DeviceWidget;
class GroupWidget;
class StrategyWidget;
class SensorWidget;
class LogWidget;
class SettingsWidget;

/**
 * @brief 主窗口类 - 大棚控制系统
 * 
 * 采用左侧菜单栏 + 右侧内容区的布局设计
 * 页面：主页、设备管理、分组管理、策略管理、日志、设置
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onMenuButtonClicked();
    void onConnectionStatusChanged(bool connected);
    void onAutoRefreshTimeout();
    void onLogMessage(const QString &message, const QString &level = QStringLiteral("INFO"));
    void updateStatusBarTime();
    void attemptAutoConnect();
    void updateCloudStatus();
    void onMqttStatusFromDashboard(int connected, int total);

private:
    void setupUi();
    void setupStatusBar();
    void setupCentralWidget();
    void createSidebar();
    void createContentArea();
    void switchToPage(int index);
    void updateMenuButtonStyles(int activeIndex);
    void updateStatusBarConnection(bool connected);

    // UI组件
    QWidget *sidebar_;
    QVBoxLayout *sidebarLayout_;
    QList<QPushButton*> menuButtons_;
    QStackedWidget *contentStack_;
    
    // 状态栏组件
    QLabel *connectionStatusLabel_;
    QLabel *cloudStatusLabel_;
    QLabel *timeLabel_;
    QLabel *alertLabel_;

    // 子页面
    HomeWidget *homeWidget_;
    DeviceWidget *deviceWidget_;
    GroupWidget *groupWidget_;
    StrategyWidget *strategyWidget_;
    SensorWidget *sensorWidget_;
    LogWidget *logWidget_;
    SettingsWidget *settingsWidget_;

    // RPC客户端
    RpcClient *rpcClient_;

    // 定时器
    QTimer *autoRefreshTimer_;
    QTimer *statusBarTimer_;
    QTimer *cloudStatusTimer_;

    // 当前页面索引
    int currentPageIndex_;

    // 最后一条报警信息
    QString lastAlertMessage_;
};

#endif // MAINWINDOW_H
