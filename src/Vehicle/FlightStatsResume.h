#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtCore/QTimer>

class Vehicle;

/// Keeps flight time and distance right across a reconnect, by deriving them from the
/// aircraft's own counters instead of a ground-side stopwatch.
///
/// QGC tracks both entirely on the ground side. `Vehicle::_flightTimerStart()` zeroes them
/// on the disarmed->armed transition and a `QElapsedTimer` runs from there. That works until
/// the link is torn down - a serial port disappearing, a TCP socket closing, an operator
/// reconnecting - because `VehicleLinkManager` then signals `allLinksRemoved`,
/// `MultiVehicleManager` deletes the Vehicle, and its replacement starts from zero while the
/// aircraft has not stopped flying. (A plain telemetry gap does not do this: `_autoDisconnect`
/// is false by default, so missed heartbeats only set `communicationLost` and the Vehicle
/// survives with its timer running. It is the teardown that resets it.)
///
/// ArduPilot's `AP_Stats` already counts both, as persisted parameters - no new protocol:
///
///   `STAT_FLTTIME`   lifetime airborne seconds
///   `STAT_DISTFLWN`  lifetime distance, metres
///   `STAT_FLTCNT`    +1 per flight
///   `STAT_BOOTCNT`   +1 per boot
///
/// Those totals are for the life of the airframe, so displaying one is useless. What is
/// needed is its value when *this* flight began, and two measured facts give it exactly:
///
///  - `STAT_FLTCNT` increments at **takeoff** (14.4 s and 15.9 s after arming, across two
///    SITL runs - the instant the aircraft left the ground). That is the flight boundary.
///  - `STAT_FLTTIME` is **frozen while not flying** (armed on the ground for 238 s, it stayed
///    at 0.0). So *any* reading taken before takeoff is an exact baseline. There is no need
///    to catch the moment itself.
///
/// So: poll the counters, keep the latest reading, and the first time `STAT_FLTCNT` is seen
/// to have moved, commit the **previous** reading as this flight's baseline and persist it.
/// Elapsed time is `STAT_FLTTIME - baseline` from then on, for any connection.
///
/// The baseline is **written once per flight and never rewritten**. That is the point of the
/// design rather than an implementation detail: a half-connected GCS - parameters failing to
/// download, a link too poor to hold - can read a baseline and display from it, but has no
/// path to replace it. An earlier version kept a rolling pair of "what we displayed" and
/// "what the vehicle read", which a failed connection could overwrite with its own near-zero
/// ground-side value; the next reconnect then resumed from the failed attempt instead of from
/// the takeoff. Nothing here can do that.
///
/// Baselines are persisted as they are committed, so a QGC that crashes mid-flight recovers
/// too - and because the write happens at takeoff rather than continuously, there is no
/// recent state that needed to have survived.
///
/// What it cannot do: a GCS that first connects *after* takeoff has no baseline and never
/// saw the transition, and nothing in the aircraft records when a flight began. It counts
/// from the connection, exactly as QGC does today. That is the right answer for a spectator
/// joining a flight in progress, and it is why the pilot's station stays the single source
/// of truth.
///
/// Accuracy: the baseline is exact, having been read while the counter was frozen, so only
/// the live reading carries `AP_Stats::flush_interval_ms` (30 s) of lag. The ground-side
/// timer still runs between reads for smoothness, and a reading only ever corrects it
/// *forward*, so the display converges on the truth and can never jump backwards.
class FlightStatsResume : public QObject
{
    Q_OBJECT

public:
    explicit FlightStatsResume(QObject *parent = nullptr);

    /// Parameters read to find, and to measure against, a flight's baseline.
    static const QStringList &statParameterNames();

public slots:
    void vehicleAdded(Vehicle *vehicle);

private slots:
    void _pollDue();

private:
    /// One reading of the aircraft's counters.
    struct Reading {
        bool    valid       = false;
        int     bootCount   = -1;
        int     flightCount = -1;
        double  flightTimeSecs = 0;
        double  distanceM   = 0;
    };

    /// What the counters read when a flight began. Written once, at the takeoff that
    /// started it, and thereafter only read.
    struct Baseline {
        bool    valid       = false;
        int     bootCount   = -1;
        int     flightCount = -1;
        double  flightTimeSecs = 0;
        double  distanceM   = 0;
        qint64  savedAtEpoch = 0;   ///< only to age out records across restarts
    };

    struct Tracked {
        QPointer<Vehicle>   vehicle;
        QSet<QString>       awaiting;   ///< requested and not yet answered
        Reading             lastSeen;   ///< the candidate baseline, while still on the ground
    };

    void _requestStats(Vehicle *vehicle);
    void _statRead(Vehicle *vehicle, const QString &paramName);
    void _statsArrived(Vehicle *vehicle);
    bool _readStats(Vehicle *vehicle, Reading &out) const;

    void _loadPersisted();
    void _persist(int vehicleId, const Baseline &baseline);

    /// Survives the Vehicle, so it is keyed by MAVLink system id rather than held on it.
    QHash<int, Baseline>    _baselines;
    QHash<int, Tracked>     _tracked;
    QTimer                  _pollTimer;
};
