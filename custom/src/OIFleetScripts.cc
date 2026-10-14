/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * See OIFleetScripts.h for what this is and why it yields the FTP lane.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#include "OIFleetScripts.h"

#include "AppSettings.h"
#include "FTPManager.h"
#include "MAVLinkLib.h"
#include "MultiVehicleManager.h"
#include "ParameterManager.h"
#include "QGCFileHelper.h"
#include "QmlObjectListModel.h"
#include "SettingsManager.h"
#include "StatusTextHandler.h"
#include "Vehicle.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtCore/QTimer>
#include <QtCore/QUrl>

QGC_LOGGING_CATEGORY(OIFleetScriptsLog, "OI.FleetScripts")

namespace {

constexpr const char *kSettingsGroup     = "OI/FleetScripts";
constexpr const char *kFleetScriptsDir   = "FleetScripts";
constexpr const char *kScriptSuffix      = "lua";

/// SCRIPTING_CMD_STOP_AND_RESTART. Tears the Lua VM down and builds a new one, and
/// AP_Scripting::thread() constructs a fresh lua_scripts on the way round - which
/// re-reads the scripts directory. That is what makes an upload take effect without
/// a reboot.
constexpr float kScriptingCmdStopAndRestart = 3.0F;

} // namespace

/*===========================================================================*/

OIFleetScript::OIFleetScript(const QString &filePath, bool enabled, QObject *parent)
    : QObject(parent)
    , _filePath(filePath)
    , _fileName(QFileInfo(filePath).fileName())
    , _enabled(enabled)
{
    refresh();
}

void OIFleetScript::setEnabled(bool enabled)
{
    if (enabled == _enabled) {
        return;
    }
    _enabled = enabled;
    emit enabledChanged();
}

void OIFleetScript::refresh()
{
    const QFileInfo info(_filePath);
    const qint64 previousSize = _sizeBytes;
    const quint32 previousCrc = _crc32;
    const bool previousValid = _valid;

    if (!info.exists() || !info.isFile()) {
        _sizeBytes = 0;
        _crc32 = 0;
        _valid = false;
    } else {
        bool ok = false;
        const quint32 crc = FTPManager::crc32ForLocalFile(_filePath, &ok);
        _sizeBytes = ok ? info.size() : 0;
        _crc32 = ok ? crc : 0;
        _valid = ok;
    }

    if (_sizeBytes != previousSize || _crc32 != previousCrc || _valid != previousValid) {
        emit contentChanged();
    }
}

/*===========================================================================*/

OIFleetVehicleSync::OIFleetVehicleSync(Vehicle *vehicle, OIFleetScriptManager *manager, QObject *parent)
    : QObject(parent)
    , _vehicle(vehicle)
    , _manager(manager)
    , _retryTimer(new QTimer(this))
{
    _retryTimer->setSingleShot(true);
    _retryTimer->setInterval(kRetryIntervalMs);
    connect(_retryTimer, &QTimer::timeout, this, &OIFleetVehicleSync::_retryTick);

    FTPManager *const ftp = _vehicle->ftpManager();
    connect(ftp, &FTPManager::listDirectoryComplete, this, &OIFleetVehicleSync::_onListDirectoryComplete);
    connect(ftp, &FTPManager::calcFileCRC32Complete, this, &OIFleetVehicleSync::_onCalcFileCRC32Complete);
    connect(ftp, &FTPManager::uploadComplete, this, &OIFleetVehicleSync::_onUploadComplete);
    connect(ftp, &FTPManager::commandProgress, this, &OIFleetVehicleSync::_onCommandProgress);

    connect(_vehicle, &Vehicle::armedChanged, this, &OIFleetVehicleSync::_onArmedChanged);
    connect(_vehicle->parameterManager(), &ParameterManager::parametersReadyChanged,
            this, &OIFleetVehicleSync::_onParametersReadyChanged);

    requestProbe();
}

void OIFleetVehicleSync::_setStatus(const QString &text, StatusLevel level)
{
    if (text == _statusText && level == _statusLevel) {
        return;
    }
    _statusText = text;
    _statusLevel = level;
    emit statusChanged();
}

bool OIFleetVehicleSync::_ftpAvailable() const
{
    return _vehicle && !_vehicle->ftpManager()->inProgress();
}

void OIFleetVehicleSync::_retryLater(const QString &text, StatusLevel level)
{
    _phase = Phase::Idle;
    _setStatus(text, level);
    _retryTimer->start();
}

void OIFleetVehicleSync::_retryTick()
{
    _beginPass();
}

int OIFleetVehicleSync::_scriptingEnabledParam() const
{
    if (!_vehicle || !_vehicle->parameterManager()->parametersReady()) {
        return -1;
    }
    if (!_vehicle->parameterManager()->parameterExists(MAV_COMP_ID_AUTOPILOT1, QStringLiteral("SCR_ENABLE"))) {
        return -1;
    }
    Fact *const fact = _vehicle->parameterManager()->getParameter(MAV_COMP_ID_AUTOPILOT1, QStringLiteral("SCR_ENABLE"));
    return fact ? fact->rawValue().toInt() : -1;
}

void OIFleetVehicleSync::requestProbe()
{
    _passCount = 0;
    _uploadedThisPass = 0;
    _announcedUploading = false;
    _beginPass();
}

void OIFleetVehicleSync::_beginPass()
{
    _retryTimer->stop();

    _remoteSizes.clear();
    _checksumQueue.clear();
    _remoteCrcs.clear();
    _uploadQueue.clear();
    _unmanagedScripts.clear();
    _phase = Phase::Idle;

    if (!_vehicle) {
        _setStatus(tr("No vehicle connected"), LevelNeutral);
        return;
    }

    if (_manager->enabledScripts().isEmpty()) {
        _setStatus(tr("No scripts selected"), LevelNeutral);
        return;
    }

    // Parameters are not just needed for SCR_ENABLE: ParameterManager fetches them over
    // this same FTP lane, and starting here first would push it onto the slow
    // per-parameter path for the whole connect.
    if (!_vehicle->parameterManager()->parametersReady()) {
        _setStatus(tr("Waiting for vehicle parameters"), LevelBusy);
        return;
    }

    if (_scriptingEnabledParam() == 0) {
        _setStatus(tr("Scripting disabled on vehicle (SCR_ENABLE = 0)"), LevelCaution);
        return;
    }

    if (_vehicle->armed()) {
        _setStatus(tr("Vehicle is armed"), LevelCaution);
        return;
    }

    _startListing();
}

void OIFleetVehicleSync::_startListing()
{
    if (!_ftpAvailable()) {
        _retryLater(tr("Vehicle busy with another file transfer"), LevelBusy);
        return;
    }

    _setStatus(tr("Checking vehicle scripts..."), LevelBusy);
    _phase = Phase::Listing;

    if (!_vehicle->ftpManager()->listDirectory(MAV_COMP_ID_AUTOPILOT1,
                                              QString::fromLatin1(OIFleetScriptManager::kVehicleScriptsDir))) {
        _retryLater(tr("Vehicle busy with another file transfer"), LevelBusy);
    }
}

void OIFleetVehicleSync::_onListDirectoryComplete(const QStringList &entries, const QString &errorMsg)
{
    if (_phase != Phase::Listing) {
        return;     // somebody else's listing on the shared manager
    }
    _phase = Phase::Idle;

    if (!errorMsg.isEmpty()) {
        // AP_Scripting::init() creates this directory itself whenever SCR_ENABLE is 1,
        // and we only get here with SCR_ENABLE non-zero. So this is not "nobody has put
        // a script here yet" - it is no SD card, or one that cannot be read.
        qCDebug(OIFleetScriptsLog) << "listing failed" << errorMsg;
        _setStatus(tr("No scripts folder on vehicle"), LevelError);
        return;
    }

    // ArduPilot emits "F<name>\t<size>", and "F<name>\t<size>\t<mtime>" for the
    // with-time variant. The size is why a probe is cheap: it rules most files in or
    // out before a single checksum is spent.
    for (const QString &entry : entries) {
        if (entry.size() < 2 || entry.at(0) != QLatin1Char('F')) {
            continue;
        }
        const QStringList fields = entry.mid(1).split(QLatin1Char('\t'));
        const QString name = fields.value(0);
        if (name.isEmpty()) {
            continue;
        }
        bool ok = false;
        const qint64 size = fields.value(1).toLongLong(&ok);
        _remoteSizes.insert(name, ok ? size : -1);
    }

    _collectUnmanaged();

    const QList<OIFleetScript *> scripts = _manager->enabledScripts();
    for (OIFleetScript *script : scripts) {
        if (!script->valid()) {
            continue;   // local copy is gone; the row says so, nothing to upload
        }
        const auto it = _remoteSizes.constFind(script->fileName());
        if (it == _remoteSizes.constEnd() || *it != script->sizeBytes()) {
            _uploadQueue.append(script->fileName());
        } else {
            _checksumQueue.append(script->fileName());
        }
    }

    _nextChecksum();
}

void OIFleetVehicleSync::_nextChecksum()
{
    if (_checksumQueue.isEmpty()) {
        _finishProbe();
        return;
    }

    if (!_ftpAvailable()) {
        _retryLater(tr("Vehicle busy with another file transfer"), LevelBusy);
        return;
    }

    _phase = Phase::Checksumming;
    const QString path = QString::fromLatin1(OIFleetScriptManager::kVehicleScriptsDir) + _checksumQueue.first();
    if (!_vehicle->ftpManager()->calcFileCRC32(MAV_COMP_ID_AUTOPILOT1, path)) {
        _retryLater(tr("Vehicle busy with another file transfer"), LevelBusy);
    }
}

void OIFleetVehicleSync::_onCalcFileCRC32Complete(const QString &file, quint32 crc32, const QString &errorMsg)
{
    Q_UNUSED(file)

    if (_phase != Phase::Checksumming) {
        return;
    }
    _phase = Phase::Idle;

    const QString name = _checksumQueue.takeFirst();

    if (errorMsg.isEmpty()) {
        _remoteCrcs.insert(name, crc32);
    } else {
        // Firmware that does not answer CalcFileCRC32 would otherwise look like a file
        // that never matches, and we would upload it on every single pass forever. The
        // size already agreed, so fall back to trusting it.
        qCDebug(OIFleetScriptsLog) << "checksum unavailable for" << name << errorMsg << "- trusting size";
    }

    _nextChecksum();
}

void OIFleetVehicleSync::_finishProbe()
{
    const QList<OIFleetScript *> scripts = _manager->enabledScripts();
    QHash<QString, quint32> localCrcs;
    for (const OIFleetScript *script : scripts) {
        localCrcs.insert(script->fileName(), script->crc32());
    }

    for (auto it = _remoteCrcs.constBegin(); it != _remoteCrcs.constEnd(); ++it) {
        const auto local = localCrcs.constFind(it.key());
        if (local != localCrcs.constEnd() && *local != it.value()) {
            _uploadQueue.append(it.key());
        }
    }

    if (_uploadQueue.isEmpty()) {
        if (_uploadedThisPass > 0) {
            _restartScripting();
        } else {
            _phase = Phase::Done;
            _setStatus(tr("All scripts present"), LevelGood);
        }
        return;
    }

    if (_passCount >= kMaxPasses) {
        // We uploaded, re-read, and the aircraft still does not hold what we sent.
        // Repeating would only write the same bytes at the same card again.
        _phase = Phase::Done;
        _setStatus(tr("Upload did not verify - check the vehicle's SD card"), LevelError);
        return;
    }

    if (!_announcedUploading) {
        OIFleetScriptManager::sayToVehicle(_vehicle, tr("Configured scripts missing, uploading..."));
        _announcedUploading = true;
    }

    _uploadIndex = 0;
    _uploadTotal = _uploadQueue.count();
    _nextUpload();
}

void OIFleetVehicleSync::_nextUpload()
{
    if (_uploadQueue.isEmpty()) {
        _finishUploads();
        return;
    }

    // Arming does not stop ArduPilot answering FTP - file_op_allowed() lets it through
    // because FTP runs off the main thread - so this is our policy, not the firmware's.
    // We let the file in flight finish (a truncated .lua is loaded on next boot and
    // fails in a way nothing distinguishes from a good one by name) and stop there.
    if (_vehicle->armed()) {
        _phase = Phase::Done;
        _setStatus(tr("Vehicle is armed - upload paused"), LevelCaution);
        return;
    }

    if (!_ftpAvailable()) {
        _retryLater(tr("Vehicle busy with another file transfer"), LevelBusy);
        return;
    }

    _currentUploadName = _uploadQueue.first();
    OIFleetScript *script = nullptr;
    const QList<OIFleetScript *> scripts = _manager->enabledScripts();
    for (OIFleetScript *candidate : scripts) {
        if (candidate->fileName() == _currentUploadName) {
            script = candidate;
            break;
        }
    }
    if (!script || !script->valid()) {
        _uploadQueue.removeFirst();
        _nextUpload();
        return;
    }

    _phase = Phase::Uploading;
    _uploadProgress = 0.0F;
    _uploadIndex++;
    _setStatus(tr("Uploading %1 - 0%  (%2 of %3)").arg(_currentUploadName).arg(_uploadIndex).arg(_uploadTotal),
               LevelBusy);

    const QString path = QString::fromLatin1(OIFleetScriptManager::kVehicleScriptsDir) + _currentUploadName;
    if (!_vehicle->ftpManager()->upload(MAV_COMP_ID_AUTOPILOT1, path, script->filePath())) {
        _uploadIndex--;
        _retryLater(tr("Vehicle busy with another file transfer"), LevelBusy);
    }
}

void OIFleetVehicleSync::_onCommandProgress(float value)
{
    if (_phase != Phase::Uploading) {
        return;
    }
    _uploadProgress = value;
    _setStatus(tr("Uploading %1 - %2%  (%3 of %4)")
                   .arg(_currentUploadName)
                   .arg(qRound(value * 100.0F))
                   .arg(_uploadIndex)
                   .arg(_uploadTotal),
               LevelBusy);
}

void OIFleetVehicleSync::_onUploadComplete(const QString &file, const QString &errorMsg)
{
    Q_UNUSED(file)

    if (_phase != Phase::Uploading) {
        return;
    }
    _phase = Phase::Idle;

    if (!errorMsg.isEmpty()) {
        qCDebug(OIFleetScriptsLog) << "upload failed" << _currentUploadName << errorMsg;
        _phase = Phase::Done;
        _setStatus(tr("Upload failed for %1: %2").arg(_currentUploadName, errorMsg), LevelError);
        return;
    }

    _uploadedThisPass++;
    _uploadQueue.removeFirst();
    _nextUpload();
}

void OIFleetVehicleSync::_finishUploads()
{
    // Re-read what the aircraft now holds rather than assuming the writes landed. This
    // is what makes a truncated file - an upload cut short by a link drop - heal itself
    // instead of sitting there looking correct by name.
    _passCount++;
    _beginPass();
}

void OIFleetVehicleSync::_restartScripting()
{
    _phase = Phase::Done;

    // We only reach this with SCR_ENABLE non-zero, and the scripts directory present -
    // which AP_Scripting::init() creates, and init() returns early when scripting is
    // off. So the Lua thread is running and will pick the new files up.
    _vehicle->sendMavCommand(MAV_COMP_ID_AUTOPILOT1,
                             static_cast<MAV_CMD>(MAV_CMD_SCRIPTING),
                             false /* showError */,
                             kScriptingCmdStopAndRestart);

    OIFleetScriptManager::sayToVehicle(_vehicle, tr("Scripts uploaded. Changes applied."));
    _setStatus(tr("All scripts present"), LevelGood);
}

void OIFleetVehicleSync::_collectUnmanaged()
{
    QStringList configured;
    const QList<OIFleetScript *> scripts = _manager->enabledScripts();
    for (const OIFleetScript *script : scripts) {
        configured.append(script->fileName());
    }

    _unmanagedScripts.clear();
    for (auto it = _remoteSizes.constBegin(); it != _remoteSizes.constEnd(); ++it) {
        if (!it.key().endsWith(QLatin1String(".lua"), Qt::CaseInsensitive)) {
            continue;
        }
        if (!configured.contains(it.key())) {
            _unmanagedScripts.append(it.key());
        }
    }
    _unmanagedScripts.sort();
}

void OIFleetVehicleSync::_onArmedChanged(bool armed)
{
    if (armed) {
        // Nothing to interrupt: an upload already in flight is left to finish, and
        // _nextUpload() will not start another.
        return;
    }
    requestProbe();
}

void OIFleetVehicleSync::_onParametersReadyChanged(bool ready)
{
    if (ready) {
        requestProbe();
    }
}

/*===========================================================================*/

OIFleetScriptManager::OIFleetScriptManager(QObject *parent)
    : QObject(parent)
    , _scripts(new QmlObjectListModel(this))
{
    const QString root = SettingsManager::instance()->appSettings()->savePath()->rawValue().toString();
    if (!root.isEmpty()) {
        _scriptsFolder = QGCFileHelper::joinPath(root, QString::fromLatin1(kFleetScriptsDir));
        QGCFileHelper::ensureDirectoryExists(_scriptsFolder);
    }

    _load();

    MultiVehicleManager *const mvm = MultiVehicleManager::instance();
    connect(mvm, &MultiVehicleManager::vehicleAdded, this, &OIFleetScriptManager::_onVehicleAdded);
    connect(mvm, &MultiVehicleManager::vehicleRemoved, this, &OIFleetScriptManager::_onVehicleRemoved);
    connect(mvm, &MultiVehicleManager::activeVehicleChanged, this, &OIFleetScriptManager::_onActiveVehicleChanged);
}

QString OIFleetScriptManager::activeVehicleName() const
{
    if (!_activeVehicle) {
        return QString();
    }
    return tr("Vehicle %1").arg(_activeVehicle->id());
}

int OIFleetScriptManager::enabledCount() const
{
    return enabledScripts().count();
}

QList<OIFleetScript *> OIFleetScriptManager::enabledScripts() const
{
    QList<OIFleetScript *> result;
    for (int i = 0; i < _scripts->count(); i++) {
        OIFleetScript *const script = qobject_cast<OIFleetScript *>((*_scripts)[i]);
        if (script && script->enabled()) {
            result.append(script);
        }
    }
    return result;
}

void OIFleetScriptManager::_load()
{
    if (_scriptsFolder.isEmpty()) {
        return;
    }

    // The folder is the set. QSettings only remembers which entries are ticked, so a
    // .lua dropped in by hand shows up, and an entry whose file was deleted outside the
    // app does not linger as a row that can never be satisfied.
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(kSettingsGroup));

    QDir dir(_scriptsFolder);
    const QStringList files = dir.entryList(QStringList() << QStringLiteral("*.lua"), QDir::Files, QDir::Name);
    for (const QString &fileName : files) {
        const bool enabled = settings.value(fileName, true).toBool();
        OIFleetScript *const script = new OIFleetScript(dir.filePath(fileName), enabled, this);
        _connectScript(script);
        _scripts->append(script);
    }

    settings.endGroup();
}

void OIFleetScriptManager::_saveEnabledFlags() const
{
    QSettings settings;
    settings.remove(QString::fromLatin1(kSettingsGroup));
    settings.beginGroup(QString::fromLatin1(kSettingsGroup));
    for (int i = 0; i < _scripts->count(); i++) {
        const OIFleetScript *const script = qobject_cast<OIFleetScript *>((*_scripts)[i]);
        if (script) {
            settings.setValue(script->fileName(), script->enabled());
        }
    }
    settings.endGroup();
}

void OIFleetScriptManager::_connectScript(OIFleetScript *script)
{
    connect(script, &OIFleetScript::enabledChanged, this, [this]() {
        _saveEnabledFlags();
        emit scriptsChanged();
        _notifySyncsOfSetChange();
    });
}

void OIFleetScriptManager::_notifySyncsOfSetChange()
{
    for (OIFleetVehicleSync *sync : std::as_const(_syncs)) {
        sync->requestProbe();
    }
}

bool OIFleetScriptManager::importScript(const QString &fileUrlOrPath)
{
    _lastError.clear();

    if (_scriptsFolder.isEmpty()) {
        _lastError = tr("No application save folder is configured.");
        return false;
    }

    const QUrl url(fileUrlOrPath);
    const QString sourcePath = url.isLocalFile() ? url.toLocalFile() : fileUrlOrPath;

    const QFileInfo info(sourcePath);
    if (!info.exists() || !info.isFile()) {
        _lastError = tr("%1 cannot be read.").arg(sourcePath);
        return false;
    }
    if (info.suffix().compare(QLatin1String(kScriptSuffix), Qt::CaseInsensitive) != 0) {
        _lastError = tr("%1 is not a .lua script.").arg(info.fileName());
        return false;
    }

    // The aircraft's scripts directory is flat, so the base name is the identity. Two
    // configured scripts sharing one would each overwrite the other on every pass.
    for (int i = 0; i < _scripts->count(); i++) {
        const OIFleetScript *const existing = qobject_cast<OIFleetScript *>((*_scripts)[i]);
        if (existing && existing->fileName().compare(info.fileName(), Qt::CaseInsensitive) == 0) {
            _lastError = tr("%1 is already in the list.").arg(info.fileName());
            return false;
        }
    }

    const QString destination = QDir(_scriptsFolder).filePath(info.fileName());
    if (QFile::exists(destination) && !QFile::remove(destination)) {
        _lastError = tr("Could not replace %1.").arg(destination);
        return false;
    }
    if (!QFile::copy(sourcePath, destination)) {
        _lastError = tr("Could not copy %1 into the fleet scripts folder.").arg(info.fileName());
        return false;
    }

    OIFleetScript *const script = new OIFleetScript(destination, true, this);
    _connectScript(script);
    _scripts->append(script);

    _saveEnabledFlags();
    emit scriptsChanged();
    _notifySyncsOfSetChange();
    return true;
}

void OIFleetScriptManager::removeScript(int index)
{
    if (index < 0 || index >= _scripts->count()) {
        return;
    }

    OIFleetScript *const script = qobject_cast<OIFleetScript *>((*_scripts)[index]);
    if (!script) {
        return;
    }

    // Only ever the GCS copy. Nothing on any aircraft is touched - see the file header.
    const QString path = script->filePath();
    _scripts->removeAt(index);
    script->deleteLater();
    (void) QFile::remove(path);

    _saveEnabledFlags();
    emit scriptsChanged();
    _notifySyncsOfSetChange();
}

void OIFleetScriptManager::setScriptEnabled(int index, bool enabled)
{
    if (index < 0 || index >= _scripts->count()) {
        return;
    }
    OIFleetScript *const script = qobject_cast<OIFleetScript *>((*_scripts)[index]);
    if (script) {
        script->setEnabled(enabled);
    }
}

void OIFleetScriptManager::refreshAll()
{
    for (int i = 0; i < _scripts->count(); i++) {
        OIFleetScript *const script = qobject_cast<OIFleetScript *>((*_scripts)[i]);
        if (script) {
            script->refresh();
        }
    }
    emit scriptsChanged();
    _notifySyncsOfSetChange();
}

void OIFleetScriptManager::sayToVehicle(Vehicle *vehicle, const QString &text)
{
    if (!vehicle || !vehicle->statusTextHandler()) {
        return;
    }
    vehicle->statusTextHandler()->handleHTMLEscapedTextMessage(
        static_cast<MAV_COMPONENT>(MAV_COMP_ID_AUTOPILOT1),
        MAV_SEVERITY_NOTICE,
        text.toHtmlEscaped(),
        QString());
}

void OIFleetScriptManager::_onVehicleAdded(Vehicle *vehicle)
{
    if (!vehicle || _syncs.contains(vehicle)) {
        return;
    }
    _syncs.insert(vehicle, new OIFleetVehicleSync(vehicle, this, this));
}

void OIFleetScriptManager::_onVehicleRemoved(Vehicle *vehicle)
{
    OIFleetVehicleSync *const sync = _syncs.take(vehicle);
    if (!sync) {
        return;
    }
    if (sync == _activeSync) {
        _activeSync = nullptr;
        emit activeSyncChanged();
    }
    sync->deleteLater();
}

void OIFleetScriptManager::_onActiveVehicleChanged(Vehicle *vehicle)
{
    _activeVehicle = vehicle;
    _activeSync = vehicle ? _syncs.value(vehicle, nullptr) : nullptr;
    emit activeSyncChanged();
}
