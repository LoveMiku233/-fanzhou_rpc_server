/**
 * @file utils.cpp
 * @brief 通用工具函数实现
 */

#include "utils.h"

#include <cerrno>
#include <cstring>

namespace fanzhou {
namespace utils {

QString sysErrorString(const char *prefix)
{
    return QString("%1: %2").arg(prefix, QString::fromLocal8Bit(std::strerror(errno)));
}

}  // namespace utils
}  // namespace fanzhou
