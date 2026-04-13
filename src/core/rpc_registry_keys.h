/**
 * @file rpc_registry_keys.h
 * @brief RpcRegistry JSON键共享定义
 */

#ifndef FANZHOU_RPC_REGISTRY_KEYS_H
#define FANZHOU_RPC_REGISTRY_KEYS_H

#include <QString>

namespace fanzhou {
namespace core {
namespace rpc_keys {

inline const QString &Ok()
{
    static const QString v = QStringLiteral("ok");
    return v;
}
inline const QString &Ch() { static const QString v = QStringLiteral("ch"); return v; }
inline const QString &Channel() { static const QString v = QStringLiteral("channel"); return v; }
inline const QString &StatusByte() { static const QString v = QStringLiteral("statusByte"); return v; }
inline const QString &CurrentA() { static const QString v = QStringLiteral("currentA"); return v; }
inline const QString &Mode() { static const QString v = QStringLiteral("mode"); return v; }
inline const QString &PhaseLost() { static const QString v = QStringLiteral("phaseLost"); return v; }
inline const QString &Node() { static const QString v = QStringLiteral("node"); return v; }
inline const QString &Online() { static const QString v = QStringLiteral("online"); return v; }
inline const QString &AgeMs() { static const QString v = QStringLiteral("ageMs"); return v; }
inline const QString &Channels() { static const QString v = QStringLiteral("channels"); return v; }
inline const QString &Nodes() { static const QString v = QStringLiteral("nodes"); return v; }
inline const QString &JobId() { static const QString v = QStringLiteral("jobId"); return v; }
inline const QString &Queued() { static const QString v = QStringLiteral("queued"); return v; }
inline const QString &Success() { static const QString v = QStringLiteral("success"); return v; }
inline const QString &GroupId() { static const QString v = QStringLiteral("groupId"); return v; }
inline const QString &Name() { static const QString v = QStringLiteral("name"); return v; }
inline const QString &Devices() { static const QString v = QStringLiteral("devices"); return v; }
inline const QString &DeviceCount() { static const QString v = QStringLiteral("deviceCount"); return v; }
inline const QString &Groups() { static const QString v = QStringLiteral("groups"); return v; }
inline const QString &Total() { static const QString v = QStringLiteral("total"); return v; }
inline const QString &Accepted() { static const QString v = QStringLiteral("accepted"); return v; }
inline const QString &Missing() { static const QString v = QStringLiteral("missing"); return v; }
inline const QString &JobIds() { static const QString v = QStringLiteral("jobIds"); return v; }
inline const QString &Pending() { static const QString v = QStringLiteral("pending"); return v; }
inline const QString &Active() { static const QString v = QStringLiteral("active"); return v; }
inline const QString &LastJobId() { static const QString v = QStringLiteral("lastJobId"); return v; }
inline const QString &Message() { static const QString v = QStringLiteral("message"); return v; }
inline const QString &FinishedMs() { static const QString v = QStringLiteral("finishedMs"); return v; }
inline const QString &Id() { static const QString v = QStringLiteral("id"); return v; }
inline const QString &Action() { static const QString v = QStringLiteral("action"); return v; }
inline const QString &IntervalSec() { static const QString v = QStringLiteral("intervalSec"); return v; }
inline const QString &Enabled() { static const QString v = QStringLiteral("enabled"); return v; }
inline const QString &AutoStart() { static const QString v = QStringLiteral("autoStart"); return v; }
inline const QString &Attached() { static const QString v = QStringLiteral("attached"); return v; }
inline const QString &Running() { static const QString v = QStringLiteral("running"); return v; }
inline const QString &Strategies() { static const QString v = QStringLiteral("strategies"); return v; }

}  // namespace rpc_keys
}  // namespace core
}  // namespace fanzhou

#endif  // FANZHOU_RPC_REGISTRY_KEYS_H
