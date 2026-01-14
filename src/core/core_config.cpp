/**
 * @file core_config.cpp
 * @brief 核心配置实现
 */

#include "core_config.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace fanzhou {
namespace core {

namespace {

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
    config.can.bitrate = 125000;
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

    AutoStrategyConfig strategy;
    strategy.strategyId = 1;
    strategy.name = QStringLiteral("default-stop");
    strategy.groupId = group.groupId;
    strategy.channel = 0;
    strategy.action = QStringLiteral("stop");
    strategy.intervalSec = 120;
    strategy.enabled = true;
    strategy.autoStart = false;
    config.strategies.append(strategy);

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

    // 主配置
    if (root.contains(QStringLiteral("main")) &&
        root[QStringLiteral("main")].isObject()) {
        const auto mainObj = root[QStringLiteral("main")].toObject();
        if (mainObj.contains(QStringLiteral("rpcPort"))) {
            main.rpcPort =
                static_cast<quint16>(mainObj[QStringLiteral("rpcPort")].toInt(main.rpcPort));
        }
        
        // 认证配置
        if (mainObj.contains(QStringLiteral("auth")) &&
            mainObj[QStringLiteral("auth")].isObject()) {
            const auto authObj = mainObj[QStringLiteral("auth")].toObject();
            main.auth.enabled = authObj.value(QStringLiteral("enabled")).toBool(false);
            main.auth.secret = authObj.value(QStringLiteral("secret")).toString();
            main.auth.tokenExpireSec = authObj.value(QStringLiteral("tokenExpireSec")).toInt(3600);
            
            if (authObj.contains(QStringLiteral("allowedTokens")) &&
                authObj[QStringLiteral("allowedTokens")].isArray()) {
                const auto arr = authObj[QStringLiteral("allowedTokens")].toArray();
                for (const auto &val : arr) {
                    main.auth.allowedTokens.append(val.toString());
                }
            }
            
            if (authObj.contains(QStringLiteral("whitelist")) &&
                authObj[QStringLiteral("whitelist")].isArray()) {
                const auto arr = authObj[QStringLiteral("whitelist")].toArray();
                for (const auto &val : arr) {
                    main.auth.whitelist.append(val.toString());
                }
            }
            
            if (authObj.contains(QStringLiteral("publicMethods")) &&
                authObj[QStringLiteral("publicMethods")].isArray()) {
                main.auth.publicMethods.clear();
                const auto arr = authObj[QStringLiteral("publicMethods")].toArray();
                for (const auto &val : arr) {
                    main.auth.publicMethods.append(val.toString());
                }
            }
        }
    }

    // 日志配置
    if (root.contains(QStringLiteral("log")) &&
        root[QStringLiteral("log")].isObject()) {
        const auto logObj = root[QStringLiteral("log")].toObject();
        if (logObj.contains(QStringLiteral("logToConsole"))) {
            log.logToConsole = logObj[QStringLiteral("logToConsole")].toBool(log.logToConsole);
        }
        if (logObj.contains(QStringLiteral("logToFile"))) {
            log.logToFile = logObj[QStringLiteral("logToFile")].toBool(log.logToFile);
        }
        if (logObj.contains(QStringLiteral("logFilePath"))) {
            log.logFilePath =
                logObj[QStringLiteral("logFilePath")].toString(log.logFilePath);
        }
        if (logObj.contains(QStringLiteral("logLevel"))) {
            log.logLevel = logObj[QStringLiteral("logLevel")].toInt(log.logLevel);
        }
    }

    // CAN配置
    if (root.contains(QStringLiteral("can")) &&
        root[QStringLiteral("can")].isObject()) {
        const auto canObj = root[QStringLiteral("can")].toObject();
        if (canObj.contains(QStringLiteral("ifname"))) {
            can.interface = canObj[QStringLiteral("ifname")].toString(can.interface);
        }
        if (canObj.contains(QStringLiteral("bitrate"))) {
            can.bitrate = canObj[QStringLiteral("bitrate")].toInt(can.bitrate);
        }
        if (canObj.contains(QStringLiteral("tripleSampling"))) {
            can.tripleSampling =
                canObj[QStringLiteral("tripleSampling")].toBool(can.tripleSampling);
        }
        if (canObj.contains(QStringLiteral("canFd"))) {
            can.canFd = canObj[QStringLiteral("canFd")].toBool(can.canFd);
        }
    }

    // 设备列表
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
    }

    // 设备组
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
    }

    // 屏幕配置
    if (root.contains(QStringLiteral("screen")) &&
        root[QStringLiteral("screen")].isObject()) {
        const auto screenObj = root[QStringLiteral("screen")].toObject();
        if (screenObj.contains(QStringLiteral("brightness"))) {
            screen.brightness = screenObj[QStringLiteral("brightness")].toInt(screen.brightness);
        }
        if (screenObj.contains(QStringLiteral("contrast"))) {
            screen.contrast = screenObj[QStringLiteral("contrast")].toInt(screen.contrast);
        }
        if (screenObj.contains(QStringLiteral("enabled"))) {
            screen.enabled = screenObj[QStringLiteral("enabled")].toBool(screen.enabled);
        }
        if (screenObj.contains(QStringLiteral("sleepTimeoutSec"))) {
            screen.sleepTimeoutSec = screenObj[QStringLiteral("sleepTimeoutSec")].toInt(screen.sleepTimeoutSec);
        }
        if (screenObj.contains(QStringLiteral("orientation"))) {
            screen.orientation = screenObj[QStringLiteral("orientation")].toString(screen.orientation);
        }
    }

    // 控制策略
    strategies.clear();
    if (root.contains(QStringLiteral("strategies")) &&
        root[QStringLiteral("strategies")].isArray()) {
        const auto arr = root[QStringLiteral("strategies")].toArray();
        for (const auto &value : arr) {
            if (!value.isObject()) continue;
            const auto obj = value.toObject();

            AutoStrategyConfig strat;
            strat.strategyId = obj.value(QStringLiteral("id")).toInt(0);
            strat.name = obj.value(QStringLiteral("name")).toString();
            strat.groupId = obj.value(QStringLiteral("groupId")).toInt(0);
            strat.channel = static_cast<qint8>(obj.value(QStringLiteral("channel")).toInt(0));
            strat.action = obj.value(QStringLiteral("action")).toString(QStringLiteral("stop"));
            strat.intervalSec = obj.value(QStringLiteral("intervalSec")).toInt(60);
            strat.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
            strat.autoStart = obj.value(QStringLiteral("autoStart")).toBool(true);
            strat.triggerType = obj.value(QStringLiteral("triggerType")).toString(QStringLiteral("interval"));
            strat.dailyTime = obj.value(QStringLiteral("dailyTime")).toString();

            strategies.append(strat);
        }
    }

    // 传感器策略
    sensorStrategies.clear();
    if (root.contains(QStringLiteral("sensorStrategies")) &&
        root[QStringLiteral("sensorStrategies")].isArray()) {
        const auto arr = root[QStringLiteral("sensorStrategies")].toArray();
        for (const auto &value : arr) {
            if (!value.isObject()) continue;
            const auto obj = value.toObject();

            SensorStrategyConfig strat;
            strat.strategyId = obj.value(QStringLiteral("id")).toInt(0);
            strat.name = obj.value(QStringLiteral("name")).toString();
            strat.sensorType = obj.value(QStringLiteral("sensorType")).toString();
            strat.sensorNode = obj.value(QStringLiteral("sensorNode")).toInt(0);
            strat.condition = obj.value(QStringLiteral("condition")).toString();
            strat.threshold = obj.value(QStringLiteral("threshold")).toDouble(0.0);
            strat.groupId = obj.value(QStringLiteral("groupId")).toInt(0);
            strat.channel = obj.value(QStringLiteral("channel")).toInt(0);
            strat.action = obj.value(QStringLiteral("action")).toString(QStringLiteral("stop"));
            strat.cooldownSec = obj.value(QStringLiteral("cooldownSec")).toInt(60);
            strat.enabled = obj.value(QStringLiteral("enabled")).toBool(true);

            sensorStrategies.append(strat);
        }
    }

    // MQTT多通道配置
    mqttChannels.clear();
    if (root.contains(QStringLiteral("mqttChannels")) &&
        root[QStringLiteral("mqttChannels")].isArray()) {
        const auto arr = root[QStringLiteral("mqttChannels")].toArray();
        for (const auto &value : arr) {
            if (!value.isObject()) continue;
            const auto obj = value.toObject();

            MqttChannelConfig mqtt;
            mqtt.channelId = obj.value(QStringLiteral("channelId")).toInt(0);
            mqtt.name = obj.value(QStringLiteral("name")).toString();
            mqtt.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
            mqtt.broker = obj.value(QStringLiteral("broker")).toString();
            mqtt.port = static_cast<quint16>(obj.value(QStringLiteral("port")).toInt(1883));
            mqtt.clientId = obj.value(QStringLiteral("clientId")).toString();
            mqtt.username = obj.value(QStringLiteral("username")).toString();
            mqtt.password = obj.value(QStringLiteral("password")).toString();
            mqtt.topicPrefix = obj.value(QStringLiteral("topicPrefix")).toString();
            mqtt.keepAliveSec = obj.value(QStringLiteral("keepAliveSec")).toInt(60);
            mqtt.autoReconnect = obj.value(QStringLiteral("autoReconnect")).toBool(true);
            mqtt.reconnectIntervalSec = obj.value(QStringLiteral("reconnectIntervalSec")).toInt(5);
            mqtt.qos = obj.value(QStringLiteral("qos")).toInt(0);

            mqttChannels.append(mqtt);
        }
    }

    return true;
}

bool CoreConfig::saveToFile(const QString &path, QString *error) const
{
    QJsonObject root;

    // 主配置
    QJsonObject mainObj;
    mainObj[QStringLiteral("rpcPort")] = static_cast<int>(main.rpcPort);
    
    // 认证配置
    if (main.auth.enabled || !main.auth.secret.isEmpty() ||
        !main.auth.allowedTokens.isEmpty() || !main.auth.whitelist.isEmpty()) {
        QJsonObject authObj;
        authObj[QStringLiteral("enabled")] = main.auth.enabled;
        if (!main.auth.secret.isEmpty()) {
            authObj[QStringLiteral("secret")] = main.auth.secret;
        }
        authObj[QStringLiteral("tokenExpireSec")] = main.auth.tokenExpireSec;
        
        if (!main.auth.allowedTokens.isEmpty()) {
            QJsonArray arr;
            for (const auto &token : main.auth.allowedTokens) {
                arr.append(token);
            }
            authObj[QStringLiteral("allowedTokens")] = arr;
        }
        
        if (!main.auth.whitelist.isEmpty()) {
            QJsonArray arr;
            for (const auto &ip : main.auth.whitelist) {
                arr.append(ip);
            }
            authObj[QStringLiteral("whitelist")] = arr;
        }
        
        // 只有在修改过时才保存publicMethods
        if (main.auth.publicMethods != AuthConfig().publicMethods) {
            QJsonArray arr;
            for (const auto &method : main.auth.publicMethods) {
                arr.append(method);
            }
            authObj[QStringLiteral("publicMethods")] = arr;
        }
        
        mainObj[QStringLiteral("auth")] = authObj;
    }
    
    root[QStringLiteral("main")] = mainObj;

    // 日志配置
    QJsonObject logObj;
    logObj[QStringLiteral("logToConsole")] = log.logToConsole;
    logObj[QStringLiteral("logToFile")] = log.logToFile;
    logObj[QStringLiteral("logFilePath")] = log.logFilePath;
    logObj[QStringLiteral("logLevel")] = log.logLevel;
    root[QStringLiteral("log")] = logObj;

    // CAN配置
    QJsonObject canObj;
    canObj[QStringLiteral("ifname")] = can.interface;
    canObj[QStringLiteral("bitrate")] = can.bitrate;
    canObj[QStringLiteral("tripleSampling")] = can.tripleSampling;
    canObj[QStringLiteral("canFd")] = can.canFd;
    root[QStringLiteral("can")] = canObj;

    // 设备列表
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

    // 设备组
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

    // 屏幕配置
    QJsonObject screenObj;
    screenObj[QStringLiteral("brightness")] = screen.brightness;
    screenObj[QStringLiteral("contrast")] = screen.contrast;
    screenObj[QStringLiteral("enabled")] = screen.enabled;
    screenObj[QStringLiteral("sleepTimeoutSec")] = screen.sleepTimeoutSec;
    screenObj[QStringLiteral("orientation")] = screen.orientation;
    root[QStringLiteral("screen")] = screenObj;

    // 控制策略
    QJsonArray stratArr;
    for (const auto &strat : strategies) {
        QJsonObject obj;
        obj[QStringLiteral("id")] = strat.strategyId;
        obj[QStringLiteral("name")] = strat.name;
        obj[QStringLiteral("groupId")] = strat.groupId;
        obj[QStringLiteral("channel")] = static_cast<int>(strat.channel);
        obj[QStringLiteral("action")] = strat.action;
        obj[QStringLiteral("intervalSec")] = strat.intervalSec;
        obj[QStringLiteral("enabled")] = strat.enabled;
        obj[QStringLiteral("autoStart")] = strat.autoStart;
        obj[QStringLiteral("triggerType")] = strat.triggerType;
        if (!strat.dailyTime.isEmpty()) {
            obj[QStringLiteral("dailyTime")] = strat.dailyTime;
        }
        stratArr.append(obj);
    }
    root[QStringLiteral("strategies")] = stratArr;

    // 传感器策略
    QJsonArray sensorStratArr;
    for (const auto &strat : sensorStrategies) {
        QJsonObject obj;
        obj[QStringLiteral("id")] = strat.strategyId;
        obj[QStringLiteral("name")] = strat.name;
        obj[QStringLiteral("sensorType")] = strat.sensorType;
        obj[QStringLiteral("sensorNode")] = strat.sensorNode;
        obj[QStringLiteral("condition")] = strat.condition;
        obj[QStringLiteral("threshold")] = strat.threshold;
        obj[QStringLiteral("groupId")] = strat.groupId;
        obj[QStringLiteral("channel")] = strat.channel;
        obj[QStringLiteral("action")] = strat.action;
        obj[QStringLiteral("cooldownSec")] = strat.cooldownSec;
        obj[QStringLiteral("enabled")] = strat.enabled;
        sensorStratArr.append(obj);
    }
    root[QStringLiteral("sensorStrategies")] = sensorStratArr;

    // MQTT多通道配置
    QJsonArray mqttArr;
    for (const auto &mqtt : mqttChannels) {
        QJsonObject obj;
        obj[QStringLiteral("channelId")] = mqtt.channelId;
        obj[QStringLiteral("name")] = mqtt.name;
        obj[QStringLiteral("enabled")] = mqtt.enabled;
        obj[QStringLiteral("broker")] = mqtt.broker;
        obj[QStringLiteral("port")] = static_cast<int>(mqtt.port);
        obj[QStringLiteral("clientId")] = mqtt.clientId;
        if (!mqtt.username.isEmpty()) {
            obj[QStringLiteral("username")] = mqtt.username;
        }
        // 注意：密码可选择是否保存，出于安全考虑可以不保存
        if (!mqtt.password.isEmpty()) {
            obj[QStringLiteral("password")] = mqtt.password;
        }
        obj[QStringLiteral("topicPrefix")] = mqtt.topicPrefix;
        obj[QStringLiteral("keepAliveSec")] = mqtt.keepAliveSec;
        obj[QStringLiteral("autoReconnect")] = mqtt.autoReconnect;
        obj[QStringLiteral("reconnectIntervalSec")] = mqtt.reconnectIntervalSec;
        obj[QStringLiteral("qos")] = mqtt.qos;
        mqttArr.append(obj);
    }
    root[QStringLiteral("mqttChannels")] = mqttArr;

    QJsonDocument doc(root);
    const QByteArray data = doc.toJson(QJsonDocument::Indented);
    return writeTextFile(path, data, error);
}

}  // namespace core
}  // namespace fanzhou
