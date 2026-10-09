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
    _clear();
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
