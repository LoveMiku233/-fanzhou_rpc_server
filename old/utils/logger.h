#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QFile>
#include <QMutex>
#include <QDateTime>
#include <QDebug>

/**
 * @brief 日志级别枚举
 * 
 * 定义系统支持的日志级别，从低到高排序
 */
enum class LogLevel {
    Debug = 0,      // 调试信息
    Info = 1,       // 普通信息
    Warning = 2,    // 警告信息
    Error = 3,      // 错误信息
    Critical = 4    // 严重错误
};

/**
 * @brief 线程安全的日志工具类
 * 
 * 此日志类支持多个日志级别、文件输出和终端输出控制，
 * 在所有日志消息中提供上下文信息（时间戳、来源、级别）
 */
class Logger
{
public:
    /**
     * @brief 获取日志单例
     */
    static Logger& instance();
    
    /**
     * @brief 初始化日志系统
     * @param logFilePath 日志文件路径（空则仅输出到终端）
     * @param minLevel 最低输出日志级别
     * @param logToConsole 是否输出到终端
     */
    void init(const QString& logFilePath = QString(), 
              LogLevel minLevel = LogLevel::Debug,
              bool logToConsole = true);
    
    /**
     * @brief 设置最低日志级别
     * @param level 最低输出级别
     */
    void setMinLevel(LogLevel level);
    
    /**
     * @brief 获取当前最低日志级别
     */
    LogLevel minLevel() const;
    
    /**
     * @brief 设置是否输出到终端
     * @param enabled 是否启用终端输出
     */
    void setConsoleEnabled(bool enabled);
    
    /**
     * @brief 获取终端输出是否启用
     */
    bool isConsoleEnabled() const;
    
    /**
     * @brief 记录指定级别的日志
     * @param level 日志级别
     * @param source 来源组件名
     * @param message 日志消息
     */
    void log(LogLevel level, const QString& source, const QString& message);
    
    /**
     * @brief 各日志级别的便捷方法
     */
    void debug(const QString& source, const QString& message);
    void info(const QString& source, const QString& message);
    void warning(const QString& source, const QString& message);
    void error(const QString& source, const QString& message);
    void critical(const QString& source, const QString& message);
    
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
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    QString levelToString(LogLevel level) const;
    QString formatMessage(LogLevel level, const QString& source, const QString& message) const;
    
    QFile logFile_;
    QMutex mutex_;
    LogLevel minLevel_ = LogLevel::Debug;
    bool initialized_ = false;
    bool fileEnabled_ = false;
    bool consoleEnabled_ = true;  // 终端输出开关，默认开启
};

// 便捷日志宏
#define LOG_DEBUG(source, msg) Logger::instance().debug(source, msg)
#define LOG_INFO(source, msg) Logger::instance().info(source, msg)
#define LOG_WARNING(source, msg) Logger::instance().warning(source, msg)
#define LOG_ERROR(source, msg) Logger::instance().error(source, msg)
#define LOG_CRITICAL(source, msg) Logger::instance().critical(source, msg)

#endif // LOGGER_H
