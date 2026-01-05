/**
 * @file utils.h
 * @brief 通用工具函数
 *
 * 提供系统级工具函数，用于错误处理和其他常用操作。
 */

#ifndef FANZHOU_UTILS_H
#define FANZHOU_UTILS_H

#include <QString>

namespace fanzhou {
namespace utils {

/**
 * @brief 获取带前缀的系统错误字符串
 * @param prefix 错误消息前缀
 * @return 格式化的错误字符串，包含errno消息
 */
QString sysErrorString(const char *prefix);

}  // namespace utils
}  // namespace fanzhou

#endif  // FANZHOU_UTILS_H
