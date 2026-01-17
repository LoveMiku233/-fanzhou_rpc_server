/**
 * @file cloud_types.h
 * @brief 云平台类型定义
 *
 * 定义云平台相关的枚举类型和辅助函数。
 */

#ifndef CLOUD_TYPES_H
#define CLOUD_TYPES_H

#include <QtGlobal>

namespace fanzhou {
namespace cloud {

/**
 * @brief 云平台类型标识
 *
 * 定义支持的云平台类型：
 *   1-10:   MQTT类型云平台
 *   11-20:  TCP类型云平台
 *   21-50:  其他类型云平台
 */
enum class CloudTypeId : int {
    FanzhouCloudMqtt = 1    ///< 泛舟云平台 (MQTT协议)
    // 可扩展其他云平台类型
};

/**
 * @brief 云平台通信协议类型
 */
enum class CloudCommTypeId : int {
    MQTTv311 = 1,   ///< MQTT v3.1.1协议
    MQTTv5 = 2,     ///< MQTT v5.0协议
    TCP = 3         ///< 原生TCP协议
};

/**
 * @brief 将云平台类型转换为字符串
 * @param type 云平台类型
 * @return 云平台类型名称
 */
inline const char* cloudTypeToString(CloudTypeId type)
{
    switch (type) {
    case CloudTypeId::FanzhouCloudMqtt: return "FanzhouCloudMqtt";
    default: return "Unknown";
    }
}

/**
 * @brief 将云平台通信类型转换为字符串
 * @param type 通信类型
 * @return 通信类型名称
 */
inline const char* cloudCommTypeToString(CloudCommTypeId type)
{
    switch (type) {
    case CloudCommTypeId::MQTTv311: return "MQTT V311";
    case CloudCommTypeId::MQTTv5: return "MQTT V5";
    case CloudCommTypeId::TCP: return "TCP";
    default: return "Unknown";
    }
}

}  // namespace cloud
}  // namespace fanzhou

/**
 * @brief QHash支持函数 - 使CloudTypeId可用作QHash键
 * @param key CloudTypeId枚举值
 * @param seed 哈希种子
 * @return 哈希值
 *
 * 注意：此函数必须定义在全局命名空间中才能被Qt的QHash正确发现。
 */
inline uint qHash(fanzhou::cloud::CloudTypeId key, uint seed = 0) noexcept
{
    return ::qHash(static_cast<int>(key), seed);
}

#endif  // CLOUD_TYPES_H


#endif // CLOUD_TYPES_H
