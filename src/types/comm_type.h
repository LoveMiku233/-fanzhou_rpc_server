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
};



}
}

#endif // COMM_CONFIG_H
