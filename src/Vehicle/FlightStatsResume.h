#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtCore/QTimer>

class Vehicle;

/// Carries flight time and distance across a reconnect.
///
/// QGC tracks both entirely on the ground side. `Vehicle::_flightTimerStart()` zeroes
/// them on the disarmed->armed transition and a `QElapsedTimer` runs from there, and
/// distance is accumulated from map trajectory points. That works until the link is
/// torn down - a serial port disappearing, a TCP socket closing, an operator
/// reconnecting - because `VehicleLinkManager` then signals `allLinksRemoved`,
/// `MultiVehicleManager` deletes the Vehicle, and its replacement starts from zero
/// while the aircraft has not stopped flying.
///
/// A plain telemetry gap does *not* do this: `_autoDisconnect` is false by default, so
/// missed heartbeats only set `communicationLost` and the Vehicle survives with its
/// timer still running. This class exists for the case where the Vehicle object itself
/// is replaced.
///
/// The vehicle is the only thing that saw the whole flight, and ArduPilot already
/// counts it. `AP_Stats` keeps `STAT_FLTTIME` (s) and `STAT_DISTFLWN` (m) as persisted
/// lifetime totals, with `STAT_FLTCNT` incrementing once per flight and `STAT_BOOTCNT`
/// once per boot. They are ordinary parameters, so no new protocol is needed.
///
/// While connected we hold a *paired* snapshot: what we were displaying, and what the
/// vehicle's counters read at that same moment. On reconnect the elapsed flight is
/// `ours at the snapshot + (vehicle's now - vehicle's at the snapshot)`. Pairing matters:
/// after comms are lost our own timer keeps running while the vehicle's counters are
/// out of reach, so combining a late local value with an old vehicle value would count
/// the blackout twice.
///
/// `STAT_BOOTCNT` and `STAT_FLTCNT` are what make the resume safe. If either moved, the
/// vehicle rebooted or flew again in the gap, the snapshot describes a different flight,
/// and we fall back to starting from zero.
///
/// Resolution is bounded by `AP_Stats::flush_interval_ms` (30 s): the counters are only
/// written to their parameters that often, so a resumed figure reads short by up to one
/// flush interval. That is the deliberate trade - a bounded error against losing the whole
/// flight. Measured in SITL across every possible dropout in a 1188 s, 30 km flight: worst
/// time error 30.8 s, worst distance error 918 m. Both are the same 30 s, the second one
/// expressed at cruise speed.
///
/// The two sides do not measure distance the same way, so the blackout is filled at the
/// vehicle's rate rather than ours: `AP_Stats::update_distance_flown` integrates the AHRS
/// position at 1 Hz in **three** dimensions (`get_distance_NED().length()`, ignoring steps
/// under 0.5 m), while `TrajectoryPoints` sums 2D great-circle legs between map points and
/// ignores anything under 2 m. A climb therefore counts on the vehicle and not on the map -
/// 79.7 m of "distance" for an 80 m ascent with no horizontal movement. In level flight the
/// two nearly cancel: over that 30 km the map total ran 1.1% above the vehicle's. Only the
/// difference across the gap is ever taken, so even that is confined to the part of the
/// flight nobody was watching.
///
/// Snapshots are written to settings as they are taken, so a QGC that crashes and is
/// restarted mid-flight picks the flight back up too - that is a crash *during* a flight,
/// which has happened, and losing the flight to it is the same failure as losing it to a
/// reconnect. The same boot/flight-count check applies, so a restart after the aircraft
/// has flown again, or been power cycled, starts from zero as it should; a wall-clock
/// bound is there as well, for the case where neither counter has moved because nothing
/// has happened for a very long time.
///
/// With no usable snapshot - QGC started for the first time mid-flight - the vehicle
/// counts from the connection as it always has, which is the right answer for a spectator
/// joining a flight already in progress.
class FlightStatsResume : public QObject
{
    Q_OBJECT

public:
    explicit FlightStatsResume(QObject *parent = nullptr);

    /// Parameters read to decide, and to measure, a resume.
    static const QStringList &statParameterNames();

public slots:
    void vehicleAdded(Vehicle *vehicle);

private slots:
    void _snapshotDue();

private:
    struct Snapshot {
        int     bootCount       = -1;
        int     flightCount     = -1;
        double  vehicleTimeSecs = 0;    ///< STAT_FLTTIME when the pair was captured
        double  vehicleDistanceM = 0;   ///< STAT_DISTFLWN at that same moment
        double  gcsTimeSecs     = 0;    ///< what we were displaying then
        double  gcsDistanceM    = 0;
        qint64  savedAtEpoch    = 0;    ///< only to age out records across restarts
    };

    struct Tracked {
        QPointer<Vehicle>   vehicle;
        QSet<QString>       awaiting;       ///< requested and not yet answered
        bool                resumeSettled = false;
    };

    void _requestStats(Vehicle *vehicle);
    void _statRead(Vehicle *vehicle, const QString &paramName);
    void _statsArrived(Vehicle *vehicle);
    bool _readStats(Vehicle *vehicle, Snapshot &out) const;
    void _takeSnapshot(Vehicle *vehicle, const Snapshot &vehicleStats);
    void _forget(int vehicleId);

    void _loadPersisted();
    void _persist(int vehicleId, const Snapshot &snapshot);
    void _dropPersisted(int vehicleId);

    /// Survives the Vehicle, so it is keyed by MAVLink system id rather than held on it.
    QHash<int, Snapshot>    _snapshots;
    QHash<int, Tracked>     _tracked;
    QTimer                  _snapshotTimer;
};
