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
#include <QRect>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QList>
#include <QElapsedTimer>
#include <QDateTime>
#include <QButtonGroup>
#include <QPixmap>

class RpcClient;
class HomeWidget;
class DeviceWidget;
class GroupWidget;
class StrategyWidget;
class SensorWidget;
class LogWidget;
class SettingsWidget;
class MonitorWidget;
class DebugWidget;
class Greenhouse3DWidget;
class PlantingAdviceWidget;
class ScreenManager;
class QResizeEvent;
class QWidget;
class QPropertyAnimation;
class QGraphicsOpacityEffect;

/**
 * @brief 主窗口类 - 大棚控制系统
 *
 * 采用顶部状态栏 + 中部内容区 + 底部菜单栏布局
 * 页面：主页、设备管理、分组管理、策略管理、日志、设置
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onConnectionStatusChanged(bool connected);
    void onAutoRefreshTimeout();
    void onLogMessage(const QString &message, const QString &level = QStringLiteral("INFO"));
    void updateStatusBarTime();
    void attemptAutoConnect();
    void updateCloudStatus();
    void onMqttStatusFromDashboard(int connected, int total);
    void onAutoScreenOffSettingsChanged(bool enabled, int timeoutSeconds);
    void onMenuButtonClicked(int index);

private:
    struct ToastRequest;
    struct ToastItem;

    void setupUi();
    void setupTopStatusBar();
    void setupCentralWidget();
    void createBottomNavBar();
    void createContentArea();
    void switchToPage(int index);
    void updateMenuSelection(int activeIndex);
    void updateStatusBarConnection(bool connected);
    void updateAlertLabel(const QString &text, const QString &style,
                          const QString &level, bool syncToast = true);
    void setupToast();
    void showToast(const QString &message, const QString &level = QStringLiteral("INFO"));
    void layoutToast();
    QString toastBackgroundForLevel(const QString &level) const;
    QString toastProgressForLevel(const QString &level) const;
    QString toastDisplayText(const QString &message, int repeatCount) const;
    void displayToast(const ToastRequest &request);
    bool mergeDuplicateToast(const QString &message, const QString &level);
    void updateToastLabel(ToastItem *item);
    void startToastTimers(ToastItem *item);
    void updateToastProgress(ToastItem *item);
    void updateAllToastProgress();
    void closeToast(ToastItem *item, bool animated = true);
    void drainToastQueue();
    void relayoutToasts(bool animated = true);
    QRect toastRectForIndex(int index) const;
    void updateBackgroundImageCache();

    QWidget *topStatusBar_;
    QHBoxLayout *topStatusLayout_;

    // UI组件
    QWidget *sidebar_;
    QHBoxLayout *sidebarLayout_;
    QButtonGroup *menuButtonGroup_;
    QList<QPushButton*> menuButtons_;
    QList<int> menuButtonToPageIndex_;
    QStackedWidget *contentStack_;
    QLabel *backgroundImageLabel_;

    // 状态栏组件
    QLabel *connectionStatusLabel_;
    QLabel *cloudStatusLabel_;
    QLabel *timeLabel_;
    QLabel *alertLabel_;

    struct ToastRequest {
        QString message;
        QString level;
        int repeatCount = 1;
        qint64 lastSeenMs = 0;
    };

    struct ToastItem {
        QWidget *container = nullptr;
        QLabel *label = nullptr;
        QWidget *progressTrack = nullptr;
        QWidget *progressFill = nullptr;
        QTimer *lifeTimer = nullptr;
        QPropertyAnimation *slideInAnim = nullptr;
        QPropertyAnimation *slideOutAnim = nullptr;
        QPropertyAnimation *fadeInAnim = nullptr;
        QPropertyAnimation *fadeOutAnim = nullptr;
        QGraphicsOpacityEffect *opacityEffect = nullptr;
        QString message;
        QString level;
        int repeatCount = 1;
        qint64 lastSeenMs = 0;
        qint64 durationMs = 2600;
        QElapsedTimer elapsed;
        bool closing = false;
    };
    QList<ToastItem*> activeToasts_;
    QList<ToastRequest> pendingToasts_;
    QTimer *toastProgressTimer_;
    int toastSequence_;
    bool lowPerformanceMode_ = true;
    bool useBackgroundImage_ = false;
    QPixmap backgroundImageSource_;
    QSize backgroundImageScaledSize_;

    // 子页面
    HomeWidget *homeWidget_;
    Greenhouse3DWidget *greenhouse3dWidget_;
    DeviceWidget *deviceWidget_;
    GroupWidget *groupWidget_;
    StrategyWidget *strategyWidget_;
    SensorWidget *sensorWidget_;
    LogWidget *logWidget_;
    SettingsWidget *settingsWidget_;
    MonitorWidget *monitorWidget_;
    DebugWidget *debugWidget_;
    PlantingAdviceWidget *plantingAdviceWidget_;

    // RPC客户端
    RpcClient *rpcClient_;

    // 屏幕管理器（自动息屏）
    ScreenManager *screenManager_;

    // 定时器
    QTimer *autoRefreshTimer_;
    QTimer *statusBarTimer_;

    // 当前页面索引
    int currentPageIndex_;

    // 最后一条报警信息
    QString lastAlertMessage_;
};

#endif // MAINWINDOW_H
