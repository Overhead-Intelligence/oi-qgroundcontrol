/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#include "OIEkfStatus.h"

#include <QtCore/QtMath>

#include "MultiVehicleManager.h"
#include "QGCLoggingCategory.h"
#include "Vehicle.h"

QGC_LOGGING_CATEGORY(OIEkfStatusLog, "OI.EkfStatus")

namespace {

/// One flag, and what its state means for navigation.
///
/// `faultWhenSet` separates the three flags that report a problem by being present from the
/// nine that report a capability by being present. `severityIfWrong` is what the icon should
/// show when the flag is not where it should be - and SeverityNominal here means "display it,
/// but do not colour anything", which is the honest answer for a capability the aircraft may
/// simply not have.
struct FlagDef {
    uint16_t    bit;
    const char *label;
    bool        faultWhenSet;
    int         severityIfWrong;
};

const FlagDef kFlags[] = {
    // Attitude is the floor. Without it nothing else in the solution means anything.
    { 0x0001, QT_TRANSLATE_NOOP("OIEkfStatus", "Attitude"),              false, OIEkfStatus::SeverityCritical },

    // Horizontal velocity and absolute horizontal position are what guided flight is steered
    // on, so losing either is a real degradation - but survivable, and often transient.
    { 0x0002, QT_TRANSLATE_NOOP("OIEkfStatus", "Velocity (horizontal)"), false, OIEkfStatus::SeverityWarning },
    { 0x0010, QT_TRANSLATE_NOOP("OIEkfStatus", "Position (abs horiz)"),  false, OIEkfStatus::SeverityWarning },

    // Informational. Vertical velocity and absolute vertical position track the horizontal
    // ones closely enough that colouring on them as well only doubles the noise, and the
    // relative/AGL estimates depend on sensors a given airframe may not carry at all:
    // POS_VERT_AGL is clear on anything without a rangefinder, POS_HORIZ_REL without flow.
    { 0x0004, QT_TRANSLATE_NOOP("OIEkfStatus", "Velocity (vertical)"),   false, OIEkfStatus::SeverityNominal },
    { 0x0008, QT_TRANSLATE_NOOP("OIEkfStatus", "Position (rel horiz)"),  false, OIEkfStatus::SeverityNominal },
    { 0x0020, QT_TRANSLATE_NOOP("OIEkfStatus", "Position (abs vert)"),   false, OIEkfStatus::SeverityNominal },
    { 0x0040, QT_TRANSLATE_NOOP("OIEkfStatus", "Position (AGL)"),        false, OIEkfStatus::SeverityNominal },

    // The predicted pair are the pre-arm estimate: they lead the live flags before takeoff and
    // say nothing new once flying.
    { 0x0100, QT_TRANSLATE_NOOP("OIEkfStatus", "Predicted (rel horiz)"), false, OIEkfStatus::SeverityNominal },
    { 0x0200, QT_TRANSLATE_NOOP("OIEkfStatus", "Predicted (abs horiz)"), false, OIEkfStatus::SeverityNominal },

    // The three that are faults by being present.
    //
    // Constant position mode means the estimator has stopped solving for position and is
    // holding the last one it had. The number on screen is then fiction, which is worse than
    // an obviously large variance.
    { 0x0080, QT_TRANSLATE_NOOP("OIEkfStatus", "Constant position mode"), true, OIEkfStatus::SeverityCritical },
    // Uninitialised means the filter is not running yet.
    { 0x0400, QT_TRANSLATE_NOOP("OIEkfStatus", "Uninitialised"),          true, OIEkfStatus::SeverityCritical },
    // A GPS glitch is the estimator rejecting the receiver. It usually recovers, and it is
    // exactly the thing worth seeing a second before the variances move.
    { 0x8000, QT_TRANSLATE_NOOP("OIEkfStatus", "GPS glitching"),          true, OIEkfStatus::SeverityWarning },
};

} // namespace

OIEkfStatus::OIEkfStatus(QObject *parent)
    : QObject(parent)
{
    _clear();

    (void) connect(MultiVehicleManager::instance(), &MultiVehicleManager::activeVehicleChanged,
                   this, &OIEkfStatus::_activeVehicleChanged);

    _staleTimer.setInterval(kStaleMs / 2);
    (void) connect(&_staleTimer, &QTimer::timeout, this, &OIEkfStatus::_checkStale);
    _staleTimer.start();
}

void OIEkfStatus::handleMessage(const Vehicle *vehicle, const mavlink_message_t &message)
{
    if (message.msgid != MAVLINK_MSG_ID_EKF_STATUS_REPORT) {
        return;
    }

    // One aircraft's estimate on screen at a time. A second vehicle's report would otherwise
    // overwrite the one being watched, with nothing on the indicator to say it had.
    if (!vehicle || (vehicle != MultiVehicleManager::instance()->activeVehicle())) {
        return;
    }

    mavlink_ekf_status_report_t report{};
    mavlink_msg_ekf_status_report_decode(&message, &report);

    _rebuild(report);
    _lastReport.start();
    emit changed();
}

void OIEkfStatus::_activeVehicleChanged()
{
    _clear();
    emit changed();
}

void OIEkfStatus::_checkStale()
{
    if (!_valid) {
        return;
    }

    if (!_lastReport.isValid() || (_lastReport.elapsed() > kStaleMs)) {
        qCDebug(OIEkfStatusLog) << "no EKF_STATUS_REPORT for" << kStaleMs << "ms, dropping";
        _clear();
        emit changed();
    }
}

void OIEkfStatus::_clear()
{
    _valid = false;
    _severity = SeverityNominal;
    _worstVariance = 0;
    _variances.clear();
    _flags.clear();
    _summary = tr("No EKF report");
    _lastReport.invalidate();
}

int OIEkfStatus::_varianceSeverity(double value)
{
    if (!qIsFinite(value)) {
        return SeverityNominal;
    }
    if (value >= kCriticalVariance) {
        return SeverityCritical;
    }
    if (value >= kWarningVariance) {
        return SeverityWarning;
    }
    return SeverityNominal;
}

void OIEkfStatus::_rebuild(const mavlink_ekf_status_report_t &report)
{
    struct VarianceDef {
        const char *label;
        double      value;
    };

    // Airspeed is included deliberately. Mission Planner shows five and leaves it out, but on a
    // quadplane it is the one that moves during transition, which is where the estimate is
    // under most stress.
    const VarianceDef definitions[] = {
        { QT_TRANSLATE_NOOP("OIEkfStatus", "Velocity"),      report.velocity_variance },
        { QT_TRANSLATE_NOOP("OIEkfStatus", "Horiz position"), report.pos_horiz_variance },
        { QT_TRANSLATE_NOOP("OIEkfStatus", "Vert position"),  report.pos_vert_variance },
        { QT_TRANSLATE_NOOP("OIEkfStatus", "Compass"),        report.compass_variance },
        { QT_TRANSLATE_NOOP("OIEkfStatus", "Terrain alt"),    report.terrain_alt_variance },
        { QT_TRANSLATE_NOOP("OIEkfStatus", "Airspeed"),       report.airspeed_variance },
    };

    _variances.clear();
    _worstVariance = 0;
    int severity = SeverityNominal;

    for (const VarianceDef &definition : definitions) {
        const double value = static_cast<double>(definition.value);
        const int varianceSeverity = _varianceSeverity(value);
        severity = qMax(severity, varianceSeverity);
        if (qIsFinite(value)) {
            _worstVariance = qMax(_worstVariance, value);
        }

        _variances.append(QVariantMap {
            { QStringLiteral("name"),     tr(definition.label) },
            { QStringLiteral("value"),    value },
            { QStringLiteral("severity"), varianceSeverity },
        });
    }

    _flags.clear();
    QStringList faults;

    for (const FlagDef &flag : kFlags) {
        const bool set = (report.flags & flag.bit) != 0;
        const bool healthy = (set != flag.faultWhenSet);
        const int flagSeverity = healthy ? SeverityNominal : flag.severityIfWrong;
        severity = qMax(severity, flagSeverity);

        if (!healthy && (flagSeverity != SeverityNominal)) {
            faults.append(tr(flag.label));
        }

        _flags.append(QVariantMap {
            { QStringLiteral("name"),     tr(flag.label) },
            { QStringLiteral("set"),      set },
            { QStringLiteral("healthy"),  healthy },
            { QStringLiteral("severity"), flagSeverity },
        });
    }

    _severity = severity;
    _valid = true;

    // Names the reason rather than restating the colour: an operator who opens the popup
    // because the icon went orange wants to know which of the two kinds of problem it was.
    if (!faults.isEmpty()) {
        _summary = faults.join(QStringLiteral(", "));
    } else if (_severity != SeverityNominal) {
        _summary = tr("Variance %1").arg(_worstVariance, 0, 'f', 2);
    } else {
        _summary = tr("Nominal");
    }
}
