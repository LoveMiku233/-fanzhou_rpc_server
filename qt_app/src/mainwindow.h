/**
 * @file mainwindow.h
 * @brief 主窗口头文件
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QSplitter>
#include <QLineEdit>
#include <QSpinBox>

class RpcClient;
class DeviceWidget;
class GroupWidget;
class RelayControlWidget;

/**
 * @brief 主窗口类
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectButtonClicked();
    void onDisconnectButtonClicked();
    void onPingButtonClicked();
    void onSysInfoButtonClicked();
    void onSaveConfigButtonClicked();

    void onRpcConnected();
    void onRpcDisconnected();
    void onRpcError(const QString &error);
    void onRpcLogMessage(const QString &message);

    void onAutoRefreshTimeout();

private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupCentralWidget();
    void createConnectionPanel();
    void createLogPanel();

    void updateConnectionStatus(bool connected);
    void appendLog(const QString &message);
    void clearLog();

    // UI组件
    QTabWidget *tabWidget_;
    QTextEdit *logTextEdit_;
    QLabel *statusLabel_;
    QLabel *connectionStatusLabel_;

    // 连接面板组件
    QLineEdit *hostEdit_;
    QSpinBox *portSpinBox_;
    QPushButton *connectButton_;
    QPushButton *disconnectButton_;

    // 子页面
    DeviceWidget *deviceWidget_;
    GroupWidget *groupWidget_;
    RelayControlWidget *relayControlWidget_;

    // RPC客户端
    RpcClient *rpcClient_;

    // 自动刷新定时器
    QTimer *autoRefreshTimer_;
};

#endif // MAINWINDOW_H
