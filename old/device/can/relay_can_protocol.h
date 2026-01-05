#ifndef RELAY_CAN_PROTOCOL_H
#define RELAY_CAN_PROTOCOL_H

#include <QtGlobal>
#include <QByteArray>

namespace RelayCanProtocol {

static constexpr quint32 kCtrlBaseId = 0x100;
static constexpr quint32 kStatusBaseId = 0x200;

enum class CmdType : quint8 {
    ControlRelay = 0x01,
    QueryStatus = 0x02
};

enum class Action : quint8 {
    Stop = 0x00,
    Forward = 0x01,
    Reverse = 0x02
};

struct CtrlCmd {
    CmdType type = CmdType::ControlRelay;
    quint8 channel = 0; // 0..3
    Action action = Action::Stop;
};

struct Status {
    quint8 channel = 0;     // 0..3
    quint8 statusByte = 0;  // Byte1
    float currentA = 0.0f;  // Byte4..7
};

inline quint8 modeBits(quint8 statusByte) { return statusByte & 0x03; }     // bit0-1
inline bool phaseLost(quint8 statusByte)  { return (statusByte & 0x04) != 0; } // bit2

// little-endian float decode from 4 bytes
inline float leFloat(const quint8 b[4]) {
    static_assert(sizeof(float) == 4, "float must be 4 bytes");
    quint32 u = (quint32(b[0])      ) |
                (quint32(b[1]) <<  8) |
                (quint32(b[2]) << 16) |
                (quint32(b[3]) << 24);
    float f;
    memcpy(&f, &u, 4);
    return f;
}

inline void putLeFloat(QByteArray& out, float v) {
    quint32 u;
    memcpy(&u, &v, 4);
    out.append(char(u & 0xFF));
    out.append(char((u >> 8) & 0xFF));
    out.append(char((u >> 16) & 0xFF));
    out.append(char((u >> 24) & 0xFF));
}

inline QByteArray encodeCtrl(const CtrlCmd& cmd) {
    QByteArray d;
    d.reserve(8);
    d.append(char(cmd.type));
    d.append(char(cmd.channel));
    d.append(char(cmd.action));
    while (d.size() < 8) d.append(char(0));
    return d;
}

inline bool decodeStatus(const QByteArray& data, Status& st) {
    if (data.size() != 8) return false;
    st.channel = quint8(data[0]);
    st.statusByte = quint8(data[1]);
    const quint8 fb[4] = { quint8(data[4]), quint8(data[5]), quint8(data[6]), quint8(data[7]) };
    st.currentA = leFloat(fb);
    return true;
}

}


#endif // RELAY_CAN_PROTOCOL_H
