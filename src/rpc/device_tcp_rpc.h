/**
 * @file device_tcp_rpc.h
 * @brief Device TCP RPC method registration helpers
 */

#ifndef FANZHOU_DEVICE_TCP_RPC_H
#define FANZHOU_DEVICE_TCP_RPC_H

namespace fanzhou {
namespace rpc {

class JsonRpcDispatcher;
class DeviceTcpServer;

void registerDeviceTcpMethods(JsonRpcDispatcher *dispatcher, DeviceTcpServer *deviceServer);

}  // namespace rpc
}  // namespace fanzhou

#endif  // FANZHOU_DEVICE_TCP_RPC_H

