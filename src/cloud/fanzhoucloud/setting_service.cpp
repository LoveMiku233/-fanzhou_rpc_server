#include "setting_service.h"

namespace fanzhou {
namespace cloud {
namespace fanzhoucloud {



// @return: true: respond to request; false: dont respond
bool SettingService::handleRequest(const QJsonObject &req, QJsonObject &resp, QString &error)
{
// handle request
//    {
//      "method": "get_response",
//      "type": "scene",
//      "data": {
//        "code": 0,
//        "message": "",
//        "result": [
//          {
//            "id": 100,						// 场景ID
//            "sceneName": "",					// 场景名称
//            "sceneType": "auto|manual",			// auto自动场景，manual手动场景
//            "matchType": 0|1,					// 触发方式：0为需符合全部条件，1为任一条件符合即执行
//            "effectiveBeginTime": "06:00",		// 生效开始时间
//            "effectiveEndTime": "18:00",		// 生效结束时间
//            "status": 1|0,					// 场景状态：0正常，1关闭
//            "version": 1,					// 场景版本号，通过版本号比对是否需要更新，每编辑一次，版本号+1
//            "updateTime": "2026-01-15 10:30:00",	// 最后更新时间
//            "actions": [{
//              "deviceCode": "864708069172099",	// 设备编码
//              "identifier": "sw2",			// 属性标识
//              "identifierValue": 1			// 属性值
//            }],
//            // 手动场景无需条件
//            "conditions": [{
//              "deviceCode": "864708069172099",	// 设备编码
//              "identifier": "airTemp",		// 属性标识
//              "identifierValue": 30,			// 属性值
//              "op": "egt"				// 条件操作符 eq等于 ne不等于 gt大于 lt小于 egt大等于 elt小等于
//            }]
//          }
//        ]
//      },
//      "requestId": "req_202307011430001",
//      "timestamp": 1625123457890
//    }


}


bool SettingService::handleScene(const QString &method,
                 const QJsonObject &data,
                 QJsonObject &resp,
                 QString &error)
{

}

bool SettingService::handleTimer(const QString &method,
                 const QJsonObject &data,
                 QJsonObject &resp,
                 QString &error)
{

}


} // namespace fanzhoucloud
} // namespace cloud
} // namespace fanzhou
