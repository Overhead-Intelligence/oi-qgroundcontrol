/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build (QGC 5.0 line).
 *
 * OIPlugin is the QGCCorePlugin subclass that QGC instantiates for this
 * build (CUSTOMCLASS in custom/CMakeLists.txt). It applies the OI defaults,
 * builds the OI telemetry bar, imports an operator's previous settings,
 * deploys the bundled custom actions, installs the QML/resource override
 * interceptor, and owns the keyboard guided-control singleton.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QList>
#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtQml/QQmlAbstractUrlInterceptor>

#include "QGCCorePlugin.h"

class FactMetaData;
class FactValueGrid;
class LinkInterface;
class OIKeyboardController;
class QQmlApplicationEngine;
class QSettings;
class Vehicle;

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

    /// Removes the URL interceptor before the QML engine goes away.
    void cleanup() final;

    /// Applies the defaults from custom/res/OI-defaults.ini to the setting being created.
    /// Returns the setting's user visibility, as the base class does.
    bool adjustSettingMetaData(const QString &settingsGroup, FactMetaData &metaData) final;

    /// Builds the OI telemetry bar layout when the operator has not customised the bar yet.
    void factValueGridCreateDefaultSettings(FactValueGrid *factValueGrid) final;

    /// Installs the override interceptor on the QML engine.
    QQmlApplicationEngine *createQmlApplicationEngine(QObject *parent) final;

    /// No first-run prompts: the OI defaults already answer the units and vehicle questions.
    QList<int> firstRunPromptStdIds() final { return QList<int>(); }

    /// Shown to the operator as the place to get builds (the update check itself is off in custom builds).
    QString stableDownloadLocation() const final;

    /// Feeds NAV_CONTROLLER_OUTPUT altitude error to the keyboard controller (altitude target display and clamp).
    bool mavlinkMessage(Vehicle *vehicle, LinkInterface *link, const mavlink_message_t &message) final;

    /// The keyboard guided-control singleton (also exposed to QML as OI.Controls/OIKeyboard).
    OIKeyboardController *keyboard() const { return _keyboard; }

private:
    /// One-time import of an operator's previous settings (the earlier OI build, else stock QGC)
    /// into a fresh settings file. Runs from the constructor.
    void _importLegacySettings();

    /// Copies custom/res/OI-Actions.json into the MavlinkActions save folder (overwrites the OI copy only).
    void _deployBundledActions();

    QSettings *_defaults = nullptr;                 ///< read-only view of :/custom/OI-defaults.ini
    QQmlApplicationEngine *_qmlEngine = nullptr;
    OIUrlInterceptor *_urlInterceptor = nullptr;
    OIKeyboardController *_keyboard = nullptr;
};
