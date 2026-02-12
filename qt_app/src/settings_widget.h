/**
 * @file settings_widget.h
 * @brief 设置页面头文件
 */

#ifndef SETTINGS_WIDGET_H
#define SETTINGS_WIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>

class RpcClient;

/**
 * @brief 设置页面 - 包含连接设置、网络设置、MQTT云平台设置和系统控制
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
    void autoScreenOffSettingsChanged(bool enabled, int timeoutSeconds);

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
    void onRefreshIntervalChanged(int value);
    void onAutoConnectToggled(bool checked);
    
    // 网络设置槽函数
    void onGetNetworkInfo();
    void onSetStaticIp();
    void onEnableDhcp();
    
    // MQTT云平台槽函数
    void onGetMqttConfig();
    void onSetMqttConfig();
    void onTestMqtt();
    
    // 云数据上传槽函数
    void onGetUploadConfig();
    void onSetUploadConfig();
    
    // 系统控制槽函数
    void onGetBrightness();
    void onSetBrightness();
    void onRebootSystem();
    void onShutdownSystem();
    
    // 自动息屏槽函数
    void onAutoScreenOffToggled(bool checked);
    void onScreenOffTimeoutChanged(int value);

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
    
    // 网络设置组件
    QLineEdit *networkInterfaceEdit_;
    QLineEdit *ipAddressEdit_;
    QLineEdit *netmaskEdit_;
    QLineEdit *gatewayEdit_;
    QLabel *networkStatusLabel_;
    
    // MQTT云平台组件
    QLineEdit *mqttBrokerEdit_;
    QSpinBox *mqttPortSpinBox_;
    QLineEdit *mqttClientIdEdit_;
    QLineEdit *mqttUsernameEdit_;
    QLineEdit *mqttPasswordEdit_;
    QLineEdit *mqttTopicEdit_;
    QCheckBox *mqttEnabledCheckBox_;
    QLabel *mqttStatusLabel_;
    
    // 云数据上传组件
    QCheckBox *uploadEnabledCheckBox_;
    QComboBox *uploadModeComboBox_;
    QSpinBox *uploadIntervalSpinBox_;
    QCheckBox *uploadChannelStatusCheckBox_;
    QCheckBox *uploadPhaseLossCheckBox_;
    QCheckBox *uploadCurrentCheckBox_;
    QCheckBox *uploadOnlineStatusCheckBox_;
    QDoubleSpinBox *currentThresholdSpinBox_;
    QCheckBox *statusChangeOnlyCheckBox_;
    QSpinBox *minUploadIntervalSpinBox_;
    
    // 系统控制组件
    QSlider *brightnessSlider_;
    
    // 自动息屏组件
    QCheckBox *autoScreenOffCheckBox_;
    QComboBox *screenOffTimeoutComboBox_;
};

#endif // SETTINGS_WIDGET_H
