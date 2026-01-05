#ifndef CAN_DEVICE_MANAGER_H
#define CAN_DEVICE_MANAGER_H

#include <QObject>
#include <QVector>

class CommCan;
class ICanDevice;

class CanDeviceManager : public QObject
{
    Q_OBJECT
public:
    explicit CanDeviceManager(CommCan* bus, QObject* parent=nullptr);

    void addDevice(ICanDevice* dev);     // dev 生命周期由外部/Qt parent 管理
    void removeDevice(ICanDevice* dev);

    // 周期性 poll 所有 DeviceAdapter
    void pollAll();

private:
    CommCan* bus_ = nullptr;
    QVector<ICanDevice*> devices_;

    void onCanFrame(quint32 canId, const QByteArray& payload, bool extended, bool rtr);
};

#endif // CAN_DEVICE_MANAGER_H
