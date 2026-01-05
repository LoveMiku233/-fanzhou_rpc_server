/**
 * @file comm_adapter.h
 * @brief 通信适配器抽象接口
 *
 * 定义所有通信适配器（CAN、串口等）的基础接口。
 */

#ifndef FANZHOU_COMM_ADAPTER_H
#define FANZHOU_COMM_ADAPTER_H

#include <QByteArray>
#include <QObject>

namespace fanzhou {
namespace comm {

/**
 * @brief 通信适配器抽象基类
 *
 * 为不同通信协议提供通用接口。
 * 子类实现特定协议如CAN或串口。
 */
class CommAdapter : public QObject
{
    Q_OBJECT

public:
    explicit CommAdapter(QObject *parent = nullptr)
        : QObject(parent) {}

    virtual ~CommAdapter() = default;

    /**
     * @brief 打开通信通道
     * @return 成功返回true，否则返回false
     */
    virtual bool open() = 0;

    /**
     * @brief 关闭通信通道
     */
    virtual void close() = 0;

    /**
     * @brief 向通信通道写入字节
     * @param data 要写入的数据
     * @return 写入的字节数，错误返回-1
     */
    virtual qint64 writeBytes(const QByteArray &data) = 0;

signals:
    /**
     * @brief 接收到数据时发出
     * @param data 接收到的数据
     */
    void bytesReceived(const QByteArray &data);

    /**
     * @brief 发生错误时发出
     * @param msg 错误消息
     */
    void errorOccurred(const QString &msg);

    /**
     * @brief 通道打开时发出
     */
    void opened();

    /**
     * @brief 通道关闭时发出
     */
    void closed();
};

}  // namespace comm
}  // namespace fanzhou

#endif  // FANZHOU_COMM_ADAPTER_H
