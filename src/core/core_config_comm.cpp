#include "core_config.h"


namespace fanzhou{
namespace core {

bool CoreConfig::loadCan(const QJsonObject &root)
{
    // CAN配置
    if (root.contains(QStringLiteral("can")) &&
        root[QStringLiteral("can")].isObject()) {
        const auto canObj = root[QStringLiteral("can")].toObject();
        if (canObj.contains(QStringLiteral("ifname"))) {
            can.interface = canObj[QStringLiteral("ifname")].toString(can.interface);
        }
        if (canObj.contains(QStringLiteral("bitrate"))) {
            can.bitrate = canObj[QStringLiteral("bitrate")].toInt(can.bitrate);
        }
        if (canObj.contains(QStringLiteral("tripleSampling"))) {
            can.tripleSampling =
                canObj[QStringLiteral("tripleSampling")].toBool(can.tripleSampling);
        }
        if (canObj.contains(QStringLiteral("canFd"))) {
            can.canFd = canObj[QStringLiteral("canFd")].toBool(can.canFd);
        }
    } else {
        return false;
    }
    return true;
}

void CoreConfig::saveCan(QJsonObject &root) const
{
    // CAN配置
    QJsonObject canObj;
    canObj[QStringLiteral("ifname")] = can.interface;
    canObj[QStringLiteral("bitrate")] = can.bitrate;
    canObj[QStringLiteral("tripleSampling")] = can.tripleSampling;
    canObj[QStringLiteral("canFd")] = can.canFd;
    root[QStringLiteral("can")] = canObj;
}

}
}
