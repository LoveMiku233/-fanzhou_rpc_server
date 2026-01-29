#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <QString>
#include <QList>
#include <QJsonObject>
#include "device/device_types.h"

namespace fanzhou{
namespace core {

struct DeviceConfig {
    QString name;
    device::DeviceTypeId deviceType;
    device::CommTypeId commType;
    int nodeId = -1;
    QString bus = "can0";
    QJsonObject params;
};

struct RelayNodeConfig {
    int nodeId = 1;
    bool enabled = true;
    int channels = 4;
    QString name;
};

/**
 * @brief 设备组配置
 */
struct DeviceGroupConfig {
    int groupId = 0;
    QString name;
    QList<int> deviceNodes;
    QList<int> channels;  ///< 指定的通道列表，空表示所有通道
    bool enabled = true;
};


/**
 * @brief 设备通道配置（用于分组）
 */
struct DeviceChannelRef {
    int nodeId = 0;    ///< 设备节点ID
    int channel = -1;  ///< 通道号，-1表示所有通道
};

enum class SensorSource {
    Local = 0,   ///< 本地采集（CAN / 串口等）
    Mqtt     ///< 云端 MQTT 推送
};

enum class SensorValueType {
    Bool,
    Int,
    Double,
    String
};

struct SensorNodeConfig {
    QString sensorId;              ///< 全局唯一ID（如 temp_1 / cloud_pm25）
    QString name;                  ///< 显示名
    SensorSource source;           ///< Local / Mqtt
    SensorValueType valueType;     ///< 数据类型

    // ===== 本地传感器 =====
    int nodeId = -1;               ///< CAN 节点（仅 Local 有效）
    int channel = -1;              ///< 通道号（如 ADC 通道）

    // ===== MQTT 传感器 =====
    int mqttChannelId = -1;         ///< MQTT 通道ID
    QString topic;                 ///< 订阅主题
    QString jsonPath;              ///< JSON路径（如 data.temp）

    // ===== 通用 =====
    QString unit;                  ///< 单位：℃、%、ppm
    double scale = 1.0;            ///< 缩放
    double offset = 0.0;           ///< 偏移
    bool enabled = true;
};



}
}


#endif // DEVICE_CONFIG_H
