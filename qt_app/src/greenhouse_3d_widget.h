/**
 * @file greenhouse_3d_widget.h
 * @brief 3D大棚可视化控制页面
 */

#ifndef GREENHOUSE_3D_WIDGET_H
#define GREENHOUSE_3D_WIDGET_H

#include <QList>
#include <QMap>
#include <QWidget>

class QLabel;
class QPushButton;
class QTimer;
class RpcClient;

class Greenhouse3DWidget : public QWidget
{
    Q_OBJECT

public:
    explicit Greenhouse3DWidget(RpcClient *rpcClient, QWidget *parent = nullptr);

signals:
    void logMessage(const QString &message, const QString &level = QStringLiteral("INFO"));

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onRefreshGroups();
    void onDeviceHotspotClicked();

private:
    struct Hotspot {
        QString name;
        QPointF relativePos;
        int groupId = -1;
        QPushButton *button = nullptr;
    };

    void setupUi();
    void layoutHotspots();
    void updateHotspotLabels();
    void triggerGroupControl(int hotspotIndex, const QString &action);

    RpcClient *rpcClient_;
    QLabel *titleLabel_;
    QLabel *hintLabel_;
    QLabel *statusLabel_;
    QTimer *refreshTimer_;

    QWidget *canvasArea_;
    QList<Hotspot> hotspots_;
    QMap<QPushButton *, int> buttonIndexMap_;
};

#endif  // GREENHOUSE_3D_WIDGET_H
