#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QtGlobal>
#include "device/device_list.h"

/**
 * @brief 设备配置结构体
 * 
 * 用于配置单个设备的参数，包括设备类型、通讯类型、节点ID等
 */
struct DeviceConfig {
    QString name;                                         // 设备名称
    DeviceTypeId deviceType = DeviceTypeId::RelayGD427;   // 设备类型
    CommTypeId commType = CommTypeId::Can;                // 通讯类型

    // 可选参数
    int node_id = -1;                                     // 节点ID
    QString bus = "can0";                                 // 总线名称

    QJsonObject params;                                   // 扩展参数
};

/**
 * @brief 继电器节点配置结构体
 */
struct RelayNodeConfig {
    int nodeId = 1;                                       // 节点ID
    bool enabled = true;                                  // 是否启用
    int channels = 4;                                     // 通道数
    QString name;                                         // 节点名称
};

/**
 * @brief CAN总线配置结构体
 */
struct CommCanConfig {
    QString canIfname = "can0";                           // CAN接口名称
    int canBitrate = 125000;                              // 波特率
    bool canTripleSampling = true;                        // 三重采样
    bool canFd = false;                                   // 是否使用CAN FD
};

/**
 * @brief 日志配置结构体
 */
struct LogConfig {
    bool logToConsole = true;                             // 是否输出到终端
    bool logToFile = true;                                // 是否输出到文件
    QString logFilePath = "/var/log/fanzhou_core/core.log"; // 日志文件路径
    int logLevel = 0;                                     // 日志级别 (0=Debug, 1=Info, 2=Warning, 3=Error, 4=Critical)
};

/**
 * @brief 主配置结构体
 */
struct MainConfig {
    quint16 rpcPort = 12345;                              // RPC服务端口
};

/**
 * @brief 设备组配置结构体
 */
struct DeviceGroupConfig {
    int groupId = 0;                                      // 组ID
    QString name;                                         // 组名称
    QList<int> deviceNodes;                               // 组内设备节点ID列表
    bool enabled = true;                                  // 是否启用
};

/**
 * @brief 自动控制策略配置
 *
 * 将策略与设备组绑定，用于周期性地向组内设备下发控制指令。
 */
struct AutoStrategyConfig {
    int strategyId = 0;                                   // 策略ID
    QString name;                                         // 策略名称
    int groupId = 0;                                      // 目标组ID
    quint8 channel = 0;                                   // 通道
    QString action = "stop";                              // 动作(stop/fwd/rev)
    int intervalSec = 60;                                 // 执行间隔(秒)
    bool enabled = true;                                  // 是否启用
    bool autoStart = true;                                // 是否自动启动
};

/**
 * @brief 核心配置类
 * 
 * 管理大棚控制系统的所有核心配置，包括RPC端口、CAN总线参数、
 * 设备列表、设备组以及日志配置等
 */
class CoreConfig
{
public:
    MainConfig core_;                                     // 主配置
    CommCanConfig can_;                                   // CAN总线配置
    LogConfig log_;                                       // 日志配置

    QList<DeviceConfig> devices_;                         // 设备列表
    QList<DeviceGroupConfig> groups_;                     // 设备组列表
    QList<AutoStrategyConfig> strategies_;                // 自动策略列表

    /**
     * @brief 从文件加载配置
     * @param path 配置文件路径
     * @param err 错误信息输出
     * @return 是否成功
     */
    bool loadFromFile(const QString& path, QString* err=nullptr);
    
    /**
     * @brief 保存配置到文件
     * @param path 配置文件路径
     * @param err 错误信息输出
     * @return 是否成功
     */
    bool saveToFile(const QString& path, QString* err=nullptr) const;
    
    /**
     * @brief 生成默认配置
     * @return 默认配置对象
     */
    static CoreConfig makeDefault();
};

#endif // CORE_CONFIG_H
