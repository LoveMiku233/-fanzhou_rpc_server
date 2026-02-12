/**
 * @file logger.h
 * @brief 线程安全日志系统
 *
 * 提供单例日志器，支持多日志级别、文件输出和控制台输出控制。
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
 * @brief 日志级别枚举
 *
 * 定义支持的日志级别，从最低到最高优先级
 */
enum class LogLevel {
    Debug = 0,     ///< 调试信息
    Info = 1,      ///< 一般信息
    Warning = 2,   ///< 警告消息
    Error = 3,     ///< 错误消息
    Critical = 4   ///< 严重错误
};

/**
 * @brief 线程安全日志工具类
 *
 * 提供多级别日志记录，支持文件和控制台输出控制。
 * 所有日志消息包含上下文信息（时间戳、来源、级别）。
 */
class Logger
{
public:
    /**
     * @brief 获取日志器单例实例
     * @return 日志器实例的引用
     */
    static Logger &instance();

    /**
     * @brief 初始化日志系统
     * @param logFilePath 日志文件路径（空则仅控制台输出）
     * @param minLevel 最小输出日志级别
     * @param logToConsole 是否输出到控制台
     * @param maxFileSizeMB 日志文件最大大小（MB），超过后清空重建，0表示不限制
     */
    void init(const QString &logFilePath = QString(),
              LogLevel minLevel = LogLevel::Debug,
              bool logToConsole = true,
              int maxFileSizeMB = 0);

    /**
     * @brief 设置最小日志级别
     * @param level 最小输出级别
     */
    void setMinLevel(LogLevel level);

    /**
     * @brief 获取当前最小日志级别
     * @return 当前最小日志级别
     */
    LogLevel minLevel() const;

    /**
     * @brief 设置控制台输出开关
     * @param enabled 是否启用控制台输出
     */
    void setConsoleEnabled(bool enabled);

    /**
     * @brief 检查控制台输出是否启用
     * @return 如果启用控制台输出则返回true
     */
    bool isConsoleEnabled() const;

    /**
     * @brief 记录指定级别的日志消息
     * @param level 日志级别
     * @param source 来源组件名称
     * @param message 日志消息
     */
    void log(LogLevel level, const QString &source, const QString &message);

    // 各日志级别的便捷方法
    void debug(const QString &source, const QString &message);
    void info(const QString &source, const QString &message);
    void warning(const QString &source, const QString &message);
    void error(const QString &source, const QString &message);
    void critical(const QString &source, const QString &message);

    /**
     * @brief 刷新日志缓冲区到文件
     */
    void flush();

    /**
     * @brief 关闭日志文件
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
    void checkAndRotateFile();

    QFile logFile_;
    QMutex mutex_;
    LogLevel minLevel_ = LogLevel::Debug;
    bool initialized_ = false;
    bool fileEnabled_ = false;
    bool consoleEnabled_ = true;
    qint64 maxFileSizeBytes_ = 0;
};

// 便捷日志宏
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
