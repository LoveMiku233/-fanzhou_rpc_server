/**
 * @file utils.h
 * @brief Common utility functions
 *
 * Provides system-level utility functions for error handling and other common operations.
 */

#ifndef FANZHOU_UTILS_H
#define FANZHOU_UTILS_H

#include <QString>

namespace fanzhou {
namespace utils {

/**
 * @brief Get system error string with prefix
 * @param prefix Error message prefix
 * @return Formatted error string including errno message
 */
QString sysErrorString(const char *prefix);

}  // namespace utils
}  // namespace fanzhou

#endif  // FANZHOU_UTILS_H
