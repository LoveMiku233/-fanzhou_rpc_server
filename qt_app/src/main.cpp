/**
 * @file main.cpp
 * @brief 泛舟RPC客户端主入口
 *
 * Qt5.12 GUI客户端，用于连接和控制泛舟RPC服务器。
 * 目标平台：Ubuntu Desktop，7寸触屏(1024x600)
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

    // 设置应用程序字体 - 针对7寸1024x600触屏优化，增大字体便于阅读
    QFont defaultFont = app.font();
    defaultFont.setFamily(QStringLiteral("Ubuntu,DejaVu Sans,Noto Sans CJK SC,Sans-serif"));
    defaultFont.setPointSize(14);  // 增大字体便于7寸屏幕阅读
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
    // 适配7寸1024x600分辨率触屏，全屏显示以充分利用小屏幕空间
    mainWindow.showMaximized();

    return app.exec();
}
