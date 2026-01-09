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

class RpcClient;

/**
 * @brief 继电器控制对话框 - 用于控制单个设备
 */
class RelayControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RelayControlDialog(RpcClient *rpcClient, int nodeId, 
                                 const QString &deviceName, QWidget *parent = nullptr);

signals:
    void controlExecuted(const QString &message);

private slots:
    void onChannelControlClicked();
    void onStopAllClicked();
    void onQueryStatusClicked();

private:
    void setupUi();
    void updateStatusDisplay(const QJsonObject &status);
    void controlRelay(int channel, const QString &action);

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
};

#endif // RELAY_CONTROL_DIALOG_H
