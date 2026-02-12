#include "core_config.h"

namespace fanzhou{
namespace core {

bool CoreConfig::loadMain(const QJsonObject &root, QString *error)
{
    // 主配置
    if (root.contains(QStringLiteral("main")) &&
        root[QStringLiteral("main")].isObject()) {
        const auto mainObj = root[QStringLiteral("main")].toObject();
        if (mainObj.contains(QStringLiteral("rpcPort"))) {
            main.rpcPort =
                static_cast<quint16>(mainObj[QStringLiteral("rpcPort")].toInt(main.rpcPort));
        }

        if (mainObj.contains(QStringLiteral("deviceId"))) {
            main.DeviceId = mainObj["deviceId"].toString();
        }

        // 认证配置
        if (mainObj.contains(QStringLiteral("auth")) &&
            mainObj[QStringLiteral("auth")].isObject()) {
            const auto authObj = mainObj[QStringLiteral("auth")].toObject();
            main.auth.enabled = authObj.value(QStringLiteral("enabled")).toBool(false);
            main.auth.secret = authObj.value(QStringLiteral("secret")).toString();
            main.auth.tokenExpireSec = authObj.value(QStringLiteral("tokenExpireSec")).toInt(3600);

            if (authObj.contains(QStringLiteral("allowedTokens")) &&
                authObj[QStringLiteral("allowedTokens")].isArray()) {
                const auto arr = authObj[QStringLiteral("allowedTokens")].toArray();
                for (const auto &val : arr) {
                    main.auth.allowedTokens.append(val.toString());
                }
            }

            if (authObj.contains(QStringLiteral("whitelist")) &&
                authObj[QStringLiteral("whitelist")].isArray()) {
                const auto arr = authObj[QStringLiteral("whitelist")].toArray();
                for (const auto &val : arr) {
                    main.auth.whitelist.append(val.toString());
                }
            }

            if (authObj.contains(QStringLiteral("publicMethods")) &&
                authObj[QStringLiteral("publicMethods")].isArray()) {
                main.auth.publicMethods.clear();
                const auto arr = authObj[QStringLiteral("publicMethods")].toArray();
                for (const auto &val : arr) {
                    main.auth.publicMethods.append(val.toString());
                }
            }
        }
    } else {
        return false;
    }
    return true;
}

bool CoreConfig::loadLog(const QJsonObject &root)
{
    // 日志配置
    if (root.contains(QStringLiteral("log")) &&
        root[QStringLiteral("log")].isObject()) {
        const auto logObj = root[QStringLiteral("log")].toObject();
        if (logObj.contains(QStringLiteral("logToConsole"))) {
            log.logToConsole = logObj[QStringLiteral("logToConsole")].toBool(log.logToConsole);
        }
        if (logObj.contains(QStringLiteral("logToFile"))) {
            log.logToFile = logObj[QStringLiteral("logToFile")].toBool(log.logToFile);
        }
        if (logObj.contains(QStringLiteral("logFilePath"))) {
            log.logFilePath =
                logObj[QStringLiteral("logFilePath")].toString(log.logFilePath);
        }
        if (logObj.contains(QStringLiteral("logLevel"))) {
            log.logLevel = logObj[QStringLiteral("logLevel")].toInt(log.logLevel);
        }
        if (logObj.contains(QStringLiteral("maxFileSizeMB"))) {
            log.maxFileSizeMB = logObj[QStringLiteral("maxFileSizeMB")].toInt(log.maxFileSizeMB);
        }
    } else {
        return false;
    }
    return true;
}

bool CoreConfig::loadScreen(const QJsonObject &root)
{
    // 屏幕配置
    if (root.contains(QStringLiteral("screen")) &&
        root[QStringLiteral("screen")].isObject()) {
        const auto screenObj = root[QStringLiteral("screen")].toObject();
        if (screenObj.contains(QStringLiteral("brightness"))) {
            screen.brightness = screenObj[QStringLiteral("brightness")].toInt(screen.brightness);
        }
        if (screenObj.contains(QStringLiteral("contrast"))) {
            screen.contrast = screenObj[QStringLiteral("contrast")].toInt(screen.contrast);
        }
        if (screenObj.contains(QStringLiteral("enabled"))) {
            screen.enabled = screenObj[QStringLiteral("enabled")].toBool(screen.enabled);
        }
        if (screenObj.contains(QStringLiteral("sleepTimeoutSec"))) {
            screen.sleepTimeoutSec = screenObj[QStringLiteral("sleepTimeoutSec")].toInt(screen.sleepTimeoutSec);
        }
        if (screenObj.contains(QStringLiteral("orientation"))) {
            screen.orientation = screenObj[QStringLiteral("orientation")].toString(screen.orientation);
        }
    } else {
        return false;
    }
    return true;
}

void CoreConfig::saveMain(QJsonObject &root) const
{
    // 主配置
    QJsonObject mainObj;
    mainObj[QStringLiteral("rpcPort")] = static_cast<int>(main.rpcPort);
    mainObj[QStringLiteral("deviceId")] = main.DeviceId;

    // 认证配置
    if (main.auth.enabled || !main.auth.secret.isEmpty() ||
        !main.auth.allowedTokens.isEmpty() || !main.auth.whitelist.isEmpty()) {
        QJsonObject authObj;
        authObj[QStringLiteral("enabled")] = main.auth.enabled;
        if (!main.auth.secret.isEmpty()) {
            authObj[QStringLiteral("secret")] = main.auth.secret;
        }

        authObj[QStringLiteral("tokenExpireSec")] = main.auth.tokenExpireSec;

        if (!main.auth.allowedTokens.isEmpty()) {
            QJsonArray arr;
            for (const auto &token : main.auth.allowedTokens) {
                arr.append(token);
            }
            authObj[QStringLiteral("allowedTokens")] = arr;
        }

        if (!main.auth.whitelist.isEmpty()) {
            QJsonArray arr;
            for (const auto &ip : main.auth.whitelist) {
                arr.append(ip);
            }
            authObj[QStringLiteral("whitelist")] = arr;
        }

        // 只有在修改过时才保存publicMethods
        if (main.auth.publicMethods != AuthConfig().publicMethods) {
            QJsonArray arr;
            for (const auto &method : main.auth.publicMethods) {
                arr.append(method);
            }
            authObj[QStringLiteral("publicMethods")] = arr;
        }

        mainObj[QStringLiteral("auth")] = authObj;
    }

    root[QStringLiteral("main")] = mainObj;
}


void CoreConfig::saveLog(QJsonObject &root) const
{
    // 日志配置
    QJsonObject logObj;
    logObj[QStringLiteral("logToConsole")] = log.logToConsole;
    logObj[QStringLiteral("logToFile")] = log.logToFile;
    logObj[QStringLiteral("logFilePath")] = log.logFilePath;
    logObj[QStringLiteral("logLevel")] = log.logLevel;
    logObj[QStringLiteral("maxFileSizeMB")] = log.maxFileSizeMB;
    root[QStringLiteral("log")] = logObj;
}


void CoreConfig::saveScreen(QJsonObject &root) const
{
    // 屏幕配置
    QJsonObject screenObj;
    screenObj[QStringLiteral("brightness")] = screen.brightness;
    screenObj[QStringLiteral("contrast")] = screen.contrast;
    screenObj[QStringLiteral("enabled")] = screen.enabled;
    screenObj[QStringLiteral("sleepTimeoutSec")] = screen.sleepTimeoutSec;
    screenObj[QStringLiteral("orientation")] = screen.orientation;
    root[QStringLiteral("screen")] = screenObj;
}

}
}
