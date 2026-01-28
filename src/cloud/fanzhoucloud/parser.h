#ifndef STRATEGYPARSER_H
#define STRATEGYPARSER_H

#include <QString>
#include <QJsonObject>

namespace fanzhou {
namespace core {
struct StrategyAction;
struct AutoStrategy;
}
namespace cloud {
namespace fanzhoucloud {


//parseSetCommand()
// ├─ type == "scene" → parseSceneSetData()
// └─ type == "timer" → parseTimerSetData()

//parseSyncData()
// ├─ type == "scene" → parseSceneSyncData()
// └─ type == "timer" → parseTimerSyncData()


QList<core::AutoStrategy> parseSceneDataFromJson(const QJsonObject &obj);

bool parseAutoStrategyFromJson(const QJsonObject &obj, core::AutoStrategy &out, QString *error = nullptr);
bool parseSceneActions(const QJsonArray &arr,
                       QList<core::StrategyAction> &out,
                       QString *error);
bool parseNodeChannelKey(const QString &key, int &node, int &channel);


bool parseSetCommand(
    const QString &type,               // scene / timer
    const QJsonObject &data,
    core::AutoStrategy &out,
    QString *error);

QList<core::AutoStrategy> parseSyncData(
    const QString &type,
    const QJsonObject &obj,
    QString *error);


// scene
bool parseSceneSetData(const QJsonObject &obj,
                       core::AutoStrategy &out,
                       QString *error);

QList<core::AutoStrategy> parseSceneSyncData(const QJsonObject &obj);

// timer
bool parseTimerSetData(const QJsonObject &obj,
                       core::AutoStrategy &out,
                       QString *error);

QList<core::AutoStrategy> parseTimerSyncData(const QJsonObject &obj);


bool parseDeleteCommand(const QString &type,
                        const QJsonValue &data,
                        QList<int> &sceneIds,
                        QString *error);

}
}
}

#endif // STRATEGYPARSER_H
