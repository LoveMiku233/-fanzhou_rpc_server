/**
 * @file strategy_converter.h
 * @brief 云平台策略转换器接口
 *
 * 提供云平台策略到RPC策略的转换接口。
 * 每个云平台实现自己的策略转换器，通过此接口转换为统一的RPC策略格式。
 *
 * 使用方式：
 * 1. 实现 IStrategyConverter 接口
 * 2. 通过 StrategyConverterFactory 注册和获取转换器
 * 3. 调用 toRpcActions() 将云平台策略转换为RPC动作
 */

#ifndef FANZHOU_CLOUD_STRATEGY_CONVERTER_H
#define FANZHOU_CLOUD_STRATEGY_CONVERTER_H

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVariant>
#include <QHash>
#include <memory>

#include "cloud_types.h"
#include "scene_types.h"

namespace fanzhou {
namespace cloud {

/**
 * @brief RPC动作结构
 *
 * 统一的RPC动作格式，由策略转换器生成。
 */
struct RpcAction {
    QString method;                    ///< RPC方法名（如 relay.control, relay.controlMulti）
    QJsonObject params;                ///< RPC参数
    int priority = 0;                  ///< 优先级（数值越大优先级越高）
    int delayMs = 0;                   ///< 延迟执行时间（毫秒）

    /**
     * @brief 转换为JSON对象
     */
    QJsonObject toJson() const;

    /**
     * @brief 从JSON对象解析
     */
    static RpcAction fromJson(const QJsonObject &obj);
};

/**
 * @brief 策略转换结果
 */
struct StrategyConvertResult {
    bool success = false;              ///< 转换是否成功
    QList<RpcAction> actions;          ///< RPC动作列表
    QString errorMsg;                  ///< 错误信息
    int errorCode = 0;                 ///< 错误码

    /**
     * @brief 创建成功结果
     */
    static StrategyConvertResult ok(const QList<RpcAction> &actions);

    /**
     * @brief 创建失败结果
     */
    static StrategyConvertResult error(int code, const QString &msg);
};

/**
 * @brief 策略转换器接口
 *
 * 定义云平台策略到RPC策略的转换接口。
 * 每个云平台应实现此接口来提供策略转换功能。
 */
class IStrategyConverter
{
public:
    virtual ~IStrategyConverter() = default;

    /**
     * @brief 获取云平台类型
     */
    virtual CloudTypeId cloudType() const = 0;

    /**
     * @brief 获取云平台名称
     */
    virtual QString cloudName() const = 0;

    /**
     * @brief 将场景动作转换为RPC动作
     * @param actions 场景动作列表
     * @return 转换结果
     */
    virtual StrategyConvertResult toRpcActions(const QList<SceneAction> &actions) = 0;

    /**
     * @brief 将单个场景动作转换为RPC动作
     * @param action 场景动作
     * @return 转换结果
     */
    virtual StrategyConvertResult toRpcAction(const SceneAction &action) = 0;

    /**
     * @brief 检查设备是否支持
     * @param deviceCode 设备编码
     * @return 支持返回true
     */
    virtual bool isDeviceSupported(const QString &deviceCode) const = 0;

    /**
     * @brief 检查属性是否支持
     * @param identifier 属性标识
     * @return 支持返回true
     */
    virtual bool isIdentifierSupported(const QString &identifier) const = 0;
};

/**
 * @brief 泛舟云平台策略转换器
 *
 * 实现泛舟云平台策略到RPC策略的转换。
 * 支持继电器控制（sw1-sw4）等属性。
 */
class FanzhouStrategyConverter : public IStrategyConverter
{
public:
    explicit FanzhouStrategyConverter();
    ~FanzhouStrategyConverter() override = default;

    CloudTypeId cloudType() const override { return CloudTypeId::FanzhouCloudMqtt; }
    QString cloudName() const override { return QStringLiteral("FanzhouCloud"); }

    StrategyConvertResult toRpcActions(const QList<SceneAction> &actions) override;
    StrategyConvertResult toRpcAction(const SceneAction &action) override;

    bool isDeviceSupported(const QString &deviceCode) const override;
    bool isIdentifierSupported(const QString &identifier) const override;

    /**
     * @brief 注册设备映射
     * @param deviceCode 云平台设备编码
     * @param nodeId 本地继电器节点ID
     */
    void registerDeviceMapping(const QString &deviceCode, quint8 nodeId);

    /**
     * @brief 获取设备映射
     * @param deviceCode 云平台设备编码
     * @param nodeId 输出：本地节点ID
     * @return 存在映射时返回true
     */
    bool getDeviceMapping(const QString &deviceCode, quint8 &nodeId) const;

    /**
     * @brief 清除所有设备映射
     */
    void clearDeviceMappings();

private:
    /**
     * @brief 解析sw标识符获取通道号
     * @param identifier 属性标识（如 sw1, sw2）
     * @param channel 输出：通道号（0-3）
     * @return 解析成功返回true
     */
    bool parseSwChannel(const QString &identifier, int &channel) const;

    /**
     * @brief 将属性值转换为继电器动作
     * @param value 属性值
     * @return 动作字符串（stop/fwd/rev）
     */
    QString valueToRelayAction(const QVariant &value) const;

    // 设备编码 -> 本地节点ID映射
    QHash<QString, quint8> deviceMappings_;
};

/**
 * @brief 策略转换器工厂
 *
 * 管理策略转换器的注册和获取。
 */
class StrategyConverterFactory
{
public:
    /**
     * @brief 获取单例实例
     */
    static StrategyConverterFactory &instance();

    /**
     * @brief 注册策略转换器
     * @param converter 转换器实例
     */
    void registerConverter(std::shared_ptr<IStrategyConverter> converter);

    /**
     * @brief 获取策略转换器
     * @param cloudType 云平台类型
     * @return 转换器指针，不存在返回nullptr
     */
    IStrategyConverter *getConverter(CloudTypeId cloudType);

    /**
     * @brief 获取泛舟云平台转换器
     * @return 泛舟云平台转换器指针
     */
    FanzhouStrategyConverter *getFanzhouConverter();

    /**
     * @brief 获取所有已注册的云平台类型
     */
    QList<CloudTypeId> registeredTypes() const;

private:
    StrategyConverterFactory();
    ~StrategyConverterFactory() = default;

    StrategyConverterFactory(const StrategyConverterFactory &) = delete;
    StrategyConverterFactory &operator=(const StrategyConverterFactory &) = delete;

    QHash<CloudTypeId, std::shared_ptr<IStrategyConverter>> converters_;
    std::shared_ptr<FanzhouStrategyConverter> fanzhouConverter_;
};

}  // namespace cloud
}  // namespace fanzhou

#endif  // FANZHOU_CLOUD_STRATEGY_CONVERTER_H
