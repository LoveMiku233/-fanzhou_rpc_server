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
        if (canObj.contains(QStringLiteral("restartMs"))) {
            can.restartMs = canObj[QStringLiteral("restartMs")].toInt(can.restartMs);
        }
        if (canObj.contains(QStringLiteral("periodicRestartMin"))) {
            can.periodicRestartMin = canObj[QStringLiteral("periodicRestartMin")].toInt(can.periodicRestartMin);
        }
        // Fake CAN 配置
        if (canObj.contains(QStringLiteral("is_fake"))) {
            can.is_fake = canObj[QStringLiteral("is_fake")].toBool(can.is_fake);
        }
        if (canObj.contains(QStringLiteral("fake_can"))) {
            can.fake_can = canObj[QStringLiteral("fake_can")].toString(can.fake_can);
        }
        if (canObj.contains(QStringLiteral("fake_can_baudrate"))) {
            can.fake_can_baudrate = canObj[QStringLiteral("fake_can_baudrate")].toInt(can.fake_can_baudrate);
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
    canObj[QStringLiteral("restartMs")] = can.restartMs;
    canObj[QStringLiteral("periodicRestartMin")] = can.periodicRestartMin;
    // Fake CAN 配置
    canObj[QStringLiteral("is_fake")] = can.is_fake;
    canObj[QStringLiteral("fake_can")] = can.fake_can;
    canObj[QStringLiteral("fake_can_baudrate")] = can.fake_can_baudrate;
    root[QStringLiteral("can")] = canObj;
}

}
}
