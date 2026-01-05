#ifndef COMM_ADAPTER_H
#define COMM_ADAPTER_H

#include <QObject>
#include <QByteArray>

class CommAdapter : public QObject
{
    Q_OBJECT
public:
    explicit CommAdapter(QObject *parent = nullptr)
        : QObject(parent) {};

    virtual ~CommAdapter() = default;
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual int64_t wirteBytes(const QByteArray& data) = 0;

signals:
    void bytesReceived(const QByteArray& data);
    void errorOccurred(const QString& msg);
    void opened();
    void closed();

};

#endif // COMM_ADAPTER_H
