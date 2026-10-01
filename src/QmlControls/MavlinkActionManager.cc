#include "MavlinkActionManager.h"
#include "MavlinkAction.h"
#include "Fact.h"
#include "JsonParsing.h"
#include "AppMessages.h"
#include "SettingsManager.h"
#include "AppSettings.h"
#include "QGCLoggingCategory.h"
#include "QmlObjectListModel.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QRegularExpression>
#include <QtQml/QQmlEngine>

QGC_LOGGING_CATEGORY(MavlinkActionManagerLog, "QMLControls.MavlinkActionManager")

MavlinkActionManager::MavlinkActionManager(QObject *parent)
    : QObject(parent)
    , _actions(new QmlObjectListModel(this))
{
    // qCDebug(MavlinkActionManagerLog) << Q_FUNC_INFO << this;
}

MavlinkActionManager::MavlinkActionManager(Fact *actionFileNameFact, QObject *parent)
    : QObject(parent)
    , _actions(new QmlObjectListModel(this))
{
    setActionFileNameFact(actionFileNameFact);
}

MavlinkActionManager::~MavlinkActionManager()
{
    // qCDebug(MavlinkActionManagerLog) << Q_FUNC_INFO << this;
}

QStringList MavlinkActionManager::fileNamesFromSettingValue(const QString &settingValue)
{
    QStringList fileNames;

    // `,` is accepted alongside the `;` we write because a settings override file is
    // hand-edited and comma is the obvious thing to reach for.
    const QStringList parts = settingValue.split(QRegularExpression(QStringLiteral("[;,]")), Qt::SkipEmptyParts);
    for (const QString &part: parts) {
        const QString fileName = part.trimmed();
        // Naming the same file twice would send every one of its actions twice.
        if (!fileName.isEmpty() && !fileNames.contains(fileName)) {
            fileNames.append(fileName);
        }
    }

    return fileNames;
}

void MavlinkActionManager::setActionFileNameFact(Fact *actionFileNameFact)
{
    _actionFileNameFact = actionFileNameFact;
    emit actionFileNameFactChanged();
    (void) connect(_actionFileNameFact, &Fact::rawValueChanged, this, &MavlinkActionManager::_loadActionsFiles);

    _loadActionsFiles();
}

void MavlinkActionManager::_loadActionsFiles()
{
    _actions->clearAndDeleteContents();

    const QStringList fileNames = fileNamesFromSettingValue(_actionFileNameFact->rawValue().toString());

    // Errors are collected rather than reported as they happen so that enabling several
    // files at once cannot produce a stack of dialogs.
    QStringList errorStrings;
    for (const QString &fileName: fileNames) {
        QString errorString;
        if (!_appendActionsFromFile(fileName, errorString)) {
            errorStrings.append(errorString);
        }
    }

    if (!errorStrings.isEmpty()) {
        QGC::showAppMessage(tr("Custom actions files could not be loaded:\n%1").arg(errorStrings.join(QStringLiteral("\n"))));
    }

    qCDebug(MavlinkActionManagerLog) << "loaded" << _actions->count() << "action(s) from" << fileNames;

    emit actionsChanged();
}

bool MavlinkActionManager::_appendActionsFromFile(const QString &fileName, QString &errorString)
{
    errorString.clear();

    // Custom actions are always loaded from the custom actions save path
    const QString savePath = SettingsManager::instance()->appSettings()->mavlinkActionsSavePath();
    const QDir saveDir = QDir(savePath);
    const QString fullPath = saveDir.absoluteFilePath(fileName);

    // It's ok for the file to not exist
    const QFileInfo fileInfo = QFileInfo(fullPath);
    if (!fileInfo.exists()) {
        qCDebug(MavlinkActionManagerLog) << "actions file not found, skipping" << fullPath;
        return true;
    }

    constexpr const char *kQgcFileType = "MavlinkActions";
    constexpr const char *kActionListKey = "actions";

    QString jsonErrorString;
    int version;
    const QJsonObject jsonObject = JsonParsing::openInternalQGCJsonFile(fullPath, kQgcFileType, 1, 1, version, jsonErrorString);
    if (!jsonErrorString.isEmpty()) {
        errorString = tr("`%1`: %2").arg(fileName, jsonErrorString);
        return false;
    }

    const QList<JsonParsing::KeyValidateInfo> keyInfoList = {
        { kActionListKey, QJsonValue::Array, /* required= */ true },
    };
    if (!JsonParsing::validateKeys(jsonObject, keyInfoList, jsonErrorString)) {
        errorString = tr("`%1` - incorrect format: %2").arg(fileName, jsonErrorString);
        return false;
    }

    // Actions go into a local list first: a file which turns out to be malformed part way
    // through must not leave half of itself in the model alongside the other files' actions.
    QList<MavlinkAction*> loadedActions;
    const auto discardLoaded = [&loadedActions]() {
        qDeleteAll(loadedActions);
        loadedActions.clear();
    };

    const QJsonArray actionList = jsonObject[kActionListKey].toArray();
    for (const auto &actionJson: actionList) {
        if (!actionJson.isObject()) {
            errorString = tr("`%1` - incorrect format: JsonValue not an object").arg(fileName);
            discardLoaded();
            return false;
        }

        const QList<JsonParsing::KeyValidateInfo> actionKeyInfoList = {
            { "label",          QJsonValue::String, /* required= */ true },
            { "description",    QJsonValue::String, /* required= */ true },
            { "mavCmd",         QJsonValue::Double, /* required= */ true },

            { "compId",         QJsonValue::Double, /* required= */ false },
            { "param1",         QJsonValue::Double, /* required= */ false },
            { "param2",         QJsonValue::Double, /* required= */ false },
            { "param3",         QJsonValue::Double, /* required= */ false },
            { "param4",         QJsonValue::Double, /* required= */ false },
            { "param5",         QJsonValue::Double, /* required= */ false },
            { "param6",         QJsonValue::Double, /* required= */ false },
            { "param7",         QJsonValue::Double, /* required= */ false },
        };

        const auto actionObj = actionJson.toObject();
        if (!JsonParsing::validateKeys(actionObj, actionKeyInfoList, jsonErrorString)) {
            errorString = tr("`%1` - incorrect format: %2").arg(fileName, jsonErrorString);
            discardLoaded();
            return false;
        }

        const auto label = actionObj["label"].toString();
        const auto description = actionObj["description"].toString();
        const auto mavCmd = (MAV_CMD)actionObj["mavCmd"].toInt();
        const auto compId = (MAV_COMPONENT)actionObj["compId"].toInt(MAV_COMP_ID_AUTOPILOT1);
        const auto param1 = actionObj["param1"].toDouble(0.0);
        const auto param2 = actionObj["param2"].toDouble(0.0);
        const auto param3 = actionObj["param3"].toDouble(0.0);
        const auto param4 = actionObj["param4"].toDouble(0.0);
        const auto param5 = actionObj["param5"].toDouble(0.0);
        const auto param6 = actionObj["param6"].toDouble(0.0);
        const auto param7 = actionObj["param7"].toDouble(0.0);

        MavlinkAction *const action = new MavlinkAction(label, description, mavCmd, compId, param1, param2, param3, param4, param5, param6, param7, this);
        QQmlEngine::setObjectOwnership(action, QQmlEngine::CppOwnership);
        loadedActions.append(action);
    }

    for (MavlinkAction *const action: loadedActions) {
        (void) _actions->append(action);
    }

    return true;
}
