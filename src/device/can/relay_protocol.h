/**
 * @file relay_protocol.h
 * @brief CAN relay protocol definitions
 *
 * Defines the protocol for communicating with CAN-based relay devices.
 */

#ifndef FANZHOU_RELAY_PROTOCOL_H
#define FANZHOU_RELAY_PROTOCOL_H

#include <QByteArray>
#include <QtGlobal>

#include <cstring>

namespace fanzhou {
namespace device {

/**
 * @brief Relay CAN protocol namespace
 *
 * Contains protocol constants, types, and encoding/decoding functions.
 */
namespace RelayProtocol {

// CAN ID base addresses
constexpr quint32 kCtrlBaseId = 0x100;    ///< Control command base ID
constexpr quint32 kStatusBaseId = 0x200;  ///< Status response base ID

/**
 * @brief Command types
 */
enum class CmdType : quint8 {
    ControlRelay = 0x01,  ///< Control relay output
    QueryStatus = 0x02    ///< Query relay status
};

/**
 * @brief Relay action types
 */
enum class Action : quint8 {
    Stop = 0x00,     ///< Stop (both outputs off)
    Forward = 0x01,  ///< Forward direction
    Reverse = 0x02   ///< Reverse direction
};

/**
 * @brief Control command structure
 */
struct CtrlCmd {
    CmdType type = CmdType::ControlRelay;
    quint8 channel = 0;         ///< Channel (0-3)
    Action action = Action::Stop;
};

/**
 * @brief Status response structure
 */
struct Status {
    quint8 channel = 0;     ///< Channel (0-3)
    quint8 statusByte = 0;  ///< Status flags
    float currentA = 0.0f;  ///< Current in amps
};

/**
 * @brief Extract mode bits from status byte
 * @param statusByte Raw status byte
 * @return Mode bits (0-3)
 */
inline quint8 modeBits(quint8 statusByte)
{
    return statusByte & 0x03;
}

/**
 * @brief Check phase lost flag from status byte
 * @param statusByte Raw status byte
 * @return True if phase lost
 */
inline bool phaseLost(quint8 statusByte)
{
    return (statusByte & 0x04) != 0;
}

/**
 * @brief Decode little-endian float from 4 bytes
 * @param bytes Pointer to 4 bytes
 * @return Decoded float value
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
 * @brief Encode float as little-endian bytes
 * @param out Output byte array
 * @param value Float value to encode
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
 * @brief Encode control command to CAN payload
 * @param cmd Control command
 * @return 8-byte CAN payload
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
 * @brief Decode status from CAN payload
 * @param data CAN payload (must be 8 bytes)
 * @param status Output status structure
 * @return True if decode successful
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
