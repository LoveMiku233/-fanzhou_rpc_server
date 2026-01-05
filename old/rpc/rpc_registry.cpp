#include "rpc_registry.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QDateTime>

#include "rpc/json_rpc_dispatcher.h"
#include "rpc/rpc_helpers.h"
#include "rpc/rpc_error_codes.h"
#include "rpc/rpc_registry.h"

#include "comm/comm_can.h"

#include "device/can/device_relay_can_f427.h"

#include "core/core_context.h"
#include "config/system_settings.h"



RpcRegistry::RpcRegistry(CoreContext* ctx, JsonRpcDispatcher* disp, QObject* parent)
    : QObject(parent), ctx_(ctx), disp_(disp) {}


void RpcRegistry::registerAll()
{
    registerBase();
    registerSys();
    registerCan();
    registerRelay();
    registerGroup();
    registerAuto();
}

// register base methods
void RpcRegistry::registerBase()
{
    disp_->registerMethod("rpc.list", [&](const QJsonObject&){
        QJsonArray arr;
        for (const auto& m : disp_->methods()) arr.append(m);
        return QJsonValue(arr);
    });

    disp_->registerMethod("rpc.ping", [&](const QJsonObject&){
        return QJsonObject{{"ok", true}};
    });

    disp_->registerMethod("echo", [&](const QJsonObject& params){
        return QJsonValue(params);
    });
    // TODO:
}

void RpcRegistry::registerSys()
{
    disp_->registerMethod("sys.can.setBitrate", [&](const QJsonObject& params){
        // ifname
        QString ifname;
        qint32 bitrate = 0;
        bool ts = false;

        // get param
        if (!RpcHelpers::getString(params, "ifname", ifname))
            return RpcHelpers::err(RpcError::MissingParameter, "missing ifname");
        if (!RpcHelpers::getI32(params, "bitrate", bitrate) || bitrate <= 0)
            return RpcHelpers::err(RpcError::BadParameterValue, "missing/invalid bitrate");
        if (!RpcHelpers::getBool(params, "tripleSampling", ts, false))
            return RpcHelpers::err(RpcError::BadParameterType, "invalid tripleSampling");

        if (!ctx_->sys) return RpcHelpers::err(RpcError::InvalidState, "SystemSettings not ready");
        const bool ok = ctx_->sys->setCanBitrate(ifname, bitrate, ts);
        return QJsonObject{{"ok", ok}};
    });

    // sys.can.dump.start: {ifname:"can0"}
    disp_->registerMethod("sys.can.dump.start", [&](const QJsonObject& params){
        QString ifname;
        if (!RpcHelpers::getString(params, "ifname", ifname))
            return RpcHelpers::err(RpcError::MissingParameter, "missing ifname");
        const bool ok = ctx_->sys && ctx_->sys->startCanDump(ifname);
        return QJsonObject{{"ok", ok}};
    });

    // sys.can.dump.stop: {}
    disp_->registerMethod("sys.can.dump.stop", [&](const QJsonObject&){
        if (ctx_->sys) ctx_->sys->stopCanDump();
        return RpcHelpers::ok(true);
    });
}


void RpcRegistry::registerCan()
{
    disp_->registerMethod("can.send", [&](const QJsonObject& params){
        if (!ctx_->canBus) return RpcHelpers::err(RpcError::InvalidState, "CAN not ready");

        qint32 id = 0;
        QByteArray data;
        bool ext = false;

        if (!RpcHelpers::getI32(params, "id", id) || id < 0)
            return RpcHelpers::err(RpcError::MissingParameter, "missing/invalid id");
        if (!RpcHelpers::getHexBytes(params, "dataHex", data))
            return RpcHelpers::err(RpcError::MissingParameter, "missing/invalid dataHex");
        if (!RpcHelpers::getBool(params, "extended", ext, false))
            return RpcHelpers::err(RpcError::BadParameterType, "invalid extended");
        if (data.size() > 8)
            return RpcHelpers::err(RpcError::BadParameterValue, "payload too long (>8)");

        const bool ok = ctx_->canBus->sendFrame(quint32(id), data, ext, false);
        return QJsonObject{{"ok", ok}};
    });
}


void RpcRegistry::registerRelay()
{
    // relay.control: {node:2, ch:0, action:"fwd"}
    disp_->registerMethod("relay.control", [&](const QJsonObject& params){
        quint8 node=0, ch=0;
        QString actionStr;

        if (!RpcHelpers::getU8(params, "node", node))
            return RpcHelpers::err(RpcError::MissingParameter, "missing/invalid node");
        if (!RpcHelpers::getU8(params, "ch", ch) || ch > 3)
            return RpcHelpers::err(RpcError::BadParameterValue, "missing/invalid ch(0..3)");
        if (!RpcHelpers::getString(params, "action", actionStr))
            return RpcHelpers::err(RpcError::MissingParameter, "missing action");

        bool okAction=false;
        const auto action = ctx_->parseAction(actionStr, &okAction);
        if (!okAction)
            return RpcHelpers::err(RpcError::BadParameterValue, "invalid action (stop/fwd/rev)");

        const auto res = ctx_->enqueueControl(node, ch, action, "rpc:relay.control");
        if (!res.accepted) {
            return RpcHelpers::err(RpcError::BadParameterValue, res.error);
        }

        QJsonObject obj{
            {"ok", true},
            {"jobId", QString::number(res.jobId)},
            {"queued", !res.executedImmediately}
        };
        if (res.executedImmediately) obj["success"] = res.success;
        return obj;
    });

    // relay.query: {node:2, ch:0}
    disp_->registerMethod("relay.query", [&](const QJsonObject& params){
        quint8 node = 0, ch = 0;

        if (!RpcHelpers::getU8(params, "node", node))
            return RpcHelpers::err(RpcError::MissingParameter, "missing/invalid node");
        if (!RpcHelpers::getU8(params, "ch", ch) || ch > 3)
            return RpcHelpers::err(RpcError::BadParameterValue, "missing/invalid ch(0..3)");

        auto* dev = ctx_->relays.value(node, nullptr);
        if (!dev) return RpcHelpers::err(RpcError::BadParameterValue, "unknown node");

        return QJsonObject{{"ok", dev->query(ch)}};
    });

    // relay.status: {node:2, ch:0}
    disp_->registerMethod("relay.status", [&](const QJsonObject& params){
        quint8 node=0, ch=0;
        if (!RpcHelpers::getU8(params, "node", node))
            return RpcHelpers::err(RpcError::MissingParameter, "missing/invalid node");
        if (!RpcHelpers::getU8(params, "ch", ch) || ch > 3)
            return RpcHelpers::err(RpcError::BadParameterValue, "missing/invalid ch(0..3)");

        auto* dev = ctx_->relays.value(node, nullptr);
        if (!dev) return RpcHelpers::err(RpcError::BadParameterValue, "unknown node");

        const auto st = dev->lastStatus(ch);
        return QJsonObject{
            {"ok", true},
            {"channel", int(st.channel)},
            {"statusByte", int(st.statusByte)},
            {"currentA", double(st.currentA)},
            {"mode", int(RelayCanProtocol::modeBits(st.statusByte))},
            {"phaseLost", RelayCanProtocol::phaseLost(st.statusByte)},
        };
    });

    disp_->registerMethod("relay.statusAll", [&](const QJsonObject& p){
        quint8 node = 0;
        if (!RpcHelpers::getU8(p, "node", node))
            return RpcHelpers::err(RpcError::MissingParameter, "missing/invalid node");

        auto* dev = ctx_->relays.value(node, nullptr);
        if (!dev)
            return RpcHelpers::err(RpcError::BadParameterValue, "unknown node");

        QJsonArray channels;
        for (quint8 ch = 0; ch < 4; ++ch) {
            const auto st = dev->lastStatus(ch);

            QJsonObject o;
            o["ch"] = int(ch);
            o["channel"] = int(st.channel);
            o["statusByte"] = int(st.statusByte);
            o["currentA"] = double(st.currentA);
            o["mode"] = int(RelayCanProtocol::modeBits(st.statusByte));
            o["phaseLost"] = RelayCanProtocol::phaseLost(st.statusByte);

            channels.append(o);
        }

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const qint64 last = dev->lastSeenMs();
        const qint64 ageMs = (last > 0) ? (now - last) : (std::numeric_limits<qint64>::max());

        const bool online = (ageMs <= 30000);

        return QJsonObject{
            {"ok", true},
            {"node", int(node)},
            {"online", online},
            {"ageMs", double(ageMs)},
            {"channels", channels},
        };
    });

    // relay.node: {}
    disp_->registerMethod("relay.nodes", [&](const QJsonObject&){
        QJsonArray arr;
        for (auto it = ctx_->relays.begin(); it != ctx_->relays.end(); ++it) {
            arr.append(int(it.key()));
        }
        return QJsonObject{{"ok", true}, {"nodes", arr}};
    });
}

void RpcRegistry::registerGroup()
{
    // group.list: {}
    disp_->registerMethod("group.list", [&](const QJsonObject&){
        QJsonArray arr;
        for (auto it = ctx_->deviceGroups.begin(); it != ctx_->deviceGroups.end(); ++it) {
            QJsonObject o;
            o["groupId"] = it.key();
            o["name"] = ctx_->groupNames.value(it.key(), "");
            
            QJsonArray devices;
            for (quint8 node : it.value()) {
                devices.append(int(node));
            }
            o["devices"] = devices;
            o["deviceCount"] = it.value().size();
            
            arr.append(o);
        }
        return QJsonObject{{"ok", true}, {"groups", arr}};
    });

    // group.create: {groupId:1, name:"Group1"}
    disp_->registerMethod("group.create", [&](const QJsonObject& params){
        qint32 groupId = 0;
        QString name;
        
        if (!RpcHelpers::getI32(params, "groupId", groupId) || groupId <= 0)
            return RpcHelpers::err(RpcError::MissingParameter, "missing/invalid groupId");
        if (!RpcHelpers::getString(params, "name", name))
            return RpcHelpers::err(RpcError::MissingParameter, "missing name");
        
        QString err;
        if (!ctx_->createGroup(groupId, name, &err))
            return RpcHelpers::err(RpcError::BadParameterValue, err);
        return QJsonObject{{"ok", true}, {"groupId", groupId}};
    });

    // group.delete: {groupId:1}
    disp_->registerMethod("group.delete", [&](const QJsonObject& params){
        qint32 groupId = 0;
        
        if (!RpcHelpers::getI32(params, "groupId", groupId))
            return RpcHelpers::err(RpcError::MissingParameter, "missing groupId");
        
        QString err;
        if (!ctx_->deleteGroup(groupId, &err))
            return RpcHelpers::err(RpcError::BadParameterValue, err);
        return QJsonObject{{"ok", true}};
    });

    // group.addDevice: {groupId:1, node:2}
    disp_->registerMethod("group.addDevice", [&](const QJsonObject& params){
        qint32 groupId = 0;
        quint8 node = 0;
        
        if (!RpcHelpers::getI32(params, "groupId", groupId))
            return RpcHelpers::err(RpcError::MissingParameter, "missing groupId");
        if (!RpcHelpers::getU8(params, "node", node))
            return RpcHelpers::err(RpcError::MissingParameter, "missing/invalid node");
        
        QString err;
        if (!ctx_->addDeviceToGroup(groupId, node, &err))
            return RpcHelpers::err(RpcError::BadParameterValue, err);
        return QJsonObject{{"ok", true}};
    });

    // group.removeDevice: {groupId:1, node:2}
    disp_->registerMethod("group.removeDevice", [&](const QJsonObject& params){
        qint32 groupId = 0;
        quint8 node = 0;
        
        if (!RpcHelpers::getI32(params, "groupId", groupId))
            return RpcHelpers::err(RpcError::MissingParameter, "missing groupId");
        if (!RpcHelpers::getU8(params, "node", node))
            return RpcHelpers::err(RpcError::MissingParameter, "missing/invalid node");
        
        QString err;
        if (!ctx_->removeDeviceFromGroup(groupId, node, &err))
            return RpcHelpers::err(RpcError::BadParameterValue, err);
        return QJsonObject{{"ok", true}};
    });

    // group.control: {groupId:1, ch:0, action:"fwd"}
    disp_->registerMethod("group.control", [&](const QJsonObject& params){
        qint32 groupId = 0;
        quint8 ch = 0;
        QString actionStr;
        
        if (!RpcHelpers::getI32(params, "groupId", groupId))
            return RpcHelpers::err(RpcError::MissingParameter, "missing groupId");
        if (!RpcHelpers::getU8(params, "ch", ch) || ch > 3)
            return RpcHelpers::err(RpcError::BadParameterValue, "missing/invalid ch(0..3)");
        if (!RpcHelpers::getString(params, "action", actionStr))
            return RpcHelpers::err(RpcError::MissingParameter, "missing action");
        
        bool okAction = false;
        const auto action = ctx_->parseAction(actionStr, &okAction);
        if (!okAction)
            return RpcHelpers::err(RpcError::BadParameterValue, "invalid action (stop/fwd/rev)");
        
        if (!ctx_->deviceGroups.contains(groupId))
            return RpcHelpers::err(RpcError::BadParameterValue, "group not found");

        const auto stats = ctx_->queueGroupControl(groupId, ch, action, "rpc:group.control");
        QJsonArray jobs;
        for (quint64 id : stats.jobIds) jobs.append(QString::number(id));

        return QJsonObject{
            {"ok", true},
            {"total", stats.total},
            {"accepted", stats.accepted},
            {"missing", stats.missing},
            {"jobIds", jobs}
        };
    });
}

void RpcRegistry::registerAuto()
{
    disp_->registerMethod("control.queue.status", [&](const QJsonObject&){
        const auto snap = ctx_->queueSnapshot();
        return QJsonObject{
            {"ok", true},
            {"pending", snap.pending},
            {"active", snap.active},
            {"lastJobId", snap.lastJobId ? QJsonValue(QString::number(snap.lastJobId)) : QJsonValue()}
        };
    });

    disp_->registerMethod("control.queue.result", [&](const QJsonObject& params){
        if (!params.contains("jobId"))
            return RpcHelpers::err(RpcError::MissingParameter, "missing jobId");

        quint64 jobId = 0;
        bool okId = false;
        const auto jobVal = params.value("jobId");
        if (jobVal.isString()) {
            jobId = jobVal.toString().toULongLong(&okId);
        } else if (jobVal.isDouble()) {
            const double v = jobVal.toDouble(&okId);
            if (okId && v >= 0) jobId = static_cast<quint64>(v);
        }

        if (!okId)
            return RpcHelpers::err(RpcError::BadParameterType, "jobId must be integer or string");
        if (jobId == 0)
            return RpcHelpers::err(RpcError::BadParameterValue, "jobId must be a positive integer identifier");

        const auto res = ctx_->jobResult(jobId);
        return QJsonObject{
            {"jobId", QString::number(jobId)},
            {"ok", res.ok},
            {"message", res.message},
            {"finishedMs", double(res.finishedMs)}
        };
    });

    disp_->registerMethod("auto.strategy.list", [&](const QJsonObject&){
        QJsonArray arr;
        const auto states = ctx_->strategyStates();
        for (const auto& st : states) {
            QJsonObject o;
            o["id"] = st.cfg.strategyId;
            o["name"] = st.cfg.name;
            o["groupId"] = st.cfg.groupId;
            o["channel"] = int(st.cfg.channel);
            o["action"] = st.cfg.action;
            o["intervalSec"] = st.cfg.intervalSec;
            o["enabled"] = st.cfg.enabled;
            o["autoStart"] = st.cfg.autoStart;
            o["attached"] = st.attached;
            o["running"] = st.running;
            arr.append(o);
        }
        return QJsonObject{{"ok", true}, {"strategies", arr}};
    });

    disp_->registerMethod("auto.strategy.enable", [&](const QJsonObject& params){
        qint32 id = 0;
        bool enabled = true;
        if (!RpcHelpers::getI32(params, "id", id))
            return RpcHelpers::err(RpcError::MissingParameter, "missing id");
        if (!params.contains("enabled"))
            return RpcHelpers::err(RpcError::MissingParameter, "missing enabled");
        if (!RpcHelpers::getBool(params, "enabled", enabled, true))
            return RpcHelpers::err(RpcError::BadParameterType, "invalid enabled");
        if (!ctx_->setStrategyEnabled(id, enabled))
            return RpcHelpers::err(RpcError::BadParameterValue, "strategy not found");
        return QJsonObject{{"ok", true}};
    });

    disp_->registerMethod("auto.strategy.trigger", [&](const QJsonObject& params){
        qint32 id = 0;
        if (!RpcHelpers::getI32(params, "id", id))
            return RpcHelpers::err(RpcError::MissingParameter, "missing id");
        if (!ctx_->triggerStrategy(id))
            return RpcHelpers::err(RpcError::BadParameterValue, "strategy not found or not attached");
        return QJsonObject{{"ok", true}};
    });
}
