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


}
}


#endif // DEVICE_CONFIG_H
