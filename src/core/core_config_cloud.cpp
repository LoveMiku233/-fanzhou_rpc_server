#include "core_config.h"

namespace fanzhou{
namespace core {

bool CoreConfig::loadCloudUpload(const QJsonObject &root)
{
    // 云数据上传配置
    if (root.contains(QStringLiteral("cloudUpload")) &&
        root[QStringLiteral("cloudUpload")].isObject()) {
        const auto uploadObj = root[QStringLiteral("cloudUpload")].toObject();
        cloudUpload.enabled = uploadObj.value(QStringLiteral("enabled")).toBool(cloudUpload.enabled);
        cloudUpload.uploadMode = uploadObj.value(QStringLiteral("uploadMode")).toString(cloudUpload.uploadMode);
        cloudUpload.intervalSec = uploadObj.value(QStringLiteral("intervalSec")).toInt(cloudUpload.intervalSec);
        cloudUpload.uploadChannelStatus = uploadObj.value(QStringLiteral("uploadChannelStatus")).toBool(cloudUpload.uploadChannelStatus);
        cloudUpload.uploadPhaseLoss = uploadObj.value(QStringLiteral("uploadPhaseLoss")).toBool(cloudUpload.uploadPhaseLoss);
        cloudUpload.uploadCurrent = uploadObj.value(QStringLiteral("uploadCurrent")).toBool(cloudUpload.uploadCurrent);
        cloudUpload.uploadOnlineStatus = uploadObj.value(QStringLiteral("uploadOnlineStatus")).toBool(cloudUpload.uploadOnlineStatus);
        cloudUpload.currentThreshold = uploadObj.value(QStringLiteral("currentThreshold")).toDouble(cloudUpload.currentThreshold);
        cloudUpload.statusChangeOnly = uploadObj.value(QStringLiteral("statusChangeOnly")).toBool(cloudUpload.statusChangeOnly);
        cloudUpload.minUploadIntervalSec = uploadObj.value(QStringLiteral("minUploadIntervalSec")).toInt(cloudUpload.minUploadIntervalSec);

        // 解析 channelBindings
        cloudUpload.channelBindings.clear();

        if (uploadObj.contains(QStringLiteral("channelBindings")) &&
            uploadObj[QStringLiteral("channelBindings")].isArray()) {

            const auto arr = uploadObj[QStringLiteral("channelBindings")].toArray();
            for (const auto &val : arr) {
                if (!val.isObject()) continue;
                const auto obj = val.toObject();

                CloudMqttChannelBinding binding;
                binding.channelId = obj.value(QStringLiteral("channelId")).toInt(0);
                // binding.topic = obj.value(QStringLiteral("topic")).toString();

                if (obj.contains(QStringLiteral("nodes")) &&
                    obj[QStringLiteral("nodes")].isArray()) {

                    const auto nodeArr = obj[QStringLiteral("nodes")].toArray();
                    for (const auto &nodeVal : nodeArr) {
                        if (!nodeVal.isObject()) continue;
                        const auto nodeObj = nodeVal.toObject();

                        CloudNodeBinding nb;
                        nb.nodeId = static_cast<quint8>(
                            nodeObj.value(QStringLiteral("nodeId")).toInt(0));
                        nb.formatId = nodeObj.value(QStringLiteral("formatId")).toString();

                        binding.nodes.append(nb);
                    }
                }

                cloudUpload.channelBindings.append(binding);
            }
        }
    } else {
        return false;
    }
    return true;
}

bool CoreConfig::loadMqttChannels(const QJsonObject &root)
{
    // MQTT多通道配置
    mqttChannels.clear();
    if (root.contains(QStringLiteral("mqttChannels")) &&
        root[QStringLiteral("mqttChannels")].isArray()) {
        const auto arr = root[QStringLiteral("mqttChannels")].toArray();
        for (const auto &value : arr) {
            if (!value.isObject()) continue;
            const auto obj = value.toObject();

            MqttChannelConfig mqtt;
            mqtt.type = static_cast<cloud::CloudTypeId>(obj.value(QStringLiteral("type")).toInt(0));
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
            mqtt.topicControlSub  = obj.value(QStringLiteral("topicControlSub")).toString();
            mqtt.topicStrategySub = obj.value(QStringLiteral("topicStrategySub")).toString();
            mqtt.topicSettingSub  = obj.value(QStringLiteral("topicSettingSub")).toString();
            mqtt.topicSettingPub  = obj.value(QStringLiteral("topicSettingPub")).toString();
            mqtt.topicStatusPub   = obj.value(QStringLiteral("topicStatusPub")).toString();
            mqtt.topicEventPub    = obj.value(QStringLiteral("topicEventPub")).toString();

            mqttChannels.append(mqtt);
        }
    } else {
        return false;
    }
    return true;
}

void CoreConfig::saveCloudUpload(QJsonObject &root) const
{
    // 云数据上传配置
    QJsonObject cloudUploadObj;
    cloudUploadObj[QStringLiteral("enabled")] = cloudUpload.enabled;
    cloudUploadObj[QStringLiteral("uploadMode")] = cloudUpload.uploadMode;
    cloudUploadObj[QStringLiteral("intervalSec")] = cloudUpload.intervalSec;
    cloudUploadObj[QStringLiteral("uploadChannelStatus")] = cloudUpload.uploadChannelStatus;
    cloudUploadObj[QStringLiteral("uploadPhaseLoss")] = cloudUpload.uploadPhaseLoss;
    cloudUploadObj[QStringLiteral("uploadCurrent")] = cloudUpload.uploadCurrent;
    cloudUploadObj[QStringLiteral("uploadOnlineStatus")] = cloudUpload.uploadOnlineStatus;
    cloudUploadObj[QStringLiteral("currentThreshold")] = cloudUpload.currentThreshold;
    cloudUploadObj[QStringLiteral("statusChangeOnly")] = cloudUpload.statusChangeOnly;
    cloudUploadObj[QStringLiteral("minUploadIntervalSec")] = cloudUpload.minUploadIntervalSec;
    // 保存 channelBindings
    if (!cloudUpload.channelBindings.isEmpty()) {
        QJsonArray bindArr;

        for (const auto &binding : cloudUpload.channelBindings) {
            QJsonObject obj;
            obj[QStringLiteral("channelId")] = binding.channelId;
            obj[QStringLiteral("topic")] = binding.topic;

            QJsonArray nodeArr;
            for (const auto &nb : binding.nodes) {
                QJsonObject nobj;
                nobj[QStringLiteral("nodeId")] = nb.nodeId;
                if (!nb.formatId.isEmpty()) {
                    nobj[QStringLiteral("formatId")] = nb.formatId;
                }
                nodeArr.append(nobj);
            }

            obj[QStringLiteral("nodes")] = nodeArr;
            bindArr.append(obj);
        }

        cloudUploadObj[QStringLiteral("channelBindings")] = bindArr;
    }
    root[QStringLiteral("cloudUpload")] = cloudUploadObj;
}

void CoreConfig::saveMqttChannels(QJsonObject &root) const
{
    // MQTT多通道配置
    QJsonArray mqttArr;
    for (const auto &mqtt : mqttChannels) {
        QJsonObject obj;
        obj[QStringLiteral("type")] = static_cast<int>(mqtt.type);
        obj[QStringLiteral("channelId")] = mqtt.channelId;
        obj[QStringLiteral("name")] = mqtt.name;
        obj[QStringLiteral("enabled")] = mqtt.enabled;
        obj[QStringLiteral("broker")] = mqtt.broker;
        obj[QStringLiteral("port")] = static_cast<int>(mqtt.port);
        obj[QStringLiteral("clientId")] = mqtt.clientId;
        if (!mqtt.username.isEmpty()) {
            obj[QStringLiteral("username")] = mqtt.username;
        }
        if (!mqtt.password.isEmpty()) {
            obj[QStringLiteral("password")] = mqtt.password;
        }
        if (!mqtt.topicControlSub.isEmpty()) {
            obj["topicControlSub"] = mqtt.topicControlSub;
        }
        if (!mqtt.topicStrategySub.isEmpty()) {
            obj["topicStrategySub"] = mqtt.topicStrategySub;
        }
        if (!mqtt.topicStatusPub.isEmpty()) {
            obj["topicStatusPub"] = mqtt.topicStatusPub;
        }
        if (!mqtt.topicEventPub.isEmpty()) {
            obj["topicEventPub"] = mqtt.topicEventPub;
        }
        if (!mqtt.topicSettingPub.isEmpty()) {
            obj["topicSettingPub"] = mqtt.topicSettingPub;
        }
        if (!mqtt.topicSettingSub.isEmpty()) {
            obj["topicSettingSub"] = mqtt.topicSettingSub;
        }

        obj[QStringLiteral("topicPrefix")] = mqtt.topicPrefix;
        obj[QStringLiteral("keepAliveSec")] = mqtt.keepAliveSec;
        obj[QStringLiteral("autoReconnect")] = mqtt.autoReconnect;
        obj[QStringLiteral("reconnectIntervalSec")] = mqtt.reconnectIntervalSec;
        obj[QStringLiteral("qos")] = mqtt.qos;
        mqttArr.append(obj);

    }
    root[QStringLiteral("mqttChannels")] = mqttArr;
}

}
}
