/**
 * @file rpc_helpers.cpp
 * @brief RPC辅助函数实现
 * 
 * 提供JSON参数解析和响应构建的辅助函数
 */

#include "rpc_helpers.h"

namespace RpcHelpers {

/**
 * @brief 从JSON对象获取无符号8位整数
 * @param p 参数对象
 * @param key 参数键名
 * @param out 输出值
 * @return 成功返回true，失败返回false
 */
bool getU8(const QJsonObject& p, const char* key, quint8& out)
{
    if (!p.contains(key) || !p.value(key).isDouble()) return false;
    const int v = p.value(key).toInt(-1);
    if (v < 0 || v > 255) return false;
    out = quint8(v);
    return true;
}

/**
 * @brief 从JSON对象获取布尔值
 * @param p 参数对象
 * @param key 参数键名
 * @param out 输出值
 * @param def 默认值
 * @return 成功返回true，失败返回false
 */
bool getBool(const QJsonObject& p, const char* key, bool& out, bool def)
{
    if (!p.contains(key)) { out = def; return true; }
    if (!p.value(key).isBool()) return false;
    out = p.value(key).toBool();
    return true;
}

/**
 * @brief 从JSON对象获取32位整数
 * @param p 参数对象
 * @param key 参数键名
 * @param out 输出值
 * @return 成功返回true，失败返回false
 */
bool getI32(const QJsonObject& p, const char* key, qint32& out)
{
    if (!p.contains(key) || !p.value(key).isDouble()) return false;
    out = p.value(key).toInt();
    return true;
}

/**
 * @brief 从JSON对象获取字符串
 * @param p 参数对象
 * @param key 参数键名
 * @param out 输出值
 * @return 成功返回true，失败返回false
 */
bool getString(const QJsonObject& p, const char* key, QString& out)
{
    if (!p.contains(key) || !p.value(key).isString()) return false;
    out = p.value(key).toString();
    return true;
}

/**
 * @brief 从JSON对象获取十六进制字节数组
 * @param p 参数对象
 * @param key 参数键名
 * @param out 输出值
 * @return 成功返回true，失败返回false
 */
bool getHexBytes(const QJsonObject& p, const char* key, QByteArray& out)
{
    if (!p.contains(key) || !p.value(key).isString()) return false;
    const QString s = p.value(key).toString().trimmed();
    const QByteArray b = QByteArray::fromHex(s.toLatin1());
    if (b.isEmpty() && !s.isEmpty()) return false;
    out = b;
    return true;
}

/**
 * @brief 创建成功/失败响应对象
 * @param v 成功标志
 * @return JSON响应对象
 */
QJsonObject ok(bool v)
{
    return QJsonObject{{"ok", v}};
}

/**
 * @brief 创建错误响应对象
 * @param code 错误代码
 * @param msg 错误消息
 * @return JSON错误响应对象
 */
QJsonObject err(int code, const QString& msg)
{
    return QJsonObject{
        {"ok", false},
        {"code", code},
        {"message", msg},
    };
}


}
