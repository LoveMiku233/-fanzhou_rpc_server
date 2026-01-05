/**
 * @file core_config.h
 * @brief 核心配置管理
 *
 * 定义系统的配置结构和加载/保存功能。
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
 * @brief 设备配置
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
 * @brief 继电器节点配置
 */
struct RelayNodeConfig {
    int nodeId = 1;
    bool enabled = true;
    int channels = 4;
    QString name;
};

/**
 * @brief CAN总线配置
 */
struct CanConfig {
    QString interface = QStringLiteral("can0");
    int bitrate = 125000;
    bool tripleSampling = true;
    bool canFd = false;
};

/**
 * @brief 日志配置
 */
struct LogConfig {
    bool logToConsole = true;
    bool logToFile = true;
    QString logFilePath = QStringLiteral("/var/log/fanzhou_core/core.log");
    int logLevel = 0;  // 0=Debug, 1=Info, 2=Warning, 3=Error, 4=Critical
};

/**
 * @brief 主配置
 */
struct MainConfig {
    quint16 rpcPort = 12345;
};

/**
 * @brief 设备组配置
 */
struct DeviceGroupConfig {
    int groupId = 0;
    QString name;
    QList<int> deviceNodes;
    bool enabled = true;
};

/**
 * @brief 自动控制策略配置
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
 * @brief 核心系统配置
 *
 * 管理控制系统的所有配置，包括RPC设置、CAN总线参数、设备列表和分组。
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
     * @brief 从文件加载配置
     * @param path 文件路径
     * @param error 错误信息输出
     * @return 成功返回true
     */
    bool loadFromFile(const QString &path, QString *error = nullptr);

    /**
     * @brief 保存配置到文件
     * @param path 文件路径
     * @param error 错误信息输出
     * @return 成功返回true
     */
    bool saveToFile(const QString &path, QString *error = nullptr) const;

    /**
     * @brief 创建默认配置
     * @return 默认配置对象
     */
    static CoreConfig makeDefault();
};

}  // namespace core
}  // namespace fanzhou

#endif  // FANZHOU_CORE_CONFIG_H
