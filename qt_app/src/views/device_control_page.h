/**
 * @file views/device_control_page.h
 * @brief 设备控制页面 - 分组Tab + 设备卡片网格
 *
 * 匹配 index3.html 设备控制视图，1024×600 深色主题。
 * 支持从RPC Server获取分组和设备数据，支持多种控件类型。
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
class QComboBox;
class QLineEdit;
class QDialog;
class RpcClient;

class DeviceControlPage : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceControlPage(RpcClient *rpc, QWidget *parent = nullptr);
    ~DeviceControlPage() override = default;

    /** 刷新页面数据（从RPC Server获取） */
    void refreshData();

signals:
    void deviceValueChanged(const QString &deviceId, int value);

private slots:
    /** 添加分组对话框 */
    void onAddGroup();
    /** 添加设备对话框（可选择控件类型） */
    void onAddDevice();
    /** 删除当前分组 */
    void onDeleteGroup();
    /** 删除设备 */
    void onDeleteDevice(const QString &deviceId);
    /** 编辑设备 */
    void onEditDevice(const QString &deviceId);
    /** RPC 分组数据返回 */
    void onGroupListReceived(const QJsonValue &result, const QJsonObject &error);

private:
    void setupUi();
    void initDemoData();
    void renderGroupTabs();
    void renderDevices();

    /** 根据控件类型创建对应的设备卡片 */
    QWidget *createDeviceCard(const Models::DeviceInfo &dev);
    /** 滑块控件卡片（DC设备开度控制） */
    QWidget *createSliderCard(const Models::DeviceInfo &dev);
    /** 双态按钮卡片（开/关切换） */
    QWidget *createToggleCard(const Models::DeviceInfo &dev);
    /** 正反转按钮卡片（停止/正转/反转） */
    QWidget *createForwardReverseCard(const Models::DeviceInfo &dev);
    /** AC设备卡片（启动/停止 + 运行信息） */
    QWidget *createACDeviceCard(const Models::DeviceInfo &dev);

    /** 创建卡片顶部（名称 + 状态 + 操作按钮） */
    QHBoxLayout *createCardHeader(const Models::DeviceInfo &dev, QVBoxLayout *parent);

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
    QPushButton *deleteGroupBtn_;
    QPushButton *addDeviceBtn_;
    QScrollArea *tabScrollArea_;

    // 设备卡片区域
    QScrollArea *cardScrollArea_;
    QWidget     *cardContainer_;
};

#endif // DEVICE_CONTROL_PAGE_H
