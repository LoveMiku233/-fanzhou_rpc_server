/**
 * @file rpc_registry.h
 * @brief RPC method registry
 *
 * Registers all RPC methods with the dispatcher.
 */

#ifndef FANZHOU_RPC_REGISTRY_H
#define FANZHOU_RPC_REGISTRY_H

#include <QObject>

namespace fanzhou {

namespace rpc {
class JsonRpcDispatcher;
}

namespace core {
class CoreContext;

/**
 * @brief RPC method registry
 *
 * Registers all available RPC methods organized by category.
 */
class RpcRegistry : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct an RPC registry
     * @param context Core context
     * @param dispatcher RPC dispatcher
     * @param parent Parent object
     */
    explicit RpcRegistry(CoreContext *context, rpc::JsonRpcDispatcher *dispatcher,
                         QObject *parent = nullptr);

    /**
     * @brief Register all RPC methods
     */
    void registerAll();

private:
    void registerBase();
    void registerSystem();
    void registerCan();
    void registerRelay();
    void registerGroup();
    void registerAuto();

    CoreContext *context_;
    rpc::JsonRpcDispatcher *dispatcher_;
};

}  // namespace core
}  // namespace fanzhou

#endif  // FANZHOU_RPC_REGISTRY_H
