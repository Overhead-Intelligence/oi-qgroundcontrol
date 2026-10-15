/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * Fleet scripting: keeps a configured set of Lua scripts present on every
 * aircraft the GCS connects to, without the operator having to remember.
 *
 * A single GCS meets many aircraft over a release cycle - tuning, flight test,
 * a bird back from the shop - and "which scripts are on this one?" is the kind
 * of setup step that is only noticed when it is missing. This checks on
 * connect and fills in whatever is absent or out of date.
 *
 * Three things shape the design:
 *
 * 1. FTP is a single lane. Vehicle::ftpManager() runs one operation at a time
 *    and simply returns false for a second caller, and it is already shared by
 *    ParameterManager (the @PARAM/param.pck fetch on every connect), component
 *    metadata, Onboard Logs, Onboard Files and the vehicle-config Scripting
 *    tab. A background sync that grabs that lane makes the operator's own file
 *    work fail for reasons they cannot see - and if it wins the race at connect
 *    it pushes ParameterManager onto the slow per-parameter path, which just
 *    looks like "QGC got slower". So this yields: it checks inProgress()
 *    before every step, abandons the pass when anything else wants the lane,
 *    and never starts before parametersReady.
 *
 * 2. Identity is contents, not name. MAVLink FTP exposes CalcFileCRC32, and
 *    ArduPilot implements it (GCS_FTP.cpp -> AP_Filesystem::crc32). A listing
 *    already carries each file's size, so size is the free first test and the
 *    checksum is only spent when sizes agree. See FTPManager::crc32ForLocalFile
 *    for why this is not zlib's crc32.
 *
 * 3. Nothing is ever deleted. This uploads, and that is all. Removing a .lua
 *    from an aircraft because the GCS did not recognise it is a flight-safety
 *    action with no business happening in the background; unrecognised scripts
 *    are reported instead, which is also the more useful signal.
 *
 * The sync is per-vehicle, not per-active-vehicle. FTPManager belongs to the
 * Vehicle, so two connected aircraft have two independent lanes and can sync at
 * once; following focus instead would abandon an upload mid-queue the moment
 * the operator looked at another aircraft, leaving it with new scripts and a
 * stale Lua VM. The UI follows focus, the work does not.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "QGCLoggingCategory.h"

class QTimer;
class QmlObjectListModel;
class Vehicle;

Q_DECLARE_LOGGING_CATEGORY(OIFleetScriptsLog)

/*===========================================================================*/

/// One script in the operator's configured set.
///
/// The GCS keeps its own copy rather than referencing where the file was picked
/// from: this set is the fleet's reference, and a reference that disappears when
/// somebody tidies their Downloads folder is not one.
class OIFleetScript : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString fileName READ fileName CONSTANT)
    Q_PROPERTY(QString filePath READ filePath CONSTANT)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(qint64 sizeBytes READ sizeBytes NOTIFY contentChanged)
    Q_PROPERTY(bool valid READ valid NOTIFY contentChanged)

public:
    OIFleetScript(const QString &filePath, bool enabled, QObject *parent = nullptr);

    /// Base name, e.g. "track_loiter.lua". This is the identity: it is what the file
    /// is called in the aircraft's flat /APM/scripts, so two configured scripts
    /// cannot share one.
    QString fileName() const { return _fileName; }
    QString filePath() const { return _filePath; }

    bool enabled() const { return _enabled; }
    void setEnabled(bool enabled);

    qint64 sizeBytes() const { return _sizeBytes; }
    quint32 crc32() const { return _crc32; }

    /// False once the local copy has gone missing or become unreadable. The row stays
    /// visible so the operator sees why it stopped being uploaded.
    bool valid() const { return _valid; }

    /// Re-reads size and checksum from disk.
    void refresh();

signals:
    void enabledChanged();
    void contentChanged();

private:
    const QString _filePath;
    const QString _fileName;
    bool _enabled;
    qint64 _sizeBytes = 0;
    quint32 _crc32 = 0;
    bool _valid = false;
};

/*===========================================================================*/

class OIFleetScriptManager;

/// The sync worker for one aircraft. Owns everything that talks to that
/// vehicle's FTPManager, and the status line the operator reads.
class OIFleetVehicleSync : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(int statusLevel READ statusLevel NOTIFY statusChanged)
    Q_PROPERTY(int unmanagedCount READ unmanagedCount NOTIFY statusChanged)
    Q_PROPERTY(QStringList unmanagedScripts READ unmanagedScripts NOTIFY statusChanged)

public:
    /// How the status line should be coloured. Red is kept for things that are
    /// actually wrong - no vehicle and armed are normal states, not faults.
    enum StatusLevel {
        LevelNeutral = 0,   ///< nothing to say
        LevelBusy,          ///< working
        LevelGood,          ///< in sync
        LevelCaution,       ///< blocked on something the operator can fix
        LevelError,         ///< broken
    };
    Q_ENUM(StatusLevel)

    OIFleetVehicleSync(Vehicle *vehicle, OIFleetScriptManager *manager, QObject *parent = nullptr);

    QString statusText() const { return _statusText; }
    int statusLevel() const { return _statusLevel; }

    int unmanagedCount() const { return _unmanagedScripts.count(); }
    QStringList unmanagedScripts() const { return _unmanagedScripts; }

    Vehicle *vehicle() const { return _vehicle; }

    /// Re-runs the check. Called when the configured set changes, when the aircraft
    /// disarms, and from the Refresh button.
    void requestProbe();

signals:
    void statusChanged();

private slots:
    void _onListDirectoryComplete(const QStringList &entries, const QString &errorMsg);
    void _onCalcFileCRC32Complete(const QString &file, quint32 crc32, const QString &errorMsg);
    void _onUploadComplete(const QString &file, const QString &errorMsg);
    void _onCommandProgress(float value);
    void _onArmedChanged(bool armed);
    void _onParametersReadyChanged(bool ready);
    void _retryTick();

private:
    enum class Phase {
        Idle,
        Listing,
        Checksumming,
        Uploading,
        Done,
    };

    void _setStatus(const QString &text, StatusLevel level);
    void _beginPass();
    void _startListing();
    void _nextChecksum();
    void _finishProbe();
    void _nextUpload();
    void _finishUploads();
    void _restartScripting();

    /// True when the vehicle's FTP lane is free. Everything this class does is
    /// conditional on it; see the note at the top of the file.
    bool _ftpAvailable() const;
    /// Schedules another attempt without holding the lane in the meantime.
    void _retryLater(const QString &text, StatusLevel level);

    /// SCR_ENABLE, or -1 when parameters are not loaded yet.
    int _scriptingEnabledParam() const;

    /// Scripts the aircraft holds that are not in the configured set. Reported, never
    /// touched - see the file header.
    void _collectUnmanaged();

    Vehicle *_vehicle = nullptr;
    OIFleetScriptManager *_manager = nullptr;
    QTimer *_retryTimer = nullptr;

    Phase _phase = Phase::Idle;
    QString _statusText;
    int _statusLevel = LevelNeutral;

    /// fileName -> size, from the directory listing.
    QHash<QString, qint64> _remoteSizes;
    /// Configured scripts whose size matches and so still need a checksum.
    QStringList _checksumQueue;
    /// fileName -> checksum, for those we asked about.
    QHash<QString, quint32> _remoteCrcs;
    QStringList _uploadQueue;
    int _uploadIndex = 0;
    int _uploadTotal = 0;
    QString _currentUploadName;
    float _uploadProgress = 0.0F;
    QStringList _unmanagedScripts;

    /// Uploads performed in this pass, which decides whether the operator is told
    /// anything at all. A pass that finds everything in place stays silent.
    int _uploadedThisPass = 0;
    /// Passes spent on the current request. A pass that uploads is followed by one
    /// that verifies; if that one still finds a mismatch something is wrong with the
    /// writes themselves, and repeating it forever would only hammer the SD card.
    int _passCount = 0;
    bool _announcedUploading = false;

    static constexpr int kMaxPasses = 2;
    /// Long enough that a background check is never what loses a race for the FTP
    /// lane, short enough that the operator is not left waiting after a disarm.
    static constexpr int kRetryIntervalMs = 3000;
};

/*===========================================================================*/

/// Owns the configured script set, its folder on disk, and one sync per connected
/// vehicle. Registered as the QML singleton OIFleetScripts.
class OIFleetScriptManager : public QObject
{
    Q_OBJECT
    Q_MOC_INCLUDE("QmlObjectListModel.h")

    Q_PROPERTY(QmlObjectListModel *scripts READ scripts CONSTANT)
    Q_PROPERTY(OIFleetVehicleSync *activeSync READ activeSync NOTIFY activeSyncChanged)
    Q_PROPERTY(QString activeVehicleName READ activeVehicleName NOTIFY activeSyncChanged)
    Q_PROPERTY(int enabledCount READ enabledCount NOTIFY scriptsChanged)
    Q_PROPERTY(QString scriptsFolder READ scriptsFolder NOTIFY scriptsChanged)

public:
    /// Where scripts live on an ArduPilot SD card. AP_Scripting::init() creates this
    /// itself whenever SCR_ENABLE is 1, which is why a missing folder means something
    /// more serious than "nobody has put a script here yet".
    static constexpr const char *kVehicleScriptsDir = "/APM/scripts/";

    explicit OIFleetScriptManager(QObject *parent = nullptr);

    QmlObjectListModel *scripts() const { return _scripts; }
    OIFleetVehicleSync *activeSync() const { return _activeSync; }
    QString activeVehicleName() const;
    int enabledCount() const;
    /// Where imported scripts are kept, created on demand. Empty when the application
    /// save path is unset. Resolved per call, so it follows the save path setting.
    QString scriptsFolder() const;

    /// Scripts that should be on every aircraft, in list order.
    QList<OIFleetScript *> enabledScripts() const;

    /// Copies a .lua into the fleet folder and adds it to the set. Accepts a local path
    /// or a file:// URL. Returns false and sets lastError() if the file cannot be read,
    /// is not a .lua, or would collide with a script already configured.
    Q_INVOKABLE bool importScript(const QString &fileUrlOrPath);

    /// Removes the script from the set and deletes the GCS copy. Never touches a vehicle.
    Q_INVOKABLE void removeScript(int index);

    Q_INVOKABLE void setScriptEnabled(int index, bool enabled);

    /// Re-checks every connected aircraft.
    Q_INVOKABLE void refreshAll();

    Q_INVOKABLE QString lastError() const { return _lastError; }

    /// Puts a line in the vehicle's own message panel, at INFO.
    ///
    /// Goes in at StatusTextHandler rather than through Vehicle::_textMessageReceived,
    /// which is the path a real STATUSTEXT takes and reads anything at NOTICE or worse
    /// out loud. INFO is already under that threshold, but the entry point is what
    /// makes that true regardless of the severity someone picks later - and INFO is
    /// also the only severity the panel renders plainly, since NOTICE and WARNING are
    /// both bold orange.
    static void sayToVehicle(Vehicle *vehicle, const QString &text);

signals:
    void activeSyncChanged();
    void scriptsChanged();

private slots:
    void _onVehicleAdded(Vehicle *vehicle);
    void _onVehicleRemoved(Vehicle *vehicle);
    void _onActiveVehicleChanged(Vehicle *vehicle);

private:
    void _load();
    void _saveEnabledFlags() const;
    void _connectScript(OIFleetScript *script);
    void _notifySyncsOfSetChange();

    QmlObjectListModel *_scripts = nullptr;
    QHash<Vehicle *, OIFleetVehicleSync *> _syncs;
    OIFleetVehicleSync *_activeSync = nullptr;
    Vehicle *_activeVehicle = nullptr;
    QString _lastError;
};
