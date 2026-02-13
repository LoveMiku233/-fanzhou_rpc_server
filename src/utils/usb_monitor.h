/**
 * @file usb_monitor.h
 * @brief U盘热插拔监控
 *
 * 定时检测U盘插入，自动完成日志备份、程序更新和配置替换。
 *
 * 功能：
 * 1. 将RPC服务器日志和QT界面日志备份到U盘（以时间戳命名）
 * 2. 检测U盘中的 fanzhou_rpc_server / fanzhou_rpc_client 并部署更新
 * 3. 检测U盘中的 core.json 并替换系统配置
 * 4. 操作完成后卸载U盘
 */

#ifndef FANZHOU_USB_MONITOR_H
#define FANZHOU_USB_MONITOR_H

#include <QObject>
#include <QTimer>
#include <QString>

namespace fanzhou {

/**
 * @brief U盘监控器
 *
 * 定时检测 /dev/sda1 是否存在，检测到后自动挂载、执行操作、卸载。
 */
class UsbMonitor : public QObject
{
    Q_OBJECT

public:
    explicit UsbMonitor(QObject *parent = nullptr);
    ~UsbMonitor() override;

    /**
     * @brief 启动U盘监控
     * @param intervalMs 检测间隔（毫秒），默认5000ms
     */
    void start(int intervalMs = 5000);

    /**
     * @brief 停止U盘监控
     */
    void stop();

    /**
     * @brief 设置RPC服务器日志路径
     * @param path 日志文件路径
     */
    void setRpcLogPath(const QString &path);

    /**
     * @brief 设置QT界面日志路径
     * @param path 日志文件路径
     */
    void setQtLogPath(const QString &path);

signals:
    /**
     * @brief U盘处理完成信号
     * @param message 处理结果描述
     */
    void usbProcessed(const QString &message);

private slots:
    void onCheckUsb();

private:
    /**
     * @brief 检测U盘设备是否存在
     */
    bool isUsbDevicePresent() const;

    /**
     * @brief 挂载U盘
     * @return 成功返回true
     */
    bool mountUsb();

    /**
     * @brief 卸载U盘
     * @return 成功返回true
     */
    bool unmountUsb();

    /**
     * @brief 备份日志到U盘
     */
    void backupLogs();

    /**
     * @brief 检测并部署程序更新
     * @return 需要重启返回true
     */
    bool deployUpdates();

    /**
     * @brief 检测并替换配置文件
     * @return 替换成功返回true
     */
    bool replaceConfig();

    /**
     * @brief 生成时间戳字符串
     */
    QString generateTimestamp() const;

    QTimer *timer_ = nullptr;
    bool processing_ = false;  ///< 防止重入

    QString rpcLogPath_;       ///< RPC服务器日志路径
    QString qtLogPath_;        ///< QT界面日志路径

    static const QString kUsbDevice;      ///< U盘设备路径
    static const QString kMountPoint;     ///< 挂载点
    static const QString kRpcServerBin;   ///< RPC服务器部署目录
    static const QString kRpcClientBin;   ///< RPC客户端部署目录
    static const QString kConfigPath;     ///< 系统配置路径
};

}  // namespace fanzhou

#endif  // FANZHOU_USB_MONITOR_H
