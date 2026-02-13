/**
 * @file relay_control_dialog.h
 * @brief 继电器控制对话框头文件
 */

#ifndef RELAY_CONTROL_DIALOG_H
#define RELAY_CONTROL_DIALOG_H

#include <QDialog>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QJsonObject>
#include <QTimer>

class RpcClient;

/**
 * @brief 继电器控制对话框 - 用于控制单个设备
 * 
 * 使用按钮控制（停止/正转/反转），并定时同步设备状态。
 */
class RelayControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RelayControlDialog(RpcClient *rpcClient, int nodeId, 
                                 const QString &deviceName, QWidget *parent = nullptr);
    ~RelayControlDialog() override;

signals:
    void controlExecuted(const QString &message);

private slots:
    void onChannelControlClicked();
    void onStopAllClicked();
    void onQueryStatusClicked();
    void stopNextChannel();
    void onSyncTimerTimeout();

private:
    void setupUi();
    void updateStatusDisplay(const QJsonObject &status);
    void controlRelay(int channel, const QString &action);
    void updateButtonStyles(int channel, int mode);
    void startSyncTimer();
    void stopSyncTimer();

    RpcClient *rpcClient_;
    int nodeId_;
    QString deviceName_;

    // 状态显示
    QLabel *statusLabel_;
    QLabel *ch0StatusLabel_;
    QLabel *ch1StatusLabel_;
    QLabel *ch2StatusLabel_;
    QLabel *ch3StatusLabel_;
    QLabel *currentLabel_;
    
    // 通道电流标签
    QLabel *ch0CurrentLabel_;
    QLabel *ch1CurrentLabel_;
    QLabel *ch2CurrentLabel_;
    QLabel *ch3CurrentLabel_;
    
    // 通道控制按钮 (每通道3个: 停止/正转/反转)
    QPushButton *ch0StopBtn_;
    QPushButton *ch0FwdBtn_;
    QPushButton *ch0RevBtn_;
    QPushButton *ch1StopBtn_;
    QPushButton *ch1FwdBtn_;
    QPushButton *ch1RevBtn_;
    QPushButton *ch2StopBtn_;
    QPushButton *ch2FwdBtn_;
    QPushButton *ch2RevBtn_;
    QPushButton *ch3StopBtn_;
    QPushButton *ch3FwdBtn_;
    QPushButton *ch3RevBtn_;
    
    // 顺序停止控制
    int stopChannelIndex_;
    
    // 防止重复操作的标志
    bool isControlling_;
    
    // 防止重复查询的标志
    bool isQuerying_;
    
    // 状态同步定时器
    QTimer *syncTimer_;
    static constexpr int kSyncIntervalMs = 2000;  // 2秒同步一次
};

#endif // RELAY_CONTROL_DIALOG_H
