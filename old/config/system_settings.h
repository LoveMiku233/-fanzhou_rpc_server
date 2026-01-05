#ifndef SYSTEM_SETTINGS_H
#define SYSTEM_SETTINGS_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QByteArray>

class QProcess;

/**
 * @brief 系统设置类
 * 
 * 提供CAN总线配置和系统命令执行功能，
 * 包括CAN接口启停、波特率设置、帧发送和接收等
 */
class SystemSettings : public QObject
{
    Q_OBJECT
public:
    explicit SystemSettings(QObject* parent=nullptr);

    /**
     * @brief 执行系统命令（阻塞等待结束）
     * @param program 程序名
     * @param args 参数列表
     * @param timeoutMs 超时时间(毫秒)
     * @return 命令输出，失败时返回空并发出errorOccurred信号
     */
    QString runCommand(const QString& program,
                       const QStringList& args,
                       int timeoutMs = 5000);

    /**
     * @brief 关闭CAN接口
     * @param ifname 接口名称
     * @return 是否成功
     */
    bool canDown(const QString& ifname);
    
    /**
     * @brief 启动CAN接口
     * @param ifname 接口名称
     * @return 是否成功
     */
    bool canUp(const QString& ifname);

    /**
     * @brief 设置CAN波特率
     * @param ifname 接口名称
     * @param bitrate 波特率
     * @param tripleSampling 是否启用三重采样
     * @return 是否成功
     */
    bool setCanBitrate(const QString& ifname, int bitrate, bool tripleSampling = false);

    /**
     * @brief 发送CAN帧
     * @param ifname 接口名称
     * @param canId CAN ID
     * @param data 数据内容
     * @param extended 是否为扩展帧
     * @return 是否成功
     */
    bool sendCanFrame(const QString& ifname, quint32 canId, const QByteArray& data, bool extended = false);

    /**
     * @brief 启动CAN数据抓取
     * @param ifname 接口名称
     * @param extraArgs 额外参数
     * @return 是否成功
     */
    bool startCanDump(const QString& ifname, const QStringList& extraArgs = {});
    
    /**
     * @brief 停止CAN数据抓取
     */
    void stopCanDump();

signals:
    /**
     * @brief 命令输出信号
     * @param line 输出行内容
     */
    void commandOutput(const QString& line);
    
    /**
     * @brief 错误发生信号
     * @param error 错误信息
     */
    void errorOccurred(const QString& error);
    
    /**
     * @brief CAN抓取数据行信号
     * @param line CAN帧数据
     */
    void candumpLine(const QString& line);

private:
    QProcess* dumpProc_ = nullptr;

    /**
     * @brief 生成cansend命令参数
     * @param canId CAN ID
     * @param data 数据内容
     * @param extended 是否为扩展帧
     * @return cansend参数字符串
     */
    static QString toCanSendArg(quint32 canId, const QByteArray& data, bool extended);
};
#endif // SYSTEM_SETTINGS_H
