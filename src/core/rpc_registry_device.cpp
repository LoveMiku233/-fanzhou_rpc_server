/**
 * @file rpc_registry_device.cpp
 * @brief 设备管理RPC方法注册
 */

#include "rpc_registry.h"
#include "core_context.h"
#include "rpc_registry_common.h"
#include "rpc_registry_keys.h"

#include "device/can/relay_gd427.h"
#include "device/can/relay_protocol.h"
#include "device/device_types.h"
#include "rpc/json_rpc_dispatcher.h"
#include "rpc/rpc_error_codes.h"
#include "rpc/rpc_helpers.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <climits>

namespace fanzhou {
namespace core {

namespace {
const QString &kKeyOk = rpc_keys::Ok();
const QString &kKeyCh = rpc_keys::Ch();
const QString &kKeyChannel = rpc_keys::Channel();
const QString &kKeyStatusByte = rpc_keys::StatusByte();
const QString &kKeyCurrentA = rpc_keys::CurrentA();
const QString &kKeyMode = rpc_keys::Mode();
const QString &kKeyPhaseLost = rpc_keys::PhaseLost();
const QString &kKeyOnline = rpc_keys::Online();
const QString &kKeyAgeMs = rpc_keys::AgeMs();
const QString &kKeyChannels = rpc_keys::Channels();
const QString &kKeyName = rpc_keys::Name();
const QString &kKeyDevices = rpc_keys::Devices();
const QString &kKeyTotal = rpc_keys::Total();

QJsonObject buildDeviceBaseObject(const DeviceConfig &dev)
{
    QJsonObject obj;
    obj[QStringLiteral("nodeId")] = dev.nodeId;
    obj[kKeyName] = dev.name;
    obj[QStringLiteral("type")] = static_cast<int>(dev.deviceType);
    obj[QStringLiteral("typeName")] = QString::fromLatin1(device::deviceTypeToString(dev.deviceType));
    obj[QStringLiteral("commType")] = static_cast<int>(dev.commType);
    obj[QStringLiteral("commTypeName")] = QString::fromLatin1(device::commTypeToString(dev.commType));
    obj[QStringLiteral("bus")] = dev.bus;
    if (!dev.params.isEmpty()) {
        obj[QStringLiteral("params")] = dev.params;
    }
    return obj;
}

}  // namespace

void RpcRegistry::registerDevice()
{
    // 获取设备类型列表
    dispatcher_->registerMethod(QStringLiteral("device.types"),
                                 [](const QJsonObject &) {
        QJsonArray arr;
        int count = 0;
        const auto *types = device::allDeviceTypes(count);
        for (int i = 0; i < count; ++i) {
            arr.append(QJsonObject{
                {QStringLiteral("id"), static_cast<int>(types[i].id)},
                {kKeyName, QString::fromLatin1(types[i].name)},
                {QStringLiteral("category"), QString::fromLatin1(types[i].category)},
                {QStringLiteral("defaultCommType"), static_cast<int>(types[i].defaultCommType)},
                {QStringLiteral("defaultCommTypeName"),
                 QString::fromLatin1(device::commTypeToString(types[i].defaultCommType))}
            });
        }
        return QJsonObject{{kKeyOk, true}, {QStringLiteral("types"), arr}};
    });

    // 获取通信类型列表
    dispatcher_->registerMethod(QStringLiteral("device.commTypes"),
                                 [](const QJsonObject &) {
        QJsonArray arr;
        int count = 0;
        const auto *types = device::allCommTypes(count);
        for (int i = 0; i < count; ++i) {
            arr.append(QJsonObject{
                {QStringLiteral("id"), static_cast<int>(types[i].id)},
                {kKeyName, QString::fromLatin1(types[i].name)},
                {QStringLiteral("description"), QString::fromUtf8(types[i].description)}
            });
        }
        return QJsonObject{{kKeyOk, true}, {QStringLiteral("commTypes"), arr}};
    });

    // 获取接口类型列表
    dispatcher_->registerMethod(QStringLiteral("device.interfaceTypes"),
                                 [](const QJsonObject &) {
        QJsonArray arr;
        int count = 0;
        const auto *types = device::allInterfaceTypes(count);
        for (int i = 0; i < count; ++i) {
            arr.append(QJsonObject{
                {QStringLiteral("id"), static_cast<int>(types[i].id)},
                {kKeyName, QString::fromLatin1(types[i].name)},
                {QStringLiteral("description"), QString::fromUtf8(types[i].description)}
            });
        }
        return QJsonObject{{kKeyOk, true}, {QStringLiteral("interfaceTypes"), arr}};
    });

    // 获取设备列表（包含在线状态、电流和通道状态信息）
    dispatcher_->registerMethod(QStringLiteral("device.list"),
                                 [this](const QJsonObject &params) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const bool forceRefresh = params.value(QStringLiteral("forceRefresh")).toBool(false);
        static qint64 cacheTsMs = 0;
        static QJsonObject cachedResponse;
        const qint64 cacheAge = now - cacheTsMs;
        if (!forceRefresh && !cachedResponse.isEmpty() && cacheAge >= 0 &&
            cacheAge <= kDeviceListCacheMs) {
            QJsonObject result = cachedResponse;
            result[QStringLiteral("cached")] = true;
            result[QStringLiteral("cacheAgeMs")] = static_cast<double>(cacheAge);
            return result;
        }

        QJsonArray arr;
        const auto devices = context_->listDevices();
        for (const auto &dev : devices) {
            QJsonObject obj = buildDeviceBaseObject(dev);

            // 添加设备在线状态、电流和通道信息（如果是继电器设备）
            auto *relayDev = context_->relays.value(static_cast<quint8>(dev.nodeId), nullptr);
            if (relayDev) {
                const qint64 lastSeen = relayDev->lastSeenMs();
                bool online = false;
                qint64 ageMs = -1;
                if (lastSeen > 0) {
                    ageMs = now - lastSeen;
                    online = (ageMs <= kOnlineTimeoutMs);
                }
                obj[kKeyOnline] = online;
                obj[kKeyAgeMs] = (ageMs >= 0) ? static_cast<double>(ageMs) : QJsonValue();

                // 添加通道状态和总电流信息
                QJsonObject channels;
                double totalCurrent = 0.0;
                for (quint8 ch = 0; ch < kDefaultChannelCount; ++ch) {
                    const auto status = relayDev->lastStatus(ch);
                    QJsonObject chObj;
                    chObj[kKeyCh] = static_cast<int>(ch);
                    chObj[kKeyChannel] = static_cast<int>(status.channel);
                    chObj[kKeyStatusByte] = static_cast<int>(status.statusByte);
                    chObj[kKeyCurrentA] = static_cast<double>(status.currentA);
                    chObj[kKeyMode] =
                        static_cast<int>(device::RelayProtocol::modeBits(status.statusByte));
                    chObj[kKeyPhaseLost] = device::RelayProtocol::phaseLost(status.statusByte);
                    // 电流单位转换为mA供前端显示
                    chObj[QStringLiteral("current")] =
                        static_cast<double>(status.currentA) * 1000.0;
                    channels[QString::number(ch)] = chObj;
                    totalCurrent += static_cast<double>(status.currentA) * 1000.0;
                }
                obj[kKeyChannels] = channels;
                obj[QStringLiteral("totalCurrent")] = totalCurrent;
            } else {
                // 非继电器设备，设置默认值
                obj[kKeyOnline] = false;
                obj[kKeyAgeMs] = QJsonValue();
            }

            arr.append(obj);
        }
        QJsonObject result{{kKeyOk, true},
                           {kKeyDevices, arr},
                           {kKeyTotal, arr.size()},
                           {QStringLiteral("cached"), false}};
        cacheTsMs = now;
        cachedResponse = result;
        return result;
    });

    // 获取单个设备信息
    dispatcher_->registerMethod(QStringLiteral("device.get"),
                                 [this](const QJsonObject &params) {
        quint8 nodeId = 0;
        if (!rpc::RpcHelpers::getU8InRange(params, "nodeId", nodeId, 1, 255))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing nodeId"));

        const auto dev = context_->getDeviceConfig(nodeId);
        if (dev.nodeId < 0) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue,
                                        QStringLiteral("device not found"));
        }

        QJsonObject obj = buildDeviceBaseObject(dev);
        obj[kKeyOk] = true;
        return obj;
    });

    // 动态添加设备
    dispatcher_->registerMethod(QStringLiteral("device.add"),
                                 [this](const QJsonObject &params) {
        qint32 nodeId = 0;
        qint32 deviceType = 0;
        qint32 commType = 0;
        QString name;
        QString bus;

        if (!rpc::RpcHelpers::getI32InRange(params, "nodeId", nodeId, 1, 255))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing/invalid nodeId (1-255)"));
        if (!rpc::RpcHelpers::getI32InRange(params, "type", deviceType, 1, INT_MAX))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing type"));
        if (!rpc::RpcHelpers::getString(params, "name", name))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing name"));

        // Optional parameters
        if (params.contains(QStringLiteral("commType")) &&
            !rpc::RpcHelpers::getI32InRange(params, "commType", commType, 1, 5)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue,
                                        QStringLiteral("invalid commType (1..5)"));
        }
        rpc::RpcHelpers::getString(params, "bus", bus);

        DeviceConfig config;
        config.nodeId = nodeId;
        config.name = name;
        config.deviceType = static_cast<device::DeviceTypeId>(deviceType);

        // Determine appropriate comm type based on device type if not specified
        if (commType > 0) {
            config.commType = static_cast<device::CommTypeId>(commType);
        } else {
            // Use the new getDefaultCommType function
            config.commType = device::getDefaultCommType(config.deviceType);
        }
        config.bus = bus.isEmpty() ? context_->coreConfig.can.interface : bus;

        if (params.contains(QStringLiteral("params")) && params[QStringLiteral("params")].isObject()) {
            config.params = params[QStringLiteral("params")].toObject();
        }

        QString error;
        if (!context_->addDevice(config, &error)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, error);
        }

        return QJsonObject{{kKeyOk, true}, {QStringLiteral("nodeId"), nodeId}};
    });

    // 动态移除设备
    dispatcher_->registerMethod(QStringLiteral("device.remove"),
                                 [this](const QJsonObject &params) {
        quint8 nodeId = 0;
        if (!rpc::RpcHelpers::getU8InRange(params, "nodeId", nodeId, 1, 255))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing nodeId"));

        QString error;
        if (!context_->removeDevice(nodeId, &error)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, error);
        }

        return QJsonObject{{kKeyOk, true}};
    });

    // 获取设备参数配置
    dispatcher_->registerMethod(QStringLiteral("device.param.get"),
                                 [this](const QJsonObject &params) {
        quint8 nodeId = 0;
        if (!rpc::RpcHelpers::getU8InRange(params, "nodeId", nodeId, 1, 255)) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing nodeId"));
        }

        const DeviceConfig cfg = context_->getDeviceConfig(nodeId);
        if (cfg.nodeId < 0) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue,
                                        QStringLiteral("device not found"));
        }

        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("nodeId"), static_cast<int>(nodeId)},
            {QStringLiteral("params"), cfg.params}
        };
    });

    // 设置设备参数配置（merge/replace）
    dispatcher_->registerMethod(QStringLiteral("device.param.set"),
                                 [this](const QJsonObject &params) {
        quint8 nodeId = 0;
        if (!rpc::RpcHelpers::getU8InRange(params, "nodeId", nodeId, 1, 255)) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing nodeId"));
        }

        if (!params.contains(QStringLiteral("params")) ||
            !params.value(QStringLiteral("params")).isObject()) {
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing/invalid params object"));
        }

        const bool merge = params.value(QStringLiteral("merge")).toBool(true);
        const bool save = params.value(QStringLiteral("save")).toBool(false);
        const QJsonObject newParams = params.value(QStringLiteral("params")).toObject();

        QString error;
        if (!context_->setDeviceParams(nodeId, newParams, merge, &error)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, error);
        }

        if (save) {
            QString saveError;
            if (!context_->saveConfig(QString(), &saveError)) {
                return rpc::RpcHelpers::err(rpc::RpcError::InternalError,
                                            QStringLiteral("params updated but save failed: %1")
                                                .arg(saveError));
            }
        }

        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("nodeId"), static_cast<int>(nodeId)},
            {QStringLiteral("params"), context_->getDeviceParams(nodeId)},
            {QStringLiteral("saved"), save}
        };
    });
}

}  // namespace core
}  // namespace fanzhou
