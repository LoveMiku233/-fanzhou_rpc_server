/**
 * @file models/data_models.h
 * @brief 数据模型定义
 *
 * 定义设备、场景、报警、传感器等数据结构，
 * 供View层和逻辑层共享使用。
 */

#ifndef DATA_MODELS_H
#define DATA_MODELS_H

#include <QString>
#include <QList>
#include <QDateTime>

namespace Models {

// ── 设备模型 ─────────────────────────────────

struct DeviceInfo {
    QString id;
    QString name;
    QString type;           // "dc" 或 "ac"
    QString status;         // "running", "stopped", "fault", "manual"
    int     value;          // DC设备开度 (0-100)
    QString spec;           // 规格描述
    QString fault;          // 故障信息
    QString controlType;    // "slider", "toggle", "forward_reverse"
    // AC 设备特有
    QString runtime;
    QString current;
    QString flow;
    QString pressure;
    // RPC 标识
    int     nodeId;         // 继电器节点ID
    int     channel;        // 通道号

    DeviceInfo()
        : value(0), controlType("slider"), nodeId(-1), channel(-1) {}
};

struct DeviceGroup {
    QString id;
    QString name;
    QString color;      // "blue", "emerald", "amber", "purple", "red", "cyan"
    QList<DeviceInfo> devices;
};

// ── 场景模型 ─────────────────────────────────

struct SceneInfo {
    int     id;
    QString name;
    QString type;       // "auto" 或 "manual"
    bool    active;
    QStringList conditions;
    int     triggers;
    QString lastRun;

    SceneInfo()
        : id(0), active(false), triggers(0) {}
};

// ── 报警模型 ─────────────────────────────────

struct AlarmInfo {
    int     id;
    QString level;      // "critical", "warning", "info"
    QString title;
    QString device;
    QString desc;
    QString time;
    QString duration;
    bool    confirmed;

    AlarmInfo()
        : id(0), confirmed(false) {}
};

// ── 传感器模型 ───────────────────────────────

struct SensorInfo {
    QString id;
    QString name;
    QString type;       // "temp", "humidity", "light", "co2", "soil"
    double  value;
    QString unit;
    QString location;
    QString lastUpdate;

    SensorInfo()
        : value(0.0) {}
};

// ── 气象数据 ─────────────────────────────────

struct WeatherData {
    double temperature;
    double humidity;
    double windSpeed;
    double lightIntensity;
    double rainfall;

    WeatherData()
        : temperature(0), humidity(0), windSpeed(0)
        , lightIntensity(0), rainfall(0) {}
};

struct IndoorData {
    double airTemp;
    double airHumidity;
    double co2;
    double light;

    IndoorData()
        : airTemp(0), airHumidity(0), co2(0), light(0) {}
};

} // namespace Models

#endif // DATA_MODELS_H
