/**
 * @file rpc_helpers.cpp
 * @brief RPC辅助函数实现
 */

#include "rpc_helpers.h"

namespace fanzhou {
namespace rpc {
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
bool getU8(const QJsonObject &params, const char *key, quint8 &out)
{
    if (!params.contains(key)) {
        return false;
    }
    
    const QJsonValue val = params.value(key);
    int value = -1;
    
    // 支持数字类型
    if (val.isDouble()) {
        value = val.toInt(-1);
    }
    // 支持字符串类型（尝试转换为整数）
    else if (val.isString()) {
        bool ok = false;
        value = val.toString().toInt(&ok);
        if (!ok) {
            return false;
        }
    } else {
        return false;
    }
    
    if (value < 0 || value > 255) {
        return false;
    }
    out = static_cast<quint8>(value);
    return true;
}

/**
 * @brief 从JSON对象提取布尔值
 *
 * @param params JSON参数对象
 * @param key 参数键
 * @param out 输出值
 * @param defaultValue 键不存在时的默认值
 * @return 成功返回true，键不存在时使用默认值并返回true，类型错误返回false
 */
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
bool getDouble(const QJsonObject &params, const char *key, double &out)
{
    if (!params.contains(key)) {
        return false;
    }

    const QJsonValue val = params.value(key);

    // 支持数字类型
    if (val.isDouble()) {
        out = val.toDouble();
        return true;
    }
    else if (val.isString()) {
        bool ok = false;
        out = val.toString().toDouble(&ok);
        return ok;
    }

    return false;
}


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
bool getI32(const QJsonObject &params, const char *key, qint32 &out)
{
    if (!params.contains(key)) {
        return false;
    }
    
    const QJsonValue val = params.value(key);
    
    // 支持数字类型
    if (val.isDouble()) {
        out = val.toInt();
        return true;
    }
    // 支持字符串类型（尝试转换为整数）
    else if (val.isString()) {
        bool ok = false;
        out = val.toString().toInt(&ok);
        return ok;
    }
    
    return false;
}

/**
 * @brief 从JSON对象提取字符串
 *
 * @param params JSON参数对象
 * @param key 参数键
 * @param out 输出值
 * @return 成功返回true，失败（缺少键或类型错误）返回false
 */
bool getString(const QJsonObject &params, const QString &key, QString &out)
{
    if (!params.contains(key) || !params.value(key).isString()) {
        return false;
    }
    out = params.value(key).toString();
    return true;
}

/**
 * @brief 从JSON对象提取十六进制编码的字节数组
 *
 * 将十六进制字符串（如"01FF"）转换为QByteArray。
 *
 * @param params JSON参数对象
 * @param key 参数键
 * @param out 输出字节数组
 * @return 成功返回true，失败（缺少键、类型错误或无效十六进制）返回false
 */
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

/**
 * @brief 创建成功响应对象
 *
 * @param value 成功标志，默认为true
 * @return 包含"ok"字段的JSON对象
 */
QJsonObject ok(bool value)
{
    return QJsonObject{{QStringLiteral("ok"), value}};
}

/**
 * @brief 创建错误响应对象
 *
 * @param code 错误码
 * @param message 错误消息
 * @return 包含"ok"、"code"、"message"字段的JSON对象
 */
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
