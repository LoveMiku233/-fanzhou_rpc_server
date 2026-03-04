/**
 * @file logger.cpp
 * @brief 线程安全日志系统实现
 */

#include "logger.h"

#include <QDir>
#include <QFileInfo>
#include <QTextStream>

namespace fanzhou {

Logger &Logger::instance()
{
    static Logger logger;
    return logger;
}

Logger::~Logger()
{
    close();
}

void Logger::init(const QString &logFilePath, LogLevel minLevel, bool logToConsole,
                  int maxFileSizeMB, const QString &errorLogFilePath)
{
    QMutexLocker locker(&mutex_);

    if (initialized_) {
        return;
    }

    minLevel_ = minLevel;
    consoleEnabled_ = logToConsole;
    maxFileSizeBytes_ = static_cast<qint64>(maxFileSizeMB) * 1024 * 1024;

    if (!logFilePath.isEmpty()) {
        const QString dirPath = QFileInfo(logFilePath).absolutePath();
        QDir().mkpath(dirPath);

        logFile_.setFileName(logFilePath);
        if (logFile_.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            fileEnabled_ = true;
            if (consoleEnabled_) {
                qInfo().noquote() << "[Logger] Log file opened:" << logFilePath;
            }
        } else {
            if (consoleEnabled_) {
                qWarning().noquote() << "[Logger] Failed to open log file:"
                                     << logFilePath << "Error:" << logFile_.errorString();
            }
        }
    }

    // 初始化ERROR/CRITICAL单独日志文件
    if (!errorLogFilePath.isEmpty()) {
        const QString errDirPath = QFileInfo(errorLogFilePath).absolutePath();
        QDir().mkpath(errDirPath);

        errorLogFile_.setFileName(errorLogFilePath);
        if (errorLogFile_.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            errorFileEnabled_ = true;
            if (consoleEnabled_) {
                qInfo().noquote() << "[Logger] Error log file opened:" << errorLogFilePath;
            }
        } else {
            if (consoleEnabled_) {
                qWarning().noquote() << "[Logger] Failed to open error log file:"
                                     << errorLogFilePath << "Error:" << errorLogFile_.errorString();
            }
        }
    }

    initialized_ = true;
    if (consoleEnabled_) {
        qInfo().noquote() << "[Logger] Initialized, level:" << levelToString(minLevel_)
                          << ", console:" << (consoleEnabled_ ? "enabled" : "disabled");
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
    case LogLevel::Debug:
        return QStringLiteral("DEBUG");
    case LogLevel::Info:
        return QStringLiteral("INFO");
    case LogLevel::Warning:
        return QStringLiteral("WARN");
    case LogLevel::Error:
        return QStringLiteral("ERROR");
    case LogLevel::Critical:
        return QStringLiteral("CRIT");
    }
    return QStringLiteral("UNKNOWN");
}

QString Logger::formatMessage(LogLevel level, const QString &source,
                               const QString &message) const
{
    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    return QStringLiteral("[%1] [%2] [%3] %4")
        .arg(timestamp)
        .arg(levelToString(level), -5)
        .arg(source)
        .arg(message);
}

void Logger::log(LogLevel level, const QString &source, const QString &message)
{
    QMutexLocker locker(&mutex_);

    if (level < minLevel_) {
        return;
    }

    // Cache volatile flags under mutex before any I/O
    const bool doConsole = consoleEnabled_;
    const bool doFile = fileEnabled_;

    const QString formatted = formatMessage(level, source, message);

    if (doConsole) {
        // Temporarily unlock mutex during console I/O to avoid holding the lock
        // during potentially slow I/O operations; Qt's qDebug/qInfo are thread-safe
        locker.unlock();
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
        locker.relock();
    }

    if (doFile && fileEnabled_) {
        checkAndRotateFile();
        QTextStream stream(&logFile_);
        stream << formatted << "\n";
        stream.flush();
    }

    // ERROR/CRITICAL也写入单独的错误日志文件
    if (errorFileEnabled_ && (level == LogLevel::Error || level == LogLevel::Critical)) {
        checkAndRotateErrorFile();
        QTextStream errStream(&errorLogFile_);
        errStream << formatted << "\n";
        errStream.flush();
    }
}

void Logger::debug(const QString &source, const QString &message)
{
    log(LogLevel::Debug, source, message);
}

void Logger::info(const QString &source, const QString &message)
{
    log(LogLevel::Info, source, message);
}

void Logger::warning(const QString &source, const QString &message)
{
    log(LogLevel::Warning, source, message);
}

void Logger::error(const QString &source, const QString &message)
{
    log(LogLevel::Error, source, message);
}

void Logger::critical(const QString &source, const QString &message)
{
    log(LogLevel::Critical, source, message);
}

void Logger::flush()
{
    if (fileEnabled_ || errorFileEnabled_) {
        QMutexLocker locker(&mutex_);
        if (fileEnabled_) {
            logFile_.flush();
        }
        if (errorFileEnabled_) {
            errorLogFile_.flush();
        }
    }
}

void Logger::checkAndRotateFile()
{
    if (maxFileSizeBytes_ <= 0 || !logFile_.isOpen()) {
        return;
    }

    if (logFile_.size() >= maxFileSizeBytes_) {
        const QString filePath = logFile_.fileName();
        logFile_.close();
        if (!logFile_.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            fileEnabled_ = false;
            if (consoleEnabled_) {
                qWarning().noquote() << "[Logger] Failed to reopen log file after rotation:"
                                     << filePath << "Error:" << logFile_.errorString();
            }
            return;
        }
        if (consoleEnabled_) {
            qInfo().noquote() << "[Logger] Log file rotated (exceeded"
                              << (maxFileSizeBytes_ / (1024 * 1024)) << "MB):" << filePath;
        }
    }
}

void Logger::checkAndRotateErrorFile()
{
    if (maxFileSizeBytes_ <= 0 || !errorLogFile_.isOpen()) {
        return;
    }

    if (errorLogFile_.size() >= maxFileSizeBytes_) {
        const QString filePath = errorLogFile_.fileName();
        errorLogFile_.close();
        if (!errorLogFile_.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            errorFileEnabled_ = false;
            if (consoleEnabled_) {
                qWarning().noquote() << "[Logger] Failed to reopen error log file after rotation:"
                                     << filePath << "Error:" << errorLogFile_.errorString();
            }
            return;
        }
        if (consoleEnabled_) {
            qInfo().noquote() << "[Logger] Error log file rotated (exceeded"
                              << (maxFileSizeBytes_ / (1024 * 1024)) << "MB):" << filePath;
        }
    }
}

void Logger::close()
{
    QMutexLocker locker(&mutex_);
    if (fileEnabled_ && logFile_.isOpen()) {
        logFile_.close();
        fileEnabled_ = false;
    }
    if (errorFileEnabled_ && errorLogFile_.isOpen()) {
        errorLogFile_.close();
        errorFileEnabled_ = false;
    }
    initialized_ = false;
}

}  // namespace fanzhou
