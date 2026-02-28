/**
 * @file views/scene_page.h
 * @brief 场景管理页面 - 场景筛选Tab + 场景卡片网格
 *
 * 匹配 index3.html 场景管理视图，1024×600 深色主题。
 */

#ifndef SCENE_PAGE_H
#define SCENE_PAGE_H

#include <QWidget>
#include <QList>
#include "models/data_models.h"

class QLabel;
class QPushButton;
class QScrollArea;
class QHBoxLayout;
class QGridLayout;
class RpcClient;

class ScenePage : public QWidget
{
    Q_OBJECT

public:
    explicit ScenePage(RpcClient *rpc, QWidget *parent = nullptr);
    ~ScenePage() override = default;

    /** 刷新页面数据（预留） */
    void refreshData();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupUi();
    void initDemoData();
    void renderScenes();
    QWidget *createSceneCard(const Models::SceneInfo &scene);

    RpcClient *rpcClient_;

    // 场景数据
    QList<Models::SceneInfo> scenes_;
    QString currentFilter_;  // "all", "auto", "manual"

    // Tab 栏
    QHBoxLayout *tabLayout_;
    QList<QPushButton *> tabButtons_;
    QPushButton *newSceneBtn_;

    // 场景卡片区域
    QScrollArea *cardScrollArea_;
    QWidget     *cardContainer_;
    QGridLayout *cardGrid_;
};

#endif // SCENE_PAGE_H
