/**
 * @file rpc_error_codes.h
 * @brief JSON-RPC error code definitions
 *
 * Defines standard and application-specific JSON-RPC error codes.
 */

#ifndef FANZHOU_RPC_ERROR_CODES_H
#define FANZHOU_RPC_ERROR_CODES_H

namespace fanzhou {
namespace rpc {

/**
 * @brief RPC error codes
 *
 * Standard JSON-RPC 2.0 error codes: -32768 to -32000
 * Application-specific codes: -60000 to -60199
 */
namespace RpcError {

// JSON-RPC 2.0 Standard errors
constexpr int ParseError = -32700;      ///< Invalid JSON received
constexpr int InvalidRequest = -32600;  ///< Not a valid Request object
constexpr int MethodNotFound = -32601;  ///< Method does not exist
constexpr int InvalidParams = -32602;   ///< Invalid method parameters
constexpr int InternalError = -32603;   ///< Internal JSON-RPC error

// Reserved for implementation-defined server errors
constexpr int ServerErrorBase = -32000;

// Application-defined errors
constexpr int NotImplemented = -60000;     ///< Feature not implemented
constexpr int Busy = -60001;               ///< Server busy
constexpr int Timeout = -60002;            ///< Operation timeout
constexpr int PermissionDenied = -60003;   ///< Permission denied

// Parameter errors
constexpr int MissingParameter = -60010;   ///< Required parameter missing
constexpr int BadParameterType = -60011;   ///< Parameter type mismatch
constexpr int BadParameterValue = -60012;  ///< Invalid parameter value
constexpr int InvalidState = -60013;       ///< Invalid operation state

// Serial errors
constexpr int SerialNotOpened = -60100;    ///< Serial port not open
constexpr int SerialOpenFailed = -60101;   ///< Failed to open serial port
constexpr int SerialWriteFailed = -60102;  ///< Serial write failed
constexpr int SerialReadFailed = -60103;   ///< Serial read failed

// CAN errors
constexpr int CanNotOpened = -60120;       ///< CAN not open
constexpr int CanOpenFailed = -60121;      ///< Failed to open CAN
constexpr int CanWriteFailed = -60122;     ///< CAN write failed
constexpr int CanReadFailed = -60123;      ///< CAN read failed
constexpr int CanPayloadTooLong = -60124;  ///< CAN payload > 8 bytes
constexpr int CanInvalidId = -60125;       ///< Invalid CAN ID

}  // namespace RpcError
}  // namespace rpc
}  // namespace fanzhou

#endif  // FANZHOU_RPC_ERROR_CODES_H
