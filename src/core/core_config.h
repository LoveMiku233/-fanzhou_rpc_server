/**
 * @file core_config.h
 * @brief Core configuration management
 *
 * Defines configuration structures and loading/saving for the system.
 */

#ifndef FANZHOU_CORE_CONFIG_H
#define FANZHOU_CORE_CONFIG_H

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QtGlobal>

#include "device/device_types.h"

namespace fanzhou {
namespace core {

/**
 * @brief Device configuration
 */
struct DeviceConfig {
    QString name;
    device::DeviceTypeId deviceType = device::DeviceTypeId::RelayGd427;
    device::CommTypeId commType = device::CommTypeId::Can;
    int nodeId = -1;
    QString bus = QStringLiteral("can0");
    QJsonObject params;
};

/**
 * @brief Relay node configuration
 */
struct RelayNodeConfig {
    int nodeId = 1;
    bool enabled = true;
    int channels = 4;
    QString name;
};

/**
 * @brief CAN bus configuration
 */
struct CanConfig {
    QString interface = QStringLiteral("can0");
    int bitrate = 125000;
    bool tripleSampling = true;
    bool canFd = false;
};

/**
 * @brief Log configuration
 */
struct LogConfig {
    bool logToConsole = true;
    bool logToFile = true;
    QString logFilePath = QStringLiteral("/var/log/fanzhou_core/core.log");
    int logLevel = 0;  // 0=Debug, 1=Info, 2=Warning, 3=Error, 4=Critical
};

/**
 * @brief Main configuration
 */
struct MainConfig {
    quint16 rpcPort = 12345;
};

/**
 * @brief Device group configuration
 */
struct DeviceGroupConfig {
    int groupId = 0;
    QString name;
    QList<int> deviceNodes;
    bool enabled = true;
};

/**
 * @brief Automatic control strategy configuration
 */
struct AutoStrategyConfig {
    int strategyId = 0;
    QString name;
    int groupId = 0;
    quint8 channel = 0;
    QString action = QStringLiteral("stop");
    int intervalSec = 60;
    bool enabled = true;
    bool autoStart = true;
};

/**
 * @brief Core system configuration
 *
 * Manages all configuration for the control system including
 * RPC settings, CAN bus parameters, device list, and groups.
 */
class CoreConfig
{
public:
    MainConfig main;
    CanConfig can;
    LogConfig log;
    QList<DeviceConfig> devices;
    QList<DeviceGroupConfig> groups;
    QList<AutoStrategyConfig> strategies;

    /**
     * @brief Load configuration from file
     * @param path File path
     * @param error Error message output
     * @return True if successful
     */
    bool loadFromFile(const QString &path, QString *error = nullptr);

    /**
     * @brief Save configuration to file
     * @param path File path
     * @param error Error message output
     * @return True if successful
     */
    bool saveToFile(const QString &path, QString *error = nullptr) const;

    /**
     * @brief Create default configuration
     * @return Default configuration object
     */
    static CoreConfig makeDefault();
};

}  // namespace core
}  // namespace fanzhou

#endif  // FANZHOU_CORE_CONFIG_H
