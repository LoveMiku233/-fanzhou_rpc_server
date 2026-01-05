#ifndef JSON_RPC_SERVER_H
#define JSON_RPC_SERVER_H

#include <QObject>
#include <QTcpServer>

class JsonRpcDispatcher;

class JsonRpcServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit JsonRpcServer(JsonRpcDispatcher* dispatcher ,QObject* parent = nullptr);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    JsonRpcDispatcher* dispatcher_ = nullptr;
    QHash<QTcpSocket*, QByteArray> buffers_;

    void processLines(QTcpSocket* s);
    static QByteArray toLine(const QJsonObject& obj);

};


#endif // JSON_RPC_SERVER_H
