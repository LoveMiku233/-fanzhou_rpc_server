/**
 * @file views/device_control_page.h
 * @brief 设备控制页面 - 分组Tab + 设备卡片网格
 *
 * 匹配 index3.html 设备控制视图，1024×600 深色主题。
 */

#ifndef DEVICE_CONTROL_PAGE_H
#define DEVICE_CONTROL_PAGE_H

#include <QWidget>
#include <QList>
#include "models/data_models.h"

class QLabel;
class QPushButton;
class QScrollArea;
class QHBoxLayout;
class QSlider;
class RpcClient;

class DeviceControlPage : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceControlPage(RpcClient *rpc, QWidget *parent = nullptr);
    ~DeviceControlPage() override = default;

    /** 刷新页面数据（预留） */
    void refreshData();

signals:
    void deviceValueChanged(const QString &deviceId, int value);

private:
    void setupUi();
    void initDemoData();
    void renderGroupTabs();
    void renderDevices();
    QWidget *createDCDeviceCard(const Models::DeviceInfo &dev);
    QWidget *createACDeviceCard(const Models::DeviceInfo &dev);

    RpcClient *rpcClient_;

    // 分组数据
    QList<Models::DeviceGroup> groups_;
    int currentGroupIndex_;

    // Tab 栏
    QHBoxLayout *tabLayout_;
    QList<QPushButton *> tabButtons_;
    QPushButton *scrollLeftBtn_;
    QPushButton *scrollRightBtn_;
    QPushButton *addGroupBtn_;
    QPushButton *addDeviceBtn_;
    QScrollArea *tabScrollArea_;

    // 设备卡片区域
    QScrollArea *cardScrollArea_;
    QWidget     *cardContainer_;
};

#endif // DEVICE_CONTROL_PAGE_H
