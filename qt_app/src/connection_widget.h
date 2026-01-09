/**
 * @file connection_widget.h
 * @brief 连接设置页面头文件
 */

#ifndef CONNECTION_WIDGET_H
#define CONNECTION_WIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>

class RpcClient;

/**
 * @brief 连接设置页面
 */
class ConnectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectionWidget(RpcClient *rpcClient, QWidget *parent = nullptr);

    QString host() const;
    quint16 port() const;

signals:
    void connectionStatusChanged(bool connected);

public slots:
    void onConnect();
    void onDisconnect();
    void onPing();
    void onSysInfo();
    void onSaveConfig();

private slots:
    void onRpcConnected();
    void onRpcDisconnected();
    void onRpcError(const QString &error);
    void onRpcLogMessage(const QString &message);

private:
    void setupUi();
    void updateConnectionStatus(bool connected);
    void appendLog(const QString &message);

    RpcClient *rpcClient_;

    // 连接设置组件
    QLineEdit *hostEdit_;
    QSpinBox *portSpinBox_;
    QPushButton *connectButton_;
    QPushButton *disconnectButton_;
    QPushButton *pingButton_;
    QPushButton *sysInfoButton_;
    QPushButton *saveConfigButton_;

    // 状态显示
    QLabel *statusLabel_;
    QTextEdit *logTextEdit_;
};

#endif // CONNECTION_WIDGET_H
