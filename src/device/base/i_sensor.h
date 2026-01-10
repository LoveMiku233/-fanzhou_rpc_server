/**
 * @file i_sensor.h
 * @brief 传感器设备接口
 *
 * 定义所有传感器设备的通用接口。
 */

#ifndef FANZHOU_I_SENSOR_H
#define FANZHOU_I_SENSOR_H

#include <QString>
#include <QVariant>
#include <QJsonObject>

namespace fanzhou {
namespace device {

/**
 * @brief 传感器数据单位
 */
enum class SensorUnit : int {
    None = 0,           ///< 无单位
    Celsius = 1,        ///< 摄氏度
    Fahrenheit = 2,     ///< 华氏度
    Percent = 3,        ///< 百分比
    Pascal = 4,         ///< 帕斯卡
    Hectopascal = 5,    ///< 百帕
    Lux = 6,            ///< 勒克斯
    Ppm = 7,            ///< 百万分之一
    Ph = 8,             ///< pH值
    MsPerCm = 9,        ///< 毫西门子/厘米（电导率）
    Mm = 10,            ///< 毫米（雨量）
    MPerS = 11,         ///< 米/秒（风速）
    UgPerM3 = 12,       ///< 微克/立方米（PM2.5）
    Degree = 13,        ///< 度（角度）
};

/**
 * @brief 传感器单位转字符串
 * @param unit 传感器单位
 * @return 单位字符串
 */
inline const char* sensorUnitToString(SensorUnit unit)
{
    switch (unit) {
    case SensorUnit::None: return "";
    case SensorUnit::Celsius: return "°C";
    case SensorUnit::Fahrenheit: return "°F";
    case SensorUnit::Percent: return "%";
    case SensorUnit::Pascal: return "Pa";
    case SensorUnit::Hectopascal: return "hPa";
    case SensorUnit::Lux: return "lux";
    case SensorUnit::Ppm: return "ppm";
    case SensorUnit::Ph: return "pH";
    case SensorUnit::MsPerCm: return "mS/cm";
    case SensorUnit::Mm: return "mm";
    case SensorUnit::MPerS: return "m/s";
    case SensorUnit::UgPerM3: return "μg/m³";
    case SensorUnit::Degree: return "°";
    default: return "";
    }
}

/**
 * @brief 传感器读数结构
 */
struct SensorReading {
    double value = 0.0;             ///< 传感器数值
    SensorUnit unit = SensorUnit::None;  ///< 数值单位
    qint64 timestampMs = 0;         ///< 读取时间戳（毫秒）
    bool valid = false;             ///< 数据是否有效
    QString error;                  ///< 错误信息（如果有）
    
    /**
     * @brief 转换为JSON对象
     * @return JSON对象
     */
    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj[QStringLiteral("value")] = value;
        obj[QStringLiteral("unit")] = QString::fromLatin1(sensorUnitToString(unit));
        obj[QStringLiteral("unitId")] = static_cast<int>(unit);
        obj[QStringLiteral("timestampMs")] = static_cast<double>(timestampMs);
        obj[QStringLiteral("valid")] = valid;
        if (!error.isEmpty()) {
            obj[QStringLiteral("error")] = error;
        }
        return obj;
    }
};

/**
 * @brief 传感器设备接口
 *
 * 所有传感器设备必须实现此接口。
 * 提供读取传感器数据、获取传感器信息等通用方法。
 */
class ISensor
{
public:
    virtual ~ISensor() = default;

    /**
     * @brief 获取传感器名称
     * @return 传感器名称
     */
    virtual QString sensorName() const = 0;

    /**
     * @brief 获取传感器类型名称
     * @return 传感器类型（如 "temperature", "humidity" 等）
     */
    virtual QString sensorTypeName() const = 0;

    /**
     * @brief 读取传感器当前值
     * @return 传感器读数结构
     */
    virtual SensorReading read() = 0;

    /**
     * @brief 获取传感器的最后读数
     * @return 最后一次读取的数据
     */
    virtual SensorReading lastReading() const = 0;

    /**
     * @brief 获取传感器的测量单位
     * @return 传感器单位
     */
    virtual SensorUnit unit() const = 0;

    /**
     * @brief 获取传感器信息
     * @return 包含传感器详细信息的JSON对象
     */
    virtual QJsonObject sensorInfo() const
    {
        QJsonObject info;
        info[QStringLiteral("name")] = sensorName();
        info[QStringLiteral("type")] = sensorTypeName();
        info[QStringLiteral("unit")] = QString::fromLatin1(sensorUnitToString(unit()));
        info[QStringLiteral("unitId")] = static_cast<int>(unit());
        return info;
    }

    /**
     * @brief 检查传感器是否可用
     * @return 可用返回true
     */
    virtual bool isAvailable() const = 0;

    /**
     * @brief 校准传感器
     * @param params 校准参数
     * @return 成功返回true
     */
    virtual bool calibrate(const QJsonObject &params)
    {
        Q_UNUSED(params);
        return false;  // 默认不支持校准
    }
};

}  // namespace device
}  // namespace fanzhou

#endif  // FANZHOU_I_SENSOR_H
