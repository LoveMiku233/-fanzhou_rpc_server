#include "core_config.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

/**
 * @brief 从整数转换为设备类型ID
 */
static DeviceTypeId deviceTypeFromInt(int v)
{
    return static_cast<DeviceTypeId>(v);
}

/**
 * @brief 从整数转换为通讯类型ID
 */
static CommTypeId commTypeFromInt(int v)
{
    return static_cast<CommTypeId>(v);
}

/**
 * @brief 写入文本文件
 * @param path 文件路径
 * @param data 数据内容
 * @param err 错误信息输出
 * @return 是否成功
 */
static bool writeTextFile(const QString& path, const QByteArray& data, QString* err)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = QString("打开文件写入失败: %1").arg(f.errorString());
        return false;
    }
    if (f.write(data) != data.size()) {
        if (err) *err = QString("写入失败: %1").arg(f.errorString());
        return false;
    }
    return true;
}

static int toInt(DeviceTypeId t) { return static_cast<int>(t); }
static int toInt(CommTypeId t)   { return static_cast<int>(t); }

/**
 * @brief 生成默认配置
 * @return 包含默认值的配置对象
 */
CoreConfig CoreConfig::makeDefault()
{
    CoreConfig c;
    c.core_.rpcPort = 12345;

    // CAN总线默认配置
    c.can_.canIfname = "can0";
    c.can_.canBitrate = 125000;
    c.can_.canTripleSampling = true;
    c.can_.canFd = false;

    // 日志默认配置
    c.log_.logToConsole = true;
    c.log_.logToFile = true;
    c.log_.logFilePath = "/var/log/fanzhou_core/core.log";
    c.log_.logLevel = 0;

    // 默认设备配置
    DeviceConfig d1;
    d1.name = "relay01";
    d1.deviceType = DeviceTypeId::RelayGD427;
    d1.commType = CommTypeId::Can;
    d1.node_id = 1;
    d1.bus = "can0";
    d1.params = QJsonObject{{"channels", 4}};
    c.devices_.append(d1);

    // 默认分组与策略，便于开箱即用
    DeviceGroupConfig g1;
    g1.groupId = 1;
    g1.name = "default";
    g1.deviceNodes = {1};
    g1.enabled = true;
    c.groups_.append(g1);

    AutoStrategyConfig s1;
    s1.strategyId = 1;
    s1.name = "default-stop";
    s1.groupId = g1.groupId;
    s1.channel = 0;
    s1.action = "stop";
    s1.intervalSec = 120;
    s1.enabled = true;
    s1.autoStart = false; // 仅提供示例，默认不自动启动
    c.strategies_.append(s1);

    return c;
}

/**
 * @brief 从JSON文件加载配置
 * @param path 配置文件路径
 * @param err 错误信息输出指针
 * @return 加载是否成功
 */
bool CoreConfig::loadFromFile(const QString& path, QString* err)
{
    QFile f(path);
    if (!f.exists()) {
        if (err) *err = "配置文件不存在";
        return false;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QString("打开文件失败: %1").arg(f.errorString());
        return false;
    }

    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        if (err) *err = "无效的JSON根节点(非对象)";
        return false;
    }
    const QJsonObject root = doc.object();

    // 读取主配置
    if (root.contains("main") && root["main"].isObject()) {
        const auto main = root["main"].toObject();
        if (main.contains("rpcPort")) core_.rpcPort = quint16(main["rpcPort"].toInt(int(core_.rpcPort)));
    }

    // 读取日志配置
    if (root.contains("log") && root["log"].isObject()) {
        const auto log = root["log"].toObject();
        if (log.contains("logToConsole")) log_.logToConsole = log["logToConsole"].toBool(log_.logToConsole);
        if (log.contains("logToFile")) log_.logToFile = log["logToFile"].toBool(log_.logToFile);
        if (log.contains("logFilePath")) log_.logFilePath = log["logFilePath"].toString(log_.logFilePath);
        if (log.contains("logLevel")) log_.logLevel = log["logLevel"].toInt(log_.logLevel);
    }

    // 读取CAN总线配置
    if (root.contains("can") && root["can"].isObject()) {
        const auto can = root["can"].toObject();
        if (can.contains("ifname")) can_.canIfname = can["ifname"].toString(can_.canIfname);
        if (can.contains("bitrate")) can_.canBitrate = can["bitrate"].toInt(can_.canBitrate);
        if (can.contains("tripleSampling")) can_.canTripleSampling = can["tripleSampling"].toBool(can_.canTripleSampling);
        if (can.contains("canFd")) can_.canFd = can["canFd"].toBool(can_.canFd);
    }

    // 读取设备数组
    devices_.clear();
    if (root.contains("devices") && root["devices"].isArray()) {
        const auto arr = root["devices"].toArray();
        for (const auto& v : arr) {
            if (!v.isObject()) continue;
            const auto o = v.toObject();

            DeviceConfig d;
            d.name = o.value("name").toString();
            d.deviceType = deviceTypeFromInt(o.value("type").toInt(toInt(d.deviceType)));
            d.commType = commTypeFromInt(o.value("commType").toInt(toInt(d.commType)));
            d.node_id = o.value("nodeId").toInt(d.node_id);
            d.bus = o.value("bus").toString(d.bus);
            if (o.contains("params") && o["params"].isObject()) d.params = o["params"].toObject();

            devices_.append(d);
        }
    }

    // 读取设备组数组
    groups_.clear();
    if (root.contains("groups") && root["groups"].isArray()) {
        const auto arr = root["groups"].toArray();
        for (const auto& v : arr) {
            if (!v.isObject()) continue;
            const auto o = v.toObject();

            DeviceGroupConfig g;
            g.groupId = o.value("groupId").toInt(0);
            g.name = o.value("name").toString();
            g.enabled = o.value("enabled").toBool(true);
            
            if (o.contains("devices") && o["devices"].isArray()) {
                const auto devArr = o["devices"].toArray();
                for (const auto& dv : devArr) {
                    g.deviceNodes.append(dv.toInt());
                }
            }

            groups_.append(g);
        }
    }

    // 读取自动策略数组
    strategies_.clear();
    if (root.contains("strategies") && root["strategies"].isArray()) {
        const auto arr = root["strategies"].toArray();
        for (const auto& v : arr) {
            if (!v.isObject()) continue;
            const auto o = v.toObject();

            AutoStrategyConfig s;
            s.strategyId = o.value("id").toInt(0);
            s.name = o.value("name").toString();
            s.groupId = o.value("groupId").toInt(0);
            s.channel = quint8(o.value("channel").toInt(0));
            s.action = o.value("action").toString("stop");
            s.intervalSec = o.value("intervalSec").toInt(60);
            s.enabled = o.value("enabled").toBool(true);
            s.autoStart = o.value("autoStart").toBool(true);

            strategies_.append(s);
        }
    }

    return true;
}

/**
 * @brief 保存配置到JSON文件
 * @param path 配置文件路径
 * @param err 错误信息输出指针
 * @return 保存是否成功
 */
bool CoreConfig::saveToFile(const QString& path, QString* err) const
{
    QJsonObject root;

    // 主配置
    QJsonObject main;
    main["rpcPort"] = int(core_.rpcPort);
    root["main"] = main;

    // 日志配置
    QJsonObject log;
    log["logToConsole"] = log_.logToConsole;
    log["logToFile"] = log_.logToFile;
    log["logFilePath"] = log_.logFilePath;
    log["logLevel"] = log_.logLevel;
    root["log"] = log;

    // CAN总线配置
    QJsonObject can;
    can["ifname"] = can_.canIfname;
    can["bitrate"] = can_.canBitrate;
    can["tripleSampling"] = can_.canTripleSampling;
    can["canFd"] = can_.canFd;
    root["can"] = can;

    // 设备列表
    QJsonArray devArr;
    for (const auto& d : devices_) {
        QJsonObject o;
        o["name"] = d.name;
        o["type"] = toInt(d.deviceType);
        o["commType"] = toInt(d.commType);
        if (d.node_id >= 0) o["nodeId"] = d.node_id;
        if (!d.bus.isEmpty()) o["bus"] = d.bus;
        if (!d.params.isEmpty()) o["params"] = d.params;
        devArr.append(o);
    }
    root["devices"] = devArr;

    // 设备组列表
    QJsonArray groupArr;
    for (const auto& g : groups_) {
        QJsonObject o;
        o["groupId"] = g.groupId;
        o["name"] = g.name;
        o["enabled"] = g.enabled;
        
        QJsonArray devNodes;
        for (int nodeId : g.deviceNodes) {
            devNodes.append(nodeId);
        }
        o["devices"] = devNodes;
        
        groupArr.append(o);
    }
    root["groups"] = groupArr;

    // 自动策略列表
    QJsonArray strategyArr;
    for (const auto& s : strategies_) {
        QJsonObject o;
        o["id"] = s.strategyId;
        o["name"] = s.name;
        o["groupId"] = s.groupId;
        o["channel"] = int(s.channel);
        o["action"] = s.action;
        o["intervalSec"] = s.intervalSec;
        o["enabled"] = s.enabled;
        o["autoStart"] = s.autoStart;
        strategyArr.append(o);
    }
    root["strategies"] = strategyArr;

    QJsonDocument doc(root);
    const QByteArray data = doc.toJson(QJsonDocument::Indented);
    return writeTextFile(path, data, err);
}
