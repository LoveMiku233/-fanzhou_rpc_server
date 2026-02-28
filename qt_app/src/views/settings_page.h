/**
 * @file views/settings_page.h
 * @brief 系统设置页面 - 网络配置/MQTT配置/关于系统
 *
 * 匹配 index3.html 设置视图，1024×600 深色主题。
 */

#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QStackedWidget;
class RpcClient;

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(RpcClient *rpc, QWidget *parent = nullptr);
    ~SettingsPage() override = default;

private:
    void setupUi();
    QWidget *createNetworkPanel();
    QWidget *createMqttPanel();
    QWidget *createAboutPanel();

    void switchTab(int index);

    RpcClient *rpcClient_;

    // Sub-tab bar buttons
    QList<QPushButton *> tabButtons_;
    QStackedWidget *contentStack_;

    // ── Network panel fields ──
    QRadioButton *radioStaticIp_;
    QRadioButton *radioDhcp_;
    QLineEdit    *editMac_;
    QLineEdit    *editIp_;
    QLineEdit    *editSubnet_;
    QLineEdit    *editGateway_;
    QLineEdit    *editDnsPrimary_;
    QLineEdit    *editDnsSecondary_;
    QSpinBox     *spinPort_;
    QLabel       *networkStatusLabel_;
    QPushButton  *btnTestConnection_;
    QPushButton  *btnSaveNetwork_;

    // ── MQTT panel fields ──
    QLineEdit *editMqttServer_;
    QSpinBox  *spinMqttPort_;
    QLineEdit *editMqttClientId_;
    QLineEdit *editMqttUser_;
    QLineEdit *editMqttPassword_;
    QSpinBox  *spinMqttKeepAlive_;
    QLineEdit *editMqttPubTopic_;
    QLineEdit *editMqttSubTopic_;
    QSpinBox  *spinMqttReportInterval_;
    QLabel    *mqttStatusLabel_;
    QPushButton *btnMqttDisconnect_;
    QPushButton *btnMqttSave_;
};

#endif // SETTINGS_PAGE_H
