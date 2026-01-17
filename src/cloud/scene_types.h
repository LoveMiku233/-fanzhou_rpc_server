/**
 * @file scene_types.h
 * @brief 场景数据类型定义
 *
 * 根据泛舟云平台大棚智能控制柜系统技术文档定义的场景数据结构。
 * 支持场景的CRUD操作和条件触发执行。
 */

#ifndef FANZHOU_CLOUD_SCENE_TYPES_H
#define FANZHOU_CLOUD_SCENE_TYPES_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVariant>

namespace fanzhou {
namespace cloud {

/**
 * @brief 场景条件操作符
 *
 * 支持的条件操作符：
 * - eq: 等于
 * - ne: 不等于
 * - gt: 大于
 * - lt: 小于
 * - egt: 大于等于
 * - elt: 小于等于
 */
enum class SceneConditionOp {
    Equal,          ///< eq - 等于
    NotEqual,       ///< ne - 不等于
    GreaterThan,    ///< gt - 大于
    LessThan,       ///< lt - 小于
    GreaterOrEqual, ///< egt - 大于等于
    LessOrEqual     ///< elt - 小于等于
};

/**
 * @brief 场景类型
 */
enum class SceneType {
    Auto,   ///< 自动场景 - 根据条件自动触发
    Manual  ///< 手动场景 - 手动触发执行
};

/**
 * @brief 场景触发方式
 */
enum class SceneMatchType {
    All = 0, ///< 需符合全部条件才执行
    Any = 1  ///< 任一条件符合即执行
};

/**
 * @brief 场景条件
 *
 * 定义触发场景的条件，包括设备编码、属性标识、属性值和操作符。
 */
struct SceneCondition {
    QString deviceCode;          ///< 设备编码
    QString identifier;          ///< 属性标识（如 airTemp, soilHum 等）
    QVariant identifierValue;    ///< 属性值
    SceneConditionOp op = SceneConditionOp::Equal; ///< 条件操作符

    /**
     * @brief 从JSON对象解析条件
     */
    static SceneCondition fromJson(const QJsonObject &obj);

    /**
     * @brief 转换为JSON对象
     */
    QJsonObject toJson() const;

    /**
     * @brief 评估条件是否满足
     * @param currentValue 当前属性值
     * @return 条件满足返回true
     */
    bool evaluate(const QVariant &currentValue) const;
};

/**
 * @brief 场景动作
 *
 * 定义场景触发时执行的动作，包括设备编码、属性标识和属性值。
 */
struct SceneAction {
    QString deviceCode;          ///< 设备编码
    QString identifier;          ///< 属性标识（如 sw1, sw2 等）
    QVariant identifierValue;    ///< 属性值

    /**
     * @brief 从JSON对象解析动作
     */
    static SceneAction fromJson(const QJsonObject &obj);

    /**
     * @brief 转换为JSON对象
     */
    QJsonObject toJson() const;
};

/**
 * @brief 场景配置
 *
 * 完整的场景配置信息，包括基本信息、条件列表和动作列表。
 */
struct SceneConfig {
    int id = 0;                              ///< 场景ID，新增时由云端分配
    QString sceneName;                        ///< 场景名称
    SceneType sceneType = SceneType::Auto;   ///< 场景类型
    SceneMatchType matchType = SceneMatchType::All; ///< 触发方式
    QString effectiveBeginTime;              ///< 生效开始时间（格式: HH:mm）
    QString effectiveEndTime;                ///< 生效结束时间（格式: HH:mm）
    int status = 0;                          ///< 场景状态：0-正常启用，1-关闭禁用
    int version = 1;                         ///< 场景版本号
    QString createTime;                      ///< 创建时间
    QString updateTime;                      ///< 最后更新时间
    QList<SceneAction> actions;              ///< 动作列表
    QList<SceneCondition> conditions;        ///< 条件列表（手动场景无需条件）

    /**
     * @brief 从JSON对象解析场景配置
     */
    static SceneConfig fromJson(const QJsonObject &obj);

    /**
     * @brief 转换为JSON对象
     */
    QJsonObject toJson() const;

    /**
     * @brief 检查场景是否有效
     */
    bool isValid() const;

    /**
     * @brief 检查当前时间是否在生效时间范围内
     */
    bool isInEffectiveTime() const;
};

/**
 * @brief Setting请求方法类型
 */
enum class SettingMethod {
    Unknown,        ///< 未知方法
    Get,            ///< 获取场景
    GetResponse,    ///< 获取响应
    Set,            ///< 设置/新增/编辑场景
    SetResponse,    ///< 设置响应
    Delete,         ///< 删除场景
    DeleteResponse, ///< 删除响应
    DeleteSync,     ///< 云端主动删除同步
    DeleteAck       ///< 删除同步确认
};

/**
 * @brief Setting请求/响应消息
 *
 * 按照泛舟云平台报文格式定义的setting字段结构。
 */
struct SettingMessage {
    SettingMethod method = SettingMethod::Unknown; ///< 方法类型
    QJsonObject data;                              ///< 数据内容
    QString requestId;                             ///< 请求唯一ID
    QString responseId;                            ///< 响应ID
    qint64 timestamp = 0;                          ///< 时间戳
    QJsonObject error;                             ///< 错误信息

    /**
     * @brief 从JSON对象解析Setting消息
     */
    static SettingMessage fromJson(const QJsonObject &obj);

    /**
     * @brief 转换为JSON对象
     */
    QJsonObject toJson() const;

    /**
     * @brief 检查是否包含错误
     */
    bool hasError() const;
};

/**
 * @brief 场景处理结果
 */
struct SceneProcessResult {
    int id = 0;                  ///< 场景ID
    QString sceneName;           ///< 场景名称
    QString status;              ///< 处理状态：success/fail/deleted/not_found
    int version = 0;             ///< 版本号
    int errorCode = 0;           ///< 错误码
    QString errorMsg;            ///< 错误信息
    QString createTime;          ///< 创建时间
    QString updateTime;          ///< 更新时间
    QString deleteTime;          ///< 删除时间

    /**
     * @brief 转换为JSON对象
     */
    QJsonObject toJson() const;
};

// ==================== 工具函数 ====================

/**
 * @brief 将方法字符串转换为枚举
 */
SettingMethod parseSettingMethod(const QString &method);

/**
 * @brief 将方法枚举转换为字符串
 */
QString settingMethodToString(SettingMethod method);

/**
 * @brief 将条件操作符字符串转换为枚举
 */
SceneConditionOp parseConditionOp(const QString &op);

/**
 * @brief 将条件操作符枚举转换为字符串
 */
QString conditionOpToString(SceneConditionOp op);

/**
 * @brief 生成请求ID
 */
QString generateRequestId();

/**
 * @brief 生成响应ID
 */
QString generateResponseId();

/**
 * @brief 获取当前时间戳（毫秒）
 */
qint64 currentTimestampMs();

/**
 * @brief 获取当前时间字符串（格式: yyyy-MM-dd HH:mm:ss）
 */
QString currentTimeString();

}  // namespace cloud
}  // namespace fanzhou

#endif  // FANZHOU_CLOUD_SCENE_TYPES_H
