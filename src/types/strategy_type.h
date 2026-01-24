#ifndef STRATEGY_TYPES_H
#define STRATEGY_TYPES_H

#include <QString>
#include <QList>

namespace fanzhou {
namespace core {

/**
  * @brief new 设备actions
  * @todo
  */
struct StrategyAction {
    QString identifier = "";        ///< "identifier": "node_1_sw1", 属性标识
    int identifierValue = 0;  ///< "identifierValue": 1,	属性值
};

/**
  * @brief new 设备conditions
  * @todo
  */
struct StrategyCondition {
    QString identifier = "";        ///< "identifier": "airTemp", 属性标识
    double identifierValue = 0.0f;  ///< "identifierValue": 30,	属性值
    QString op = "";                ///< 条件操作符 eq等于 ne不等于 gt大于 lt小于 egt大等于 elt小等于
};

enum class MatchType : qint8 {
    All = 0,
    Any = 1
};


/**
  * @brief new 自动控制策略配置
  * @todo
  */
struct AutoStrategy {
    int strategyId = 0;
    int groupId = 0;                     ///< 绑定的分组ID
    int version = 1;                     ///< 场景版本号，通过版本号比对是否需要更新，每编辑一次，版本号+1
    bool enabled = true;                 ///< 是否启用
    QString type = "scene";              ///<
    QString updateTime = "";             ///< 最后更新时间
    QString strategyName = "";
    QString strategyType = "auto";       ///< auto自动场景，manual手动场景
    QString effectiveBeginTime = "";     ///< 生效开始时间
    QString effectiveEndTime = "";       ///< 生效结束时间
    qint8 matchType = 0;                 ///< 触发方式：0为需符合全部条件，1为任一条件符合即执行
    qint8 status = 0;                    ///< 场景状态：0正常，1关闭
    QList<StrategyAction> actions;             ///< 执行设备
    QList<StrategyCondition> conditions;       ///< 条件
};


// old 旧策略
struct AutoStrategyConfig {
    int strategyId = 0;
    QString name;
    int groupId = 0;
    qint8 channel = 0;
    QString action = "stop";
    int intervalSec = 60;
    bool enabled = true;
    bool autoStart = true;
    QString triggerType = "interval";
    QString dailyTime;
};

}
}

#endif // STRATEGY_TYPES_H
