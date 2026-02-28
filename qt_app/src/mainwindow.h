/**
 * @file mainwindow.h
 * @brief 主窗口 - 侧边栏导航 + 顶部标题栏 + 堆叠页面
 *
 * 1024×600 固定分辨率，深色主题，匹配 index3.html 布局。
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QLabel;
class QTimer;
class QPushButton;
class QStackedWidget;
class QHBoxLayout;
class QVBoxLayout;

class RpcClient;
class ScreenManager;

// Forward-declare page widgets
class DashboardPage;
class DeviceControlPage;
class ScenePage;
class AlarmPage;
class SensorPage;
class SettingsPage;

/**
 * @brief 应用主窗口
 *
 * 左侧 80px 侧边栏 + 右侧内容区（顶部标题栏 44px + QStackedWidget）。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(RpcClient *rpc, ScreenManager *screen,
                        QWidget *parent = nullptr);
    ~MainWindow() override;

    /** 切换到指定页面索引 */
    void switchToPage(int index);

    /** 更新报警角标数量 */
    void setAlarmCount(int count);

private slots:
    void updateClock();
    void onConnectionChanged(bool connected);

private:
    void setupUi();
    void setupConnections();

    QPushButton *createNavButton(const QString &icon, const QString &label,
                                 int pageIndex);

    // External components (not owned)
    RpcClient     *rpcClient_;
    ScreenManager *screenManager_;

    // Sidebar
    QWidget       *sidebar_;
    QLabel        *logoLabel_;
    QPushButton   *navButtons_[6];
    QLabel        *alarmBadge_;

    // Header
    QWidget       *headerBar_;
    QLabel        *headerTitle_;
    QLabel        *headerStatus_;
    QLabel        *connectionIndicator_;
    QLabel        *headerTime_;

    // Content
    QStackedWidget *contentStack_;

    // Pages
    DashboardPage     *dashboardPage_;
    DeviceControlPage *deviceControlPage_;
    ScenePage         *scenePage_;
    AlarmPage         *alarmPage_;
    SensorPage        *sensorPage_;
    SettingsPage      *settingsPage_;

    // Timer
    QTimer *clockTimer_;

    int currentPage_;
};

#endif // MAINWINDOW_H
