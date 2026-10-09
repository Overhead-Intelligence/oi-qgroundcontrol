/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * Angle of attack and sideslip, from ArduPilot's AOA_SSA (id 11020).
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

signals:
    void changed();

private slots:
    void _activeVehicleChanged();
    void _checkStale();

private:
    void _clear();

    bool    _valid = false;
    double  _aoa = 0;
    double  _ssa = 0;

    /// EXTRA1 runs at up to 10 Hz and AP_AHRS recomputes at 20 Hz, so a second of silence is
    /// already many missed updates. A flight path vector frozen on an old sideslip would sit
    /// there looking authoritative.
    static constexpr int kStaleMs = 2000;
    QElapsedTimer   _lastReport;
    QTimer          _staleTimer;
};
