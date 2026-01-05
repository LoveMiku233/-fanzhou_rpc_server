#ifndef RPC_REGISTRY_H
#define RPC_REGISTRY_H

#include <QObject>

class JsonRpcDispatcher;
class CoreContext;

class RpcRegistry : public QObject
{
    Q_OBJECT
public:
    explicit RpcRegistry(CoreContext* ctx, JsonRpcDispatcher* disp, QObject* parent = nullptr);
    void registerAll();

private:
    CoreContext* ctx_;
    JsonRpcDispatcher* disp_;

    void registerBase();
    void registerSys();
    void registerCan();
    void registerRelay();
    void registerGroup();
    void registerAuto();
};

#endif // RPC_REGISTRY_H
