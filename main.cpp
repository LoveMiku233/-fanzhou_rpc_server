/**
 * @file main.cpp
 * @brief 泛舟RPC服务器入口
 *
 * 温室控制系统核心服务主入口。
 * 目标平台：全志A133
 */

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>

#include "src/core/core_config.h"
#include "src/core/core_context.h"
#include "src/core/rpc_registry.h"
#include "src/rpc/json_rpc_dispatcher.h"
#include "src/rpc/json_rpc_server.h"
#include "src/utils/logger.h"

namespace {

const char *const kLogSource = "Main";
const QString kDefaultLogPath = QStringLiteral("/var/log/fanzhou_core/core.log");
const QString kDefaultConfigPath = QStringLiteral("/var/lib/fanzhou_core/core.json");

/**
 * @brief 获取配置文件路径
 * @param app 应用程序实例
 * @return 配置文件路径
 */
QString getConfigPath(const QCoreApplication &app)
{
    Q_UNUSED(app);
    // TODO: 支持命令行参数指定配置文件路径
    return kDefaultConfigPath;
}

/**
 * @brief 确保父目录存在
 * @param filePath 文件路径
 * @param error 错误信息输出
 * @return 成功返回true
 */
bool ensureParentDir(const QString &filePath, QString *error = nullptr)
{
    const QString dirPath = QFileInfo(filePath).absolutePath();
    QDir dir;
    if (dir.exists(dirPath)) {
        return true;
    }
    if (!dir.mkpath(dirPath)) {
        if (error) {
            *error = QStringLiteral("Failed to create directory: %1").arg(dirPath);
        }
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("fanzhou-rpc-server"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

    // 1. 加载配置
    const QString configPath = getConfigPath(app);

    fanzhou::core::CoreConfig config = fanzhou::core::CoreConfig::makeDefault();
    QString error;
    bool configLoaded = config.loadFromFile(configPath, &error);

    // 2. 初始化日志系统
    const QString logPath = config.log.logToFile ? config.log.logFilePath : QString();
    const fanzhou::LogLevel logLevel = static_cast<fanzhou::LogLevel>(config.log.logLevel);
    fanzhou::Logger::instance().init(logPath, logLevel, config.log.logToConsole);

    LOG_INFO(kLogSource, QStringLiteral("FanZhou RPC Server starting..."));
    LOG_INFO(kLogSource, QStringLiteral("Config file: %1").arg(configPath));

    if (!configLoaded) {
        LOG_WARNING(kLogSource,
                    QStringLiteral("Failed to load config: %1 -> Writing default config").arg(error));

        QString mkError;
        if (!ensureParentDir(configPath, &mkError)) {
            LOG_ERROR(kLogSource, QStringLiteral("Failed to create config directory: %1").arg(mkError));
        } else {
            QString saveError;
            if (!config.saveToFile(configPath, &saveError)) {
                LOG_ERROR(kLogSource, QStringLiteral("Failed to save default config: %1").arg(saveError));
            } else {
                LOG_INFO(kLogSource, QStringLiteral("Default config saved to: %1").arg(configPath));
            }
        }
    } else {
        LOG_INFO(kLogSource, QStringLiteral("Configuration loaded successfully"));
    }

    // 3. 初始化核心上下文
    fanzhou::core::CoreContext context;
    // 设置配置文件路径，使config.save RPC方法可以正确保存配置
    context.configFilePath = configPath;
    
    LOG_INFO(kLogSource, QStringLiteral("Initializing core context..."));
    if (!context.init(config)) {
        LOG_CRITICAL(kLogSource, QStringLiteral("Core context initialization failed"));
        return 1;
    }
    LOG_INFO(kLogSource, QStringLiteral("Core context initialized"));

    // 4. 注册RPC方法
    LOG_INFO(kLogSource, QStringLiteral("Registering RPC methods..."));
    fanzhou::rpc::JsonRpcDispatcher dispatcher;
    fanzhou::core::RpcRegistry registry(&context, &dispatcher);
    registry.registerAll();
    LOG_INFO(kLogSource, QStringLiteral("RPC methods registered"));

    // 5. 启动JSON-RPC服务器
    fanzhou::rpc::JsonRpcServer server(&dispatcher);
    const quint16 port = context.rpcPort;
    LOG_INFO(kLogSource, QStringLiteral("Starting JSON-RPC server on port %1...").arg(port));

    if (!server.listen(QHostAddress::Any, port)) {
        LOG_CRITICAL(kLogSource, QStringLiteral("Listen failed: %1").arg(server.errorString()));
        return 1;
    }

    LOG_INFO(kLogSource,
             QStringLiteral("Server started! JSON-RPC port: %1, Config: %2")
                 .arg(port)
                 .arg(configPath));

    return app.exec();
}
