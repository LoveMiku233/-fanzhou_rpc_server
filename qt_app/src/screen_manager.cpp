/**
 * @file screen_manager.cpp
 * @brief 屏幕管理器实现 - 自动息屏功能
 *
 * 通过GPIO PD22控制屏幕背光，支持自动息屏和触控唤醒。
 */

#include "screen_manager.h"

#include <QFile>
#include <QProcess>
#include <QDir>
#include <QMouseEvent>
#include <QTouchEvent>
#include <QDebug>

ScreenManager::ScreenManager(QObject *parent)
    : QObject(parent)
    , idleTimer_(new QTimer(this))
    , timeoutSeconds_(60)
    , screenOn_(true)
    , autoScreenOffEnabled_(false)
    , gpioInitialized_(false)
{
    connect(idleTimer_, &QTimer::timeout, this, &ScreenManager::onIdleTimeout);
    
    // 尝试初始化GPIO
    gpioInitialized_ = initGpio();
    if (!gpioInitialized_) {
        qWarning() << "[SCREEN_MANAGER] GPIO初始化失败，自动息屏功能可能无法正常工作";
    } else {
        qDebug() << "[SCREEN_MANAGER] GPIO初始化成功";
    }
}

ScreenManager::~ScreenManager()
{
    disableAutoScreenOff();
    
    // 确保屏幕在退出时是亮的
    if (!screenOn_) {
        turnScreenOn();
    }
}

void ScreenManager::enableAutoScreenOff(int timeoutSeconds)
{
    if (timeoutSeconds <= 0) {
        timeoutSeconds = 60;
    }
    
    timeoutSeconds_ = timeoutSeconds;
    autoScreenOffEnabled_ = true;
    
    // 安装全局事件过滤器以捕获所有触控/鼠标事件
    if (QApplication::instance()) {
        QApplication::instance()->installEventFilter(this);
    }
    
    // 启动空闲计时器
    idleTimer_->start(timeoutSeconds_ * 1000);
    
    qDebug() << "[SCREEN_MANAGER] 自动息屏已启用，超时时间:" << timeoutSeconds_ << "秒";
    emit logMessage(QStringLiteral("自动息屏已启用，超时时间: %1秒").arg(timeoutSeconds_));
}

void ScreenManager::disableAutoScreenOff()
{
    autoScreenOffEnabled_ = false;
    idleTimer_->stop();
    
    // 移除全局事件过滤器
    if (QApplication::instance()) {
        QApplication::instance()->removeEventFilter(this);
    }
    
    // 确保屏幕亮起
    if (!screenOn_) {
        turnScreenOn();
    }
    
    qDebug() << "[SCREEN_MANAGER] 自动息屏已禁用";
    emit logMessage(QStringLiteral("自动息屏已禁用"));
}

bool ScreenManager::isAutoScreenOffEnabled() const
{
    return autoScreenOffEnabled_;
}

int ScreenManager::screenOffTimeout() const
{
    return timeoutSeconds_;
}

void ScreenManager::setScreenOffTimeout(int seconds)
{
    if (seconds <= 0) {
        seconds = 60;
    }
    
    timeoutSeconds_ = seconds;
    
    // 如果计时器正在运行，重新启动
    if (idleTimer_->isActive()) {
        idleTimer_->start(timeoutSeconds_ * 1000);
    }
    
    qDebug() << "[SCREEN_MANAGER] 息屏超时时间设置为:" << timeoutSeconds_ << "秒";
}

void ScreenManager::turnScreenOn()
{
    if (screenOn_) {
        return;
    }
    
    if (setGpioValue(true)) {
        screenOn_ = true;
        qDebug() << "[SCREEN_MANAGER] 屏幕已亮起";
        emit screenStateChanged(true);
    } else {
        qWarning() << "[SCREEN_MANAGER] 屏幕亮起失败";
    }
}

void ScreenManager::turnScreenOff()
{
    if (!screenOn_) {
        return;
    }
    
    if (setGpioValue(false)) {
        screenOn_ = false;
        qDebug() << "[SCREEN_MANAGER] 屏幕已关闭";
        emit screenStateChanged(false);
    } else {
        qWarning() << "[SCREEN_MANAGER] 屏幕关闭失败";
    }
}

bool ScreenManager::isScreenOn() const
{
    return screenOn_;
}

void ScreenManager::resetIdleTimer()
{
    if (autoScreenOffEnabled_ && idleTimer_) {
        idleTimer_->start(timeoutSeconds_ * 1000);
    }
    
    // 如果屏幕是关的，打开它
    if (!screenOn_) {
        turnScreenOn();
    }
}

bool ScreenManager::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)
    
    // 检测任何用户交互事件
    switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseMove:
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        case QEvent::Wheel:
            // 有用户交互，重置计时器并唤醒屏幕
            resetIdleTimer();
            break;
        default:
            break;
    }
    
    // 不拦截事件，让它继续传递
    return false;
}

void ScreenManager::onIdleTimeout()
{
    if (autoScreenOffEnabled_ && screenOn_) {
        qDebug() << "[SCREEN_MANAGER] 空闲超时，关闭屏幕";
        emit logMessage(QStringLiteral("屏幕已自动关闭"));
        turnScreenOff();
    }
}

bool ScreenManager::initGpio()
{
    // 检查debugfs是否已挂载
    QDir debugfsDir(kGpioDebugfsPath);
    if (!debugfsDir.exists()) {
        qDebug() << "[SCREEN_MANAGER] debugfs未挂载，尝试挂载...";
        
        // 尝试挂载debugfs
        QProcess mountProcess;
        mountProcess.start(QStringLiteral("mount"), 
            QStringList() << QStringLiteral("-t") << QStringLiteral("debugfs") 
                          << QStringLiteral("debug") << QStringLiteral("/proc/sys/debug"));
        
        if (!mountProcess.waitForFinished(5000)) {
            qWarning() << "[SCREEN_MANAGER] 挂载debugfs超时";
            return false;
        }
        
        if (mountProcess.exitCode() != 0) {
            // 可能已经挂载了，忽略错误
            qDebug() << "[SCREEN_MANAGER] 挂载debugfs返回码:" << mountProcess.exitCode() 
                     << "(" << mountProcess.readAllStandardError() << ")";
        }
    }
    
    // 再次检查目录是否存在
    if (!debugfsDir.exists()) {
        qWarning() << "[SCREEN_MANAGER] debugfs目录不存在:" << kGpioDebugfsPath;
        // 在非目标硬件上（如开发机器）返回true以允许功能工作
        return true;
    }
    
    // 配置GPIO引脚
    QString sunxiPinPath = QString::fromLatin1("%1/sunxi_pin").arg(kGpioDebugfsPath);
    QFile sunxiPinFile(sunxiPinPath);
    
    if (sunxiPinFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        sunxiPinFile.write(kGpioPin);
        sunxiPinFile.close();
        qDebug() << "[SCREEN_MANAGER] GPIO引脚" << kGpioPin << "已配置";
        return true;
    } else {
        qWarning() << "[SCREEN_MANAGER] 无法打开" << sunxiPinPath;
        // 在非目标硬件上返回true
        return true;
    }
}

bool ScreenManager::setGpioValue(bool high)
{
    QString dataPath = QString::fromLatin1("%1/data").arg(kGpioDebugfsPath);
    QFile dataFile(dataPath);
    
    if (dataFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QString command = QStringLiteral("%1 %2").arg(QLatin1String(kGpioPin)).arg(high ? 1 : 0);
        dataFile.write(command.toLatin1());
        dataFile.close();
        qDebug() << "[SCREEN_MANAGER] GPIO" << kGpioPin << "设置为" << (high ? "高电平" : "低电平");
        return true;
    } else {
        // 如果GPIO文件不存在（非目标硬件），静默返回成功
        QFileInfo fi(dataPath);
        if (!fi.exists()) {
            qDebug() << "[SCREEN_MANAGER] GPIO数据文件不存在，可能在非目标硬件上运行";
            return true;
        }
        
        qWarning() << "[SCREEN_MANAGER] 无法打开GPIO数据文件:" << dataPath;
        return false;
    }
}
