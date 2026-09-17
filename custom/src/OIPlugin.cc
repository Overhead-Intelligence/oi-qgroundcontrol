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
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QIODevice>
#include <QtCore/QMetaType>
#include <QtCore/QSettings>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtQml/QQmlApplicationEngine>

#include "AppSettings.h"
#include "FactMetaData.h"
#include "FactValueGrid.h"
#include "InstrumentValueData.h"
#include "QGCLoggingCategory.h"
#include "QmlObjectListModel.h"
#include "SettingsManager.h"

QGC_LOGGING_CATEGORY(OILog, "OI.Plugin")

Q_APPLICATION_STATIC(OIPlugin, _oiPluginInstance);

namespace {

constexpr const char *kDefaultsResource = ":/custom/OI-defaults.ini";
constexpr const char *kActionsResource  = ":/custom/OI-Actions.json";
constexpr const char *kActionsFileName  = "OI-Actions.json";

/// One cell of the OI telemetry bar. Fact names use the capitalised spelling that
/// InstrumentValueData expects (the same spelling QGC writes to its settings file).
struct TelemetryCell {
    const char *factGroup;
    const char *factName;
    const char *text;
    const char *icon;       ///< empty string = no icon
    bool showUnits;
};

/// The OI telemetry bar: 7 columns x 2 rows, exported from the operator settings of the
/// previous OI build. Column order left to right, top row first.
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
};

constexpr int kTelemetryColumns = sizeof(kTelemetryBar) / sizeof(kTelemetryBar[0]);
constexpr int kTelemetryRows = 2;

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
}

QGCCorePlugin *OIPlugin::instance()
{
    return _oiPluginInstance();
}

void OIPlugin::init()
{
    QGCCorePlugin::init();
    _deployBundledActions();
}

QString OIPlugin::stableDownloadLocation() const
{
    return QStringLiteral("github.com/Overhead-Intelligence/oi-qgroundcontrol/releases");
}

/*===========================================================================*/

void OIPlugin::adjustSettingMetaData(const QString &settingsGroup, FactMetaData &metaData, bool &userVisible)
{
    QGCCorePlugin::adjustSettingMetaData(settingsGroup, metaData, userVisible);

    // QSettings stores keys of the empty group (App and MAVLink settings) under [General].
    const QString iniGroup = settingsGroup.isEmpty() ? QStringLiteral("General") : settingsGroup;
    const QString key = iniGroup + QLatin1Char('/') + metaData.name();
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

void OIPlugin::_deployBundledActions()
{
    const QString saveDir = SettingsManager::instance()->appSettings()->mavlinkActionsSavePath();
    if (saveDir.isEmpty()) {
        qCWarning(OILog) << "MavlinkActions save path not available, OI actions not deployed";
        return;
    }

    QFile bundled(QString::fromLatin1(kActionsResource));
    if (!bundled.open(QIODevice::ReadOnly)) {
        qCWarning(OILog) << "Bundled actions resource missing:" << kActionsResource;
        return;
    }
    const QByteArray bundledBytes = bundled.readAll();

    if (!QDir().mkpath(saveDir)) {
        qCWarning(OILog) << "Could not create" << saveDir;
        return;
    }

    const QString targetPath = QDir(saveDir).filePath(QString::fromLatin1(kActionsFileName));
    QFile target(targetPath);
    if (target.exists() && target.open(QIODevice::ReadOnly)) {
        const bool unchanged = (target.readAll() == bundledBytes);
        target.close();
        if (unchanged) {
            qCDebug(OILog) << "OI actions already current at" << targetPath;
            return;
        }
    }

    if (!target.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(OILog) << "Could not write" << targetPath << target.errorString();
        return;
    }
    target.write(bundledBytes);
    target.close();
    qCDebug(OILog) << "OI actions deployed to" << targetPath;
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
