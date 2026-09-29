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
/// 30 s. Polling faster would re-read the same values. Polling continues on the ground,
/// because the reading taken there is what becomes the next flight's baseline.
constexpr int kPollIntervalMs = 30000;

const char *kBootCount = "STAT_BOOTCNT";
const char *kFlightCount = "STAT_FLTCNT";
const char *kFlightTime = "STAT_FLTTIME";
const char *kDistanceFlown = "STAT_DISTFLWN";

const char *kSettingsGroup = "FlightStatsResume";

/// A baseline older than this is not worth resuming into, whatever the counters say. The
/// counters are the real guard - the aircraft has to have neither rebooted nor flown again -
/// so this only has to be loose enough never to cut a real flight short.
constexpr qint64 kMaxBaselineAgeSecs = 12 * 60 * 60;

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

    _pollTimer.setInterval(kPollIntervalMs);
    (void) connect(&_pollTimer, &QTimer::timeout, this, &FlightStatsResume::_pollDue);
    _pollTimer.start();
}

void FlightStatsResume::vehicleAdded(Vehicle *vehicle)
{
    if (!vehicle) {
        return;
    }

    Tracked tracked;
    tracked.vehicle = vehicle;
    _tracked.insert(vehicle->id(), tracked);

    ParameterManager *const params = vehicle->parameterManager();
    if (!params) {
        return;
    }

    // The counters must be read from the aircraft, not taken from the parameter cache: on a
    // reconnect that cache holds the values from before the gap, which is exactly what the
    // resume is measuring against.
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
        // No AP_Stats on this firmware. Nothing to derive anything from.
        return;
    }

    auto tracked = _tracked.find(vehicle->id());
    if (tracked == _tracked.end()) {
        return;
    }

    // Only ask for what this firmware actually has - STAT_DISTFLWN is newer than the rest -
    // otherwise bulkRefresh logs a warning for the missing name every cycle.
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
    Reading now;
    if (!_readStats(vehicle, now)) {
        return;
    }

    auto tracked = _tracked.find(vehicle->id());
    if (tracked == _tracked.end()) {
        return;
    }

    const int vehicleId = vehicle->id();
    Baseline baseline = _baselines.value(vehicleId);

    const bool matches = baseline.valid &&
                         (baseline.bootCount == now.bootCount) &&
                         (baseline.flightCount == now.flightCount);
    bool committed = false;

    if (!matches) {
        // No baseline for the flight the aircraft says it is on. Either we watched it start,
        // or we arrived after it did.
        const Reading &previous = tracked->lastSeen;
        const bool sawTakeoff = previous.valid &&
                                (previous.bootCount == now.bootCount) &&
                                (previous.flightCount != now.flightCount);

        baseline = Baseline();
        baseline.valid = true;
        baseline.bootCount = now.bootCount;
        baseline.flightCount = now.flightCount;
        baseline.savedAtEpoch = QDateTime::currentSecsSinceEpoch();

        if (sawTakeoff) {
            // The previous reading was taken while the counter still showed the old flight,
            // so the aircraft was on the ground and STAT_FLTTIME was frozen. That makes it an
            // exact baseline rather than an approximation.
            //
            // It is only wrong if the aircraft landed and took off again between two polls,
            // in which case the previous reading was airborne and this under-reports. Thirty
            // seconds is not long enough to do that in.
            baseline.flightTimeSecs = previous.flightTimeSecs;
            baseline.distanceM = previous.distanceM;
        } else {
            // We arrived without seeing the transition - a spectator joining a flight in
            // progress, or a GCS opened after takeoff. Nothing in the aircraft records when
            // the flight began, so the honest answer is to count from here, which is what
            // QGC has always done.
            baseline.flightTimeSecs = now.flightTimeSecs;
            baseline.distanceM = now.distanceM;
        }

        _baselines.insert(vehicleId, baseline);
        _persist(vehicleId, baseline);
        committed = true;

        qCDebug(FlightStatsResumeLog)
                << "baseline committed for vehicle" << vehicleId
                << "boot" << baseline.bootCount << "flight" << baseline.flightCount
                << "time" << baseline.flightTimeSecs << "distance" << baseline.distanceM
                << (sawTakeoff ? "(saw takeoff)" : "(joined mid-flight)");
    }

    const double elapsed = now.flightTimeSecs - baseline.flightTimeSecs;
    const double distance = now.distanceM - baseline.distanceM;

    qCDebug(FlightStatsResumeLog)
            << "vehicle" << vehicleId << "flight" << now.flightCount
            << "elapsed" << elapsed << "s distance" << distance << "m"
            << (committed ? "(new baseline)" : "(from stored baseline)");

    // A freshly committed baseline is applied outright, because it marks a different flight
    // from whatever was on screen. Otherwise the reading only corrects forward: the local
    // timer is smoother between polls, and the counters lag by up to one flush interval, so
    // taking the larger of the two converges without ever running backwards.
    vehicle->syncFlightStats(qMax(0.0, elapsed), qMax(0.0, distance), committed);

    // Kept last, because it is the candidate baseline for the *next* flight: while the
    // aircraft is on the ground STAT_FLTTIME does not move, so whatever this reading holds
    // is what the next takeoff should be measured from.
    tracked->lastSeen = now;
}

bool FlightStatsResume::_readStats(Vehicle *vehicle, Reading &out) const
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
    out.flightTimeSecs = params->getParameter(compId, QLatin1String(kFlightTime))->rawValue().toDouble();

    // Older firmware has no distance counter. Time still resumes; distance simply keeps the
    // ground-side behaviour rather than blocking the whole feature.
    out.distanceM = params->parameterExists(compId, QLatin1String(kDistanceFlown))
                        ? params->getParameter(compId, QLatin1String(kDistanceFlown))->rawValue().toDouble()
                        : 0.0;

    out.valid = true;
    return true;
}

void FlightStatsResume::_pollDue()
{
    for (auto it = _tracked.begin(); it != _tracked.end(); ) {
        Vehicle *const vehicle = it->vehicle;
        if (!vehicle) {
            // The Vehicle is gone; its baseline is deliberately kept, since resuming into its
            // replacement is the whole point.
            it = _tracked.erase(it);
            continue;
        }
        _requestStats(vehicle);
        ++it;
    }
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
        Baseline baseline;
        baseline.bootCount = settings.value(QStringLiteral("bootCount"), -1).toInt();
        baseline.flightCount = settings.value(QStringLiteral("flightCount"), -1).toInt();
        baseline.flightTimeSecs = settings.value(QStringLiteral("flightTimeSecs"), 0).toDouble();
        baseline.distanceM = settings.value(QStringLiteral("distanceM"), 0).toDouble();
        baseline.savedAtEpoch = settings.value(QStringLiteral("savedAtEpoch"), 0).toLongLong();
        settings.endGroup();

        // A clock that has gone backwards since the write gives a negative age; treat anything
        // outside the window as unusable rather than trusting the arithmetic.
        const qint64 age = now - baseline.savedAtEpoch;
        if ((baseline.bootCount < 0) || (age < 0) || (age > kMaxBaselineAgeSecs)) {
            settings.remove(key);
            continue;
        }

        baseline.valid = true;
        qCDebug(FlightStatsResumeLog) << "loaded baseline for vehicle" << vehicleId
                                      << "boot" << baseline.bootCount
                                      << "flight" << baseline.flightCount
                                      << "age" << age << "s";
        _baselines.insert(vehicleId, baseline);
    }
}

void FlightStatsResume::_persist(int vehicleId, const Baseline &baseline)
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    settings.beginGroup(QString::number(vehicleId));
    settings.setValue(QStringLiteral("bootCount"), baseline.bootCount);
    settings.setValue(QStringLiteral("flightCount"), baseline.flightCount);
    settings.setValue(QStringLiteral("flightTimeSecs"), baseline.flightTimeSecs);
    settings.setValue(QStringLiteral("distanceM"), baseline.distanceM);
    settings.setValue(QStringLiteral("savedAtEpoch"), baseline.savedAtEpoch);
    settings.endGroup();
    settings.endGroup();

    // Written through rather than left to Qt's own scheduling: one case this exists for is
    // the process not getting to exit.
    settings.sync();
}
