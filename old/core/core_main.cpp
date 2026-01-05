#include <QCoreApplication>
#include <QHostAddress>
#include <QDebug>
#include <QDir>

#include "rpc/json_rpc_server.h"
#include "rpc/json_rpc_dispatcher.h"
#include "rpc/rpc_registry.h"

#include "core/core_context.h"
#include "core/core_config.h"
#include "utils/logger.h"

static const char* LOG_SOURCE = "核心主程序";
static const QString DEFAULT_LOG_PATH = "/var/log/fanzhou_core/core.log";

/**
 * @brief 获取配置文件路径
 * @param app 应用程序实例
 * @return 配置文件路径
 */
static QString pickConfigPath(const QCoreApplication& app)
{
    Q_UNUSED(app);
    // TODO: 支持命令行参数指定配置路径
    return "/var/lib/fanzhou_core/core.json";
}

/**
 * @brief 确保父目录存在
 * @param filePath 文件路径
 * @param err 错误信息输出
 * @return 是否成功
 */
static bool ensureParentDir(const QString& filePath, QString* err=nullptr)
{
    const QString dirPath = QFileInfo(filePath).absolutePath();
    QDir d;
    if (d.exists(dirPath)) return true;
    if (!d.mkpath(dirPath)) {
        if (err) *err = QString("创建目录失败: %1").arg(dirPath);
        return false;
    }
    return true;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // 1) 先加载配置（需要获取日志配置）
    const QString cfgPath = pickConfigPath(app);

    CoreConfig cfg = CoreConfig::makeDefault();
    QString err;
    bool configLoaded = cfg.loadFromFile(cfgPath, &err);

    // 2) 使用配置初始化日志系统（配置加载失败时使用makeDefault的默认值）
    const QString logPath = cfg.log_.logToFile ? cfg.log_.logFilePath : QString();
    const LogLevel logLevel = static_cast<LogLevel>(cfg.log_.logLevel);
    Logger::instance().init(logPath, logLevel, cfg.log_.logToConsole);
    
    LOG_INFO(LOG_SOURCE, "大棚控制系统核心服务启动中...");
    LOG_INFO(LOG_SOURCE, QString("配置文件路径: %1").arg(cfgPath));

    if (!configLoaded) {
        LOG_WARNING(LOG_SOURCE, QString("加载配置失败: %1 -> 将写入默认配置").arg(err));

        QString mkErr;
        if (!ensureParentDir(cfgPath, &mkErr)) {
            LOG_ERROR(LOG_SOURCE, QString("创建配置目录失败: %1").arg(mkErr));
        } else {
            QString err2;
            if (!cfg.saveToFile(cfgPath, &err2)) {
                LOG_ERROR(LOG_SOURCE, QString("写入默认配置失败: %1").arg(err2));
            } else {
                LOG_INFO(LOG_SOURCE, QString("已写入默认配置到: %1").arg(cfgPath));
            }
        }
    } else {
        LOG_INFO(LOG_SOURCE, "配置加载成功");
    }

    // 3) 初始化核心上下文
    CoreContext ctx;
    LOG_INFO(LOG_SOURCE, "正在初始化核心上下文...");
    if (!ctx.init(cfg)) {
        LOG_CRITICAL(LOG_SOURCE, "核心上下文初始化失败");
        return 1;
    }
    LOG_INFO(LOG_SOURCE, "核心上下文初始化成功");

    // 4) 注册RPC方法
    LOG_INFO(LOG_SOURCE, "正在注册RPC方法...");
    JsonRpcDispatcher dispatcher;
    RpcRegistry reg(&ctx, &dispatcher);
    reg.registerAll();
    LOG_INFO(LOG_SOURCE, "RPC方法注册完成");

    // 5) 启动JSON-RPC服务器
    JsonRpcServer server(&dispatcher);
    const quint16 port = ctx.rpcPort;
    LOG_INFO(LOG_SOURCE, QString("正在启动JSON-RPC服务器，端口: %1...").arg(port));
    if (!server.listen(QHostAddress::Any, port)) {
        LOG_CRITICAL(LOG_SOURCE, QString("监听失败: %1").arg(server.errorString()));
        return 1;
    }

    LOG_INFO(LOG_SOURCE, QString("核心服务启动成功！JSON-RPC监听端口: %1, 配置文件: %2")
             .arg(port).arg(cfgPath));

    return app.exec();
}
