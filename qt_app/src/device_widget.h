/**
 * @file device_widget.h
 * @brief 设备管理页面头文件 - 网格卡片布局（一行两个）
 */

#ifndef DEVICE_WIDGET_H
#define DEVICE_WIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QList>
#include <QJsonArray>
#include <QJsonObject>
#include <QFrame>

class RpcClient;

/**
 * @brief 设备卡片
 */
class DeviceCard : public QFrame
{
    Q_OBJECT

public:
    explicit DeviceCard(int nodeId, const QString &name, QWidget *parent = nullptr);
    
    int nodeId() const { return nodeId_; }
    QString deviceName() const { return name_; }
    void updateStatus(bool online, qint64 ageMs, double totalCurrent, const QJsonObject &channels);

signals:
    void clicked(int nodeId, const QString &name);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    void setupUi();

    int nodeId_;
    QString name_;
    
    QLabel *nameLabel_;
    QLabel *nodeIdLabel_;
    QLabel *statusLabel_;
    QLabel *currentLabel_;
    QLabel *ch0Label_;
    QLabel *ch1Label_;
    QLabel *ch2Label_;
    QLabel *ch3Label_;
};

/**
 * @brief 设备管理页面 - 网格卡片布局（一行两个）
 */
class DeviceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceWidget(RpcClient *rpcClient, QWidget *parent = nullptr);

signals:
    void deviceControlRequested(int nodeId, const QString &name);
    void logMessage(const QString &message, const QString &level = QStringLiteral("INFO"));

public slots:
    void refreshDeviceList();
    void refreshDeviceStatus();

private slots:
    void onQueryAllClicked();
    void onAddDeviceClicked();
    void onDeviceCardClicked(int nodeId, const QString &name);

private:
    void setupUi();
    void updateDeviceCards(const QJsonArray &devices);
    void updateDeviceCardStatus(int nodeId, const QJsonObject &status);
    void clearDeviceCards();
    void tryRelayNodesAsFallback();

    RpcClient *rpcClient_;
    QLabel *statusLabel_;
    QPushButton *refreshButton_;
    QPushButton *queryAllButton_;
    QPushButton *addDeviceButton_;
    
    QWidget *cardsContainer_;
    QGridLayout *cardsLayout_;
    QList<DeviceCard*> deviceCards_;
};

#endif // DEVICE_WIDGET_H
