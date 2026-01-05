/**
 * @file device_adapter.h
 * @brief 设备适配器抽象接口
 *
 * 定义所有设备适配器的基础接口。
 */

#ifndef FANZHOU_DEVICE_ADAPTER_H
#define FANZHOU_DEVICE_ADAPTER_H

#include <QObject>
#include <QString>

namespace fanzhou {
namespace device {

/**
 * @brief 设备适配器抽象基类
 *
 * 为不同设备类型提供通用接口。
 * 子类实现特定的设备协议。
 */
class DeviceAdapter : public QObject
{
    Q_OBJECT

public:
    explicit DeviceAdapter(QObject *parent = nullptr)
        : QObject(parent) {}

    virtual ~DeviceAdapter() = default;

    /**
     * @brief 初始化设备
     * @return 成功返回true
     */
    virtual bool init() = 0;

    /**
     * @brief 轮询设备以更新状态
     */
    virtual void poll() = 0;

    /**
     * @brief 获取设备名称
     * @return 设备名称字符串
     */
    virtual QString name() const = 0;

signals:
    /**
     * @brief 设备状态更新时发出
     */
    void updated();
};

}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_DEVICE_ADAPTER_H
