#ifndef COMM_SERIAL_H
#define COMM_SERIAL_H

#include <QObject>
#include "base/comm_adapter.h"
#include <QSocketNotifier>

struct SerialConfig {
    QString dev;     // "/dev/ttyS0" "/dev/ttyS2" "/dev/ttyUSB1"
    int baud = 115200;
    int dataBits = 8;
    int stopBits = 1;
    char parity = 'N'; // N/E/O
    bool rs485 = false;
    int rs485DelayBeforeUs = 0;
    int rs485DelayAfterUs = 0;
};

class CommSerial : public CommAdapter
{
    Q_OBJECT
public:
    explicit CommSerial(SerialConfig cfg, QObject* parent = nullptr)
        : CommAdapter(parent), cfg_(std::move(cfg)){};

    bool open() override;
    void close() override;
    int64_t wirteBytes(const QByteArray& data) override;

private slots:
void onReadable();

private:
bool setupTermios();
bool setupRs485IfNeeded();

SerialConfig cfg_;
int fd_ = -1;       // fd_
QSocketNotifier* rd_ = nullptr;
};

#endif // COMM_SERIAL_H
