/**
 * @file group_widget.h
 * @brief 分组管理页面头文件 - 卡片式布局
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
#include <QJsonObject>
#include <QGridLayout>
#include <QFrame>
#include <QScrollArea>

class RpcClient;

/**
 * @brief 分组卡片组件
 * 显示单个分组的信息和操作按钮
 */
class GroupCard : public QFrame
{
    Q_OBJECT

public:
    explicit GroupCard(int groupId, const QString &name, QWidget *parent = nullptr);

    int groupId() const { return groupId_; }
    void updateInfo(const QString &name, int deviceCount, int channelCount,
                   const QJsonArray &channels);

signals:
    void controlClicked(int groupId, const QString &action);
    void manageClicked(int groupId);
    void deleteClicked(int groupId);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    void setupUi();

    int groupId_;
    QString name_;
    QLabel *nameLabel_;
    QLabel *idLabel_;
    QLabel *deviceCountLabel_;
    QLabel *channelCountLabel_;
    QLabel *channelsLabel_;
};

/**
 * @brief 分组管理页面 - 卡片式布局
 * 
 * 按通道绑定设置分组，参考Web端设计
 */
class GroupWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GroupWidget(RpcClient *rpcClient, QWidget *parent = nullptr);

public slots:
    void refreshGroupList();

signals:
    void logMessage(const QString &message, const QString &level = QStringLiteral("INFO"));

private slots:
    void onCreateGroupClicked();
    void onDeleteGroupClicked();
    void onManageChannelsClicked();
    void onGroupControlClicked(int groupId, const QString &action);
    void onManageGroupClicked(int groupId);
    void onDeleteGroupFromCard(int groupId);

private:
    void setupUi();
    void updateGroupCards(const QJsonArray &groups);
    void clearGroupCards();
    void fetchGroupChannels(int groupId);
    int getSelectedGroupId();

    RpcClient *rpcClient_;
    
    // UI组件
    QLabel *statusLabel_;
    QWidget *cardsContainer_;
    QGridLayout *cardsLayout_;
    QList<GroupCard*> groupCards_;
    
    // 缓存
    QJsonArray groupsCache_;
    int selectedGroupId_;
};

#endif // GROUP_WIDGET_H
