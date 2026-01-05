/**
 * @file rpc_error_codes.h
 * @brief JSON-RPC错误码定义
 *
 * 定义标准和应用特定的JSON-RPC错误码。
 */

#ifndef FANZHOU_RPC_ERROR_CODES_H
#define FANZHOU_RPC_ERROR_CODES_H

namespace fanzhou {
namespace rpc {

/**
 * @brief RPC错误码
 *
 * JSON-RPC 2.0标准错误码: -32768 到 -32000
 * 应用特定错误码: -60000 到 -60199
 */
namespace RpcError {

// JSON-RPC 2.0 标准错误
constexpr int ParseError = -32700;      ///< 接收到无效JSON
constexpr int InvalidRequest = -32600;  ///< 不是有效的请求对象
constexpr int MethodNotFound = -32601;  ///< 方法不存在
constexpr int InvalidParams = -32602;   ///< 无效的方法参数
constexpr int InternalError = -32603;   ///< 内部JSON-RPC错误

// 保留给实现定义的服务器错误
constexpr int ServerErrorBase = -32000;

// 应用定义的错误
constexpr int NotImplemented = -60000;     ///< 功能未实现
constexpr int Busy = -60001;               ///< 服务器忙
constexpr int Timeout = -60002;            ///< 操作超时
constexpr int PermissionDenied = -60003;   ///< 权限拒绝

// 参数错误
constexpr int MissingParameter = -60010;   ///< 缺少必需参数
constexpr int BadParameterType = -60011;   ///< 参数类型不匹配
constexpr int BadParameterValue = -60012;  ///< 无效的参数值
constexpr int InvalidState = -60013;       ///< 无效的操作状态

// 串口错误
constexpr int SerialNotOpened = -60100;    ///< 串口未打开
constexpr int SerialOpenFailed = -60101;   ///< 打开串口失败
constexpr int SerialWriteFailed = -60102;  ///< 串口写入失败
constexpr int SerialReadFailed = -60103;   ///< 串口读取失败

// CAN错误
constexpr int CanNotOpened = -60120;       ///< CAN未打开
constexpr int CanOpenFailed = -60121;      ///< 打开CAN失败
constexpr int CanWriteFailed = -60122;     ///< CAN写入失败
constexpr int CanReadFailed = -60123;      ///< CAN读取失败
constexpr int CanPayloadTooLong = -60124;  ///< CAN载荷超过8字节
constexpr int CanInvalidId = -60125;       ///< 无效的CAN ID

}  // namespace RpcError
}  // namespace rpc
}  // namespace fanzhou

#endif  // FANZHOU_RPC_ERROR_CODES_H
