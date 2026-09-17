/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * OIPlugin is the QGCCorePlugin subclass that QGC instantiates for this
 * build (CUSTOMCLASS in custom/CMakeLists.txt). It applies the OI defaults,
 * builds the OI telemetry bar, deploys the bundled custom actions and
 * installs the QML/resource override interceptor.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtQml/QQmlAbstractUrlInterceptor>

#include "QGCCorePlugin.h"

class FactMetaData;
class FactValueGrid;
class OIKeyboardController;
class QQmlApplicationEngine;
class QSettings;

Q_DECLARE_LOGGING_CATEGORY(OILog)

/// Serves qrc:/<path> from :/Custom/<path> whenever custom.qrc provides an override.
/// This is how stock QML files and images are replaced without editing src/.
class OIUrlInterceptor : public QQmlAbstractUrlInterceptor
{
public:
    OIUrlInterceptor() = default;

    QUrl intercept(const QUrl &url, QQmlAbstractUrlInterceptor::DataType type) final;
};

class OIPlugin : public QGCCorePlugin
{
    Q_OBJECT

public:
    explicit OIPlugin(QObject *parent = nullptr);

    static QGCCorePlugin *instance();

    // QGCCorePlugin overrides

    /// Creates the OI objects that need the settings system to exist and deploys the bundled actions file.
    void init() final;

    /// Applies the defaults from custom/res/OI-defaults.ini to the setting being created.
    void adjustSettingMetaData(const QString &settingsGroup, FactMetaData &metaData, bool &userVisible) final;

    /// Builds the OI telemetry bar layout when the operator has not customised the bar yet.
    void factValueGridCreateDefaultSettings(FactValueGrid *factValueGrid) final;

    /// Installs the override interceptor on the QML engine.
    QQmlApplicationEngine *createQmlApplicationEngine(QObject *parent) final;

    /// Removes the interceptor before the engine is destroyed.
    void destroyQmlApplicationEngine(QQmlApplicationEngine *qmlEngine) final;

    /// Shown to the operator as the place to get builds (the update check itself is off in custom builds).
    QString stableDownloadLocation() const final;

    /// No first-run "Preferences" prompt: the OI defaults already answer the vehicle and units
    /// questions (both false makes firstRunPromptStdIds() empty).
    bool showInitialSetupVehiclePreferences() const final { return false; }
    bool showInitialSetupMeasurementUnits() const final { return false; }

    /// Feeds NAV_CONTROLLER_OUTPUT altitude error to the keyboard controller (altitude target display and clamp).
    bool mavlinkMessage(Vehicle *vehicle, LinkInterface *link, const mavlink_message_t &message) final;

    /// The keyboard guided-control singleton (also exposed to QML as OI.Controls/OIKeyboard).
    OIKeyboardController *keyboard() const { return _keyboard; }

private:
    /// One-time import of the telemetry bar, links, units, video and Fly view settings from the
    /// previous OI build (or stock QGC) into a fresh settings file. Runs from the constructor.
    void _importLegacySettings();

    /// Copies custom/res/OI-Actions.json into the MavlinkActions save folder (overwrites the OI copy only).
    void _deployBundledActions();

    QSettings *_defaults = nullptr;                 ///< read-only view of :/custom/OI-defaults.ini
    QQmlApplicationEngine *_qmlEngine = nullptr;
    OIUrlInterceptor *_urlInterceptor = nullptr;
    OIKeyboardController *_keyboard = nullptr;
};
