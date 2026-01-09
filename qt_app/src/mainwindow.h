/**
 * @file mainwindow.h
 * @brief 主窗口头文件
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

class RpcClient;
class ConnectionWidget;
class DeviceWidget;
class GroupWidget;
class RelayControlWidget;

/**
 * @brief 主窗口类
 * 
 * 采用左侧菜单栏 + 右侧内容区的布局设计
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

private:
    void setupUi();
    void setupStatusBar();
    void setupCentralWidget();
    void createSidebar();
    void createContentArea();
    void switchToPage(int index);
    void updateMenuButtonStyles(int activeIndex);

    // UI组件
    QWidget *sidebar_;
    QVBoxLayout *sidebarLayout_;
    QList<QPushButton*> menuButtons_;
    QStackedWidget *contentStack_;
    QLabel *connectionStatusLabel_;

    // 子页面
    ConnectionWidget *connectionWidget_;
    DeviceWidget *deviceWidget_;
    GroupWidget *groupWidget_;
    RelayControlWidget *relayControlWidget_;

    // RPC客户端
    RpcClient *rpcClient_;

    // 自动刷新定时器
    QTimer *autoRefreshTimer_;

    // 当前页面索引
    int currentPageIndex_;
};

#endif // MAINWINDOW_H
