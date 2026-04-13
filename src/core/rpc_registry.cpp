/**
 * @file rpc_registry.cpp
 * @brief RPC方法注册器主入口（按模块分发）
 */

#include "rpc_registry.h"

namespace fanzhou {
namespace core {

RpcRegistry::RpcRegistry(CoreContext *context, rpc::JsonRpcDispatcher *dispatcher,
                         QObject *parent)
    : QObject(parent)
    , context_(context)
    , dispatcher_(dispatcher)
{
}

void RpcRegistry::registerAll()
{
    registerBase();
    registerSystem();
    registerCan();
    registerRelay();
    registerGroup();
    registerAuto();
    registerDevice();
    registerScreen();
    registerConfig();
    registerMqtt();
    registerMonitor();
    registerAuth();
    registerScene();
}

}  // namespace core
}  // namespace fanzhou
