/**
 * @file rpc_helpers.h
 * @brief RPC helper functions
 *
 * Provides utilities for parsing JSON-RPC parameters and building responses.
 */

#ifndef FANZHOU_RPC_HELPERS_H
#define FANZHOU_RPC_HELPERS_H

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QtGlobal>

namespace fanzhou {
namespace rpc {

/**
 * @brief RPC helper utilities
 */
namespace RpcHelpers {

/**
 * @brief Extract unsigned 8-bit integer from JSON object
 * @param params JSON parameters object
 * @param key Parameter key
 * @param out Output value
 * @return True if successful
 */
bool getU8(const QJsonObject &params, const char *key, quint8 &out);

/**
 * @brief Extract boolean from JSON object
 * @param params JSON parameters object
 * @param key Parameter key
 * @param out Output value
 * @param defaultValue Default value if key not present
 * @return True if successful
 */
bool getBool(const QJsonObject &params, const char *key, bool &out,
             bool defaultValue = false);

/**
 * @brief Extract 32-bit integer from JSON object
 * @param params JSON parameters object
 * @param key Parameter key
 * @param out Output value
 * @return True if successful
 */
bool getI32(const QJsonObject &params, const char *key, qint32 &out);

/**
 * @brief Extract string from JSON object
 * @param params JSON parameters object
 * @param key Parameter key
 * @param out Output value
 * @return True if successful
 */
bool getString(const QJsonObject &params, const char *key, QString &out);

/**
 * @brief Extract hex-encoded bytes from JSON object
 * @param params JSON parameters object
 * @param key Parameter key
 * @param out Output byte array
 * @return True if successful
 */
bool getHexBytes(const QJsonObject &params, const char *key, QByteArray &out);

/**
 * @brief Create success response object
 * @param value Success flag
 * @return JSON response object
 */
QJsonObject ok(bool value = true);

/**
 * @brief Create error response object
 * @param code Error code
 * @param message Error message
 * @return JSON error response object
 */
QJsonObject err(int code, const QString &message);

}  // namespace RpcHelpers
}  // namespace rpc
}  // namespace fanzhou

#endif  // FANZHOU_RPC_HELPERS_H
