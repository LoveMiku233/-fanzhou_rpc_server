/**
 * @file rpc_registry_system.cpp
 * @brief 系统RPC方法注册
 */

#include "rpc_registry.h"
#include "core_context.h"
#include "rpc_registry_common.h"
#include "rpc_registry_keys.h"

#include "cloud/mqtt/mqtt_channel_manager.h"
#include "comm/can/can_comm.h"
#include "device/device_types.h"
#include "device/can/relay_gd427.h"
#include "rpc/json_rpc_dispatcher.h"
#include "rpc/rpc_error_codes.h"
#include "rpc/rpc_helpers.h"
#include "utils/system_monitor.h"
#include "utils/system_settings.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QRegExp>
#include <QStringList>
#include <climits>

namespace fanzhou {
namespace core {

namespace {
const QString &kKeyOk = rpc_keys::Ok();
const QString &kKeyMessage = rpc_keys::Message();
const QString &kKeyMode = rpc_keys::Mode();
}  // namespace

void RpcRegistry::registerSystem()
{
    // 获取系统信息
    dispatcher_->registerMethod(QStringLiteral("sys.info"),
                                 [this](const QJsonObject &) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const bool canOpened = context_->canBus && context_->canBus->isOpened();
        const int canTxQueueSize = context_->canBus ? context_->canBus->txQueueSize() : 0;
        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("serverVersion"), QStringLiteral("1.0.0")},
            {QStringLiteral("serverTime"), QString::number(now)},
            {QStringLiteral("rpcPort"), context_->coreConfig.main.rpcPort},
            {QStringLiteral("canInterface"), context_->coreConfig.can.interface},
            {QStringLiteral("canBitrate"), context_->coreConfig.can.bitrate},
            {QStringLiteral("canOpened"), canOpened},
            {QStringLiteral("canTxQueueSize"), canTxQueueSize},
            {rpc_keys::DeviceCount(), context_->relays.size()},
            {QStringLiteral("groupCount"), context_->deviceGroups.size()}
        };
    });

    // 系统重启
    dispatcher_->registerMethod(QStringLiteral("sys.reboot"),
                                 [](const QJsonObject &) {
        QProcess::startDetached(QStringLiteral("reboot"), QStringList());
        return QJsonObject{{kKeyOk, true}, {kKeyMessage, QStringLiteral("System rebooting...")}};
    });

    // 获取仪表板汇总信息（优化：一次RPC返回所有仪表板需要的数据）
    dispatcher_->registerMethod(QStringLiteral("sys.dashboard"),
                                 [this](const QJsonObject &) {
        // 设备统计
        int totalDevices = context_->relays.size();
        int onlineDevices = 0;
        int offlineDevices = 0;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        for (auto it = context_->relays.begin(); it != context_->relays.end(); ++it) {
            auto *relay = it.value();
            if (relay) {
                qint64 ageMs = 0;
                bool online = false;
                calcDeviceOnlineStatus(relay->lastSeenMs(), now, ageMs, online);
                if (online) {
                    onlineDevices++;
                } else {
                    offlineDevices++;
                }
            }
        }

        // 分组数量
        int totalGroups = context_->deviceGroups.size();

        // 策略数量
        int totalStrategies = context_->strategyStates().size();

        // 传感器数量（包括MQTT传感器和物理传感器设备）
        int totalSensors = context_->sensorConfigs.size();
        const auto devices = context_->listDevices();
        for (const auto &dev : devices) {
            if (device::isSensorType(dev.deviceType)) {
                totalSensors++;
            }
        }

        // CAN状态
        const bool canOpened = context_->canBus && context_->canBus->isOpened();
        const QString canInterface = context_->coreConfig.can.interface;

        // MQTT状态
        int mqttConnected = 0;
        int mqttTotal = 0;
        if (context_->mqttManager) {
            auto channelInfos = context_->mqttManager->channelStatusList();
            mqttTotal = channelInfos.size();
            for (const auto &info : channelInfos) {
                if (info.connected) {
                    mqttConnected++;
                }
            }
        }

        // 系统运行时间
        QString uptime;
        if (context_->systemMonitor) {
            qint64 uptimeSec = context_->systemMonitor->currentSnapshot().uptimeSec;
            int days = uptimeSec / 86400;
            int hours = (uptimeSec % 86400) / 3600;
            int minutes = (uptimeSec % 3600) / 60;
            int seconds = uptimeSec % 60;
            if (days > 0) {
                uptime = QStringLiteral("%1d %2h %3m %4s").arg(days).arg(hours).arg(minutes).arg(seconds);
            } else if (hours > 0) {
                uptime = QStringLiteral("%1h %2m %3s").arg(hours).arg(minutes).arg(seconds);
            } else if (minutes > 0) {
                uptime = QStringLiteral("%1m %2s").arg(minutes).arg(seconds);
            } else {
                uptime = QStringLiteral("%1s").arg(seconds);
            }
        }

        return QJsonObject{
            {kKeyOk, true},
            // 设备统计
            {QStringLiteral("totalDevices"), totalDevices},
            {QStringLiteral("onlineDevices"), onlineDevices},
            {QStringLiteral("offlineDevices"), offlineDevices},
            // 分组
            {QStringLiteral("totalGroups"), totalGroups},
            // 策略
            {QStringLiteral("totalStrategies"), totalStrategies},
            // 传感器
            {QStringLiteral("totalSensors"), totalSensors},
            // CAN
            {QStringLiteral("canOpened"), canOpened},
            {QStringLiteral("canInterface"), canInterface},
            // MQTT
            {QStringLiteral("mqttConnected"), mqttConnected},
            {QStringLiteral("mqttTotal"), mqttTotal},
            // 系统
            {QStringLiteral("uptime"), uptime}
        };
    });

    dispatcher_->registerMethod(QStringLiteral("sys.can.setBitrate"),
                                 [this](const QJsonObject &params) {
        QString interface;
        qint32 bitrate = 0;
        bool tripleSampling = false;

        if (!rpc::RpcHelpers::getString(params, "ifname", interface))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing ifname"));
        if (!rpc::RpcHelpers::getI32InRange(params, "bitrate", bitrate, 1, INT_MAX))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("missing/invalid bitrate"));
        if (!rpc::RpcHelpers::getBool(params, "tripleSampling", tripleSampling, false))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid tripleSampling"));

        if (!context_->systemSettings)
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("SystemSettings not ready"));

        const bool ok = context_->systemSettings->setCanBitrate(interface, bitrate, tripleSampling);
        return QJsonObject{{kKeyOk, ok}};
    });

    dispatcher_->registerMethod(QStringLiteral("sys.can.dump.start"),
                                 [this](const QJsonObject &params) {
        QString interface;
        if (!rpc::RpcHelpers::getString(params, "ifname", interface))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing ifname"));
        const bool ok = context_->systemSettings && context_->systemSettings->startCanDump(interface);
        return QJsonObject{{kKeyOk, ok}};
    });

    dispatcher_->registerMethod(QStringLiteral("sys.can.dump.stop"),
                                 [this](const QJsonObject &) {
        if (context_->systemSettings)
            context_->systemSettings->stopCanDump();
        return rpc::RpcHelpers::ok(true);
    });

    // ===================== RTC时间管理RPC方法 =====================

    // 获取系统时间
    dispatcher_->registerMethod(QStringLiteral("sys.time.get"),
                                 [this](const QJsonObject &) {
        if (!context_->systemSettings)
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("SystemSettings not ready"));

        const QString time = context_->systemSettings->getSystemTime();
        const qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("datetime"), time},
            {QStringLiteral("timestamp"), static_cast<double>(timestamp)}
        };
    });

    // 设置系统时间
    dispatcher_->registerMethod(QStringLiteral("sys.time.set"),
                                 [this](const QJsonObject &params) {
        QString datetime;
        if (!rpc::RpcHelpers::getString(params, "datetime", datetime))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing datetime"));

        if (!context_->systemSettings)
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("SystemSettings not ready"));

        const bool ok = context_->systemSettings->setSystemTime(datetime);
        if (!ok)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("failed to set time"));

        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("datetime"), context_->systemSettings->getSystemTime()}
        };
    });

    // 保存系统时间到硬件时钟
    dispatcher_->registerMethod(QStringLiteral("sys.time.saveHwclock"),
                                 [this](const QJsonObject &) {
        if (!context_->systemSettings)
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("SystemSettings not ready"));

        const bool ok = context_->systemSettings->saveHardwareClock();
        return QJsonObject{{kKeyOk, ok}};
    });

    // 从硬件时钟读取时间
    dispatcher_->registerMethod(QStringLiteral("sys.time.readHwclock"),
                                 [this](const QJsonObject &) {
        if (!context_->systemSettings)
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("SystemSettings not ready"));

        const QString hwTime = context_->systemSettings->readHardwareClock();
        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("hwclock"), hwTime}
        };
    });

    // ===================== 网络配置RPC方法 =====================

    // 获取网络接口信息
    dispatcher_->registerMethod(QStringLiteral("sys.network.info"),
                                 [this](const QJsonObject &params) {
        QString interface;
        rpc::RpcHelpers::getString(params, "interface", interface);  // 可选参数

        if (!context_->systemSettings)
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("SystemSettings not ready"));

        const QString info = context_->systemSettings->getNetworkInfo(interface);
        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("info"), info}
        };
    });

    // 测试网络连通性
    dispatcher_->registerMethod(QStringLiteral("sys.network.ping"),
                                 [this](const QJsonObject &params) {
        QString host;
        qint32 count = 4;
        qint32 timeoutSec = 10;

        if (!rpc::RpcHelpers::getString(params, "host", host))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing host"));

        // 可选参数
        if (params.contains(QStringLiteral("count")) &&
            !rpc::RpcHelpers::getI32InRange(params, "count", count, 1, 20)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue,
                                        QStringLiteral("invalid count (1..20)"));
        }
        if (params.contains(QStringLiteral("timeout")) &&
            !rpc::RpcHelpers::getI32InRange(params, "timeout", timeoutSec, 1, 60)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue,
                                        QStringLiteral("invalid timeout (1..60)"));
        }

        if (!context_->systemSettings)
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("SystemSettings not ready"));

        const bool ok = context_->systemSettings->pingTest(host, count, timeoutSec);
        return QJsonObject{
            {kKeyOk, ok},
            {QStringLiteral("host"), host},
            {QStringLiteral("reachable"), ok}
        };
    });

    // 设置静态IP地址
    dispatcher_->registerMethod(QStringLiteral("sys.network.setStaticIp"),
                                 [this](const QJsonObject &params) {
        QString interface;
        QString address;
        QString netmask;
        QString gateway;

        if (!rpc::RpcHelpers::getString(params, "interface", interface))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing interface"));
        if (!rpc::RpcHelpers::getString(params, "address", address))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing address"));

        // 可选参数
        rpc::RpcHelpers::getString(params, "netmask", netmask);
        rpc::RpcHelpers::getString(params, "gateway", gateway);

        if (!context_->systemSettings)
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("SystemSettings not ready"));

        const bool ok = context_->systemSettings->setStaticIp(interface, address, netmask, gateway);
        return QJsonObject{
            {kKeyOk, ok},
            {QStringLiteral("interface"), interface},
            {QStringLiteral("address"), address}
        };
    });

    // 启用DHCP
    dispatcher_->registerMethod(QStringLiteral("sys.network.enableDhcp"),
                                 [this](const QJsonObject &params) {
        QString interface;

        if (!rpc::RpcHelpers::getString(params, "interface", interface))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing interface"));

        if (!context_->systemSettings)
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("SystemSettings not ready"));

        const bool ok = context_->systemSettings->enableDhcp(interface);
        return QJsonObject{
            {kKeyOk, ok},
            {QStringLiteral("interface"), interface},
            {kKeyMode, QStringLiteral("dhcp")}
        };
    });

    // 获取详细网络信息
    dispatcher_->registerMethod(QStringLiteral("sys.network.infoDetailed"),
                                 [this](const QJsonObject &params) {
        QString interface;
        rpc::RpcHelpers::getString(params, "interface", interface);

        if (!context_->systemSettings)
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("SystemSettings not ready"));

        QJsonObject info = context_->systemSettings->getNetworkInfoDetailed(interface);
        info[kKeyOk] = true;
        return info;
    });

    // 设置DNS服务器
    dispatcher_->registerMethod(QStringLiteral("sys.network.setDns"),
                                 [this](const QJsonObject &params) {
        QString primary;
        QString secondary;

        if (!rpc::RpcHelpers::getString(params, "primary", primary))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter, QStringLiteral("missing primary DNS"));

        rpc::RpcHelpers::getString(params, "secondary", secondary);

        if (!context_->systemSettings)
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("SystemSettings not ready"));

        const bool ok = context_->systemSettings->setDns(primary, secondary);
        return QJsonObject{
            {kKeyOk, ok},
            {QStringLiteral("primary"), primary},
            {QStringLiteral("secondary"), secondary}
        };
    });

    // ===================== 亮度调整 =====================
    // 设置屏幕亮度 (0-255)
    // 使用命令: echo <brightness> > /sys/class/backlight/*/brightness
    dispatcher_->registerMethod(QStringLiteral("sys.brightness.set"),
                                 [](const QJsonObject &params) {
        qint32 brightness = 0;
        if (!rpc::RpcHelpers::getI32InRange(params, "brightness", brightness, 0, 255))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, QStringLiteral("brightness must be 0-255"));

        // Try common backlight paths
        QStringList backlightPaths = {
            QStringLiteral("/sys/class/backlight/backlight/brightness"),
            QStringLiteral("/sys/class/backlight/lcd-backlight/brightness"),
            QStringLiteral("/sys/class/backlight/pwm-backlight/brightness")
        };

        bool success = false;
        QString usedPath;
        
        for (const QString &path : backlightPaths) {
            QFile file(path);
            if (file.exists() && file.open(QIODevice::WriteOnly)) {
                file.write(QString::number(brightness).toUtf8());
                file.close();
                success = true;
                usedPath = path;
                break;
            }
        }

        if (!success) {
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, 
                QStringLiteral("No backlight device found"));
        }

        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("brightness"), brightness},
            {QStringLiteral("path"), usedPath}
        };
    });

    // 获取当前屏幕亮度
    dispatcher_->registerMethod(QStringLiteral("sys.brightness.get"),
                                 [](const QJsonObject &) {
        QStringList backlightPaths = {
            QStringLiteral("/sys/class/backlight/backlight/brightness"),
            QStringLiteral("/sys/class/backlight/lcd-backlight/brightness"),
            QStringLiteral("/sys/class/backlight/pwm-backlight/brightness")
        };

        for (const QString &path : backlightPaths) {
            QFile file(path);
            if (file.exists() && file.open(QIODevice::ReadOnly)) {
                QString content = QString::fromUtf8(file.readAll()).trimmed();
                file.close();
                bool ok = false;
                int brightness = content.toInt(&ok);
                if (ok) {
                    return QJsonObject{
                        {kKeyOk, true},
                        {QStringLiteral("brightness"), brightness},
                        {QStringLiteral("path"), path}
                    };
                }
            }
        }

        return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, 
            QStringLiteral("No backlight device found"));
    });

    // ===================== 4G模块状态 =====================
    // 查询4G模块状态 (使用mmcli命令)
    dispatcher_->registerMethod(QStringLiteral("sys.4g.status"),
                                 [](const QJsonObject &) {
        QJsonObject result;
        result[kKeyOk] = true;

        // 1. 获取modem列表
        QProcess modemList;
        modemList.start(QStringLiteral("mmcli"), QStringList() << QStringLiteral("-L"));
        modemList.waitForFinished(5000);
        QString modemListOutput = QString::fromUtf8(modemList.readAllStandardOutput()).trimmed();
        result[QStringLiteral("modemList")] = modemListOutput;

        // 2. 尝试获取第一个modem的详细信息
        QProcess modemInfo;
        modemInfo.start(QStringLiteral("mmcli"), QStringList() << QStringLiteral("-m") << QStringLiteral("0"));
        modemInfo.waitForFinished(5000);
        QString modemInfoOutput = QString::fromUtf8(modemInfo.readAllStandardOutput()).trimmed();
        QString modemInfoError = QString::fromUtf8(modemInfo.readAllStandardError()).trimmed();
        
        if (!modemInfoOutput.isEmpty()) {
            result[QStringLiteral("modemInfo")] = modemInfoOutput;
            
            // 解析关键信息
            QJsonObject parsed;
            
            // 解析制造商
            QRegExp mfgRx(QStringLiteral("manufacturer:\\s*(.+)"));
            if (mfgRx.indexIn(modemInfoOutput) != -1) {
                parsed[QStringLiteral("manufacturer")] = mfgRx.cap(1).trimmed();
            }
            
            // 解析型号
            QRegExp modelRx(QStringLiteral("model:\\s*(.+)"));
            if (modelRx.indexIn(modemInfoOutput) != -1) {
                parsed[QStringLiteral("model")] = modelRx.cap(1).trimmed();
            }
            
            // 解析状态
            QRegExp stateRx(QStringLiteral("state:\\s*(.+)"));
            if (stateRx.indexIn(modemInfoOutput) != -1) {
                parsed[QStringLiteral("state")] = stateRx.cap(1).trimmed();
            }
            
            // 解析失败原因
            QRegExp failedRx(QStringLiteral("failed reason:\\s*(.+)"));
            if (failedRx.indexIn(modemInfoOutput) != -1) {
                parsed[QStringLiteral("failedReason")] = failedRx.cap(1).trimmed();
            }
            
            // 解析信号质量
            QRegExp signalRx(QStringLiteral("signal quality:\\s*(\\d+)%"));
            if (signalRx.indexIn(modemInfoOutput) != -1) {
                parsed[QStringLiteral("signalQuality")] = signalRx.cap(1).toInt();
            }
            
            // 解析主端口
            QRegExp portRx(QStringLiteral("primary port:\\s*(.+)"));
            if (portRx.indexIn(modemInfoOutput) != -1) {
                parsed[QStringLiteral("primaryPort")] = portRx.cap(1).trimmed();
            }
            
            // 解析设备ID
            QRegExp equipRx(QStringLiteral("equipment id:\\s*(.+)"));
            if (equipRx.indexIn(modemInfoOutput) != -1) {
                parsed[QStringLiteral("equipmentId")] = equipRx.cap(1).trimmed();
            }
            
            result[QStringLiteral("parsed")] = parsed;
        } else if (!modemInfoError.isEmpty()) {
            result[QStringLiteral("modemError")] = modemInfoError;
        }

        // 3. 获取网络接口状态 (usb0)
        QProcess ifconfig;
        ifconfig.start(QStringLiteral("ifconfig"), QStringList() << QStringLiteral("usb0"));
        ifconfig.waitForFinished(3000);
        QString ifconfigOutput = QString::fromUtf8(ifconfig.readAllStandardOutput()).trimmed();
        if (!ifconfigOutput.isEmpty()) {
            result[QStringLiteral("usb0Info")] = ifconfigOutput;
            
            // 检查是否有IP地址
            QRegExp ipRx(QStringLiteral("inet\\s+(\\d+\\.\\d+\\.\\d+\\.\\d+)"));
            if (ipRx.indexIn(ifconfigOutput) != -1) {
                result[QStringLiteral("usb0Ip")] = ipRx.cap(1);
            }
            
            // 检查是否UP
            result[QStringLiteral("usb0Up")] = ifconfigOutput.contains(QStringLiteral("RUNNING"));
        } else {
            result[QStringLiteral("usb0Info")] = QStringLiteral("Interface not found");
        }

        return result;
    });

    // 4G拨号连接
    dispatcher_->registerMethod(QStringLiteral("sys.4g.connect"),
                                 [](const QJsonObject &params) {
        QString atPort = QStringLiteral("/dev/ttyUSB1");
        QString netPort = QStringLiteral("usb0");
        
        // 可选参数
        rpc::RpcHelpers::getString(params, "atPort", atPort);
        rpc::RpcHelpers::getString(params, "netPort", netPort);

        QJsonObject result;
        result[kKeyOk] = true;
        result[QStringLiteral("steps")] = QJsonArray();

        QJsonArray steps;

        // Step 1: 发送AT命令启动拨号
        QProcess atCmd;
        atCmd.start(QStringLiteral("bash"), QStringList() 
            << QStringLiteral("-c") 
            << QStringLiteral("echo -e 'AT+QNETDEVCTL=3,1,1\\r' > %1").arg(atPort));
        atCmd.waitForFinished(3000);
        steps.append(QJsonObject{
            {QStringLiteral("step"), QStringLiteral("AT command")},
            {QStringLiteral("command"), QStringLiteral("AT+QNETDEVCTL=3,1,1")},
            {rpc_keys::Success(), atCmd.exitCode() == 0}
        });

        // Step 2: 使用udhcpc获取IP
        QProcess udhcpc;
        udhcpc.start(QStringLiteral("udhcpc"), QStringList() << QStringLiteral("-i") << netPort << QStringLiteral("-n"));
        udhcpc.waitForFinished(10000);
        QString udhcpcOutput = QString::fromUtf8(udhcpc.readAllStandardOutput());
        QString udhcpcError = QString::fromUtf8(udhcpc.readAllStandardError());
        steps.append(QJsonObject{
            {QStringLiteral("step"), QStringLiteral("DHCP")},
            {QStringLiteral("command"), QStringLiteral("udhcpc -i %1").arg(netPort)},
            {rpc_keys::Success(), udhcpc.exitCode() == 0},
            {QStringLiteral("output"), udhcpcOutput},
            {QStringLiteral("error"), udhcpcError}
        });

        result[QStringLiteral("steps")] = steps;
        result[kKeyOk] = (atCmd.exitCode() == 0 && udhcpc.exitCode() == 0);

        return result;
    });
}


}  // namespace core
}  // namespace fanzhou
