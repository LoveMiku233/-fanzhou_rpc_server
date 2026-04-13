/**
 * @file rpc_registry_can.cpp
 * @brief CAN RPC方法注册
 */

#include "rpc_registry.h"
#include "core_context.h"
#include "rpc_registry_keys.h"

#include "comm/can/can_comm.h"
#include "rpc/json_rpc_dispatcher.h"
#include "rpc/rpc_error_codes.h"
#include "rpc/rpc_helpers.h"

#include <QDateTime>
#include <QJsonObject>
#include <climits>

namespace fanzhou {
namespace core {

namespace {
const QString &kKeyOk = rpc_keys::Ok();
constexpr qint32 kCanStdIdMax = 0x7FF;
constexpr qint32 kCanExtIdMax = 0x1FFFFFFF;
}  // namespace

void RpcRegistry::registerCan()
{
    // 获取CAN总线状态
    dispatcher_->registerMethod(QStringLiteral("can.status"),
                                 [this](const QJsonObject &) {
        const bool hasCanBus = context_->canBus != nullptr;
        const bool canOpened = hasCanBus && context_->canBus->isOpened();
        const int txQueueSize = hasCanBus ? context_->canBus->txQueueSize() : 0;

        QJsonObject result{
            {kKeyOk, true},
            {QStringLiteral("interface"), context_->coreConfig.can.interface},
            {QStringLiteral("bitrate"), context_->coreConfig.can.bitrate},
            {QStringLiteral("opened"), canOpened},
            {QStringLiteral("txQueueSize"), txQueueSize}
        };

        // 添加详细的CAN状态信息
        if (hasCanBus) {
            result[QStringLiteral("resetAttemptCount")] = context_->canBus->resetAttemptCount();
            result[QStringLiteral("droppedFrameCount")] = context_->canBus->droppedFrameCount();
            result[QStringLiteral("resetInProgress")] = context_->canBus->isResetInProgress();

            const qint64 lastResetMs = context_->canBus->lastResetTimeMs();
            if (lastResetMs > 0) {
                result[QStringLiteral("lastResetTimeMs")] = static_cast<double>(lastResetMs);
                const qint64 now = QDateTime::currentMSecsSinceEpoch();
                result[QStringLiteral("timeSinceLastResetMs")] = static_cast<double>(now - lastResetMs);
            }

            const qint64 lastResetDuration = context_->canBus->lastResetDurationMs();
            if (lastResetDuration > 0) {
                result[QStringLiteral("lastResetDurationMs")] = static_cast<double>(lastResetDuration);
            }
        }

        // 如果CAN未打开，提供诊断信息
        if (!canOpened) {
            result[QStringLiteral("diagnostic")] = QStringLiteral(
                "CAN bus not opened. Please check:\n"
                "  1. CAN interface exists: ip link show %1\n"
                "  2. CAN interface is up: ip link set %1 up\n"
                "  3. Bitrate is set: canconfig %1 bitrate %2")
                .arg(context_->coreConfig.can.interface)
                .arg(context_->coreConfig.can.bitrate);
        }

        return result;
    });

    dispatcher_->registerMethod(QStringLiteral("can.send"),
                                 [this](const QJsonObject &params) {
        if (!context_->canBus)
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState,
                                        QStringLiteral("CAN not ready"));

        qint32 id = 0;
        QByteArray data;
        bool extended = false;

        if (!rpc::RpcHelpers::getBool(params, "extended", extended, false))
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterType,
                                        QStringLiteral("invalid extended"));
        if (!rpc::RpcHelpers::getI32InRange(params, "id", id, 0, INT_MAX))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing/invalid id"));
        const qint32 idMax = extended ? kCanExtIdMax : kCanStdIdMax;
        if (id > idMax) {
            return rpc::RpcHelpers::err(
                rpc::RpcError::BadParameterValue,
                QStringLiteral("invalid CAN id (extended=%1, max=0x%2)")
                    .arg(extended ? QStringLiteral("true") : QStringLiteral("false"))
                    .arg(QString::number(idMax, 16).toUpper()));
        }
        if (!rpc::RpcHelpers::getHexBytes(params, "dataHex", data))
            return rpc::RpcHelpers::err(rpc::RpcError::MissingParameter,
                                        QStringLiteral("missing/invalid dataHex"));
        if (data.size() > 8)
            return rpc::RpcHelpers::err(rpc::RpcError::BadParameterValue,
                                        QStringLiteral("payload too long (>8)"));

        const bool ok = context_->canBus->sendFrame(static_cast<quint32>(id), data, extended,
                                                     false);
        return QJsonObject{{kKeyOk, ok}};
    });
}

}  // namespace core
}  // namespace fanzhou
