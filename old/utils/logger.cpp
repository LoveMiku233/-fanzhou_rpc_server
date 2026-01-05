#include "logger.h"
#include <QTextStream>
#include <QDir>
#include <QFileInfo>

/**
 * @brief 获取日志单例
 */
Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

Logger::~Logger()
{
    close();
}

/**
 * @brief 初始化日志系统
 * @param logFilePath 日志文件路径
 * @param minLevel 最低日志级别
 * @param logToConsole 是否输出到终端
 */
void Logger::init(const QString& logFilePath, LogLevel minLevel, bool logToConsole)
{
    QMutexLocker locker(&mutex_);
    
    if (initialized_) {
        return;
    }
    
    minLevel_ = minLevel;
    consoleEnabled_ = logToConsole;
    
    if (!logFilePath.isEmpty()) {
        // 确保父目录存在
        const QString dirPath = QFileInfo(logFilePath).absolutePath();
        QDir().mkpath(dirPath);
        
        logFile_.setFileName(logFilePath);
        if (logFile_.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            fileEnabled_ = true;
            if (consoleEnabled_) {
                qInfo().noquote() << "[日志] 日志文件已打开:" << logFilePath;
            }
        } else {
            if (consoleEnabled_) {
                qWarning().noquote() << "[日志] 打开日志文件失败:" << logFilePath 
                                      << "错误:" << logFile_.errorString();
            }
        }
    }
    
    initialized_ = true;
    if (consoleEnabled_) {
        qInfo().noquote() << "[日志] 初始化完成，级别:" << levelToString(minLevel_)
                          << ", 终端输出:" << (consoleEnabled_ ? "启用" : "禁用");
    }
}

void Logger::setMinLevel(LogLevel level)
{
    QMutexLocker locker(&mutex_);
    minLevel_ = level;
}

LogLevel Logger::minLevel() const
{
    return minLevel_;
}

void Logger::setConsoleEnabled(bool enabled)
{
    QMutexLocker locker(&mutex_);
    consoleEnabled_ = enabled;
}

bool Logger::isConsoleEnabled() const
{
    return consoleEnabled_;
}

QString Logger::levelToString(LogLevel level) const
{
    switch (level) {
        case LogLevel::Debug:    return "调试";
        case LogLevel::Info:     return "信息";
        case LogLevel::Warning:  return "警告";
        case LogLevel::Error:    return "错误";
        case LogLevel::Critical: return "严重";
    }
    return "未知";
}

QString Logger::formatMessage(LogLevel level, const QString& source, const QString& message) const
{
    const QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    return QString("[%1] [%2] [%3] %4")
        .arg(timestamp)
        .arg(levelToString(level), -6)  // 使用-6宽度以适应中文字符
        .arg(source)
        .arg(message);
}

void Logger::log(LogLevel level, const QString& source, const QString& message)
{
    if (level < minLevel_) {
        return;
    }
    
    const QString formatted = formatMessage(level, source, message);
    
    // 根据配置决定是否输出到终端
    if (consoleEnabled_) {
        switch (level) {
            case LogLevel::Debug:
                qDebug().noquote() << formatted;
                break;
            case LogLevel::Info:
                qInfo().noquote() << formatted;
                break;
            case LogLevel::Warning:
                qWarning().noquote() << formatted;
                break;
            case LogLevel::Error:
            case LogLevel::Critical:
                qCritical().noquote() << formatted;
                break;
        }
    }
    
    // 写入日志文件
    if (fileEnabled_) {
        QMutexLocker locker(&mutex_);
        QTextStream stream(&logFile_);
        stream << formatted << "\n";
        stream.flush();
    }
}

void Logger::debug(const QString& source, const QString& message)
{
    log(LogLevel::Debug, source, message);
}

void Logger::info(const QString& source, const QString& message)
{
    log(LogLevel::Info, source, message);
}

void Logger::warning(const QString& source, const QString& message)
{
    log(LogLevel::Warning, source, message);
}

void Logger::error(const QString& source, const QString& message)
{
    log(LogLevel::Error, source, message);
}

void Logger::critical(const QString& source, const QString& message)
{
    log(LogLevel::Critical, source, message);
}

void Logger::flush()
{
    if (fileEnabled_) {
        QMutexLocker locker(&mutex_);
        logFile_.flush();
    }
}

void Logger::close()
{
    QMutexLocker locker(&mutex_);
    if (fileEnabled_ && logFile_.isOpen()) {
        logFile_.close();
        fileEnabled_ = false;
    }
    initialized_ = false;
}
