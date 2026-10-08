/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * EKF health for the Fly view, from ArduPilot's EKF_STATUS_REPORT (id 193).
 *
 * QGC parses that message and throws it away - nothing in the stock tree reads
 * it. It carries six normalised variances and a twelve-bit health mask, and the
 * fleet already streams it: EXTRA3 is the group it rides in, and every SR*_EXTRA3
 * on the aircraft checked was 1-3 Hz. So this needs no stream request and no new
 * protocol, only somewhere to put the numbers.
 *
 * Read through QGCCorePlugin::mavlinkMessage(), which exists to let a custom build
 * see raw traffic, so the whole feature stays inside custom/.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QVariantList>

#include "QGCMAVLink.h"

class Vehicle;

/// Latest EKF status for the active vehicle, and the severity derived from it.
///
/// **Severity is taken from the flags as well as the variances, and that is the point.**
/// A variance threshold alone misses the cases that matter most to anticipate: the estimator
/// can set GPS_GLITCHING or drop into CONST_POS_MODE while every variance is still low,
/// because it has stopped trusting a source rather than diverged yet. Watching only the
/// numbers means seeing that one sample too late.
///
/// Equally, not every clear flag is a fault. POS_VERT_AGL is clear on any aircraft with no
/// rangefinder and POS_HORIZ_REL on any without optical flow, so colouring on "anything not
/// set" would leave the icon permanently red and worth ignoring. Only the flags whose state
/// actually describes the navigation solution carry a severity; the rest are shown in the
/// table but do not colour the icon. See kFlags in the implementation for the split.
class OIEkfStatus : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool         valid               READ valid              NOTIFY changed)
    Q_PROPERTY(int          severity            READ severity           NOTIFY changed)
    Q_PROPERTY(double       worstVariance       READ worstVariance      NOTIFY changed)
    /// One entry per variance: { name, value, severity }.
    Q_PROPERTY(QVariantList variances           READ variances          NOTIFY changed)
    /// One entry per flag: { name, set, healthy, severity }. `healthy` is whether the flag is
    /// in the state it should be, which for three of them means *clear* rather than set.
    Q_PROPERTY(QVariantList flags               READ flags              NOTIFY changed)
    /// Short summary for the indicator popup heading.
    Q_PROPERTY(QString      summary             READ summary            NOTIFY changed)

public:
    /// Matches the colour steps the operator asked for: white, orange, red.
    enum Severity {
        SeverityNominal     = 0,
        SeverityWarning     = 1,
        SeverityCritical    = 2,
    };
    Q_ENUM(Severity)

    /// Variance thresholds, matching the ones Mission Planner coluors against so a reading
    /// means the same thing in either ground station.
    static constexpr double kWarningVariance  = 0.5;
    static constexpr double kCriticalVariance = 0.8;

    explicit OIEkfStatus(QObject *parent = nullptr);

    /// Feed a raw message. Ignores anything that is not EKF_STATUS_REPORT, and anything from a
    /// vehicle that is not the active one - the indicator shows one aircraft.
    void handleMessage(const Vehicle *vehicle, const mavlink_message_t &message);

    bool valid() const { return _valid; }
    int severity() const { return _severity; }
    double worstVariance() const { return _worstVariance; }
    QVariantList variances() const { return _variances; }
    QVariantList flags() const { return _flags; }
    QString summary() const { return _summary; }

signals:
    void changed();

private slots:
    void _activeVehicleChanged();
    void _checkStale();

private:
    void _clear();
    void _rebuild(const mavlink_ekf_status_report_t &report);
    static int _varianceSeverity(double value);

    bool            _valid = false;
    int             _severity = SeverityNominal;
    double          _worstVariance = 0;
    QVariantList    _variances;
    QVariantList    _flags;
    QString         _summary;

    /// EXTRA3 arrives at 1-3 Hz, so silence for this long means the aircraft has stopped
    /// reporting rather than that nothing has changed. Showing a stale estimate as current is
    /// the one failure this must not have.
    static constexpr int kStaleMs = 5000;
    QElapsedTimer   _lastReport;
    QTimer          _staleTimer;
};
