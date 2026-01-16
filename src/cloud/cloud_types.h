#ifndef CLOUD_TYPES_H
#define CLOUD_TYPES_H

namespace fanzhou {
namespace cloud {

/**
 * @brief cloud server typed
 *
 * 设备类型范围：
 *   1-10:   MQTT
 *   11-20:  TCP
 *   21-50:  Others
 */


enum class CloudTypeId : int {
    FanzhouCloudMqtt = 1

    /* @TODO */
};


enum class CloudCommTypeId : int {
    MQTTv311 = 1,
    MQTTv5 = 2,
    TCP = 3,
    /* @TODO */
};


inline const char* cloudTypeToString(CloudTypeId type)
{
    switch (type) {
    case CloudTypeId::FanzhouCloudMqtt: return "FanzhouCloudMqtt";
    default: return "UnKnown";
    }
}

inline const char* cloudCommTypeToString(CloudCommTypeId type)
{
    switch (type) {
    case CloudCommTypeId::MQTTv311: return "MQTT V311";
    case CloudCommTypeId::MQTTv5: return "MQTT V5";
    case CloudCommTypeId::TCP: return "TCP";
    default: return "Unknown";
    }
}


}
}


#endif // CLOUD_TYPES_H
