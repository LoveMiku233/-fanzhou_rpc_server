/**
 * @file system_settings.h
 * @brief 系统设置和命令执行
 *
 * 提供CAN配置和系统命令执行功能。
 */

#ifndef FANZHOU_SYSTEM_SETTINGS_H
#define FANZHOU_SYSTEM_SETTINGS_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;

namespace fanzhou {
namespace config {

/**
 * @brief 系统设置控制器
 *
 * 提供CAN总线配置和系统命令执行，
 * 包括CAN接口启停、波特率设置和帧发送。
 */
class SystemSettings : public QObject
{
    Q_OBJECT

public:
    explicit SystemSettings(QObject *parent = nullptr);

    /**
     * @brief 执行系统命令（阻塞）
     * @param program 程序名
     * @param args 命令参数
     * @param timeoutMs 超时（毫秒）
     * @return 命令输出，失败返回空
     */
    QString runCommand(const QString &program,
                       const QStringList &args,
                       int timeoutMs = 5000);

    /**
     * @brief 关闭CAN接口
     * @param interface 接口名
     * @return 成功返回true
     */
    bool canDown(const QString &interface);

    /**
     * @brief 启动CAN接口
     * @param interface 接口名
     * @return 成功返回true
     */
    bool canUp(const QString &interface);

    /**
     * @brief 设置CAN波特率
     * @param interface 接口名
     * @param bitrate 波特率（位/秒）
     * @param tripleSampling 启用三次采样
     * @return 成功返回true
     */
    bool setCanBitrate(const QString &interface, int bitrate,
                       bool tripleSampling = false);

    /**
     * @brief 使用cansend工具发送CAN帧
     * @param interface 接口名
     * @param canId CAN标识符
     * @param data 帧数据
     * @param extended 使用扩展标识符
     * @return 成功返回true
     */
    bool sendCanFrame(const QString &interface, quint32 canId,
                      const QByteArray &data, bool extended = false);

    /**
     * @brief 启动candump捕获
     * @param interface 接口名
     * @param extraArgs 额外参数
     * @return 启动成功返回true
     */
    bool startCanDump(const QString &interface,
                      const QStringList &extraArgs = {});

    /**
     * @brief 停止candump捕获
     */
    void stopCanDump();

    // ===================== RTC时间管理 =====================

    /**
     * @brief 获取系统时间
     * @return 当前系统时间字符串（ISO格式）
     */
    QString getSystemTime();

    /**
     * @brief 设置系统时间
     * @param datetime 日期时间字符串（格式：YYYY-MM-DD HH:mm:ss）
     * @return 成功返回true
     */
    bool setSystemTime(const QString &datetime);

    /**
     * @brief 保存系统时间到硬件时钟
     * @return 成功返回true
     */
    bool saveHardwareClock();

    /**
     * @brief 从硬件时钟读取时间
     * @return 硬件时钟时间字符串
     */
    QString readHardwareClock();

    // ===================== 网络配置管理 =====================

    /**
     * @brief 获取网络接口信息
     * @param interface 接口名（如eth0），空则获取所有接口
     * @return 网络信息字符串
     */
    QString getNetworkInfo(const QString &interface = QString());

    /**
     * @brief 测试网络连通性
     * @param host 主机地址或域名
     * @param count ping次数
     * @param timeoutSec 超时秒数
     * @return 成功返回true
     */
    bool pingTest(const QString &host, int count = 4, int timeoutSec = 10);

    /**
     * @brief 设置静态IP地址
     * @param interface 接口名
     * @param address IP地址
     * @param netmask 子网掩码
     * @param gateway 网关地址
     * @return 成功返回true
     */
    bool setStaticIp(const QString &interface,
                     const QString &address,
                     const QString &netmask,
                     const QString &gateway);

signals:
    /**
     * @brief 命令输出时发出
     * @param line 输出行
     */
    void commandOutput(const QString &line);

    /**
     * @brief 发生错误时发出
     * @param error 错误消息
     */
    void errorOccurred(const QString &error);

    /**
     * @brief candump输出行时发出
     * @param line CAN帧数据
     */
    void candumpLine(const QString &line);

private:
    static QString toCanSendArg(quint32 canId, const QByteArray &data,
                                 bool extended);

    QProcess *dumpProcess_ = nullptr;
};

}  // namespace config
}  // namespace fanzhou

#endif  // FANZHOU_SYSTEM_SETTINGS_H
