#ifndef DEVICE_ADAPTER_H
#define DEVICE_ADAPTER_H

#include <QObject>
#include <QString>

// 纯抽象基类

class DeviceAdapter : public QObject
{
    Q_OBJECT
public:
    explicit DeviceAdapter(QObject* parent = nullptr)
            : QObject(parent) {}

    virtual ~DeviceAdapter() = default;
    virtual bool init() = 0;
    virtual void poll() = 0;
    virtual QString name() const = 0;

signals:
    void updated();
};

#endif // DEVICE_ADAPTER_H
