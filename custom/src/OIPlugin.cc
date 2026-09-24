/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#include "OIPlugin.h"

#include <QtCore/QApplicationStatic>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QIODevice>
#include <QtCore/QMetaType>
#include <QtCore/QSettings>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/qqml.h>

#include "AppSettings.h"
#include "Fact.h"
#include "FactMetaData.h"
#include "FactValueGrid.h"
#include "InstrumentValueData.h"
#include "MavlinkActionManager.h"
#include "MavlinkActionsSettings.h"
#include "OIKeyboardController.h"
#include "OIMapOverlays.h"
#include "QGCLoggingCategory.h"
#include "QmlComponentInfo.h"
#include "QmlObjectListModel.h"
#include "SettingsManager.h"

QGC_LOGGING_CATEGORY(OILog, "OI.Plugin")

Q_APPLICATION_STATIC(OIPlugin, _oiPluginInstance);

namespace {

constexpr const char *kDefaultsResource = ":/custom/OI-defaults.ini";

/// The bundled MAVLink actions, one file per capability rather than one combined list.
/// None of them is enabled by default (see [MavlinkActions] in OI-defaults.ini): every one
/// of these needs hardware or an aircraft-side script that only some airframes have, so a
/// kit is configured by ticking the files that apply to it in Fly View Settings.
constexpr const char *kActionsFileNames[] = {
    "OI-Gripper.json",
    "OI-Starnav.json",
    "OI-WingtipLights.json",
};

/// Up to 1.0.1 the above shipped as one combined OI-Actions.json which the defaults
/// selected. That file is retired: it also carried a "PosXY GPS Enable"/"Disable" pair
/// written against a much older starnav.lua, and against the current script those two are
/// inverted - "Enable" sends SCRIPTING_4 LOW, which selects EK3_SRC1_POSXY/VELXY = 0/0, the
/// no-aiding set, not GPS. OI-Starnav.json drives STARNAV_ENABLE instead, which is the
/// control the current script actually supports.
///
/// An operator's copy is deleted only when it is byte-for-byte the one we shipped, so an
/// edited file is never touched. The fingerprint is taken with CR stripped: the resource is
/// embedded from the working tree, which is CRLF on Windows and LF elsewhere.
constexpr const char *kLegacyActionsFileName = "OI-Actions.json";
constexpr const char *kLegacyActionsSha256 =
    "5030e54dd99493ea7d8b4305c0eaaf18954b0b9b9f138e293246eb57c58ed591";
constexpr const char *kLegacyActionsRetiredKey = "OI/retiredLegacyActionsFile";

/// One cell of the OI telemetry bar. Fact names use the capitalised spelling that
/// InstrumentValueData expects (the same spelling QGC writes to its settings file).
struct TelemetryCell {
    const char *factGroup;
    const char *factName;
    const char *text;
    const char *icon;       ///< empty string = no icon
    bool showUnits;
};

/// The OI telemetry bar: 8 columns x 2 rows, taken from Roger's live operator settings of
/// the previous OI build (2026-09-16). Column order left to right, top row first.
constexpr TelemetryCell kTelemetryBar[][2] = {
    { { "Vehicle",  "AltitudeRelative",  "Alt (Rel)",           "arrow-thick-up.svg",    true  },
      { "Vehicle",  "DistanceToHome",    "Distance to Home",    "bookmark copy 3.svg",   true  } },
    { { "Vehicle",  "ClimbRate",         "Climb Rate",          "arrow-simple-up.svg",   true  },
      { "Vehicle",  "GroundSpeed",       "Ground Speed",        "arrow-simple-right.svg", true } },
    { { "Vehicle",  "AirSpeed",          "AirSpd",              "",                      true  },
      { "Vehicle",  "ThrottlePct",       "Thr",                 "",                      true  } },
    { { "Vehicle",  "FlightTime",        "Flight Time",         "timer.svg",             false },
      { "Vehicle",  "FlightDistance",    "Flight Distance",     "travel-walk.svg",       true  } },
    { { "Vehicle",  "AltitudeAboveTerr", "Alt (Above Terrain)", "",                      true  },
      { "Battery0", "Voltage",           "Voltage",             "",                      true  } },
    { { "Wind",     "Direction",         "Wind Direction",      "",                      true  },
      { "Wind",     "Speed",             "Wind Spd",            "",                      true  } },
    { { "Gps",      "Mgrs",              "MGRS Position",       "",                      true  },
      { "Vehicle",  "MissionItemIndex",  "Mission Item Index",  "",                      true  } },
    { { "DistanceSensor", "RotationPitch270", "Down",           "",                      true  },
      { "DistanceSensor", "RotationNone",     "Forward",        "",                      true  } },
};

constexpr int kTelemetryColumns = static_cast<int>(sizeof(kTelemetryBar) / sizeof(kTelemetryBar[0]));
constexpr int kTelemetryRows = 2;

// One-time import of an operator's existing settings into a fresh settings file, so
// nobody rebuilds their telemetry bar or fleet links after installing this build.
// Candidates are the previous OI build and stock QGC, both under %APPDATA%\QGroundControl.
constexpr const char *kLegacyOrgName = "QGroundControl";
constexpr const char *kImportMarkerKey = "OI/importedSettingsFrom";
constexpr const char *kTelemetryBarGroupPrefix = "TelemetryBarUserSettings";

const QStringList kLegacyAppNames = {
    QStringLiteral("QGroundControl OI Build"),
    QStringLiteral("QGroundControl"),
};

/// Groups copied verbatim. Their keys are unchanged between QGC 5.0 and 5.1. Groups whose
/// key names changed (GimbalController) or whose values are version specific (FlightMode)
/// are left to the OI defaults instead.
const QStringList kImportGroups = {
    QStringLiteral("LinkConfigurations"),
    QStringLiteral("Units"),
    QStringLiteral("Video"),
    QStringLiteral("FlyView"),
    QStringLiteral("FlightMapPosition"),
};

int copySettingsGroup(QSettings &from, QSettings &to, const QString &group)
{
    from.beginGroup(group);
    to.beginGroup(group);
    const QStringList keys = from.allKeys();
    for (const QString &key : keys) {
        to.setValue(key, from.value(key));
    }
    to.endGroup();
    from.endGroup();
    return static_cast<int>(keys.size());
}

} // namespace

/*===========================================================================*/

QUrl OIUrlInterceptor::intercept(const QUrl &url, QQmlAbstractUrlInterceptor::DataType type)
{
    switch (type) {
    case QQmlAbstractUrlInterceptor::QmlFile:
    case QQmlAbstractUrlInterceptor::UrlString:
        if (url.scheme() == QStringLiteral("qrc")) {
            const QString origPath = url.path();
            const QString overrideRes = QStringLiteral(":/Custom%1").arg(origPath);
            if (QFile::exists(overrideRes)) {
                QUrl result;
                result.setScheme(QStringLiteral("qrc"));
                result.setPath(QStringLiteral("/Custom%1").arg(origPath));
                return result;
            }
        }
        break;
    default:
        break;
    }

    return url;
}

/*===========================================================================*/

OIPlugin::OIPlugin(QObject *parent)
    : QGCCorePlugin(parent)
    , _defaults(new QSettings(QString::fromLatin1(kDefaultsResource), QSettings::IniFormat, this))
{
    qCDebug(OILog) << this << "defaults:" << _defaults->allKeys().size() << "keys";

    // The plugin is created by the first SettingsGroup, before any setting has been read,
    // so imported values are picked up in this same run.
    _importLegacySettings();
}

QGCCorePlugin *OIPlugin::instance()
{
    return _oiPluginInstance();
}

void OIPlugin::init()
{
    QGCCorePlugin::init();
    _deployBundledActions();
    _retireLegacyActionsFile();

    // Created after SettingsManager::init() because it reads QSettings, and
    // registered before the QML engine exists so the Maps settings section can
    // "import OI.Controls" and reach the manager.
    _mapOverlays = new OIMapOverlayManager(this);
    (void) qmlRegisterSingletonInstance("OI.Controls", 1, 0, "OIMapOverlays", _mapOverlays);

    // Installs an application-wide key filter, so it must exist before the QML
    // engine does. It stays disarmed until an operator arms it.
    _keyboard = new OIKeyboardController(this);
    (void) qmlRegisterSingletonInstance("OI.Controls", 1, 0, "OIKeyboard", _keyboard);
}

QString OIPlugin::stableDownloadLocation() const
{
    return QStringLiteral("github.com/Overhead-Intelligence/oi-qgroundcontrol/releases");
}

const QmlObjectListModel *OIPlugin::customMapItems()
{
    // init() has always run by the time the Fly view map asks for these. Fall back
    // to the stock empty model if that ever stops being true.
    return _mapOverlays ? _mapOverlays->markers() : QGCCorePlugin::customMapItems();
}

/*===========================================================================*/

const QVariantList &OIPlugin::analyzePages()
{
    if (_analyzePages.isEmpty()) {
        // Start from the stock list so upstream additions keep showing up, then
        // append the OI page. requiresVehicle = true makes AnalyzeView show
        // "Requires a connected vehicle" instead of the browser until one is
        // connected, and unload it again on disconnect; FTPController has no
        // vehicle to talk to otherwise.
        _analyzePages = QGCCorePlugin::analyzePages();
        _analyzePages.append(QVariant::fromValue(new QmlComponentInfo(
            tr("Onboard Files"),
            QUrl::fromUserInput(QStringLiteral("qrc:/custom/qml/OIOnboardFilesPage.qml")),
            QUrl::fromUserInput(QStringLiteral("qrc:/InstrumentValueIcons/folder.svg")),
            nullptr,
            true /* requiresVehicle */)));
    }

    return _analyzePages;
}

/*===========================================================================*/

void OIPlugin::adjustSettingMetaData(const QString &settingsGroup, FactMetaData &metaData, bool &userVisible)
{
    QGCCorePlugin::adjustSettingMetaData(settingsGroup, metaData, userVisible);

    // Settings with an empty group name (App and MAVLink) sit in the [General] section of
    // the ini, which QSettings exposes as top-level keys (no "General/" prefix).
    const QString key = settingsGroup.isEmpty() ? metaData.name() : (settingsGroup + QLatin1Char('/') + metaData.name());
    if (!_defaults->contains(key)) {
        return;
    }

    QVariant rawValue = _defaults->value(key);
    if (rawValue.userType() == QMetaType::QStringList) {
        // An unquoted value with commas comes back as a list; put it back together.
        rawValue = rawValue.toStringList().join(QLatin1Char(','));
    }

    QVariant typedValue;
    QString errorString;
    if (!metaData.convertAndValidateRaw(rawValue, true /* convertOnly */, typedValue, errorString)) {
        qCWarning(OILog) << "OI-defaults.ini:" << key << "value" << rawValue << "does not fit setting type:" << errorString;
        return;
    }

    metaData.setRawDefaultValue(typedValue);
    qCDebug(OILog) << "OI default" << key << "=" << typedValue;
}

/*===========================================================================*/

void OIPlugin::factValueGridCreateDefaultSettings(FactValueGrid *factValueGrid)
{
    // Per-vehicle cards (multi-vehicle list) keep the stock two-value layout.
    if (factValueGrid->specificVehicleForCard()) {
        QGCCorePlugin::factValueGridCreateDefaultSettings(factValueGrid);
        return;
    }

    factValueGrid->setFontSize(FactValueGrid::LargeFontSize);

    // A fresh grid has one row and no columns.
    for (int col = 0; col < kTelemetryColumns; col++) {
        (void) factValueGrid->appendColumn();
    }
    for (int row = 1; row < kTelemetryRows; row++) {
        factValueGrid->appendRow();
    }

    for (int col = 0; col < kTelemetryColumns; col++) {
        QmlObjectListModel *column = factValueGrid->columns()->value<QmlObjectListModel*>(col);
        if (!column) {
            qCWarning(OILog) << "Telemetry bar column" << col << "missing";
            continue;
        }

        for (int row = 0; row < kTelemetryRows; row++) {
            InstrumentValueData *value = column->value<InstrumentValueData*>(row);
            if (!value) {
                qCWarning(OILog) << "Telemetry bar cell" << col << row << "missing";
                continue;
            }

            const TelemetryCell &cell = kTelemetryBar[col][row];
            value->setFact(QString::fromLatin1(cell.factGroup), QString::fromLatin1(cell.factName));
            value->setText(QString::fromLatin1(cell.text));
            if (cell.icon[0] != '\0') {
                value->setIcon(QString::fromLatin1(cell.icon));
            }
            value->setShowUnits(cell.showUnits);
        }
    }
}

/*===========================================================================*/

void OIPlugin::_importLegacySettings()
{
    QSettings settings;
    const QString markerKey = QString::fromLatin1(kImportMarkerKey);
    if (settings.contains(markerKey)) {
        return;
    }

    // Only a fresh settings file is seeded; anything the operator already saved here wins.
    const bool hasLinks = settings.contains(QStringLiteral("LinkConfigurations/count"));
    const bool hasTelemetryBar = !settings.childGroups().filter(QString::fromLatin1(kTelemetryBarGroupPrefix)).isEmpty();
    if (hasLinks || hasTelemetryBar) {
        settings.setValue(markerKey, QStringLiteral("skipped, settings already present"));
        return;
    }

    for (const QString &legacyApp : kLegacyAppNames) {
        QSettings legacy(QSettings::IniFormat, QSettings::UserScope, QString::fromLatin1(kLegacyOrgName), legacyApp);
        if (!QFile::exists(legacy.fileName())) {
            continue;
        }

        int copied = 0;
        const QStringList groups = legacy.childGroups();
        for (const QString &group : groups) {
            if (group.startsWith(QString::fromLatin1(kTelemetryBarGroupPrefix)) || kImportGroups.contains(group)) {
                copied += copySettingsGroup(legacy, settings, group);
            }
        }

        settings.setValue(markerKey, legacy.fileName());
        settings.sync();
        qCInfo(OILog) << "Imported" << copied << "settings from" << legacy.fileName();
        return;
    }

    settings.setValue(markerKey, QStringLiteral("no previous settings found"));
}

/*===========================================================================*/

void OIPlugin::_deployBundledActions()
{
    const QString saveDir = SettingsManager::instance()->appSettings()->mavlinkActionsSavePath();
    if (saveDir.isEmpty()) {
        qCWarning(OILog) << "MavlinkActions save path not available, OI actions not deployed";
        return;
    }

    if (!QDir().mkpath(saveDir)) {
        qCWarning(OILog) << "Could not create" << saveDir;
        return;
    }

    for (const char *const fileName : kActionsFileNames) {
        const QString resourcePath = QStringLiteral(":/custom/") + QString::fromLatin1(fileName);
        QFile bundled(resourcePath);
        if (!bundled.open(QIODevice::ReadOnly)) {
            qCWarning(OILog) << "Bundled actions resource missing:" << resourcePath;
            continue;
        }
        const QByteArray bundledBytes = bundled.readAll();

        // Overwrite our own copy so a fixed action reaches the operator, but only when it
        // has actually changed - rewriting it every start would churn the file's mtime and
        // make an edited copy indistinguishable from ours at a glance.
        const QString targetPath = QDir(saveDir).filePath(QString::fromLatin1(fileName));
        QFile target(targetPath);
        if (target.exists() && target.open(QIODevice::ReadOnly)) {
            const bool unchanged = (target.readAll() == bundledBytes);
            target.close();
            if (unchanged) {
                qCDebug(OILog) << "OI actions already current at" << targetPath;
                continue;
            }
        }

        if (!target.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qCWarning(OILog) << "Could not write" << targetPath << target.errorString();
            continue;
        }
        target.write(bundledBytes);
        target.close();
        qCInfo(OILog) << "Deployed OI actions to" << targetPath;
    }
}

/*===========================================================================*/

void OIPlugin::_retireLegacyActionsFile()
{
    QSettings settings;
    const QString retiredKey = QString::fromLatin1(kLegacyActionsRetiredKey);
    if (settings.contains(retiredKey)) {
        return;
    }

    const QString saveDir = SettingsManager::instance()->appSettings()->mavlinkActionsSavePath();
    if (saveDir.isEmpty()) {
        return;
    }

    const QString legacyName = QString::fromLatin1(kLegacyActionsFileName);
    const QString legacyPath = QDir(saveDir).filePath(legacyName);
    QFile legacy(legacyPath);
    if (!legacy.exists()) {
        settings.setValue(retiredKey, QStringLiteral("not present"));
        return;
    }

    if (!legacy.open(QIODevice::ReadOnly)) {
        qCWarning(OILog) << "Could not read" << legacyPath << "- leaving it alone";
        return;
    }
    QByteArray contents = legacy.readAll();
    legacy.close();
    contents.replace('\r', "");

    const QByteArray digest = QCryptographicHash::hash(contents, QCryptographicHash::Sha256).toHex();
    if (digest != QByteArray(kLegacyActionsSha256)) {
        // The operator has edited it. Their file, their call - leave it in place and stop
        // asking, so this never silently eats work.
        qCInfo(OILog) << legacyPath << "has been edited, keeping it";
        settings.setValue(retiredKey, QStringLiteral("kept, edited by operator"));
        return;
    }

    if (!QFile::remove(legacyPath)) {
        qCWarning(OILog) << "Could not remove" << legacyPath;
        return;
    }

    // Drop it from both menus, through the facts rather than QSettings directly so the
    // in-memory value and any MavlinkActionManager already watching it stay in step.
    MavlinkActionsSettings *const actionsSettings = SettingsManager::instance()->mavlinkActionsSettings();
    Fact *const facts[] = { actionsSettings->flyViewActionsFile(), actionsSettings->joystickActionsFile() };
    for (Fact *const fact : facts) {
        QStringList names = MavlinkActionManager::fileNamesFromSettingValue(fact->rawValue().toString());
        if (names.removeAll(legacyName) > 0) {
            fact->setRawValue(names.join(QStringLiteral(";")));
        }
    }

    settings.setValue(retiredKey, QStringLiteral("removed, unmodified"));
    qCInfo(OILog) << "Retired" << legacyPath << "in favour of the per-capability actions files";
}

/*===========================================================================*/

QQmlApplicationEngine *OIPlugin::createQmlApplicationEngine(QObject *parent)
{
    _qmlEngine = QGCCorePlugin::createQmlApplicationEngine(parent);

    _urlInterceptor = new OIUrlInterceptor();
    _qmlEngine->addUrlInterceptor(_urlInterceptor);

    return _qmlEngine;
}

void OIPlugin::destroyQmlApplicationEngine(QQmlApplicationEngine *qmlEngine)
{
    if (qmlEngine && (qmlEngine == _qmlEngine)) {
        qmlEngine->removeUrlInterceptor(_urlInterceptor);
        delete _urlInterceptor;
        _urlInterceptor = nullptr;
        _qmlEngine = nullptr;
    }

    QGCCorePlugin::destroyQmlApplicationEngine(qmlEngine);
}
