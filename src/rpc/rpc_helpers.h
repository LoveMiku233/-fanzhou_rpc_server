/**
 * @file rpc_helpers.h
 * @brief RPC辅助函数
 *
 * 提供解析JSON-RPC参数和构建响应的工具函数。
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
 * @brief RPC辅助工具
 */
namespace RpcHelpers {

/**
 * @brief 从JSON对象提取无符号8位整数
 *
 * 支持数字类型和字符串类型的值，字符串会尝试转换为整数。
 *
 * @param params JSON参数对象
 * @param key 参数键
 * @param out 输出值
 * @return 成功返回true，失败（缺少键、类型不支持或超出范围）返回false
 */
bool getU8(const QJsonObject &params, const char *key, quint8 &out);

/**
 * @brief 从JSON对象提取布尔值
 * @param params JSON参数对象
 * @param key 参数键
 * @param out 输出值
 * @param defaultValue 键不存在时的默认值
 * @return 成功返回true
 */
bool getBool(const QJsonObject &params, const char *key, bool &out,
             bool defaultValue = false);

/**
 * @brief 从JSON对象提取Double整数
 *
 * 支持数字类型和字符串类型的值，字符串会尝试转换为Double。
 *
 * @param params JSON参数对象
 * @param key 参数键
 * @param out 输出值
 * @return 成功返回true，失败（缺少键或类型不支持）返回false
 */
bool getDouble(const QJsonObject &params, const char *key, double &out);

/**
 * @brief 从JSON对象提取32位整数
 *
 * 支持数字类型和字符串类型的值，字符串会尝试转换为整数。
 *
 * @param params JSON参数对象
 * @param key 参数键
 * @param out 输出值
 * @return 成功返回true，失败（缺少键或类型不支持）返回false
 */
bool getI32(const QJsonObject &params, const char *key, qint32 &out);

/**
 * @brief 从JSON对象提取字符串
 * @param params JSON参数对象
 * @param key 参数键
 * @param out 输出值
 * @return 成功返回true
 */
bool getString(const QJsonObject &params, const QString &key, QString &out);

/**
 * @brief 从JSON对象提取十六进制编码字节
 * @param params JSON参数对象
 * @param key 参数键
 * @param out 输出字节数组
 * @return 成功返回true
 */
bool getHexBytes(const QJsonObject &params, const char *key, QByteArray &out);

/**
 * @brief 创建成功响应对象
 * @param value 成功标志
 * @return JSON响应对象
 */
QJsonObject ok(bool value = true);

/**
 * @brief 创建错误响应对象
 * @param code 错误码
 * @param message 错误消息
 * @return JSON错误响应对象
 */
QJsonObject err(int code, const QString &message);

}  // namespace RpcHelpers
}  // namespace rpc
}  // namespace fanzhou

#endif  // FANZHOU_RPC_HELPERS_H
