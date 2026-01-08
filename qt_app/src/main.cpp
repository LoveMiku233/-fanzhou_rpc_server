/**
 * @file main.cpp
 * @brief 泛舟RPC客户端主入口
 *
 * Qt5.12 GUI客户端，用于连接和控制泛舟RPC服务器。
 * 目标平台：Ubuntu Desktop
 */

#include <QApplication>
#include <QFile>
#include <QFont>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("泛舟RPC客户端"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("FanZhou"));

    // 设置应用程序字体
    QFont defaultFont = app.font();
    defaultFont.setFamily(QStringLiteral("Microsoft YaHei,SimHei,Sans"));
    defaultFont.setPointSize(10);
    app.setFont(defaultFont);

    // 加载样式表
    QFile styleFile(QStringLiteral(":/styles/style.qss"));
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = QString::fromUtf8(styleFile.readAll());
        app.setStyleSheet(styleSheet);
        styleFile.close();
    }

    MainWindow mainWindow;
    mainWindow.setWindowTitle(QStringLiteral("泛舟RPC客户端 - 温室控制系统"));
    mainWindow.resize(1200, 800);
    mainWindow.show();

    return app.exec();
}
