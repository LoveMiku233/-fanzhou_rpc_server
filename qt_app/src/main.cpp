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
#include <QScreen>
#include <QGuiApplication>

#include "mainwindow.h"
#include "style_constants.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("泛舟RPC客户端"));
    app.setApplicationVersion(QStringLiteral("1.1.0"));
    app.setOrganizationName(QStringLiteral("FanZhou"));

    // 设置应用程序字体 - 针对7寸1024x600触屏优化
    QFont defaultFont = app.font();
    defaultFont.setFamily(QStringLiteral("Ubuntu,DejaVu Sans,Noto Sans CJK SC,Sans-serif"));
    defaultFont.setPointSize(10);  // 优化字体大小，在小屏幕上获得完美显示
    app.setFont(defaultFont);

    // 加载浅色温和样式表
    QFile styleFile(QStringLiteral(":/styles/style.qss"));
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = QString::fromUtf8(styleFile.readAll());
        app.setStyleSheet(styleSheet);
        styleFile.close();
    }

    MainWindow mainWindow;
    mainWindow.setWindowTitle(QStringLiteral("泛舟RPC客户端 - 温室控制系统"));

    // 默认尺寸 + 最小尺寸，避免固定窗口导致卡片在高DPI/窗口管理器下被压缩
    mainWindow.resize(UIConstants::WINDOW_WIDTH, UIConstants::WINDOW_HEIGHT);
    mainWindow.setMinimumSize(900, 560);

    // 居中显示
    const QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect screenGeometry = screen->availableGeometry();
        mainWindow.move((screenGeometry.width() - UIConstants::WINDOW_WIDTH) / 2,
                        (screenGeometry.height() - UIConstants::WINDOW_HEIGHT) / 2);
    }

    mainWindow.show();

    return app.exec();
}
