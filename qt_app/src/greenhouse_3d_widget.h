/**
 * @file greenhouse_3d_widget.h
 * @brief 大棚常用流程页面
 */

#ifndef GREENHOUSE_3D_WIDGET_H
#define GREENHOUSE_3D_WIDGET_H

#include <QList>
#include <QMap>
#include <QPixmap>
#include <QRect>
#include <QStringList>
#include <QWidget>

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
    void onDeviceButtonClicked();

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

    void setupUi();
    void showToast(const QString &message, const QString &level = QStringLiteral("INFO"));
    void hideToastAnimated();
    void layoutToast();
    void updateGreenhouseImage();
    void updateBindingSummary();
    void updateDeviceButtonStyles();
    void showDeviceStatusDialog(const QString &deviceName);

    RpcClient *rpcClient_;
    QLabel *titleLabel_;
    QLabel *hintLabel_;
    QLabel *statusLabel_;
    QLabel *bindingLabel_;
    QLabel *greenhouseImageLabel_;
    QWidget *toastWidget_;
    QLabel *toastLabel_;
    QTimer *refreshTimer_;
    QTimer *toastHideTimer_;
    QPropertyAnimation *toastShowAnim_;
    QPropertyAnimation *toastHideAnim_;
    QRect toastVisibleRect_;
    QPixmap greenhousePixmap_;
    QPixmap greenhouseDisplayPixmap_;
    QSize greenhouseDisplaySize_;

    QList<RoleBinding> roleBindings_;
    QList<QPushButton *> deviceButtons_;
    QMap<QString, QString> deviceStatusMap_;
};

#endif  // GREENHOUSE_3D_WIDGET_H
