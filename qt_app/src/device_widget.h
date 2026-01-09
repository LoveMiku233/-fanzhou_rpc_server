/**
 * @file device_widget.h
 * @brief 设备管理页面头文件
 */

#ifndef DEVICE_WIDGET_H
#define DEVICE_WIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>

class RpcClient;

/**
 * @brief 设备管理页面
 */
class DeviceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceWidget(RpcClient *rpcClient, QWidget *parent = nullptr);

public slots:
    void refreshDeviceList();
    void refreshDeviceStatus();

private slots:
    void onQueryAllClicked();
    void onDeviceTableCellClicked(int row, int column);

private:
    void setupUi();
    void updateDeviceTable(const QJsonArray &devices);
    void updateDeviceStatus(int nodeId, const QJsonObject &status);

    RpcClient *rpcClient_;
    QTableWidget *deviceTable_;
    QLabel *statusLabel_;
    QPushButton *refreshButton_;
    QPushButton *queryAllButton_;
};

#endif // DEVICE_WIDGET_H
