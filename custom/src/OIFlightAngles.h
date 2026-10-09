/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * The flight quantities a primary flight display needs that QGC parses and throws away:
 * angle of attack and sideslip from AOA_SSA (11020), and lateral acceleration from RAW_IMU.
 *
 * QGC parses the message and discards it - nothing in the stock tree reads it -
 * yet the fleet already streams it: AOA_SSA rides in the EXTRA1 group and
 * SR1_EXTRA1 is 10 Hz on the aircraft checked. So the two values a conventional
 * primary flight display needs and QGC has never shown - a stall margin and a
 * slip indication - have been arriving the whole time.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>

#include "QGCMAVLink.h"

class Vehicle;

/// Latest angle of attack and sideslip for the active vehicle.
///
/// **These are estimates, not measurements, and that bounds how they may be shown.**
/// `AP_AHRS::update_AOA_SSA()` derives both from the EKF velocity solution minus the wind
/// estimate, rotated into the body frame - there is no vane. Near zero airspeed the geometry
/// is ill-conditioned and the wind estimate is at its worst, so in a VTOL hover the numbers
/// are noise wearing the shape of data. A consumer must gate on airspeed before presenting
/// them; this class reports what arrived and how old it is, and leaves that judgement to the
/// display.
///
/// ArduPilot only sends this from Plane builds, so a multirotor never populates it.
class OIFlightAngles : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool     valid           READ valid          NOTIFY changed)
    Q_PROPERTY(double   angleOfAttack   READ angleOfAttack  NOTIFY changed)   ///< degrees
    Q_PROPERTY(double   sideslip        READ sideslip       NOTIFY changed)   ///< degrees
    /// Angle of attack as a fraction of the critical angle, 0..1+ - what an AOA indexer shows.
    Q_PROPERTY(double   criticalFraction READ criticalFraction NOTIFY changed)
    Q_PROPERTY(double   criticalAngle   READ criticalAngle  CONSTANT)

    /// The airframe's minimum airspeed, or NaN if it has not said. See the note on the getter:
    /// this is here, rather than called from QML, because calling it from QML does not work.
    Q_PROPERTY(double   minimumAirspeed READ minimumAirspeed NOTIFY changed)
    /// Whether the aircraft is fast enough for the angles to mean anything.
    Q_PROPERTY(bool     airspeedSufficient READ airspeedSufficient NOTIFY changed)
    /// valid && airspeedSufficient - the single test a display should gate on.
    Q_PROPERTY(bool     usable          READ usable         NOTIFY changed)

    /// Lateral specific force in g, positive toward the right wing - the slip/skid ball.
    Q_PROPERTY(double   lateralAcceleration READ lateralAcceleration NOTIFY changed)
    Q_PROPERTY(bool     lateralAccelerationValid READ lateralAccelerationValid NOTIFY changed)

public:
    /// Mission Planner's default, and there is no better source: ArduPilot exposes no critical
    /// angle parameter, because it does not compute one. Treated as a display reference only -
    /// nothing is gated on it in the aircraft.
    static constexpr double kCriticalAngleDeg = 25.0;

    explicit OIFlightAngles(QObject *parent = nullptr);

    /// Ignores anything that is not AOA_SSA, and anything from a vehicle that is not active.
    void handleMessage(const Vehicle *vehicle, const mavlink_message_t &message);

    bool valid() const { return _valid; }
    double angleOfAttack() const { return _aoa; }
    double sideslip() const { return _ssa; }
    double criticalFraction() const;
    double criticalAngle() const { return kCriticalAngleDeg; }

    /// Read live from the active vehicle every time, and deliberately not cached.
    ///
    /// **This cannot be done from QML.** `Vehicle::minimumEquivalentAirspeed()` is Q_INVOKABLE,
    /// a method rather than a property, so a binding that calls it captures no dependency on
    /// the parameter set - it evaluates once, when the vehicle is assigned, and never again.
    /// The vehicle is assigned on its first heartbeat, seconds before the parameter download
    /// finishes, so `AIRSPEED_MIN` does not exist yet and the binding locks in NaN for the
    /// whole session. Measured in flight on 2026-10-09: the parameter was present and set to
    /// 16, and the angle of attack indexer stayed greyed out the entire time.
    ///
    /// Reading it here instead works because this object emits changed() on every AOA_SSA, and
    /// on parametersReadyChanged, so anything bound to it re-reads.
    double minimumAirspeed() const;
    bool airspeedSufficient() const;
    bool usable() const { return _valid && airspeedSufficient(); }

    /// Body-frame lateral specific force, in g, positive toward the right wing.
    ///
    /// This is what a slip/skid ball actually shows, and it is **not** the sideslip angle the
    /// flight path vector already displays. Sideslip is an aerodynamic angle; this is a force
    /// balance, and the two disagree in normal flight - measured over a 15 minute sortie on
    /// 2026-10-09, sideslip ran a median +3.4 deg while lateral acceleration ran a median
    /// -0.036 g. A ball driven from sideslip would be a second drawing of a number already on
    /// screen, and would be wrong.
    ///
    /// The sign convention was established against that flight rather than assumed: regressed
    /// against the steady-turn coordination model (omega*V/g)*cos(phi) - sin(phi), it comes out
    /// positively correlated (r = +0.35 in banked flight) with the means agreeing to 0.01 g.
    double lateralAcceleration() const { return _lateralAccel; }
    bool lateralAccelerationValid() const { return _lateralAccelValid; }

signals:
    void changed();

private slots:
    void _activeVehicleChanged();
    void _checkStale();
    void _checkAccelStale();
    void _parametersReadyChanged();

private:
    void _clear();

    /// Only to reach the parameter set and the airspeed fact; no ownership.
    QPointer<Vehicle> _vehicle;

    bool    _valid = false;
    double  _aoa = 0;
    double  _ssa = 0;

    bool    _lateralAccelValid = false;
    double  _lateralAccel = 0;

    /// RAW_IMU rides the RAW_SENS group, which the fleet runs at 2 Hz against EXTRA1's 4.2 -
    /// so the 2 s window the angles use would be only four samples here. Three seconds is six.
    static constexpr int kAccelStaleMs = 3000;
    QElapsedTimer   _lastAccelReport;

    /// EXTRA1 runs at up to 10 Hz and AP_AHRS recomputes at 20 Hz, so a second of silence is
    /// already many missed updates. A flight path vector frozen on an old sideslip would sit
    /// there looking authoritative.
    static constexpr int kStaleMs = 2000;
    QElapsedTimer   _lastReport;
    QTimer          _staleTimer;
};
