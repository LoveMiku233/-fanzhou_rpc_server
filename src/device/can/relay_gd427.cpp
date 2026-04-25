/**
 * @file relay_gd427.cpp
 * @brief GD427 CAN继电器设备实现
 * 
 * 设备控制流程说明：
 * 1. RPC调用 relay.control 到达 RpcRegistry
 * 2. RpcRegistry 调用 CoreContext::enqueueControl() 将命令入队
 * 3. CoreContext::executeJob() 执行命令，调用 RelayGd427::control()
 * 4. control() 通过 CanComm::sendFrame() 发送CAN帧
 * 
 * 协议版本：v1.2
 * - 支持单通道控制（0x10x）和多通道控制（0x12x）
 * - 支持单通道查询（0x14x）和全通道查询（0x16x）
 * - 支持单通道状态响应（0x20x）和自动状态上报（0x22x）
 * - 支持设置命令（0x30x）和设置响应（0x38x）
 * 
 * 如果设备无法控制，请检查：
 * 1. CAN总线是否打开 - 调用 can.status RPC 检查
 * 2. CAN接口是否启动 - 执行 ip link show can0
 * 3. 波特率是否匹配 - 默认 125000
 * 4. 终端电阻是否正确 - 需要 120Ω
 * 5. 接线是否正确 - CAN_H/CAN_L
 */

#include "relay_gd427.h"
#include "comm/can/can_comm.h"
#include "rpc/device_tcp_server.h"
#include "utils/logger.h"

#include <QDateTime>
#include <QJsonArray>
#include <QtGlobal>

namespace fanzhou {
namespace device {

namespace {
const char *const kLogSource = "RelayGD427";

const char *actionToString(RelayProtocol::Action action)
{
    switch (action) {
    case RelayProtocol::Action::Stop:    return "stop";
    case RelayProtocol::Action::Forward: return "fwd";
    case RelayProtocol::Action::Reverse: return "rev";
    }
    return "unknown";
}

int toTcpChannel(quint8 channelZeroBased)
{
    return static_cast<int>(channelZeroBased) + 1;
}

int toTcpState(RelayProtocol::Action action)
{
    // V1.3 TCP JSON接口 state 仅定义 0/1：0=停, 1=开
    return (action == RelayProtocol::Action::Stop) ? 0 : 1;
}

RelayProtocol::Action currentActionFromStatusByte(quint8 statusByte)
{
    switch (statusByte & 0x03) {
    case 1:
        return RelayProtocol::Action::Forward;
    case 2:
        return RelayProtocol::Action::Reverse;
    default:
        return RelayProtocol::Action::Stop;
    }
}
}  // namespace

RelayGd427::RelayGd427(quint8 nodeId,
                       comm::CanComm *bus,
                       TransportType transport,
                       rpc::DeviceTcpServer *tcpGateway,
                       QObject *parent)
    : DeviceAdapter(parent)
    , nodeId_(nodeId)
    , bus_(bus)
    , transport_(transport)
    , tcpGateway_(tcpGateway)
{
    lastRxTimer_.start();
    if (transport_ == TransportType::TcpClient && tcpGateway_) {
        connect(tcpGateway_, &rpc::DeviceTcpServer::boardMessageReceived,
                this, &RelayGd427::onTcpBoardMessage);
    }
}

bool RelayGd427::init()
{
    // 初始化时查询所有通道状态
    return queryAll();
}

void RelayGd427::poll()
{
    // 使用全通道查询替代逐个通道查询，减少CAN总线负载
    queryAll();
}

QString RelayGd427::name() const
{
    return QStringLiteral("RelayGD427(node=0x%1)")
        .arg(nodeId_, 2, 16, QLatin1Char('0'))
        .toUpper();
}

void RelayGd427::markSeen()
{
    lastSeenMs_ = QDateTime::currentMSecsSinceEpoch();
}

qint64 RelayGd427::lastSeenMs() const
{
    return lastSeenMs_;
}

/**
 * @brief 控制单个继电器通道
 * 
 * 发送单通道控制命令到CAN总线（0x10x）。
 * 
 * @param channel 通道号 (0-3)
 * @param action 动作 (Stop/Forward/Reverse)
 * @return 命令是否成功入队发送
 */
bool RelayGd427::control(quint8 channel, RelayProtocol::Action action)
{
    if (channel > kMaxChannel) {
        LOG_WARNING(kLogSource, QStringLiteral("control failed: node=0x%1, ch=%2, bus=%3")
                        .arg(nodeId_, 2, 16, QLatin1Char('0'))
                        .arg(channel)
                        .arg(bus_ ? "ok" : "null"));
        return false;
    }

    if (transport_ == TransportType::TcpClient) {
        if (action == RelayProtocol::Action::Reverse) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("control failed: TCP JSON protocol does not support reverse action, node=%1 ch=%2")
                            .arg(nodeId_)
                            .arg(channel));
            return false;
        }
        QJsonObject args{
            {QStringLiteral("ch"), toTcpChannel(channel)},
            {QStringLiteral("state"), toTcpState(action)}
        };
        return sendTcpCommand(QJsonObject{
            {QStringLiteral("id"), static_cast<double>(QDateTime::currentMSecsSinceEpoch())},
            {QStringLiteral("cmd"), QStringLiteral("relay.set")},
            {QStringLiteral("args"), args}
        });
    }

    if (!bus_) {
        LOG_WARNING(kLogSource, QStringLiteral("control failed: node=0x%1, ch=%2, bus=null")
                        .arg(nodeId_, 2, 16, QLatin1Char('0'))
                        .arg(channel));
        return false;
    }

    RelayProtocol::CtrlCmd cmd;
    cmd.type = RelayProtocol::CmdType::ControlRelay;
    cmd.channel = channel;
    cmd.action = action;

    // CAN ID = 单通道控制基地址 + 节点ID
    const quint32 canId = RelayProtocol::kSingleCtrlBaseId + nodeId_;
    const bool ok = bus_->sendFrame(canId, RelayProtocol::encodeCtrl(cmd), false, false);
    if (!ok) {
        LOG_ERROR(kLogSource, QStringLiteral("control sendFrame failed: node=0x%1, ch=%2, action=%3")
                      .arg(nodeId_, 2, 16, QLatin1Char('0'))
                      .arg(channel)
                      .arg(actionToString(action)));
    } else {
        LOG_INFO(kLogSource, QStringLiteral("control: node=0x%1, ch=%2, action=%3")
                     .arg(nodeId_, 2, 16, QLatin1Char('0'))
                     .arg(channel)
                     .arg(actionToString(action)));
    }
    return ok;
}

/**
 * @brief 同时控制多个继电器通道
 * 
 * 发送多通道控制命令到CAN总线（0x12x）。
 * 一次性控制所有4个通道。
 * 
 * @param actions 4个通道的动作数组
 * @return 命令是否成功入队发送
 */
bool RelayGd427::controlMulti(const RelayProtocol::Action actions[4])
{
    if (transport_ == TransportType::TcpClient) {
        for (int i = 0; i < 4; ++i) {
            if (actions[i] == RelayProtocol::Action::Reverse) {
                LOG_WARNING(kLogSource,
                            QStringLiteral("controlMulti failed: TCP JSON protocol does not support reverse action, node=%1")
                                .arg(nodeId_));
                return false;
            }
        }
        bool allStop = true;
        for (int i = 0; i < 4; ++i) {
            if (actions[i] != RelayProtocol::Action::Stop) {
                allStop = false;
                break;
            }
        }

        if (allStop) {
            return sendTcpCommand(QJsonObject{
                {QStringLiteral("id"), static_cast<double>(QDateTime::currentMSecsSinceEpoch())},
                {QStringLiteral("cmd"), QStringLiteral("relay.stopall")}
            });
        }

        bool ok = true;
        bool hasChange = false;
        for (int ch = 0; ch < 4; ++ch) {
            const RelayProtocol::Action current =
                currentActionFromStatusByte(status_[ch].statusByte);
            if (actions[ch] == current) {
                continue;
            }
            hasChange = true;
            QJsonObject args{
                {QStringLiteral("ch"), toTcpChannel(static_cast<quint8>(ch))},
                {QStringLiteral("state"), toTcpState(actions[ch])}
            };
            ok = sendTcpCommand(QJsonObject{
                {QStringLiteral("id"), static_cast<double>(QDateTime::currentMSecsSinceEpoch() + ch)},
                {QStringLiteral("cmd"), QStringLiteral("relay.set")},
                {QStringLiteral("args"), args}
            }) && ok;
        }
        if (!hasChange) {
            if (allStop) {
                return sendTcpCommand(QJsonObject{
                    {QStringLiteral("id"), static_cast<double>(QDateTime::currentMSecsSinceEpoch())},
                    {QStringLiteral("cmd"), QStringLiteral("relay.stopall")}
                });
            }
            return true;
        }
        return ok;
    }

    if (!bus_) {
        LOG_WARNING(kLogSource, QStringLiteral("controlMulti failed: node=0x%1, bus=null")
                        .arg(nodeId_, 2, 16, QLatin1Char('0')));
        return false;
    }

    RelayProtocol::MultiCtrlCmd cmd;
    cmd.type = RelayProtocol::CmdType::MultiControlRelay;
    for (int i = 0; i < 4; ++i) {
        cmd.actions[i] = actions[i];
    }

    // CAN ID = 多通道控制基地址 + 节点ID
    const quint32 canId = RelayProtocol::kMultiCtrlBaseId + nodeId_;
    const bool ok = bus_->sendFrame(canId, RelayProtocol::encodeMultiCtrl(cmd), false, false);
    if (!ok) {
        LOG_ERROR(kLogSource, QStringLiteral("controlMulti sendFrame failed: node=0x%1, actions=[%2,%3,%4,%5]")
                      .arg(nodeId_, 2, 16, QLatin1Char('0'))
                      .arg(actionToString(actions[0]))
                      .arg(actionToString(actions[1]))
                      .arg(actionToString(actions[2]))
                      .arg(actionToString(actions[3])));
    } else {
        LOG_INFO(kLogSource, QStringLiteral("controlMulti: node=0x%1, actions=[%2,%3,%4,%5]")
                     .arg(nodeId_, 2, 16, QLatin1Char('0'))
                     .arg(actionToString(actions[0]))
                     .arg(actionToString(actions[1]))
                     .arg(actionToString(actions[2]))
                     .arg(actionToString(actions[3])));
    }
    return ok;
}

/**
 * @brief 查询单个通道状态
 * 
 * 发送单通道查询命令到CAN总线（0x14x）。
 * 
 * @param channel 通道号 (0-3)
 * @return 查询是否成功入队发送
 */
bool RelayGd427::query(quint8 channel)
{
    if (channel > kMaxChannel) {
        LOG_WARNING(kLogSource, QStringLiteral("query failed: node=0x%1, ch=%2, bus=%3")
                        .arg(nodeId_, 2, 16, QLatin1Char('0'))
                        .arg(channel)
                        .arg(bus_ ? "ok" : "null"));
        return false;
    }

    if (transport_ == TransportType::TcpClient) {
        Q_UNUSED(channel);
        return sendTcpCommand(QJsonObject{
            {QStringLiteral("id"), static_cast<double>(QDateTime::currentMSecsSinceEpoch())},
            {QStringLiteral("cmd"), QStringLiteral("status")}
        });
    }

    if (!bus_) {
        return false;
    }

    // CAN ID = 单通道查询基地址 + 节点ID
    const quint32 canId = RelayProtocol::kSingleQueryBaseId + nodeId_;
    const bool ok = bus_->sendFrame(canId, RelayProtocol::encodeSingleQuery(channel), false, false);
    if (!ok) {
        LOG_WARNING(kLogSource, QStringLiteral("query sendFrame failed: node=0x%1, ch=%2")
                        .arg(nodeId_, 2, 16, QLatin1Char('0'))
                        .arg(channel));
    }
    return ok;
}

/**
 * @brief 查询所有通道状态
 * 
 * 发送全通道查询命令到CAN总线（0x16x）。
 * 设备会依次响应4个单通道状态帧（0x20x）。
 * 
 * @return 查询是否成功入队发送
 */
bool RelayGd427::queryAll()
{
    if (transport_ == TransportType::TcpClient) {
        return sendTcpCommand(QJsonObject{
            {QStringLiteral("id"), static_cast<double>(QDateTime::currentMSecsSinceEpoch())},
            {QStringLiteral("cmd"), QStringLiteral("status")}
        });
    }

    if (!bus_) {
        LOG_WARNING(kLogSource, QStringLiteral("queryAll failed: node=0x%1, bus=null")
                        .arg(nodeId_, 2, 16, QLatin1Char('0')));
        return false;
    }

    // CAN ID = 全通道查询基地址 + 节点ID
    const quint32 canId = RelayProtocol::kAllQueryBaseId + nodeId_;
    const bool ok = bus_->sendFrame(canId, RelayProtocol::encodeAllQuery(), false, false);
    if (!ok) {
        LOG_WARNING(kLogSource, QStringLiteral("queryAll sendFrame failed: node=0x%1")
                        .arg(nodeId_, 2, 16, QLatin1Char('0')));
    }
    return ok;
}

/**
 * @brief 设置过流标志
 * 
 * 发送设置过流标志命令到CAN总线（0x30x，命令类型0x17）。
 * 
 * @param channel 通道号（0-3，或0xFF表示所有通道）
 * @param flag 过流标志值
 * @return 命令是否成功入队发送
 */
bool RelayGd427::setOvercurrentFlag(quint8 channel, quint8 flag)
{
    if (transport_ == TransportType::TcpClient) {
        Q_UNUSED(channel);
        Q_UNUSED(flag);
        LOG_WARNING(kLogSource, QStringLiteral("setOvercurrentFlag not supported in TCP client transport"));
        return false;
    }

    if (!bus_) {
        LOG_WARNING(kLogSource, QStringLiteral("setOvercurrentFlag failed: node=0x%1, bus=null")
                        .arg(nodeId_, 2, 16, QLatin1Char('0')));
        return false;
    }

    // CAN ID = 设置命令基地址 + 节点ID
    const quint32 canId = RelayProtocol::kSettingsCmdBaseId + nodeId_;
    const bool ok = bus_->sendFrame(canId, RelayProtocol::encodeSetOvercurrentFlag(channel, flag), false, false);
    if (!ok) {
        LOG_ERROR(kLogSource, QStringLiteral("setOvercurrentFlag sendFrame failed: node=0x%1, ch=%2, flag=%3")
                      .arg(nodeId_, 2, 16, QLatin1Char('0'))
                      .arg(channel)
                      .arg(flag));
    } else {
        LOG_INFO(kLogSource, QStringLiteral("setOvercurrentFlag: node=0x%1, ch=%2, flag=%3")
                     .arg(nodeId_, 2, 16, QLatin1Char('0'))
                     .arg(channel)
                     .arg(flag));
    }
    return ok;
}

bool RelayGd427::setCommMode(RelayProtocol::CommMode commMode, RelayProtocol::NetworkMode networkMode)
{
    if (transport_ == TransportType::TcpClient) {
        Q_UNUSED(commMode);
        Q_UNUSED(networkMode);
        LOG_WARNING(kLogSource, QStringLiteral("setCommMode not supported in TCP client transport"));
        return false;
    }

    if (!bus_) {
        LOG_WARNING(kLogSource, QStringLiteral("setCommMode failed: node=0x%1, bus=null")
                        .arg(nodeId_, 2, 16, QLatin1Char('0')));
        return false;
    }

    const quint32 canId = RelayProtocol::kSettingsCmdBaseId + nodeId_;
    const bool ok = bus_->sendFrame(canId, RelayProtocol::encodeSetCommMode(commMode, networkMode), false, false);
    if (!ok) {
        LOG_ERROR(kLogSource, QStringLiteral("setCommMode sendFrame failed: node=0x%1, commMode=%2, networkMode=%3")
                      .arg(nodeId_, 2, 16, QLatin1Char('0'))
                      .arg(static_cast<int>(commMode))
                      .arg(static_cast<int>(networkMode)));
    } else {
        LOG_INFO(kLogSource, QStringLiteral("setCommMode: node=0x%1, commMode=%2, networkMode=%3")
                     .arg(nodeId_, 2, 16, QLatin1Char('0'))
                     .arg(static_cast<int>(commMode))
                     .arg(static_cast<int>(networkMode)));
    }
    return ok;
}

/**
 * @brief 处理单通道状态响应帧（0x20x）
 */
void RelayGd427::onSingleStatusFrame(quint32 canId, const QByteArray &payload)
{
    Q_UNUSED(canId);

    markSeen();
    lastRxTimer_.restart();

    RelayProtocol::Status status;
    if (!RelayProtocol::decodeStatus(payload, status)) {
        LOG_WARNING(kLogSource, QStringLiteral("decodeStatus failed: node=0x%1, payload size=%2")
                        .arg(nodeId_, 2, 16, QLatin1Char('0'))
                        .arg(payload.size()));
        return;
    }

    if (status.channel > kMaxChannel) {
        LOG_WARNING(kLogSource, QStringLiteral("invalid channel in status: node=0x%1, ch=%2")
                        .arg(nodeId_, 2, 16, QLatin1Char('0'))
                        .arg(status.channel));
        return;
    }

    status_[status.channel] = status;

    emit statusUpdated(status.channel, status);
    emit updated();
}

/**
 * @brief 处理自动状态上报帧（0x22x）
 */
void RelayGd427::onAutoStatusFrame(quint32 canId, const QByteArray &payload)
{
    Q_UNUSED(canId);

    markSeen();
    lastRxTimer_.restart();

    RelayProtocol::AutoStatusReport report;
    if (!RelayProtocol::decodeAutoStatus(payload, report)) {
        LOG_WARNING(kLogSource, QStringLiteral("decodeAutoStatus failed: node=0x%1, payload size=%2")
                        .arg(nodeId_, 2, 16, QLatin1Char('0'))
                        .arg(payload.size()));
        return;
    }

    // 保存自动状态上报
    autoStatus_ = report;

    // 同步更新各通道状态
    for (int i = 0; i < 4; ++i) {
        status_[i].channel = static_cast<quint8>(i);
        status_[i].statusByte = static_cast<quint8>(report.status[i]);
        status_[i].phaseLostFlag = report.phaseLost[i] ? 1 : 0;
        status_[i].currentA = report.currentA[i];
        status_[i].overcurrent = report.overcurrent[i];
        status_[i].noCurrent = false;
    }

    emit autoStatusReceived(report);
    emit updated();
}

/**
 * @brief 处理设置响应帧（0x38x）
 */
void RelayGd427::onSettingsRespFrame(quint32 canId, const QByteArray &payload)
{
    Q_UNUSED(canId);

    markSeen();

    RelayProtocol::SettingsResp resp;
    if (!RelayProtocol::decodeSettingsResp(payload, resp)) {
        LOG_WARNING(kLogSource, QStringLiteral("decodeSettingsResp failed: node=0x%1, payload size=%2")
                        .arg(nodeId_, 2, 16, QLatin1Char('0'))
                        .arg(payload.size()));
        return;
    }

    LOG_INFO(kLogSource, QStringLiteral("settings response: node=0x%1, cmdType=0x%2, status=0x%3")
                 .arg(nodeId_, 2, 16, QLatin1Char('0'))
                 .arg(static_cast<quint8>(resp.cmdType), 2, 16, QLatin1Char('0'))
                 .arg(static_cast<quint8>(resp.status), 2, 16, QLatin1Char('0')));

    emit settingsResponseReceived(static_cast<quint8>(resp.cmdType), static_cast<quint8>(resp.status));
}

bool RelayGd427::sendTcpCommand(const QJsonObject &command)
{
    if (!tcpGateway_) {
        LOG_WARNING(kLogSource, QStringLiteral("sendTcpCommand failed: node=%1, tcpGateway=null").arg(nodeId_));
        return false;
    }
    QString error;
    const bool ok = tcpGateway_->sendCommand(command, nodeId_, &error);
    if (!ok) {
        LOG_WARNING(kLogSource, QStringLiteral("sendTcpCommand failed: node=%1, error=%2")
                        .arg(nodeId_)
                        .arg(error));
    }
    return ok;
}

void RelayGd427::onTcpBoardMessage(int devId, const QJsonObject &message)
{
    if (transport_ != TransportType::TcpClient || static_cast<quint8>(devId) != nodeId_) {
        return;
    }

    markSeen();

    const QString event = message.value(QStringLiteral("event")).toString();
    if (event == QStringLiteral("hello") || event == QStringLiteral("heartbeat")) {
        return;
    }

    if (message.contains(QStringLiteral("ok")) && !message.value(QStringLiteral("ok")).toBool()) {
        const QString err = message.value(QStringLiteral("error")).toString();
        if (!err.isEmpty()) {
            LOG_WARNING(kLogSource,
                        QStringLiteral("TCP board command failed: node=%1 error=%2")
                            .arg(nodeId_)
                            .arg(err));
        }
        return;
    }

    const QJsonObject data = message.value(QStringLiteral("data")).toObject();
    const QJsonArray relayArr = data.value(QStringLiteral("relay")).toArray();
    const QJsonArray currentArr = data.value(QStringLiteral("current_mA")).toArray();
    const QJsonArray faultPhaseArr = data.value(QStringLiteral("fault_phase")).toArray();
    const QJsonArray faultOverArr = data.value(QStringLiteral("fault_over")).toArray();
    const QJsonArray faultNoCurArr = data.value(QStringLiteral("fault_nocur")).toArray();

    if (relayArr.isEmpty() && currentArr.isEmpty() &&
        faultPhaseArr.isEmpty() && faultOverArr.isEmpty() && faultNoCurArr.isEmpty()) {
        return;
    }

    lastRxTimer_.restart();

    for (int ch = 0; ch < 4; ++ch) {
        const int mode = (ch < relayArr.size()) ? relayArr[ch].toInt(0) : 0;
        const double currentmA = (ch < currentArr.size()) ? currentArr[ch].toDouble(0.0) : 0.0;
        status_[ch].channel = static_cast<quint8>(ch);
        status_[ch].statusByte = static_cast<quint8>(mode & 0x03);
        status_[ch].currentA = static_cast<float>(currentmA / 1000.0);
        status_[ch].phaseLostFlag =
            static_cast<quint8>((ch < faultPhaseArr.size()) ? faultPhaseArr[ch].toInt(0) : 0);
        status_[ch].overcurrent =
            (ch < faultOverArr.size()) ? (faultOverArr[ch].toInt(0) != 0) : false;
        status_[ch].noCurrent =
            (ch < faultNoCurArr.size()) ? (faultNoCurArr[ch].toInt(0) != 0) : false;
        emit statusUpdated(static_cast<quint8>(ch), status_[ch]);
    }
    emit updated();
}

RelayProtocol::Status RelayGd427::lastStatus(quint8 channel) const
{
    if (channel > kMaxChannel) {
        return RelayProtocol::Status{};
    }
    return status_[channel];
}

bool RelayGd427::canAccept(quint32 canId, bool extended, bool rtr) const
{
    if (transport_ == TransportType::TcpClient) {
        Q_UNUSED(canId);
        Q_UNUSED(extended);
        Q_UNUSED(rtr);
        return false;
    }

    if (extended || rtr) {
        return false;
    }

    // 计算期望的CAN ID
    const quint32 singleStatusId = RelayProtocol::kSingleStatusBaseId + nodeId_;
    const quint32 autoStatusId = RelayProtocol::kAutoStatusBaseId + nodeId_;
    const quint32 settingsRespId = RelayProtocol::kSettingsRespBaseId + nodeId_;

    // 接受单通道状态响应、自动状态上报和设置响应
    return canId == singleStatusId || canId == autoStatusId || canId == settingsRespId;
}

void RelayGd427::canOnFrame(quint32 canId, const QByteArray &payload,
                             bool extended, bool rtr)
{
    if (transport_ == TransportType::TcpClient) {
        Q_UNUSED(canId);
        Q_UNUSED(payload);
        Q_UNUSED(extended);
        Q_UNUSED(rtr);
        return;
    }

    Q_UNUSED(extended);
    Q_UNUSED(rtr);

    const quint32 singleStatusId = RelayProtocol::kSingleStatusBaseId + nodeId_;
    const quint32 autoStatusId = RelayProtocol::kAutoStatusBaseId + nodeId_;
    const quint32 settingsRespId = RelayProtocol::kSettingsRespBaseId + nodeId_;

    if (canId == singleStatusId) {
        onSingleStatusFrame(canId, payload);
    } else if (canId == autoStatusId) {
        onAutoStatusFrame(canId, payload);
    } else if (canId == settingsRespId) {
        onSettingsRespFrame(canId, payload);
    }
}

}  // namespace device
}  // namespace fanzhou
