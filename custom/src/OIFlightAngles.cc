/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#include "OIFlightAngles.h"

#include <QtCore/QtMath>

#include "MultiVehicleManager.h"
#include "ParameterManager.h"
#include "QGCLoggingCategory.h"
#include "Vehicle.h"

QGC_LOGGING_CATEGORY(OIFlightAnglesLog, "OI.FlightAngles")

OIFlightAngles::OIFlightAngles(QObject *parent)
    : QObject(parent)
{
    _clear();

    (void) connect(MultiVehicleManager::instance(), &MultiVehicleManager::activeVehicleChanged,
                   this, &OIFlightAngles::_activeVehicleChanged);

    _staleTimer.setInterval(kStaleMs / 2);
    (void) connect(&_staleTimer, &QTimer::timeout, this, &OIFlightAngles::_checkStale);
    _staleTimer.start();

    _activeVehicleChanged();
}

double OIFlightAngles::minimumAirspeed() const
{
    if (!_vehicle || !_vehicle->parameterManager()->parametersReady()) {
        return qQNaN();
    }

    // NaN when the airframe has no AIRSPEED_MIN to read, which is the honest answer rather than
    // a guessed floor: a stall indication tied to a number we invented is worse than none.
    return _vehicle->minimumEquivalentAirspeed();
}

bool OIFlightAngles::airspeedSufficient() const
{
    const double minimum = minimumAirspeed();
    if (!qIsFinite(minimum) || (minimum <= 0)) {
        return false;
    }

    return _vehicle->airSpeed()->rawValue().toDouble() >= minimum;
}

void OIFlightAngles::handleMessage(const Vehicle *vehicle, const mavlink_message_t &message)
{
    if (message.msgid != MAVLINK_MSG_ID_AOA_SSA) {
        return;
    }

    if (!vehicle || (vehicle != MultiVehicleManager::instance()->activeVehicle())) {
        return;
    }

    mavlink_aoa_ssa_t angles{};
    mavlink_msg_aoa_ssa_decode(&message, &angles);

    if (!qIsFinite(angles.AOA) || !qIsFinite(angles.SSA)) {
        return;
    }

    _aoa = static_cast<double>(angles.AOA);
    _ssa = static_cast<double>(angles.SSA);
    _valid = true;
    _lastReport.start();

    emit changed();
}

double OIFlightAngles::criticalFraction() const
{
    if (!_valid) {
        return 0;
    }

    // Deliberately not clamped at the top. An indexer should be able to show that the angle has
    // gone past critical rather than parking at the red band's edge; a consumer that needs a
    // bounded value can clamp it with the limit in view.
    return _aoa / kCriticalAngleDeg;
}

void OIFlightAngles::_activeVehicleChanged()
{
    if (_vehicle) {
        (void) disconnect(_vehicle->parameterManager(), &ParameterManager::parametersReadyChanged,
                          this, &OIFlightAngles::_parametersReadyChanged);
    }

    _vehicle = MultiVehicleManager::instance()->activeVehicle();

    // The parameter download finishes seconds after the vehicle appears, and the minimum
    // airspeed is not readable until it does. Without this the gate would stay shut until the
    // next AOA_SSA happened to re-read it - which works, but only by accident.
    if (_vehicle) {
        (void) connect(_vehicle->parameterManager(), &ParameterManager::parametersReadyChanged,
                       this, &OIFlightAngles::_parametersReadyChanged);
    }

    _clear();
    emit changed();
}

void OIFlightAngles::_parametersReadyChanged()
{
    qCDebug(OIFlightAnglesLog) << "parameters ready, minimum airspeed now" << minimumAirspeed();
    emit changed();
}

void OIFlightAngles::_checkStale()
{
    if (!_valid) {
        return;
    }

    if (!_lastReport.isValid() || (_lastReport.elapsed() > kStaleMs)) {
        qCDebug(OIFlightAnglesLog) << "no AOA_SSA for" << kStaleMs << "ms, dropping";
        _clear();
        emit changed();
    }
}

void OIFlightAngles::_clear()
{
    _valid = false;
    _aoa = 0;
    _ssa = 0;
    _lastReport.invalidate();
}
