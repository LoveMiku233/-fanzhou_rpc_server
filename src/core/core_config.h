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
#include <QJsonArray>

#include "device/device_types.h"
#include "types/cloud_type.h"
#include "types/comm_type.h"
#include "types/device_type.h"
#include "types/strategy_type.h"
#include "types/system_type.h"


namespace fanzhou {
namespace core {

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
    ScreenConfig screen;  ///< 屏幕配置
    CloudUploadConfig cloudUpload;  ///< 云数据上传配置
    QList<DeviceConfig> devices;
    QList<DeviceGroupConfig> groups;
    QList<AutoStrategy> strategies;
    QList<MqttChannelConfig> mqttChannels;  ///< MQTT多通道配置列表
    QList<SensorNodeConfig> sensors;

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


private:
        bool loadMain(const QJsonObject &root, QString *error);
        bool loadLog(const QJsonObject &root);
        bool loadCan(const QJsonObject &root);
        bool loadDevices(const QJsonObject &root);
        bool loadGroups(const QJsonObject &root);
        bool loadScreen(const QJsonObject &root);
        bool loadCloudUpload(const QJsonObject &root);
        bool loadMqttChannels(const QJsonObject &root);
        bool loadStrategies(const QJsonObject &root);
        bool loadSensors(const QJsonObject &root);

        void saveMain(QJsonObject &root) const;
        void saveLog(QJsonObject &root) const;
        void saveCan(QJsonObject &root) const;
        void saveDevices(QJsonObject &root) const;
        void saveGroups(QJsonObject &root) const;
        void saveScreen(QJsonObject &root) const;
        void saveCloudUpload(QJsonObject &root) const;
        void saveMqttChannels(QJsonObject &root) const;
        void saveStrategies(QJsonObject &root) const;
        void saveSensors(QJsonObject &root) const;
};

}  // namespace core
}  // namespace fanzhou

#endif  // FANZHOU_CORE_CONFIG_H
