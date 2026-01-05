#ifndef RPC_HELPERS_H
#define RPC_HELPERS_H

#include <QJsonObject>
#include <QJsonValue>
#include <QByteArray>
#include <QtGlobal>

namespace RpcHelpers {

bool getU8(const QJsonObject& p, const char* key, quint8& out);
bool getBool(const QJsonObject& p, const char* key, bool& out, bool def=false);
bool getI32(const QJsonObject& p, const char* key, qint32& out);
bool getString(const QJsonObject& p, const char* key, QString& out);
bool getHexBytes(const QJsonObject& p, const char* key, QByteArray& out);

QJsonObject ok(bool v=true);
QJsonObject err(int code, const QString& msg);

}

#endif // RPC_HELPERS_H
