/**
 * @file device_tcp_rpc.cpp
 * @brief Device TCP RPC method registration helpers
 */

#include "device_tcp_rpc.h"

#include "device_tcp_server.h"
#include "json_rpc_dispatcher.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <functional>

namespace fanzhou {
namespace rpc {

namespace {
constexpr int kErrInvalidParams = 400;
constexpr int kErrNotConnected = 503;
constexpr int kDefaultWaitMs = 1500;
constexpr int kMinWaitMs = 0;
constexpr int kMaxWaitMs = 10000;

QJsonObject makeErrorResponse(const QString &error, int code)
{
    return QJsonObject{
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), error},
        {QStringLiteral("message"), error},
        {QStringLiteral("code"), code}
    };
}

int normalizedWaitMs(const QJsonObject &params, int defaultMs)
{
    const int waitMs = params.value(QStringLiteral("waitMs")).toInt(defaultMs);
    if (waitMs < kMinWaitMs) {
        return kMinWaitMs;
    }
    if (waitMs > kMaxWaitMs) {
        return kMaxWaitMs;
    }
    return waitMs;
}

bool parseTargetDev(const QJsonObject &params, int *outDev, QJsonObject *outErr)
{
    if (!outDev) {
        if (outErr) {
            *outErr = makeErrorResponse(QStringLiteral("internal error: outDev is null"), kErrInvalidParams);
        }
        return false;
    }
    const int dev = params.value(QStringLiteral("dev")).toInt(-1);
    if (dev < 1 || dev > 255) {
        if (outErr) {
            *outErr = makeErrorResponse(QStringLiteral("missing or invalid dev (1..255)"), kErrInvalidParams);
        }
        return false;
    }
    *outDev = dev;
    return true;
}

QJsonObject makeSendResult(bool ok,
                           int targetDev,
                           const QJsonObject &payload,
                           const QJsonObject &response = QJsonObject(),
                           const QString &error = QString())
{
    QJsonObject result{
        {QStringLiteral("ok"), ok},
        {QStringLiteral("targetDev"), targetDev},
        {QStringLiteral("payload"), payload}
    };
    if (!response.isEmpty()) {
        result[QStringLiteral("response")] = response;
    }
    if (!ok) {
        result[QStringLiteral("error")] = error;
        result[QStringLiteral("message")] = error;
        result[QStringLiteral("code")] = kErrNotConnected;
    }
    return result;
}

bool parseFlexibleBool(const QJsonValue &value, bool *out)
{
    if (!out) {
        return false;
    }
    if (value.isBool()) {
        *out = value.toBool();
        return true;
    }
    if (value.isDouble()) {
        const int v = value.toInt(-1);
        if (v == 0 || v == 1) {
            *out = (v == 1);
            return true;
        }
    }
    if (value.isString()) {
        const QString s = value.toString().trimmed().toLower();
        if (s == QStringLiteral("true") || s == QStringLiteral("1")) {
            *out = true;
            return true;
        }
        if (s == QStringLiteral("false") || s == QStringLiteral("0")) {
            *out = false;
            return true;
        }
    }
    return false;
}

QJsonObject buildTcpPayload(const QJsonObject &params, const QString &cmd, const QJsonObject &args)
{
    QJsonObject payload;
    if (params.contains(QStringLiteral("id"))) {
        payload[QStringLiteral("id")] = params.value(QStringLiteral("id"));
    } else {
        payload[QStringLiteral("id")] = static_cast<double>(QDateTime::currentMSecsSinceEpoch());
    }
    payload[QStringLiteral("cmd")] = cmd;
    payload[QStringLiteral("args")] = args;
    return payload;
}

QJsonObject validateCfgSetArgs(const QJsonObject &args)
{
    if (args.contains(QStringLiteral("device_addr"))) {
        const int deviceAddr = args.value(QStringLiteral("device_addr")).toInt(-1);
        if (deviceAddr < 1 || deviceAddr > 255) {
            return makeErrorResponse(QStringLiteral("invalid args: device_addr out of range (1..255)"),
                                     kErrInvalidParams);
        }
    }

    if (args.contains(QStringLiteral("comm_mode"))) {
        const int commMode = args.value(QStringLiteral("comm_mode")).toInt(-1);
        if (commMode < 0 || commMode > 3) {
            return makeErrorResponse(QStringLiteral("invalid args: comm_mode out of range (0..3)"),
                                     kErrInvalidParams);
        }
    }

    if (args.contains(QStringLiteral("network_mode"))) {
        const int networkMode = args.value(QStringLiteral("network_mode")).toInt(-1);
        if (networkMode < 0 || networkMode > 1) {
            return makeErrorResponse(QStringLiteral("invalid args: network_mode out of range (0..1)"),
                                     kErrInvalidParams);
        }
    }

    if (args.contains(QStringLiteral("tcp_gateway"))) {
        if (!args.value(QStringLiteral("tcp_gateway")).isObject()) {
            return makeErrorResponse(QStringLiteral("invalid args: tcp_gateway must be object"),
                                     kErrInvalidParams);
        }
        const QJsonObject gw = args.value(QStringLiteral("tcp_gateway")).toObject();
        if (gw.contains(QStringLiteral("ip"))) {
            if (!gw.value(QStringLiteral("ip")).isArray()) {
                return makeErrorResponse(QStringLiteral("invalid args: tcp_gateway.ip must be [x,x,x,x]"),
                                         kErrInvalidParams);
            }
            const QJsonArray ip = gw.value(QStringLiteral("ip")).toArray();
            if (ip.size() != 4) {
                return makeErrorResponse(QStringLiteral("invalid args: tcp_gateway.ip length must be 4"),
                                         kErrInvalidParams);
            }
            for (int i = 0; i < ip.size(); ++i) {
                const int octet = ip.at(i).toInt(-1);
                if (octet < 0 || octet > 255) {
                    return makeErrorResponse(QStringLiteral("invalid args: tcp_gateway.ip octet out of range (0..255)"),
                                             kErrInvalidParams);
                }
            }
        }
        if (gw.contains(QStringLiteral("port"))) {
            const int port = gw.value(QStringLiteral("port")).toInt(-1);
            if (port < 1 || port > 65535) {
                return makeErrorResponse(QStringLiteral("invalid args: tcp_gateway.port out of range (1..65535)"),
                                         kErrInvalidParams);
            }
        }
        if (gw.contains(QStringLiteral("enable"))) {
            const int enable = gw.value(QStringLiteral("enable")).toInt(-1);
            if (enable != 0 && enable != 1) {
                return makeErrorResponse(QStringLiteral("invalid args: tcp_gateway.enable must be 0 or 1"),
                                         kErrInvalidParams);
            }
        }
    }

    if (args.contains(QStringLiteral("save"))) {
        bool parsed = false;
        if (!parseFlexibleBool(args.value(QStringLiteral("save")), &parsed)) {
            return makeErrorResponse(QStringLiteral("invalid args: save must be true/false or 0/1"),
                                     kErrInvalidParams);
        }
    }

    return QJsonObject();
}

QJsonObject sendTcpCommand(DeviceTcpServer *deviceServer,
                           const QJsonObject &payload,
                           int targetDev,
                           int waitMs = 0)
{
    if (!deviceServer) {
        return makeErrorResponse(QStringLiteral("device tcp server not initialized"), kErrNotConnected);
    }
    QString sendError;
    QJsonObject response;
    bool ok = false;
    if (waitMs > 0) {
        ok = deviceServer->sendCommandAndWait(payload, targetDev, waitMs, &response, &sendError);
    } else {
        ok = deviceServer->sendCommand(payload, targetDev, &sendError);
    }
    return makeSendResult(ok, targetDev, payload, response, sendError);
}

bool extractCfgSetArgs(const QJsonObject &params,
                       const QString &errorPrefix,
                       QJsonObject *outArgs,
                       QJsonObject *outErr)
{
    QJsonObject args;
    if (params.contains(QStringLiteral("args"))) {
        if (!params.value(QStringLiteral("args")).isObject()) {
            if (outErr) {
                *outErr = makeErrorResponse(QStringLiteral("args must be object"), kErrInvalidParams);
            }
            return false;
        }
        args = params.value(QStringLiteral("args")).toObject();
    } else {
        for (auto it = params.begin(); it != params.end(); ++it) {
            if (it.key() == QStringLiteral("id") || it.key() == QStringLiteral("dev")) {
                continue;
            }
            args.insert(it.key(), it.value());
        }
    }
    if (args.isEmpty()) {
        if (outErr) {
            *outErr = makeErrorResponse(QStringLiteral("missing args for %1").arg(errorPrefix), kErrInvalidParams);
        }
        return false;
    }
    const QJsonObject err = validateCfgSetArgs(args);
    if (!err.isEmpty()) {
        if (outErr) {
            *outErr = err;
        }
        return false;
    }
    if (outArgs) {
        *outArgs = args;
    }
    return true;
}

}  // namespace

void registerDeviceTcpMethods(JsonRpcDispatcher *dispatcher, DeviceTcpServer *deviceServer)
{
    if (!dispatcher) {
        return;
    }

    dispatcher->registerMethod(QStringLiteral("device.tcp.status"),
                               [deviceServer](const QJsonObject &) {
        if (!deviceServer) {
            return makeErrorResponse(QStringLiteral("device tcp server not initialized"), kErrNotConnected);
        }
        return deviceServer->connectionStatus();
    });

    dispatcher->registerMethod(QStringLiteral("device.tcp.send"),
                               [deviceServer](const QJsonObject &params) {
        const QString cmd = params.value(QStringLiteral("cmd")).toString();
        if (cmd.isEmpty()) {
            return makeErrorResponse(QStringLiteral("missing cmd"), kErrInvalidParams);
        }

        QJsonObject args;
        if (params.contains(QStringLiteral("args"))) {
            if (!params.value(QStringLiteral("args")).isObject()) {
                return makeErrorResponse(QStringLiteral("args must be object"), kErrInvalidParams);
            }
            args = params.value(QStringLiteral("args")).toObject();
        }
        const QJsonObject payload = buildTcpPayload(params, cmd, args);
        const int targetDev = params.value(QStringLiteral("dev")).toInt(-1);
        const int waitMs = normalizedWaitMs(params, 0);
        return sendTcpCommand(deviceServer, payload, targetDev, waitMs);
    });

    auto registerCfgGet = [dispatcher, deviceServer](const QString &rpcMethod, const QString &cmd) {
        dispatcher->registerMethod(rpcMethod, [deviceServer, cmd](const QJsonObject &params) {
            int targetDev = -1;
            QJsonObject err;
            if (!parseTargetDev(params, &targetDev, &err)) {
                return err;
            }
            const QJsonObject payload = buildTcpPayload(params, cmd, QJsonObject{});
            const int waitMs = normalizedWaitMs(params, kDefaultWaitMs);
            return sendTcpCommand(deviceServer, payload, targetDev, waitMs);
        });
    };

    auto registerCfgSet = [dispatcher, deviceServer](const QString &rpcMethod,
                                                     const QString &cmd,
                                                     const QString &errorPrefix) {
        dispatcher->registerMethod(rpcMethod, [deviceServer, cmd, errorPrefix](const QJsonObject &params) {
            int targetDev = -1;
            QJsonObject err;
            if (!parseTargetDev(params, &targetDev, &err)) {
                return err;
            }

            QJsonObject args;
            QJsonObject argsErr;
            if (!extractCfgSetArgs(params, errorPrefix, &args, &argsErr)) {
                return argsErr;
            }

            const QJsonObject payload = buildTcpPayload(params, cmd, args);
            const int waitMs = normalizedWaitMs(params, kDefaultWaitMs);
            return sendTcpCommand(deviceServer, payload, targetDev, waitMs);
        });
    };

    auto registerSimpleCommand = [dispatcher, deviceServer](const QString &rpcMethod,
                                                            const QString &cmd,
                                                            int defaultWaitMs,
                                                            const std::function<QJsonObject(const QJsonObject &, QJsonObject *)> &buildArgs) {
        dispatcher->registerMethod(rpcMethod, [deviceServer, cmd, defaultWaitMs, buildArgs](const QJsonObject &params) {
            int targetDev = -1;
            QJsonObject err;
            if (!parseTargetDev(params, &targetDev, &err)) {
                return err;
            }

            QJsonObject args;
            if (buildArgs) {
                args = buildArgs(params, &err);
                if (!err.isEmpty()) {
                    return err;
                }
            }

            const QJsonObject payload = buildTcpPayload(params, cmd, args);
            const int waitMs = normalizedWaitMs(params, defaultWaitMs);
            return sendTcpCommand(deviceServer, payload, targetDev, waitMs);
        });
    };

    registerCfgGet(QStringLiteral("device.tcp.cfg.get"), QStringLiteral("cfg.get"));
    registerCfgGet(QStringLiteral("device.tcp.config.get"), QStringLiteral("config.get"));
    registerCfgSet(QStringLiteral("device.tcp.cfg.set"), QStringLiteral("cfg.set"), QStringLiteral("cfg.set"));
    registerCfgSet(QStringLiteral("device.tcp.config.set"), QStringLiteral("config.set"), QStringLiteral("config.set"));

    registerSimpleCommand(QStringLiteral("device.tcp.ping"), QStringLiteral("ping"), kDefaultWaitMs,
                          [](const QJsonObject &, QJsonObject *) { return QJsonObject{}; });
    registerSimpleCommand(QStringLiteral("device.tcp.status.get"), QStringLiteral("status"), kDefaultWaitMs,
                          [](const QJsonObject &, QJsonObject *) { return QJsonObject{}; });
    registerSimpleCommand(QStringLiteral("device.tcp.relay.stopall"), QStringLiteral("relay.stopall"), kDefaultWaitMs,
                          [](const QJsonObject &, QJsonObject *) { return QJsonObject{}; });
    registerSimpleCommand(
        QStringLiteral("device.tcp.relay.set"), QStringLiteral("relay.set"), kDefaultWaitMs,
        [](const QJsonObject &params, QJsonObject *err) {
            int ch = params.value(QStringLiteral("ch")).toInt(-1);
            int state = params.value(QStringLiteral("state")).toInt(-1);
            if (params.contains(QStringLiteral("args")) && params.value(QStringLiteral("args")).isObject()) {
                const QJsonObject argsObj = params.value(QStringLiteral("args")).toObject();
                if (argsObj.contains(QStringLiteral("ch"))) {
                    ch = argsObj.value(QStringLiteral("ch")).toInt(-1);
                }
                if (argsObj.contains(QStringLiteral("state"))) {
                    state = argsObj.value(QStringLiteral("state")).toInt(-1);
                }
            }
            if (ch < 1 || ch > 4) {
                if (err) *err = makeErrorResponse(QStringLiteral("invalid ch (1..4)"), kErrInvalidParams);
                return QJsonObject{};
            }
            if (state != 0 && state != 1) {
                if (err) *err = makeErrorResponse(QStringLiteral("invalid state (0|1)"), kErrInvalidParams);
                return QJsonObject{};
            }
            return QJsonObject{
                {QStringLiteral("ch"), ch},
                {QStringLiteral("state"), state}
            };
        });
}

}  // namespace rpc
}  // namespace fanzhou
