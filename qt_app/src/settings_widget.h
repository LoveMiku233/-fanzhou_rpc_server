/**
 * @file settings_widget.h
 * @brief 设置页面头文件
 */

#ifndef SETTINGS_WIDGET_H
#define SETTINGS_WIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>

class RpcClient;

/**
 * @brief 设置页面 - 包含连接设置和其他系统设置
 */
class SettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsWidget(RpcClient *rpcClient, QWidget *parent = nullptr);

    QString host() const;
    quint16 port() const;

signals:
    void connectionStatusChanged(bool connected);
    void logMessage(const QString &message, const QString &level = QStringLiteral("INFO"));

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

private:
    void setupUi();
    void updateConnectionStatus(bool connected);

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

    // 系统设置
    QSpinBox *refreshIntervalSpinBox_;
    QCheckBox *autoConnectCheckBox_;
};

#endif // SETTINGS_WIDGET_H
