/**
 * @file rpc_registry.h
 * @brief RPC方法注册器
 *
 * 将所有RPC方法注册到分发器。
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
 * @brief RPC方法注册器
 *
 * 按类别注册所有可用的RPC方法。
 */
class RpcRegistry : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造RPC注册器
     * @param context 核心上下文
     * @param dispatcher RPC分发器
     * @param parent 父对象
     */
    explicit RpcRegistry(CoreContext *context, rpc::JsonRpcDispatcher *dispatcher,
                         QObject *parent = nullptr);

    /**
     * @brief 注册所有RPC方法
     */
    void registerAll();

private:
    void registerBase();
    void registerSystem();
    void registerCan();
    void registerRelay();
    void registerGroup();
    void registerAuto();
    void registerDevice();
    void registerScreen();
    void registerConfig();  ///< 配置保存/加载相关方法
    void registerMqtt();    ///< MQTT多通道管理相关方法
    void registerMonitor(); ///< 系统资源监控相关方法
    void registerAuth();    ///< 认证相关方法
    void registerScene();   ///< 场景管理相关方法

    CoreContext *context_;
    rpc::JsonRpcDispatcher *dispatcher_;
};

}  // namespace core
}  // namespace fanzhou

#endif  // FANZHOU_RPC_REGISTRY_H
