#ifndef FANZHOUCLOUD_PROTOCOL_H
#define FANZHOUCLOUD_PROTOCOL_H

#include <QString>

namespace fanzhou {
namespace cloud {
namespace protocol {

enum class FanzhouSettingMethod {
    GET,
    SET,
    DELETE,
    UNKNOWN
};

// type
enum class FanzhouSettingType {
    Scene,
    Timer,
    Unknown
};

enum FanzhouSettingError {
    OK = 0,
    SCENE_FORMAT_ERROR   = 3001,
    SCENE_NAME_EXISTS    = 3002,
    COND_CONFILICTS      = 3003,
    ACTIONS_CONFIG_ERROR = 3004,
    PERMISSIONS_ERROR    = 3005,
    SCENE_LIMIT          = 3006,
    TIME_FORMAT_ERROR    = 3007,
};


struct FanzhouSettingHeader {
    FanzhouSettingMethod method;
    FanzhouSettingType   type;
    QString       requestId;
    qint64        timestamp;
};

}
}
}


#endif // FANZHOUCLOUD_PROTOCOL_H
