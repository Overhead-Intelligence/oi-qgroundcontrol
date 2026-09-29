#include "FlightStatsResume.h"

#include <QtCore/QDateTime>
#include <QtCore/QSettings>

#include "Fact.h"
#include "ParameterManager.h"
#include "QGCLoggingCategory.h"
#include "Vehicle.h"

QGC_LOGGING_CATEGORY(FlightStatsResumeLog, "Vehicle.FlightStatsResume")

namespace {

/// AP_Stats writes its counters to their parameters every `flush_interval_ms`, which is
/// 30 s. Snapshotting faster would not buy accuracy - it would re-read the same values.
constexpr int kSnapshotIntervalMs = 30000;

const char *kBootCount = "STAT_BOOTCNT";
const char *kFlightCount = "STAT_FLTCNT";
const char *kFlightTime = "STAT_FLTTIME";
const char *kDistanceFlown = "STAT_DISTFLWN";

const char *kSettingsGroup = "FlightStatsResume";

/// A record older than this is not worth resuming into, whatever the counters say. The
/// counters are the real guard - the vehicle has to have neither rebooted nor flown again -
/// so this only has to be loose enough never to cut a real flight short. No flight is
/// twelve hours long.
constexpr qint64 kMaxRecordAgeSecs = 12 * 60 * 60;

} // namespace

const QStringList &FlightStatsResume::statParameterNames()
{
    static const QStringList names {
        QLatin1String(kBootCount),
        QLatin1String(kFlightCount),
        QLatin1String(kFlightTime),
        QLatin1String(kDistanceFlown),
    };
    return names;
}

FlightStatsResume::FlightStatsResume(QObject *parent)
    : QObject(parent)
{
    _loadPersisted();

    _snapshotTimer.setInterval(kSnapshotIntervalMs);
    (void) connect(&_snapshotTimer, &QTimer::timeout, this, &FlightStatsResume::_snapshotDue);
    _snapshotTimer.start();
}

void FlightStatsResume::_loadPersisted()
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));

    const qint64 now = QDateTime::currentSecsSinceEpoch();

    for (const QString &key : settings.childGroups()) {
        bool ok = false;
        const int vehicleId = key.toInt(&ok);
        if (!ok) {
            continue;
        }

        settings.beginGroup(key);
        Snapshot snapshot;
        snapshot.bootCount = settings.value(QStringLiteral("bootCount"), -1).toInt();
        snapshot.flightCount = settings.value(QStringLiteral("flightCount"), -1).toInt();
        snapshot.vehicleTimeSecs = settings.value(QStringLiteral("vehicleTimeSecs"), 0).toDouble();
        snapshot.vehicleDistanceM = settings.value(QStringLiteral("vehicleDistanceM"), 0).toDouble();
        snapshot.gcsTimeSecs = settings.value(QStringLiteral("gcsTimeSecs"), 0).toDouble();
        snapshot.gcsDistanceM = settings.value(QStringLiteral("gcsDistanceM"), 0).toDouble();
        snapshot.savedAtEpoch = settings.value(QStringLiteral("savedAtEpoch"), 0).toLongLong();
        settings.endGroup();

        // A clock that has gone backwards since the write gives a negative age; treat
        // anything outside the window as unusable rather than trusting the arithmetic.
        const qint64 age = now - snapshot.savedAtEpoch;
        if ((snapshot.bootCount < 0) || (age < 0) || (age > kMaxRecordAgeSecs)) {
            settings.remove(key);
            continue;
        }

        qCDebug(FlightStatsResumeLog) << "loaded persisted flight stats for vehicle" << vehicleId
                                      << "age" << age << "s";
        _snapshots.insert(vehicleId, snapshot);
    }
}

void FlightStatsResume::_persist(int vehicleId, const Snapshot &snapshot)
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    settings.beginGroup(QString::number(vehicleId));
    settings.setValue(QStringLiteral("bootCount"), snapshot.bootCount);
    settings.setValue(QStringLiteral("flightCount"), snapshot.flightCount);
    settings.setValue(QStringLiteral("vehicleTimeSecs"), snapshot.vehicleTimeSecs);
    settings.setValue(QStringLiteral("vehicleDistanceM"), snapshot.vehicleDistanceM);
    settings.setValue(QStringLiteral("gcsTimeSecs"), snapshot.gcsTimeSecs);
    settings.setValue(QStringLiteral("gcsDistanceM"), snapshot.gcsDistanceM);
    settings.setValue(QStringLiteral("savedAtEpoch"), snapshot.savedAtEpoch);
    settings.endGroup();
    settings.endGroup();

    // Written through rather than left to Qt's own scheduling: the case this exists for is
    // the process not getting to exit.
    settings.sync();
}

void FlightStatsResume::_dropPersisted(int vehicleId)
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    settings.remove(QString::number(vehicleId));
    settings.endGroup();
    settings.sync();
}

void FlightStatsResume::vehicleAdded(Vehicle *vehicle)
{
    if (!vehicle) {
        return;
    }

    const int vehicleId = vehicle->id();

    Tracked tracked;
    tracked.vehicle = vehicle;
    _tracked.insert(vehicleId, tracked);

    // A finished flight must not be resumed into the next one. Disarming is the only
    // moment we can be sure of that, because STAT_FLTCNT does not move until the *next*
    // takeoff - so between a re-arm and leaving the ground it would still match.
    (void) connect(vehicle, &Vehicle::armedChanged, this, [this, vehicleId](bool armed) {
        if (!armed) {
            _forget(vehicleId);
        }
    });

    ParameterManager *const params = vehicle->parameterManager();
    if (!params) {
        return;
    }

    // The counters have to be read from the vehicle, not taken from whatever the
    // parameter cache holds: on a reconnect the cache is what we saw before the gap,
    // which is exactly the value the resume is trying to measure against.
    (void) connect(params, &ParameterManager::_paramRequestReadSuccess, vehicle,
                   [this, vehicle](int, const QString &paramName, int) {
                       _statRead(vehicle, paramName);
                   });

    if (params->parametersReady()) {
        _requestStats(vehicle);
    } else {
        (void) connect(params, &ParameterManager::parametersReadyChanged, vehicle,
                       [this, vehicle](bool ready) {
                           if (ready) {
                               _requestStats(vehicle);
                           }
                       });
    }
}

void FlightStatsResume::_requestStats(Vehicle *vehicle)
{
    ParameterManager *const params = vehicle ? vehicle->parameterManager() : nullptr;
    if (!params) {
        return;
    }

    const int compId = vehicle->defaultComponentId();
    if (!params->parameterExists(compId, QLatin1String(kFlightTime))) {
        // No AP_Stats on this firmware. Nothing to resume from, and nothing to do.
        return;
    }

    auto tracked = _tracked.find(vehicle->id());
    if (tracked == _tracked.end()) {
        return;
    }

    // Only ask for what this firmware actually has - STAT_DISTFLWN is newer than the
    // rest - otherwise bulkRefresh logs a warning for the missing name every cycle.
    QStringList present;
    tracked->awaiting.clear();
    for (const QString &name : statParameterNames()) {
        if (params->parameterExists(compId, name)) {
            present.append(name);
            (void) tracked->awaiting.insert(name);
        }
    }

    params->bulkRefresh(compId, present, false /* notifyFailure */);
}

void FlightStatsResume::_statRead(Vehicle *vehicle, const QString &paramName)
{
    auto tracked = _tracked.find(vehicle->id());
    if ((tracked == _tracked.end()) || tracked->awaiting.isEmpty()) {
        return;
    }
    if (!tracked->awaiting.remove(paramName)) {
        return;
    }
    if (tracked->awaiting.isEmpty()) {
        _statsArrived(vehicle);
    }
}

void FlightStatsResume::_statsArrived(Vehicle *vehicle)
{
    Snapshot vehicleStats;
    if (!_readStats(vehicle, vehicleStats)) {
        return;
    }

    auto tracked = _tracked.find(vehicle->id());
    if (tracked == _tracked.end()) {
        return;
    }

    if (!tracked->resumeSettled) {
        tracked->resumeSettled = true;

        const auto previous = _snapshots.constFind(vehicle->id());
        const bool sameFlight = (previous != _snapshots.constEnd()) &&
                                (previous->bootCount == vehicleStats.bootCount) &&
                                (previous->flightCount == vehicleStats.flightCount);

        if (vehicle->armed() && sameFlight) {
            const double resumedTime = previous->gcsTimeSecs +
                                       (vehicleStats.vehicleTimeSecs - previous->vehicleTimeSecs);
            const double resumedDistance = previous->gcsDistanceM +
                                           (vehicleStats.vehicleDistanceM - previous->vehicleDistanceM);

            qCDebug(FlightStatsResumeLog)
                    << "resuming flight stats for vehicle" << vehicle->id()
                    << "time" << resumedTime << "distance" << resumedDistance
                    << "(gap of" << (vehicleStats.vehicleTimeSecs - previous->vehicleTimeSecs) << "s)";

            vehicle->resumeFlightStats(resumedTime, resumedDistance);
        } else if (previous != _snapshots.constEnd()) {
            qCDebug(FlightStatsResumeLog)
                    << "not resuming vehicle" << vehicle->id()
                    << "- armed" << vehicle->armed()
                    << "boot" << previous->bootCount << "->" << vehicleStats.bootCount
                    << "flight" << previous->flightCount << "->" << vehicleStats.flightCount;
        }
    }

    _takeSnapshot(vehicle, vehicleStats);
}

bool FlightStatsResume::_readStats(Vehicle *vehicle, Snapshot &out) const
{
    ParameterManager *const params = vehicle ? vehicle->parameterManager() : nullptr;
    if (!params) {
        return false;
    }

    const int compId = vehicle->defaultComponentId();
    for (const char *name : {kBootCount, kFlightCount, kFlightTime}) {
        if (!params->parameterExists(compId, QLatin1String(name))) {
            return false;
        }
    }

    out.bootCount = params->getParameter(compId, QLatin1String(kBootCount))->rawValue().toInt();
    out.flightCount = params->getParameter(compId, QLatin1String(kFlightCount))->rawValue().toInt();
    out.vehicleTimeSecs = params->getParameter(compId, QLatin1String(kFlightTime))->rawValue().toDouble();

    // Older firmware has no distance counter. Time still resumes; distance simply keeps
    // the ground-side behaviour rather than blocking the whole feature.
    out.vehicleDistanceM = params->parameterExists(compId, QLatin1String(kDistanceFlown))
                               ? params->getParameter(compId, QLatin1String(kDistanceFlown))->rawValue().toDouble()
                               : 0.0;

    return true;
}

void FlightStatsResume::_takeSnapshot(Vehicle *vehicle, const Snapshot &vehicleStats)
{
    if (!vehicle->armed()) {
        return;
    }

    Snapshot snapshot = vehicleStats;
    // Captured here, with the values that have just come back, so the pair describes one
    // instant. Reading our own side later - at comms loss, say - would fold the blackout
    // into both halves of the sum.
    snapshot.gcsTimeSecs = vehicle->flightTimeSecs();
    snapshot.gcsDistanceM = vehicle->flightDistanceM();
    snapshot.savedAtEpoch = QDateTime::currentSecsSinceEpoch();

    _snapshots.insert(vehicle->id(), snapshot);
    _persist(vehicle->id(), snapshot);
}

void FlightStatsResume::_snapshotDue()
{
    for (auto it = _tracked.begin(); it != _tracked.end(); ) {
        Vehicle *const vehicle = it->vehicle;
        if (!vehicle) {
            // The Vehicle is gone; its snapshot is deliberately kept, since resuming into
            // its replacement is the whole point.
            it = _tracked.erase(it);
            continue;
        }
        if (vehicle->armed()) {
            _requestStats(vehicle);
        }
        ++it;
    }
}

void FlightStatsResume::_forget(int vehicleId)
{
    (void) _snapshots.remove(vehicleId);
    _dropPersisted(vehicleId);
}
