/**
 * @file group_widget.h
 * @brief 分组管理页面头文件
 */

#ifndef GROUP_WIDGET_H
#define GROUP_WIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>

class RpcClient;

/**
 * @brief 分组管理页面
 */
class GroupWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GroupWidget(RpcClient *rpcClient, QWidget *parent = nullptr);

public slots:
    void refreshGroupList();

private slots:
    void onCreateGroupClicked();
    void onDeleteGroupClicked();
    void onAddDeviceClicked();
    void onRemoveDeviceClicked();
    void onGroupControlClicked();
    void onGroupTableCellClicked(int row, int column);

private:
    void setupUi();
    void updateGroupTable(const QJsonArray &groups);

    RpcClient *rpcClient_;
    
    // 表格
    QTableWidget *groupTable_;
    QLabel *statusLabel_;
    
    // 创建分组
    QSpinBox *newGroupIdSpinBox_;
    QLineEdit *newGroupNameEdit_;
    
    // 添加/移除设备
    QSpinBox *targetGroupIdSpinBox_;
    QSpinBox *deviceNodeIdSpinBox_;
    
    // 分组控制
    QSpinBox *controlGroupIdSpinBox_;
    QComboBox *controlChannelCombo_;
    QComboBox *controlActionCombo_;
};

#endif // GROUP_WIDGET_H
