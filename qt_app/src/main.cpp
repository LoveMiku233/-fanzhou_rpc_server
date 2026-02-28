/**
 * @file main.cpp
 * @brief 泛舟RPC客户端入口
 *
 * 温室控制柜GUI客户端主入口。
 * 目标平台：Qt 5.12, 1024x600 触屏
 */

#include <QApplication>
#include <QFile>
#include <QTextStream>

#include "mainwindow.h"
#include "rpc_client.h"
#include "screen_manager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("fanzhou-rpc-client"));
    app.setApplicationVersion(QStringLiteral("2.0.0"));

    // 加载深色主题样式表
    QFile styleFile(QStringLiteral(":/styles/dark_theme.qss"));
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream stream(&styleFile);
        app.setStyleSheet(stream.readAll());
        styleFile.close();
    }

    // 创建RPC客户端
    RpcClient rpcClient;

    // 创建屏幕管理器
    ScreenManager screenManager;

    // 创建并显示主窗口
    MainWindow mainWindow(&rpcClient, &screenManager);
    mainWindow.show();

    return app.exec();
}
