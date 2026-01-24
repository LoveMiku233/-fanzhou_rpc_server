#include "core_config.h"

namespace fanzhou{
namespace core {


device::DeviceTypeId deviceTypeFromInt(int value)
{
    return static_cast<device::DeviceTypeId>(value);
}

device::CommTypeId commTypeFromInt(int value)
{
    return static_cast<device::CommTypeId>(value);
}

int toInt(device::DeviceTypeId type)
{
    return static_cast<int>(type);
}

int toInt(device::CommTypeId type)
{
    return static_cast<int>(type);
}

bool CoreConfig::loadDevices(const QJsonObject &root)
{
    devices.clear();
    if (root.contains(QStringLiteral("devices")) &&
        root[QStringLiteral("devices")].isArray()) {
        const auto arr = root[QStringLiteral("devices")].toArray();
        for (const auto &value : arr) {
            if (!value.isObject()) continue;
            const auto obj = value.toObject();

            DeviceConfig dev;
            dev.name = obj.value(QStringLiteral("name")).toString();
            dev.deviceType =
                deviceTypeFromInt(obj.value(QStringLiteral("type")).toInt(toInt(dev.deviceType)));
            dev.commType =
                commTypeFromInt(obj.value(QStringLiteral("commType")).toInt(toInt(dev.commType)));
            dev.nodeId = obj.value(QStringLiteral("nodeId")).toInt(dev.nodeId);
            dev.bus = obj.value(QStringLiteral("bus")).toString(dev.bus);
            if (obj.contains(QStringLiteral("params")) &&
                obj[QStringLiteral("params")].isObject()) {
                dev.params = obj[QStringLiteral("params")].toObject();
            }

            devices.append(dev);
        }
    } else {
        return false;
    }
    return true;
}

bool CoreConfig::loadGroups(const QJsonObject &root)
{
    groups.clear();
    if (root.contains(QStringLiteral("groups")) &&
        root[QStringLiteral("groups")].isArray()) {
        const auto arr = root[QStringLiteral("groups")].toArray();
        for (const auto &value : arr) {
            if (!value.isObject()) continue;
            const auto obj = value.toObject();

            DeviceGroupConfig grp;
            grp.groupId = obj.value(QStringLiteral("groupId")).toInt(0);
            grp.name = obj.value(QStringLiteral("name")).toString();
            grp.enabled = obj.value(QStringLiteral("enabled")).toBool(true);

            if (obj.contains(QStringLiteral("devices")) &&
                obj[QStringLiteral("devices")].isArray()) {
                const auto devArr = obj[QStringLiteral("devices")].toArray();
                for (const auto &devValue : devArr) {
                    grp.deviceNodes.append(devValue.toInt());
                }
            }

            // 加载通道列表
            if (obj.contains(QStringLiteral("channels")) &&
                obj[QStringLiteral("channels")].isArray()) {
                const auto chArr = obj[QStringLiteral("channels")].toArray();
                for (const auto &chValue : chArr) {
                    grp.channels.append(chValue.toInt());
                }
            }

            groups.append(grp);
        }
    } else {
        return false;
    }
    return true;
}

void CoreConfig::saveDevices(QJsonObject &root) const
{
    QJsonArray devArr;
    for (const auto &dev : devices) {
        QJsonObject obj;
        obj[QStringLiteral("name")] = dev.name;
        obj[QStringLiteral("type")] = toInt(dev.deviceType);
        obj[QStringLiteral("commType")] = toInt(dev.commType);
        if (dev.nodeId >= 0) {
            obj[QStringLiteral("nodeId")] = dev.nodeId;
        }
        if (!dev.bus.isEmpty()) {
            obj[QStringLiteral("bus")] = dev.bus;
        }
        if (!dev.params.isEmpty()) {
            obj[QStringLiteral("params")] = dev.params;
        }
        devArr.append(obj);
    }
    root[QStringLiteral("devices")] = devArr;
}

void CoreConfig::saveGroups(QJsonObject &root) const
{
    QJsonArray groupArr;
    for (const auto &grp : groups) {
        QJsonObject obj;
        obj[QStringLiteral("groupId")] = grp.groupId;
        obj[QStringLiteral("name")] = grp.name;
        obj[QStringLiteral("enabled")] = grp.enabled;

        QJsonArray devNodes;
        for (int nodeId : grp.deviceNodes) {
            devNodes.append(nodeId);
        }
        obj[QStringLiteral("devices")] = devNodes;

        // 保存通道列表
        if (!grp.channels.isEmpty()) {
            QJsonArray chArr;
            for (int ch : grp.channels) {
                chArr.append(ch);
            }
            obj[QStringLiteral("channels")] = chArr;
        }

        groupArr.append(obj);
    }
    root[QStringLiteral("groups")] = groupArr;
}

}
}
