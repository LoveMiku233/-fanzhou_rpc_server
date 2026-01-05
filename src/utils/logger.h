/**
 * @file logger.h
 * @brief Thread-safe logging system
 *
 * Provides a singleton logger with multiple log levels, file output,
 * and console output control.
 */

#ifndef FANZHOU_LOGGER_H
#define FANZHOU_LOGGER_H

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QMutex>
#include <QString>

namespace fanzhou {

/**
 * @brief Log level enumeration
 *
 * Defines supported log levels from lowest to highest priority
 */
enum class LogLevel {
    Debug = 0,     ///< Debug information
    Info = 1,      ///< General information
    Warning = 2,   ///< Warning messages
    Error = 3,     ///< Error messages
    Critical = 4   ///< Critical errors
};

/**
 * @brief Thread-safe logging utility class
 *
 * Provides multi-level logging with file and console output control.
 * All log messages include context information (timestamp, source, level).
 */
class Logger
{
public:
    /**
     * @brief Get logger singleton instance
     * @return Reference to the logger instance
     */
    static Logger &instance();

    /**
     * @brief Initialize the logging system
     * @param logFilePath Log file path (empty for console-only output)
     * @param minLevel Minimum log level to output
     * @param logToConsole Whether to output to console
     */
    void init(const QString &logFilePath = QString(),
              LogLevel minLevel = LogLevel::Debug,
              bool logToConsole = true);

    /**
     * @brief Set minimum log level
     * @param level Minimum level to output
     */
    void setMinLevel(LogLevel level);

    /**
     * @brief Get current minimum log level
     * @return Current minimum log level
     */
    LogLevel minLevel() const;

    /**
     * @brief Set console output enabled
     * @param enabled Whether to enable console output
     */
    void setConsoleEnabled(bool enabled);

    /**
     * @brief Check if console output is enabled
     * @return True if console output is enabled
     */
    bool isConsoleEnabled() const;

    /**
     * @brief Log a message at the specified level
     * @param level Log level
     * @param source Source component name
     * @param message Log message
     */
    void log(LogLevel level, const QString &source, const QString &message);

    // Convenience methods for each log level
    void debug(const QString &source, const QString &message);
    void info(const QString &source, const QString &message);
    void warning(const QString &source, const QString &message);
    void error(const QString &source, const QString &message);
    void critical(const QString &source, const QString &message);

    /**
     * @brief Flush log buffer to file
     */
    void flush();

    /**
     * @brief Close log file
     */
    void close();

private:
    Logger() = default;
    ~Logger();

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    QString levelToString(LogLevel level) const;
    QString formatMessage(LogLevel level, const QString &source,
                          const QString &message) const;

    QFile logFile_;
    QMutex mutex_;
    LogLevel minLevel_ = LogLevel::Debug;
    bool initialized_ = false;
    bool fileEnabled_ = false;
    bool consoleEnabled_ = true;
};

// Convenience logging macros
#define LOG_DEBUG(source, msg) \
    fanzhou::Logger::instance().debug(source, msg)
#define LOG_INFO(source, msg) \
    fanzhou::Logger::instance().info(source, msg)
#define LOG_WARNING(source, msg) \
    fanzhou::Logger::instance().warning(source, msg)
#define LOG_ERROR(source, msg) \
    fanzhou::Logger::instance().error(source, msg)
#define LOG_CRITICAL(source, msg) \
    fanzhou::Logger::instance().critical(source, msg)

}  // namespace fanzhou

#endif  // FANZHOU_LOGGER_H
