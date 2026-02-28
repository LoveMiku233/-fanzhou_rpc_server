/**
 * @file views/dashboard_page.h
 * @brief 驾驶舱页面 - 气象/环境/紧急停止/趋势
 *
 * 匹配 index3.html 驾驶舱视图，1024×600 深色主题。
 */

#ifndef DASHBOARD_PAGE_H
#define DASHBOARD_PAGE_H

#include <QWidget>

class QFrame;
class QLabel;
class QProgressBar;
class QPushButton;
class RpcClient;

class DashboardPage : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardPage(RpcClient *rpc, QWidget *parent = nullptr);
    ~DashboardPage() override = default;

    /** 刷新页面数据（预留） */
    void refreshData();

signals:
    void emergencyStopClicked();

private:
    void setupUi();

    QFrame *createGlassPanel(const QString &title);

    RpcClient *rpcClient_;

    // 室外气象站值标签
    QLabel *weatherTempValue_;
    QLabel *weatherHumidityValue_;
    QLabel *weatherWindValue_;
    QLabel *weatherLightValue_;
    QLabel *weatherRainValue_;

    // 棚内环境监测值标签 + 进度条
    QLabel       *indoorTempValue_;
    QLabel       *indoorHumidityValue_;
    QLabel       *indoorCo2Value_;
    QLabel       *indoorLightValue_;
    QProgressBar *indoorTempBar_;
    QProgressBar *indoorHumidityBar_;
    QProgressBar *indoorCo2Bar_;
    QProgressBar *indoorLightBar_;

    // 紧急停止按钮
    QPushButton *emergencyStopBtn_;

    // 趋势图占位
    QLabel *trendPlaceholder_;
};

#endif // DASHBOARD_PAGE_H
