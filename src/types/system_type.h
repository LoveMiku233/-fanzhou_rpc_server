#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <QString>
#include <QStringList>

namespace fanzhou {
namespace core {

/**
 * @brief 日志配置
 */
struct LogConfig {
    bool logToConsole = true;
    bool logToFile = true;
    QString logFilePath = QStringLiteral("/var/log/fanzhou_core/core.log");
    int logLevel = 0;  // 0=Debug, 1=Info, 2=Warning, 3=Error, 4=Critical
};


/**
 * @brief 认证配置
 */
struct AuthConfig {
    bool enabled = false;                ///< 是否启用认证
    QString secret;                       ///< 认证密钥（用于生成token）
    QStringList allowedTokens;           ///< 预设的有效token列表
    int tokenExpireSec = 3600;           ///< Token过期时间（秒），0表示永不过期
    QStringList whitelist;               ///< IP白名单，在此列表中的IP不需要认证
    QStringList publicMethods = {        ///< 无需认证即可访问的公共方法
        QStringLiteral("rpc.ping"),
        QStringLiteral("rpc.list"),
        QStringLiteral("auth.login"),
        QStringLiteral("auth.verify"),
        QStringLiteral("auth.status")
    };
};
/**
 * @brief 屏幕参数配置
 */
struct ScreenConfig {
    int brightness = 100;      ///< 亮度 (0-100)
    int contrast = 50;         ///< 对比度 (0-100)
    bool enabled = true;       ///< 屏幕开关
    int sleepTimeoutSec = 300; ///< 休眠超时（秒），0表示不休眠
    QString orientation = QStringLiteral("landscape");  ///< 屏幕方向
};

struct MainConfig {
    quint16 rpcPort = 12345;
    AuthConfig auth;
    QString DeviceId = "NULL";
};

}
}

#endif // SYSTEM_CONFIG_H
