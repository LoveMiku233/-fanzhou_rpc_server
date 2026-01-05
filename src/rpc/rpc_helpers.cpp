/**
 * @file rpc_helpers.cpp
 * @brief RPC helper functions implementation
 */

#include "rpc_helpers.h"

namespace fanzhou {
namespace rpc {
namespace RpcHelpers {

bool getU8(const QJsonObject &params, const char *key, quint8 &out)
{
    if (!params.contains(key) || !params.value(key).isDouble()) {
        return false;
    }
    const int value = params.value(key).toInt(-1);
    if (value < 0 || value > 255) {
        return false;
    }
    out = static_cast<quint8>(value);
    return true;
}

bool getBool(const QJsonObject &params, const char *key, bool &out,
             bool defaultValue)
{
    if (!params.contains(key)) {
        out = defaultValue;
        return true;
    }
    if (!params.value(key).isBool()) {
        return false;
    }
    out = params.value(key).toBool();
    return true;
}

bool getI32(const QJsonObject &params, const char *key, qint32 &out)
{
    if (!params.contains(key) || !params.value(key).isDouble()) {
        return false;
    }
    out = params.value(key).toInt();
    return true;
}

bool getString(const QJsonObject &params, const char *key, QString &out)
{
    if (!params.contains(key) || !params.value(key).isString()) {
        return false;
    }
    out = params.value(key).toString();
    return true;
}

bool getHexBytes(const QJsonObject &params, const char *key, QByteArray &out)
{
    if (!params.contains(key) || !params.value(key).isString()) {
        return false;
    }
    const QString str = params.value(key).toString().trimmed();
    const QByteArray bytes = QByteArray::fromHex(str.toLatin1());
    if (bytes.isEmpty() && !str.isEmpty()) {
        return false;
    }
    out = bytes;
    return true;
}

QJsonObject ok(bool value)
{
    return QJsonObject{{QStringLiteral("ok"), value}};
}

QJsonObject err(int code, const QString &message)
{
    return QJsonObject{
        {QStringLiteral("ok"), false},
        {QStringLiteral("code"), code},
        {QStringLiteral("message"), message}
    };
}

}  // namespace RpcHelpers
}  // namespace rpc
}  // namespace fanzhou
