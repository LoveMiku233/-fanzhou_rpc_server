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
#include <QJsonArray>

class RpcClient;

/**
 * @brief 分组管理页面 - 按钮弹窗方式操作
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
    void onManageDevicesClicked();
    void onGroupControlClicked();
    void onGroupTableCellClicked(int row, int column);

private:
    void setupUi();
    void updateGroupTable(const QJsonArray &groups);
    int getSelectedGroupId();  // 获取当前选中的分组ID

    RpcClient *rpcClient_;
    
    // 表格
    QTableWidget *groupTable_;
    QLabel *statusLabel_;
    
    // 当前选中的分组ID
    int selectedGroupId_;
};

#endif // GROUP_WIDGET_H
