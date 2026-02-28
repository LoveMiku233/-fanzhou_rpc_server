/**
 * @file views/sensor_page.h
 * @brief 传感器数据监测页面 - 传感器卡片网格
 *
 * 匹配 index3.html 传感器视图，1024×600 深色主题。
 * 支持从RPC Server获取传感器列表并显示。
 */

#ifndef SENSOR_PAGE_H
#define SENSOR_PAGE_H

#include <QWidget>
#include <QList>
#include "models/data_models.h"

class QLabel;
class QScrollArea;
class QGridLayout;
class QJsonValue;
class QJsonObject;
class RpcClient;

class SensorPage : public QWidget
{
    Q_OBJECT

public:
    explicit SensorPage(RpcClient *rpc, QWidget *parent = nullptr);
    ~SensorPage() override = default;

    /** 刷新页面数据（从RPC Server获取） */
    void refreshData();

private:
    void setupUi();
    void renderSensors();
    QWidget *createSensorCard(const Models::SensorInfo &sensor);

    static const char *getTypeColor(const QString &type);
    static QString getTypeName(const QString &type);
    static QString mapDeviceTypeToSensorType(const QString &typeName);

    /** RPC回调：传感器列表 */
    void onSensorListReceived(const QJsonValue &result, const QJsonObject &error);

    RpcClient *rpcClient_;

    // 数据
    QList<Models::SensorInfo> sensors_;
    bool hasRpcData_;

    // 标题栏
    QLabel *countLabel_;
    QLabel *statusLabel_;

    // 传感器网格容器
    QScrollArea *scrollArea_;
    QWidget     *gridContainer_;
    QGridLayout *gridLayout_;
};

#endif // SENSOR_PAGE_H
