#ifndef COMM_CONFIG_H
#define COMM_CONFIG_H

#include <QString>

namespace fanzhou {
namespace core {

struct CanConfig {
    QString interface = "can0";
    int bitrate = 250000;
    bool tripleSampling = true;
    bool canFd = false;
    int restartMs = 100;  ///< CAN控制器bus-off自动重启延迟（毫秒），0表示禁用
    int periodicRestartMin = 0;  ///< 定时重启CAN接口的间隔（分钟），0表示禁用
};



}
}

#endif // COMM_CONFIG_H
