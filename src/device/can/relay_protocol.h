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

// CAN ID基地址
constexpr quint32 kCtrlBaseId = 0x100;    ///< 控制命令基地址
constexpr quint32 kStatusBaseId = 0x200;  ///< 状态响应基地址

/**
 * @brief 命令类型
 */
enum class CmdType : quint8 {
    ControlRelay = 0x01,  ///< 控制继电器输出
    QueryStatus = 0x02    ///< 查询继电器状态
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
 * @brief 状态响应结构
 */
struct Status {
    quint8 channel = 0;     ///< 通道（0-3）
    quint8 statusByte = 0;  ///< 状态标志
    float currentA = 0.0f;  ///< 电流（安培）
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
 * @brief 从CAN载荷解码状态
 * @param data CAN载荷（必须8字节）
 * @param status 输出状态结构
 * @return 解码成功返回true
 */
inline bool decodeStatus(const QByteArray &data, Status &status)
{
    if (data.size() != 8) {
        return false;
    }
    status.channel = static_cast<quint8>(data[0]);
    status.statusByte = static_cast<quint8>(data[1]);
    const quint8 floatBytes[4] = {
        static_cast<quint8>(data[4]),
        static_cast<quint8>(data[5]),
        static_cast<quint8>(data[6]),
        static_cast<quint8>(data[7])
    };
    status.currentA = leFloat(floatBytes);
    return true;
}

}  // namespace RelayProtocol
}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_RELAY_PROTOCOL_H
