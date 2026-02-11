/**
 * @file screen_manager.h
 * @brief 屏幕管理器头文件 - 自动息屏功能
 *
 * 通过GPIO PD22控制屏幕背光，支持：
 * - 自动息屏：无触控操作后自动关闭屏幕
 * - 触控唤醒：检测到触控事件时自动亮起屏幕
 */

#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include <QObject>
#include <QTimer>
#include <QEvent>
#include <QApplication>

/**
 * @brief 屏幕管理器类 - 用于自动息屏和触控唤醒
 *
 * 使用GPIO PD22控制屏幕背光：
 * - PD22 = 1 (高电平): 屏幕亮起
 * - PD22 = 0 (低电平): 屏幕关闭
 *
 * 需要先挂载debugfs:
 *   mount -t debugfs debug /proc/sys/debug
 */
class ScreenManager : public QObject
{
    Q_OBJECT

public:
    explicit ScreenManager(QObject *parent = nullptr);
    ~ScreenManager() override;

    /**
     * @brief 启用自动息屏功能
     * @param timeoutSeconds 无操作后息屏的时间（秒），默认60秒
     */
    void enableAutoScreenOff(int timeoutSeconds = 60);

    /**
     * @brief 禁用自动息屏功能
     */
    void disableAutoScreenOff();

    /**
     * @brief 检查自动息屏功能是否启用
     * @return true 如果启用
     */
    bool isAutoScreenOffEnabled() const;

    /**
     * @brief 获取当前息屏超时时间
     * @return 超时时间（秒）
     */
    int screenOffTimeout() const;

    /**
     * @brief 设置息屏超时时间
     * @param seconds 超时时间（秒）
     */
    void setScreenOffTimeout(int seconds);

    /**
     * @brief 手动打开屏幕
     */
    void turnScreenOn();

    /**
     * @brief 手动关闭屏幕
     */
    void turnScreenOff();

    /**
     * @brief 检查屏幕是否开启
     * @return true 如果屏幕开启
     */
    bool isScreenOn() const;

    /**
     * @brief 重置空闲计时器（有用户交互时调用）
     */
    void resetIdleTimer();

signals:
    /**
     * @brief 屏幕状态变化信号
     * @param on true 如果屏幕开启
     */
    void screenStateChanged(bool on);

    /**
     * @brief 日志消息信号
     */
    void logMessage(const QString &message, const QString &level = QStringLiteral("INFO"));

protected:
    /**
     * @brief 事件过滤器 - 用于捕获全局触控/鼠标事件
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    /**
     * @brief 空闲计时器超时 - 关闭屏幕
     */
    void onIdleTimeout();

private:
    /**
     * @brief 初始化GPIO（挂载debugfs并配置PD22）
     * @return true 如果成功
     */
    bool initGpio();

    /**
     * @brief 设置GPIO PD22的值
     * @param high true为高电平（屏幕亮），false为低电平（屏幕灭）
     * @return true 如果成功
     */
    bool setGpioValue(bool high);

    QTimer *idleTimer_;          ///< 空闲计时器
    int timeoutSeconds_;         ///< 息屏超时时间（秒）
    bool screenOn_;              ///< 屏幕当前状态
    bool autoScreenOffEnabled_;  ///< 自动息屏功能是否启用
    bool gpioInitialized_;       ///< GPIO是否已初始化

    static constexpr const char *kGpioDebugfsPath = "/proc/sys/debug/sunxi_pinctrl";
    static constexpr const char *kGpioPin = "PD22";
};

#endif // SCREEN_MANAGER_H
