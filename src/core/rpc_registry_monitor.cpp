/**
 * @file rpc_registry_monitor.cpp
 * @brief 系统资源监控RPC方法注册
 */

#include "rpc_registry.h"
#include "core_context.h"

#include "rpc/json_rpc_dispatcher.h"
#include "rpc/rpc_error_codes.h"
#include "rpc/rpc_helpers.h"
#include "utils/system_monitor.h"

#include <QJsonObject>

namespace fanzhou {
namespace core {

void RpcRegistry::registerMonitor()
{
    // 获取当前系统资源快照
    dispatcher_->registerMethod(QStringLiteral("sys.monitor.current"),
                                 [this](const QJsonObject &) {
        if (!context_->systemMonitor) {
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("System monitor not available"));
        }

        context_->systemMonitor->refresh();
        return context_->systemMonitor->currentSnapshotJson();
    });

    // 获取历史数据（用于图表）
    dispatcher_->registerMethod(QStringLiteral("sys.monitor.history"),
                                 [this](const QJsonObject &params) {
        if (!context_->systemMonitor) {
            return rpc::RpcHelpers::err(rpc::RpcError::InvalidState, QStringLiteral("System monitor not available"));
        }

        qint32 count = 60;
        rpc::RpcHelpers::getI32(params, "count", count);
        if (count <= 0) count = 60;
        if (count > 300) count = 300;

        return context_->systemMonitor->historySnapshotsJson(count);
    });
}

}  // namespace core
}  // namespace fanzhou
