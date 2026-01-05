/**
 * @file serial_comm.h
 * @brief 串口通信适配器
 *
 * 在Linux上提供支持RS485的串口通信。
 */

#ifndef FANZHOU_SERIAL_COMM_H
#define FANZHOU_SERIAL_COMM_H

#include <QObject>
#include <QSocketNotifier>

#include "base/comm_adapter.h"

namespace fanzhou {
namespace comm {

/**
 * @brief 串口配置
 */
struct SerialConfig {
    QString device;                 ///< 设备路径（如"/dev/ttyS0"）
    int baudRate = 115200;          ///< 波特率
    int dataBits = 8;               ///< 数据位（5-8）
    int stopBits = 1;               ///< 停止位（1或2）
    char parity = 'N';              ///< 校验：'N'=无, 'E'=偶, 'O'=奇
    bool rs485 = false;             ///< 启用RS485模式
    int rs485DelayBeforeUs = 0;     ///< RS485发送前延迟（微秒）
    int rs485DelayAfterUs = 0;      ///< RS485发送后延迟（微秒）
};

/**
 * @brief 串口通信适配器
 *
 * 实现带可选RS485模式支持的串口通信。
 */
class SerialComm : public CommAdapter
{
    Q_OBJECT

public:
    /**
     * @brief 构造串口通信适配器
     * @param config 串口配置
     * @param parent 父对象
     */
    explicit SerialComm(SerialConfig config, QObject *parent = nullptr);

    ~SerialComm() override;

    bool open() override;
    void close() override;
    qint64 writeBytes(const QByteArray &data) override;

private slots:
    void onReadable();

private:
    bool setupTermios();
    bool setupRs485IfNeeded();

    SerialConfig config_;
    int fd_ = -1;
    QSocketNotifier *readNotifier_ = nullptr;
};

}  // namespace comm
}  // namespace fanzhou

#endif  // FANZHOU_SERIAL_COMM_H
