/**
 * @file relay_control_widget.h
 * @brief 继电器控制页面头文件
 */

#ifndef RELAY_CONTROL_WIDGET_H
#define RELAY_CONTROL_WIDGET_H

#include <QWidget>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>

class RpcClient;

/**
 * @brief 继电器控制页面
 */
class RelayControlWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RelayControlWidget(RpcClient *rpcClient, QWidget *parent = nullptr);

private slots:
    void onControlClicked();
    void onQueryClicked();
    void onQueryAllClicked();
    void onStopAllClicked();
    void onQuickControlClicked();

private:
    void setupUi();
    void appendResult(const QString &message);
    void controlRelay(int node, int channel, const QString &action);

    RpcClient *rpcClient_;
    
    // 控制面板
    QSpinBox *nodeSpinBox_;
    QComboBox *channelCombo_;
    QComboBox *actionCombo_;
    
    // 结果显示
    QTextEdit *resultTextEdit_;
    QLabel *statusLabel_;
};

#endif // RELAY_CONTROL_WIDGET_H
