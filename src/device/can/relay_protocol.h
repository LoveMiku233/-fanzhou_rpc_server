/**
 * @file relay_protocol.h
 * @brief CAN继电器协议定义
 *
 * 定义与CAN继电器设备通信的协议。
 */

#ifndef FANZHOU_RELAY_PROTOCOL_H
#define FANZHOU_RELAY_PROTOCOL_H

#include <QByteArray>
#include <QtGlobal>

#include <cstring>

namespace fanzhou {
namespace device {

/**
 * @brief 继电器CAN协议命名空间
 *
 * 包含协议常量、类型和编解码函数。
 */
namespace RelayProtocol {

// ========================= CAN ID 基地址定义 =========================
// 完整 CAN ID = 基地址 + 设备地址（0-15）

// 控制命令（发送到设备）
constexpr quint32 kSingleCtrlBaseId = 0x100;     ///< 单通道控制命令 (0x100-0x11F)
constexpr quint32 kMultiCtrlBaseId = 0x120;      ///< 多通道控制命令 (0x120-0x13F)
constexpr quint32 kSingleQueryBaseId = 0x140;    ///< 单通道查询命令 (0x140-0x15F)
constexpr quint32 kAllQueryBaseId = 0x160;       ///< 全通道查询命令 (0x160-0x17F)

// 状态响应（从设备接收）
constexpr quint32 kSingleStatusBaseId = 0x200;   ///< 单通道状态响应 (0x200-0x21F)
constexpr quint32 kAutoStatusBaseId = 0x220;     ///< 自动状态上报 (0x220-0x23F)

// 系统设置
constexpr quint32 kSettingsCmdBaseId = 0x300;    ///< 设置命令 (0x300-0x37F)
constexpr quint32 kSettingsRespBaseId = 0x380;   ///< 设置响应 (0x380-0x3FF)

// 旧版兼容（保持向后兼容）
constexpr quint32 kCtrlBaseId = kSingleCtrlBaseId;    ///< @deprecated 使用 kSingleCtrlBaseId
constexpr quint32 kStatusBaseId = kSingleStatusBaseId; ///< @deprecated 使用 kSingleStatusBaseId

/**
 * @brief 命令类型
 */
enum class CmdType : quint8 {
    ControlRelay = 0x01,       ///< 单通道继电器控制
    QueryStatus = 0x02,        ///< 单通道状态查询
    MultiControlRelay = 0x03,  ///< 多通道继电器控制
    QueryAllStatus = 0x04      ///< 全通道状态查询
};

/**
 * @brief 设置命令类型
 */
enum class SettingsCmdType : quint8 {
    SetDeviceAddress = 0x10,    ///< 设置设备地址
    SetCommMode = 0x11,         ///< 设置通信模式
    SetCanBitrate = 0x13,       ///< 设置CAN波特率
    SetLedStatus = 0x14,        ///< 设置LED状态
    SetCurrentThreshold = 0x16, ///< 设置电流阈值
    SetOvercurrentFlag = 0x17,  ///< 设置过流标志
    GetSystemStatus = 0x20,     ///< 获取系统状态
    GetSystemConfig = 0x21,     ///< 获取系统配置
    SaveConfig = 0x30,          ///< 保存配置到EEPROM
    SystemReboot = 0x3F         ///< 系统重启
};

/**
 * @brief 设置响应状态码
 */
enum class SettingsRespStatus : quint8 {
    Success = 0x00,           ///< 操作成功
    InvalidCommand = 0x01,    ///< 无效命令
    InvalidParameter = 0x02,  ///< 无效参数
    NotSupported = 0x03,      ///< 不支持的操作
    SystemBusy = 0x04,        ///< 系统忙
    OperationFailed = 0x05    ///< 操作失败
};

/**
 * @brief 继电器动作类型
 */
enum class Action : quint8 {
    Stop = 0x00,     ///< 停止（两路输出都关闭）
    Forward = 0x01,  ///< 正转方向
    Reverse = 0x02   ///< 反转方向
};

/**
 * @brief 控制命令结构
 */
struct CtrlCmd {
    CmdType type = CmdType::ControlRelay;
    quint8 channel = 0;         ///< 通道（0-3）
    Action action = Action::Stop;
};

/**
 * @brief 状态响应结构（单通道，0x20x响应）
 */
struct Status {
    quint8 channel = 0;       ///< 通道（0-3）
    quint8 statusByte = 0;    ///< 状态：0=停止, 1=正转, 2=反转
    quint8 phaseLostFlag = 0; ///< 缺相标志：0=正常, 1=缺相
    float currentA = 0.0f;    ///< 电流（安培）
    bool overcurrent = false; ///< 过流标志
};

/**
 * @brief 多通道控制命令结构
 */
struct MultiCtrlCmd {
    CmdType type = CmdType::MultiControlRelay;
    Action actions[4] = {Action::Stop, Action::Stop, Action::Stop, Action::Stop};
};

/**
 * @brief 自动状态上报结构（0x22x响应，压缩格式）
 *
 * 数据格式（8字节）：
 * - 字节0: 通道0-1状态（低4位=通道0, 高4位=通道1）
 * - 字节1: 通道2-3状态（低4位=通道2, 高4位=通道3）
 * - 字节2: 标志位（bit0-3=缺相, bit4-7=过流）
 * - 字节3-6: 通道0-3电流（0.1A单位）
 * - 字节7: 保留
 */
struct AutoStatusReport {
    Action status[4] = {Action::Stop, Action::Stop, Action::Stop, Action::Stop};
    bool phaseLost[4] = {false, false, false, false};
    bool overcurrent[4] = {false, false, false, false};
    float currentA[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

/**
 * @brief 设置命令结构
 */
struct SettingsCmd {
    SettingsCmdType type = SettingsCmdType::GetSystemStatus;
    quint8 param1 = 0;  ///< 参数1（通常是通道号或0xFF表示所有通道）
    quint8 param2 = 0;  ///< 参数2（具体参数值）
    quint8 param3 = 0;  ///< 参数3（保留）
};

/**
 * @brief 设置响应结构
 */
struct SettingsResp {
    SettingsCmdType cmdType = SettingsCmdType::GetSystemStatus;
    SettingsRespStatus status = SettingsRespStatus::Success;
};

/**
 * @brief 从状态字节提取模式位
 * @param statusByte 原始状态字节
 * @return 模式位（0-3）
 */
inline quint8 modeBits(quint8 statusByte)
{
    return statusByte & 0x03;
}

/**
 * @brief 从状态字节检查缺相标志
 * @param statusByte 原始状态字节
 * @return 如果缺相返回true
 */
inline bool phaseLost(quint8 statusByte)
{
    return (statusByte & 0x04) != 0;
}

/**
 * @brief 从4字节解码小端浮点数
 * @param bytes 指向4字节的指针
 * @return 解码后的浮点值
 */
inline float leFloat(const quint8 bytes[4])
{
    static_assert(sizeof(float) == 4, "float must be 4 bytes");
    quint32 u = (static_cast<quint32>(bytes[0])) |
                (static_cast<quint32>(bytes[1]) << 8) |
                (static_cast<quint32>(bytes[2]) << 16) |
                (static_cast<quint32>(bytes[3]) << 24);
    float f;
    std::memcpy(&f, &u, sizeof(float));
    return f;
}

/**
 * @brief 将浮点数编码为小端字节
 * @param out 输出字节数组
 * @param value 要编码的浮点值
 */
inline void putLeFloat(QByteArray &out, float value)
{
    quint32 u;
    std::memcpy(&u, &value, sizeof(float));
    out.append(static_cast<char>(u & 0xFF));
    out.append(static_cast<char>((u >> 8) & 0xFF));
    out.append(static_cast<char>((u >> 16) & 0xFF));
    out.append(static_cast<char>((u >> 24) & 0xFF));
}

/**
 * @brief 将控制命令编码为CAN载荷
 * @param cmd 控制命令
 * @return 8字节CAN载荷
 */
inline QByteArray encodeCtrl(const CtrlCmd &cmd)
{
    QByteArray data;
    data.reserve(8);
    data.append(static_cast<char>(cmd.type));
    data.append(static_cast<char>(cmd.channel));
    data.append(static_cast<char>(cmd.action));
    while (data.size() < 8) {
        data.append(char(0));
    }
    return data;
}

/**
 * @brief 从CAN载荷解码单通道状态（0x20x响应）
 * @param data CAN载荷（必须8字节）
 * @param status 输出状态结构
 * @return 解码成功返回true
 *
 * 数据格式：
 * - 字节0: 通道号（0-3）
 * - 字节1: 状态（0=停止, 1=正转, 2=反转）
 * - 字节2: 缺相标志（0=正常, 1=缺相）
 * - 字节3: 保留
 * - 字节4-7: 电流值（IEEE 754单精度浮点）
 */
inline bool decodeStatus(const QByteArray &data, Status &status)
{
    if (data.size() != 8) {
        return false;
    }
    status.channel = static_cast<quint8>(data[0]);
    status.statusByte = static_cast<quint8>(data[1]);
    status.phaseLostFlag = static_cast<quint8>(data[2]);
    status.overcurrent = false;  // 单通道响应不包含过流标志，默认为false
    const quint8 floatBytes[4] = {
        static_cast<quint8>(data[4]),
        static_cast<quint8>(data[5]),
        static_cast<quint8>(data[6]),
        static_cast<quint8>(data[7])
    };
    status.currentA = leFloat(floatBytes);
    return true;
}

/**
 * @brief 将多通道控制命令编码为CAN载荷（0x12x）
 * @param cmd 多通道控制命令
 * @return 8字节CAN载荷
 *
 * 数据格式：
 * - 字节0: 命令类型（0x03）
 * - 字节1-4: 通道0-3动作
 * - 字节5-7: 保留
 */
inline QByteArray encodeMultiCtrl(const MultiCtrlCmd &cmd)
{
    QByteArray data;
    data.reserve(8);
    data.append(static_cast<char>(cmd.type));
    data.append(static_cast<char>(cmd.actions[0]));
    data.append(static_cast<char>(cmd.actions[1]));
    data.append(static_cast<char>(cmd.actions[2]));
    data.append(static_cast<char>(cmd.actions[3]));
    while (data.size() < 8) {
        data.append(char(0));
    }
    return data;
}

/**
 * @brief 编码单通道查询命令（0x14x）
 * @param channel 通道号（0-3）
 * @return 8字节CAN载荷
 */
inline QByteArray encodeSingleQuery(quint8 channel)
{
    QByteArray data;
    data.reserve(8);
    data.append(static_cast<char>(CmdType::QueryStatus));
    data.append(static_cast<char>(channel));
    while (data.size() < 8) {
        data.append(char(0));
    }
    return data;
}

/**
 * @brief 编码全通道查询命令（0x16x）
 * @return 8字节CAN载荷
 */
inline QByteArray encodeAllQuery()
{
    QByteArray data;
    data.reserve(8);
    data.append(static_cast<char>(CmdType::QueryAllStatus));
    while (data.size() < 8) {
        data.append(char(0));
    }
    return data;
}

/**
 * @brief 从CAN载荷解码自动状态上报（0x22x响应）
 * @param data CAN载荷（必须8字节）
 * @param report 输出自动状态上报结构
 * @return 解码成功返回true
 *
 * 数据格式（压缩格式）：
 * - 字节0: 通道0-1状态（低4位=通道0, 高4位=通道1）
 * - 字节1: 通道2-3状态（低4位=通道2, 高4位=通道3）
 * - 字节2: 标志位（bit0-3=缺相标志, bit4-7=过流标志）
 * - 字节3-6: 通道0-3电流（0.1A单位，0-255 = 0-25.5A）
 * - 字节7: 保留
 */
inline bool decodeAutoStatus(const QByteArray &data, AutoStatusReport &report)
{
    if (data.size() != 8) {
        return false;
    }

    // 解码通道0-1状态（字节0）
    const quint8 byte0 = static_cast<quint8>(data[0]);
    report.status[0] = static_cast<Action>(byte0 & 0x0F);
    report.status[1] = static_cast<Action>((byte0 >> 4) & 0x0F);

    // 解码通道2-3状态（字节1）
    const quint8 byte1 = static_cast<quint8>(data[1]);
    report.status[2] = static_cast<Action>(byte1 & 0x0F);
    report.status[3] = static_cast<Action>((byte1 >> 4) & 0x0F);

    // 解码标志位（字节2）
    const quint8 flags = static_cast<quint8>(data[2]);
    for (int i = 0; i < 4; ++i) {
        report.phaseLost[i] = ((flags >> i) & 0x01) != 0;      // bit0-3: 缺相标志
        report.overcurrent[i] = ((flags >> (i + 4)) & 0x01) != 0; // bit4-7: 过流标志
    }

    // 解码电流值（字节3-6，0.1A单位）
    for (int i = 0; i < 4; ++i) {
        const quint8 currentByte = static_cast<quint8>(data[3 + i]);
        report.currentA[i] = static_cast<float>(currentByte) * 0.1f;
    }

    return true;
}

/**
 * @brief 编码设置命令（0x30x）
 * @param cmd 设置命令
 * @return 8字节CAN载荷
 */
inline QByteArray encodeSettingsCmd(const SettingsCmd &cmd)
{
    QByteArray data;
    data.reserve(8);
    data.append(static_cast<char>(cmd.type));
    data.append(static_cast<char>(cmd.param1));
    data.append(static_cast<char>(cmd.param2));
    data.append(static_cast<char>(cmd.param3));
    while (data.size() < 8) {
        data.append(char(0));
    }
    return data;
}

/**
 * @brief 编码设置过流标志命令
 * @param channel 通道号（0-3，或0xFF表示所有通道）
 * @param flags 过流标志位（单通道时：0或1；全通道时：bit0-3对应通道0-3）
 * @return 8字节CAN载荷
 */
inline QByteArray encodeSetOvercurrentFlag(quint8 channel, quint8 flags)
{
    SettingsCmd cmd;
    cmd.type = SettingsCmdType::SetOvercurrentFlag;
    cmd.param1 = channel;
    cmd.param2 = flags;
    return encodeSettingsCmd(cmd);
}

/**
 * @brief 从CAN载荷解码设置响应（0x38x响应）
 * @param data CAN载荷
 * @param resp 输出设置响应结构
 * @return 解码成功返回true
 */
inline bool decodeSettingsResp(const QByteArray &data, SettingsResp &resp)
{
    if (data.size() < 2) {
        return false;
    }
    resp.cmdType = static_cast<SettingsCmdType>(static_cast<quint8>(data[0]));
    resp.status = static_cast<SettingsRespStatus>(static_cast<quint8>(data[1]));
    return true;
}

}  // namespace RelayProtocol
}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_RELAY_PROTOCOL_H
