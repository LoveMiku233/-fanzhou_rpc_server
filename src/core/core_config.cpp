/**
 * @file core_config.cpp
 * @brief 核心配置实现
 */

#include "core_config.h"

#include <QFile>
#include <QJsonDocument>

namespace fanzhou {
namespace core {

namespace {

bool writeTextFile(const QString &path, const QByteArray &data, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QStringLiteral("Failed to open file for writing: %1")
                         .arg(file.errorString());
        }
        return false;
    }
    if (file.write(data) != data.size()) {
        if (error) {
            *error = QStringLiteral("Write failed: %1").arg(file.errorString());
        }
        return false;
    }
    return true;
}



}  // namespace

CoreConfig CoreConfig::makeDefault()
{
    CoreConfig config;
    config.main.rpcPort = 12345;

    config.can.interface = QStringLiteral("can0");
    config.can.bitrate = 250000;
    config.can.tripleSampling = true;
    config.can.canFd = false;

    config.log.logToConsole = true;
    config.log.logToFile = true;
    config.log.logFilePath = QStringLiteral("/var/log/fanzhou_core/core.log");
    config.log.logLevel = 0;

    DeviceConfig device;
    device.name = QStringLiteral("relay01");
    device.deviceType = device::DeviceTypeId::RelayGd427;
    device.commType = device::CommTypeId::Can;
    device.nodeId = 1;
    device.bus = QStringLiteral("can0");
    device.params = QJsonObject{{QStringLiteral("channels"), 4}};
    config.devices.append(device);

    DeviceGroupConfig group;
    group.groupId = 1;
    group.name = QStringLiteral("default");
    group.deviceNodes = {1};
    group.enabled = true;
    config.groups.append(group);

    return config;
}

bool CoreConfig::loadFromFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.exists()) {
        if (error) {
            *error = QStringLiteral("Configuration file does not exist");
        }
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Failed to open file: %1").arg(file.errorString());
        }
        return false;
    }

    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        if (error) {
            *error = QStringLiteral("Invalid JSON root (not an object)");
        }
        return false;
    }
    const QJsonObject root = doc.object();

    if (!loadMain(root, error)) return false;
    loadLog(root);
    loadCan(root);
    loadDevices(root);
    loadGroups(root);
    loadScreen(root);
    loadCloudUpload(root);
    loadStrategies(root);
    loadMqttChannels(root);
    loadSensors(root);

    return true;
}

bool CoreConfig::saveToFile(const QString &path, QString *error) const
{
    QJsonObject root;

    saveMain(root);
    saveLog(root);
    saveCan(root);
    saveDevices(root);
    saveGroups(root);
    saveScreen(root);
    saveCloudUpload(root);
    saveStrategies(root);
    saveMqttChannels(root);
    saveSensors(root);

    QJsonDocument doc(root);
    const QByteArray data = doc.toJson(QJsonDocument::Indented);
    return writeTextFile(path, data, error);
}

}  // namespace core
}  // namespace fanzhou
