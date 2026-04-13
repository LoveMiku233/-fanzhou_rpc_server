/**
 * @file rpc_registry_screen.cpp
 * @brief 屏幕与云上传配置RPC方法注册
 */

#include "rpc_registry.h"
#include "core_context.h"
#include "rpc_registry_keys.h"

#include "rpc/json_rpc_dispatcher.h"
#include "rpc/rpc_error_codes.h"
#include "rpc/rpc_helpers.h"

#include <QJsonObject>
#include <climits>

namespace fanzhou {
namespace core {

namespace {
const QString &kKeyOk = rpc_keys::Ok();
const QString &kKeyEnabled = rpc_keys::Enabled();
constexpr int kMinPercent = 0;
constexpr int kMaxPercent = 100;
}  // namespace

void RpcRegistry::registerScreen()
{
    // 获取屏幕配置
    dispatcher_->registerMethod(QStringLiteral("screen.get"),
                                 [this](const QJsonObject &) {
        const auto config = context_->getScreenConfig();
        return QJsonObject{
            {kKeyOk, true},
            {QStringLiteral("brightness"), config.brightness},
            {QStringLiteral("contrast"), config.contrast},
            {kKeyEnabled, config.enabled},
            {QStringLiteral("sleepTimeoutSec"), config.sleepTimeoutSec},
            {QStringLiteral("orientation"), config.orientation}
        };
    });

    // 设置屏幕配置
    dispatcher_->registerMethod(QStringLiteral("screen.set"),
                                 [this](const QJsonObject &params) {
        ScreenConfig config = context_->getScreenConfig();

        // Update only provided parameters
        if (params.contains(QStringLiteral("brightness"))) {
            qint32 brightness = 0;
            if (!rpc::RpcHelpers::getI32InRange(params, "brightness", brightness, kMinPercent, kMaxPercent))
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid brightness (0..100)"));
            config.brightness = brightness;
        }
        if (params.contains(QStringLiteral("contrast"))) {
            qint32 contrast = 0;
            if (!rpc::RpcHelpers::getI32InRange(params, "contrast", contrast, kMinPercent, kMaxPercent))
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid contrast (0..100)"));
            config.contrast = contrast;
        }
        if (params.contains(kKeyEnabled)) {
            bool enabled = true;
            if (!rpc::RpcHelpers::getBool(params, "enabled", enabled, true))
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid enabled"));
            config.enabled = enabled;
        }
        if (params.contains(QStringLiteral("sleepTimeoutSec"))) {
            qint32 timeout = 0;
            if (!rpc::RpcHelpers::getI32InRange(params, "sleepTimeoutSec", timeout, 0, INT_MAX))
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid sleepTimeoutSec (>=0)"));
            config.sleepTimeoutSec = timeout;
        }
        if (params.contains(QStringLiteral("orientation"))) {
            QString orientation;
            if (!rpc::RpcHelpers::getString(params, "orientation", orientation))
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid orientation"));
            config.orientation = orientation;
        }

        QString error;
        if (!context_->setScreenConfig(config, &error)) {
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, error);
        }

        return QJsonObject{{kKeyOk, true}};
    });
    
    // ==================== 云数据上传配置 ====================
    
    // 获取云数据上传配置
    dispatcher_->registerMethod(QStringLiteral("cloud.upload.get"),
                                 [this](const QJsonObject &) {
        const auto &config = context_->cloudUploadConfig;
        return QJsonObject{
            {kKeyOk, true},
            {kKeyEnabled, config.enabled},
            {QStringLiteral("uploadMode"), config.uploadMode},
            {QStringLiteral("intervalSec"), config.intervalSec},
            {QStringLiteral("uploadChannelStatus"), config.uploadChannelStatus},
            {QStringLiteral("uploadPhaseLoss"), config.uploadPhaseLoss},
            {QStringLiteral("uploadCurrent"), config.uploadCurrent},
            {QStringLiteral("uploadOnlineStatus"), config.uploadOnlineStatus},
            {QStringLiteral("currentThreshold"), config.currentThreshold},
            {QStringLiteral("statusChangeOnly"), config.statusChangeOnly},
            {QStringLiteral("minUploadIntervalSec"), config.minUploadIntervalSec}
        };
    });
    
    // 设置云数据上传配置
    dispatcher_->registerMethod(QStringLiteral("cloud.upload.set"),
                                 [this](const QJsonObject &params) {
        auto &config = context_->cloudUploadConfig;
        
        // 更新提供的参数
        if (params.contains(kKeyEnabled)) {
            bool enabled = true;
            if (!rpc::RpcHelpers::getBool(params, "enabled", enabled, true))
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid enabled"));
            config.enabled = enabled;
        }
        if (params.contains(QStringLiteral("uploadMode"))) {
            QString mode;
            if (!rpc::RpcHelpers::getString(params, "uploadMode", mode))
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid uploadMode"));
            if (mode != QStringLiteral("interval") && mode != QStringLiteral("change")) {
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, 
                    QStringLiteral("uploadMode must be 'interval' or 'change'"));
            }
            config.uploadMode = mode;
        }
        if (params.contains(QStringLiteral("intervalSec"))) {
            qint32 interval = 0;
            if (!rpc::RpcHelpers::getI32InRange(params, "intervalSec", interval, 1, INT_MAX))
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid intervalSec (>=1)"));
            config.intervalSec = interval;
        }
        if (params.contains(QStringLiteral("uploadChannelStatus"))) {
            bool val = true;
            if (!rpc::RpcHelpers::getBool(params, "uploadChannelStatus", val, true))
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid uploadChannelStatus"));
            config.uploadChannelStatus = val;
        }
        if (params.contains(QStringLiteral("uploadPhaseLoss"))) {
            bool val = true;
            if (!rpc::RpcHelpers::getBool(params, "uploadPhaseLoss", val, true))
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid uploadPhaseLoss"));
            config.uploadPhaseLoss = val;
        }
        if (params.contains(QStringLiteral("uploadCurrent"))) {
            bool val = true;
            if (!rpc::RpcHelpers::getBool(params, "uploadCurrent", val, true))
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid uploadCurrent"));
            config.uploadCurrent = val;
        }
        if (params.contains(QStringLiteral("uploadOnlineStatus"))) {
            bool val = true;
            if (!rpc::RpcHelpers::getBool(params, "uploadOnlineStatus", val, true))
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid uploadOnlineStatus"));
            config.uploadOnlineStatus = val;
        }
        if (params.contains(QStringLiteral("currentThreshold"))) {
            double threshold = 0.0;
            if (!rpc::RpcHelpers::getDouble(params, "currentThreshold", threshold))
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid currentThreshold"));
            if (threshold < 0.0) {
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue, 
                    QStringLiteral("currentThreshold must be >= 0"));
            }
            config.currentThreshold = threshold;
        }
        if (params.contains(QStringLiteral("statusChangeOnly"))) {
            bool val = true;
            if (!rpc::RpcHelpers::getBool(params, "statusChangeOnly", val, true))
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid statusChangeOnly"));
            config.statusChangeOnly = val;
        }
        if (params.contains(QStringLiteral("minUploadIntervalSec"))) {
            qint32 interval = 0;
            if (!rpc::RpcHelpers::getI32InRange(params, "minUploadIntervalSec", interval, 0, INT_MAX))
                return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType, QStringLiteral("invalid minUploadIntervalSec (>=0)"));
            config.minUploadIntervalSec = interval;
        }
        
        return QJsonObject{{kKeyOk, true}};
    });
}

}  // namespace core
}  // namespace fanzhou
