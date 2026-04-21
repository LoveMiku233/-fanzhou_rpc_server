/**
 * @file greenhouse_3d_widget.h
 * @brief 大棚常用流程页面
 */

#ifndef GREENHOUSE_3D_WIDGET_H
#define GREENHOUSE_3D_WIDGET_H

#include <QList>
#include <QMap>
#include <QRect>
#include <QStringList>
#include <QWidget>
#include <functional>

class QLabel;
class QResizeEvent;
class QPaintEvent;
class QPushButton;
class QPropertyAnimation;
class QTimer;
class RpcClient;
class QWidget;

class Greenhouse3DWidget : public QWidget
{
    Q_OBJECT

public:
    explicit Greenhouse3DWidget(RpcClient *rpcClient, QWidget *parent = nullptr);

signals:
    void logMessage(const QString &message, const QString &level = QStringLiteral("INFO"));

private slots:
    void onRefreshGroups();
    void onWorkflowClicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct RoleBinding {
        QString role;
        QStringList keywords;
        int groupId = -1;
        QString groupName;
    };

    struct WorkflowStep {
        QString role;
        QString action;
    };

    struct Workflow {
        QString name;
        QString description;
        QString trigger;
        QString exitCondition;
        QString protection;
        QList<WorkflowStep> steps;
        QPushButton *button = nullptr;
        QString style;
        QString runtimeStatus;
    };

    void setupUi();
    void setupWorkflows();
    void animateWorkflowButtons();
    void showToast(const QString &message, const QString &level = QStringLiteral("INFO"));
    void hideToastAnimated();
    void layoutToast();
    void updateBindingSummary();
    QString roleDisplayName(const QString &role) const;
    QString workflowStepsText(const Workflow &workflow) const;
    QString workflowBindingText(const Workflow &workflow) const;
    void updateWorkflowButtonText(int workflowIndex);
    void updateAllWorkflowButtonText();
    void setWorkflowButtonsEnabled(bool enabled);
    void executeGroupControl(int groupId, const QString &groupName, const QString &action,
                             std::function<void(bool, const QString &)> callback);
    void runWorkflow(int workflowIndex);

    RpcClient *rpcClient_;
    QLabel *titleLabel_;
    QLabel *hintLabel_;
    QLabel *statusLabel_;
    QLabel *bindingLabel_;
    QWidget *toastWidget_;
    QLabel *toastLabel_;
    QTimer *refreshTimer_;
    QTimer *toastHideTimer_;
    QPropertyAnimation *toastShowAnim_;
    QPropertyAnimation *toastHideAnim_;
    QRect toastVisibleRect_;

    QList<RoleBinding> roleBindings_;
    QList<Workflow> workflows_;
    QMap<QPushButton *, int> workflowButtonIndexMap_;

    bool workflowRunning_ = false;
};

#endif  // GREENHOUSE_3D_WIDGET_H
