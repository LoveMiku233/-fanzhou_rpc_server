// JSON-RPC 2.0 标准错误码：
// https://www.jsonrpc.org/specification#error_object
//
// 约定：
// - 标准错误码保持不变（-32768 ~ -32000）
// - 应用自定义错误码使用更小的负数区间，避免与标准冲突，例如：-60000 ~ -60199

namespace RpcError {

// -------- JSON-RPC 2.0 Standard errors --------
static constexpr int ParseError     = -32700; // Invalid JSON was received by the server.
static constexpr int InvalidRequest = -32600; // The JSON sent is not a valid Request object.
static constexpr int MethodNotFound = -32601; // The method does not exist / is not available.
static constexpr int InvalidParams  = -32602; // Invalid method parameter(s).
static constexpr int InternalError  = -32603; // Internal JSON-RPC error.

// Reserved for implementation-defined server-errors: -32000 to -32099
static constexpr int ServerErrorBase = -32000;

// -------- Application-defined errors (project specific) --------
// 通用
static constexpr int NotImplemented      = -60000;
static constexpr int Busy               = -60001;
static constexpr int Timeout            = -60002;
static constexpr int PermissionDenied   = -60003;

// 参数/状态
static constexpr int MissingParameter   = -60010; // params 缺字段
static constexpr int BadParameterType   = -60011; // 字段类型不对
static constexpr int BadParameterValue  = -60012; // 字段值非法（范围/格式）
static constexpr int InvalidState       = -60013; // 当前状态不允许此操作

// 串口相关
static constexpr int SerialNotOpened    = -60100;
static constexpr int SerialOpenFailed   = -60101;
static constexpr int SerialWriteFailed  = -60102;
static constexpr int SerialReadFailed   = -60103;

// CAN 相关
static constexpr int CanNotOpened       = -60120;
static constexpr int CanOpenFailed      = -60121;
static constexpr int CanWriteFailed     = -60122;
static constexpr int CanReadFailed      = -60123;
static constexpr int CanPayloadTooLong  = -60124; // classic CAN > 8 bytes
static constexpr int CanInvalidId       = -60125;

} // namespace RpcError
