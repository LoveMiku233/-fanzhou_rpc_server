/**
 * @file views/alarm_page.h
 * @brief 报警看板页面 - 报警列表/过滤/确认
 *
 * 匹配 index3.html 报警看板视图，1024×600 深色主题。
 * 支持从RPC Server获取报警数据并显示。
 */

#ifndef ALARM_PAGE_H
#define ALARM_PAGE_H

#include <QWidget>
#include <QList>
#include "models/data_models.h"

class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QJsonValue;
class QJsonObject;
class RpcClient;

class AlarmPage : public QWidget
{
    Q_OBJECT

public:
    explicit AlarmPage(RpcClient *rpc, QWidget *parent = nullptr);
    ~AlarmPage() override = default;

    /** 刷新页面数据（从RPC Server获取） */
    void refreshData();

signals:
    /** 未确认报警数变化，供 MainWindow 更新侧边栏角标 */
    void alarmCountChanged(int unconfirmedCount);

private:
    void setupUi();
    void renderAlarms();
    QWidget *createAlarmItem(const Models::AlarmInfo &alarm);
    void updateStats();

    /** RPC回调：报警设备状态 */
    void onDeviceStatusReceived(const QJsonValue &result, const QJsonObject &error);

    RpcClient *rpcClient_;

    // 当前过滤级别: "all", "critical", "warning", "info"
    QString currentFilter_;

    // 数据
    QList<Models::AlarmInfo> alarms_;

    // Tab 按钮
    QPushButton *tabAll_;
    QPushButton *tabCritical_;
    QPushButton *tabWarning_;
    QPushButton *tabInfo_;

    // 一键确认
    QPushButton *confirmAllBtn_;

    // 报警列表容器
    QScrollArea *scrollArea_;
    QWidget     *listContainer_;
    QVBoxLayout *listLayout_;

    // 底部统计
    QLabel *criticalCountLabel_;
    QLabel *warningCountLabel_;
};

#endif // ALARM_PAGE_H
