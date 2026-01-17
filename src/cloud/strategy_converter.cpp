/**
 * @file strategy_converter.cpp
 * @brief 云平台策略转换器实现
 */

#include "strategy_converter.h"
#include "utils/logger.h"

#include <QJsonArray>
#include <QRegularExpression>

namespace fanzhou {
namespace cloud {

namespace {
const char *const kLogSource = "StrategyConverter";
}  // namespace


inline uint qHash(const CloudTypeId& key, uint seed = 0) noexcept
{
    return ::qHash(static_cast<int>(key), seed);
}

// ==================== RpcAction ====================

QJsonObject RpcAction::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("method")] = method;
    obj[QStringLiteral("params")] = params;
    if (priority != 0) {
        obj[QStringLiteral("priority")] = priority;
    }
    if (delayMs != 0) {
        obj[QStringLiteral("delayMs")] = delayMs;
    }
    return obj;
}

RpcAction RpcAction::fromJson(const QJsonObject &obj)
{
    RpcAction action;
    action.method = obj.value(QStringLiteral("method")).toString();
    action.params = obj.value(QStringLiteral("params")).toObject();
    action.priority = obj.value(QStringLiteral("priority")).toInt(0);
    action.delayMs = obj.value(QStringLiteral("delayMs")).toInt(0);
    return action;
}

// ==================== StrategyConvertResult ====================

StrategyConvertResult StrategyConvertResult::ok(const QList<RpcAction> &actions)
{
    StrategyConvertResult result;
    result.success = true;
    result.actions = actions;
    return result;
}

StrategyConvertResult StrategyConvertResult::error(int code, const QString &msg)
{
    StrategyConvertResult result;
    result.success = false;
    result.errorCode = code;
    result.errorMsg = msg;
    return result;
}

// ==================== FanzhouStrategyConverter ====================

FanzhouStrategyConverter::FanzhouStrategyConverter()
{
}

StrategyConvertResult FanzhouStrategyConverter::toRpcActions(const QList<SceneAction> &actions)
{
    QList<RpcAction> rpcActions;

    for (const auto &action : actions) {
        StrategyConvertResult singleResult = toRpcAction(action);
        if (!singleResult.success) {
            // 单个动作转换失败，记录警告但继续处理
            LOG_WARNING(kLogSource,
                        QStringLiteral("Failed to convert action: deviceCode=%1, identifier=%2, error=%3")
                            .arg(action.deviceCode)
                            .arg(action.identifier)
                            .arg(singleResult.errorMsg));
            continue;
        }
        rpcActions.append(singleResult.actions);
    }

    if (rpcActions.isEmpty() && !actions.isEmpty()) {
        return StrategyConvertResult::error(5001, QStringLiteral("No valid actions converted"));
    }

    return StrategyConvertResult::ok(rpcActions);
}

StrategyConvertResult FanzhouStrategyConverter::toRpcAction(const SceneAction &action)
{
    // 检查设备支持
    if (!isDeviceSupported(action.deviceCode)) {
        return StrategyConvertResult::error(5002,
                                             QStringLiteral("Device not supported: %1").arg(action.deviceCode));
    }

    // 检查属性支持
    if (!isIdentifierSupported(action.identifier)) {
        return StrategyConvertResult::error(5003,
                                             QStringLiteral("Identifier not supported: %1").arg(action.identifier));
    }

    // 获取设备映射
    quint8 nodeId = 0;
    if (!getDeviceMapping(action.deviceCode, nodeId)) {
        return StrategyConvertResult::error(5004,
                                             QStringLiteral("No device mapping for: %1").arg(action.deviceCode));
    }

    // 解析通道号
    int channel = 0;
    if (!parseSwChannel(action.identifier, channel)) {
        return StrategyConvertResult::error(5005,
                                             QStringLiteral("Failed to parse channel from: %1").arg(action.identifier));
    }

    // 转换属性值为继电器动作
    QString relayAction = valueToRelayAction(action.identifierValue);

    // 构建RPC动作
    RpcAction rpcAction;
    rpcAction.method = QStringLiteral("relay.controlMulti");
    rpcAction.params[QStringLiteral("node")] = static_cast<int>(nodeId);

    // 处理通道：-1表示所有通道，使用actions数组格式；否则使用ch+action格式
    if (channel == -1) {
        // 所有通道使用同一动作，生成actions数组
        QJsonArray actionsArr;
        for (int i = 0; i < 4; ++i) {
            actionsArr.append(relayAction);
        }
        rpcAction.params[QStringLiteral("actions")] = actionsArr;
    } else {
        // 单通道使用ch+action格式
        rpcAction.params[QStringLiteral("ch")] = channel;
        rpcAction.params[QStringLiteral("action")] = relayAction;
    }

    LOG_DEBUG(kLogSource,
              QStringLiteral("Converted action: deviceCode=%1, identifier=%2 -> node=%3, ch=%4, action=%5")
                  .arg(action.deviceCode)
                  .arg(action.identifier)
                  .arg(nodeId)
                  .arg(channel)
                  .arg(relayAction));

    return StrategyConvertResult::ok({rpcAction});
}

bool FanzhouStrategyConverter::isDeviceSupported(const QString &deviceCode) const
{
    return deviceMappings_.contains(deviceCode);
}

bool FanzhouStrategyConverter::isIdentifierSupported(const QString &identifier) const
{
    // 支持 sw0, sw1, sw2, sw3 或 sw (表示全部通道)
    static QRegularExpression swPattern(QStringLiteral("^sw[0-3]?$"));
    return swPattern.match(identifier).hasMatch();
}

void FanzhouStrategyConverter::registerDeviceMapping(const QString &deviceCode, quint8 nodeId)
{
    deviceMappings_[deviceCode] = nodeId;
    LOG_INFO(kLogSource,
             QStringLiteral("Registered device mapping: %1 -> node %2")
                 .arg(deviceCode)
                 .arg(nodeId));
}

bool FanzhouStrategyConverter::getDeviceMapping(const QString &deviceCode, quint8 &nodeId) const
{
    auto it = deviceMappings_.find(deviceCode);
    if (it != deviceMappings_.end()) {
        nodeId = it.value();
        return true;
    }
    return false;
}

void FanzhouStrategyConverter::clearDeviceMappings()
{
    deviceMappings_.clear();
}

bool FanzhouStrategyConverter::parseSwChannel(const QString &identifier, int &channel) const
{
    // identifier格式:
    // - sw0, sw1, sw2, sw3: 控制指定通道（channel = 0-3）
    // - sw: 控制所有通道（channel = -1，会被转换为actions数组格式）
    if (identifier == QStringLiteral("sw")) {
        channel = -1;  // -1 表示全部通道，toRpcAction会将其转换为actions数组格式
        return true;
    }

    if (identifier.length() == 3 && identifier.startsWith(QStringLiteral("sw"))) {
        QChar chNum = identifier[2];
        if (chNum >= '0' && chNum <= '3') {
            channel = chNum.digitValue();
            return true;
        }
    }

    return false;
}

QString FanzhouStrategyConverter::valueToRelayAction(const QVariant &value) const
{
    // 将属性值转换为继电器动作
    // 支持的值：
    // - 0, "0", false, "stop" -> stop
    // - 1, "1", true, "fwd", "forward", "on" -> fwd
    // - 2, "2", "rev", "reverse" -> rev

    if (value.canConvert<int>()) {
        int intVal = value.toInt();
        switch (intVal) {
        case 0: return QStringLiteral("stop");
        case 1: return QStringLiteral("fwd");
        case 2: return QStringLiteral("rev");
        default: return QStringLiteral("stop");
        }
    }

    QString strVal = value.toString().toLower();
    if (strVal == QStringLiteral("stop") || strVal == QStringLiteral("off") ||
        strVal == QStringLiteral("0") || strVal == QStringLiteral("false")) {
        return QStringLiteral("stop");
    }
    if (strVal == QStringLiteral("fwd") || strVal == QStringLiteral("forward") ||
        strVal == QStringLiteral("on") || strVal == QStringLiteral("1") ||
        strVal == QStringLiteral("true")) {
        return QStringLiteral("fwd");
    }
    if (strVal == QStringLiteral("rev") || strVal == QStringLiteral("reverse") ||
        strVal == QStringLiteral("2")) {
        return QStringLiteral("rev");
    }

    return QStringLiteral("stop");
}

// ==================== StrategyConverterFactory ====================

StrategyConverterFactory &StrategyConverterFactory::instance()
{
    static StrategyConverterFactory instance;
    return instance;
}

StrategyConverterFactory::StrategyConverterFactory()
{
    // 默认注册泛舟云平台转换器
    fanzhouConverter_ = std::make_shared<FanzhouStrategyConverter>();
    converters_[CloudTypeId::FanzhouCloudMqtt] = fanzhouConverter_;
}

void StrategyConverterFactory::registerConverter(std::shared_ptr<IStrategyConverter> converter)
{
    if (converter) {
        converters_[converter->cloudType()] = converter;
        LOG_INFO(kLogSource,
                 QStringLiteral("Registered strategy converter: %1")
                     .arg(converter->cloudName()));
    }
}

IStrategyConverter *StrategyConverterFactory::getConverter(CloudTypeId cloudType)
{
    auto it = converters_.find(cloudType);
    if (it != converters_.end()) {
        return it.value().get();
    }
    return nullptr;
}

FanzhouStrategyConverter *StrategyConverterFactory::getFanzhouConverter()
{
    return fanzhouConverter_.get();
}

QList<CloudTypeId> StrategyConverterFactory::registeredTypes() const
{
    return converters_.keys();
}

}  // namespace cloud
}  // namespace fanzhou
