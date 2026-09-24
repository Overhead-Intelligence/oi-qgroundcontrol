/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * Keyboard guided control. An application-wide event filter turns key presses
 * into discrete GUIDED-mode commands for ArduPlane.
 *
 * WHY EVERYTHING IS A DISCRETE ABSOLUTE STEP
 * Measured against ArduPlane 4.6.3 (quadplane SITL, Plane-4.6.3-2-gfd260469d96):
 *   - GUIDED_CHANGE_ALTITUDE (43001) is acked but its param3 rate limit does not
 *     work - 1 m/s delivered 0.018 m/s, 5 m/s delivered 0.155 m/s. It only moves
 *     the aircraft with param3 = 0, i.e. with no limit at all. Worse, once used it
 *     latches: ModeGuided::update_target_altitude() stays in its slew branch for
 *     the rest of the GUIDED session and SET_POSITION_TARGET_LOCAL_NED offsets are
 *     then ignored, which would silently break QGC's own altitude slider.
 *     So altitude goes through the stock offset path, clamped here.
 *   - GUIDED_CHANGE_HEADING (43002) works in fixed wing and its param3 IS a real
 *     rate control (measured 1.55 deg/s at 0.5, 4.41 deg/s at 3.0). In VTOL it is
 *     acked and silently ignored: ModeGuided::update() early-returns into
 *     quadplane.guided_update() before reaching the heading slew.
 *   - The gimbal rate path did not move a servo mount in SITL at all; absolute
 *     DO_GIMBAL_MANAGER_PITCHYAW did, precisely. Steps also mean a missed key-up
 *     cannot leave a rate latched.
 *
 * SAFETY CONTRACT
 *   - Disarmed at startup; never persisted. Arming is an explicit operator action.
 *   - Disarms on: Esc, active vehicle change, flight mode change, loss of window
 *     focus, and vehicle disconnect.
 *   - Keys are ignored entirely while a text input has focus.
 *   - Acts only on an armed, flying ArduPlane vehicle already in GUIDED. It never
 *     changes flight mode to get there - QGC's guidedModeChangeAltitude() would
 *     switch a vehicle out of AUTO, so this class does not use that entry point
 *     without checking the mode first.
 *   - The altitude target is tracked here, clamped to the Fly View guided minimum
 *     and maximum, and additionally capped to altitudeLead ahead of the aircraft's
 *     measured altitude, because the firmware accumulates offsets with no bound of
 *     its own (ArduPlane GCS_Mavlink.cpp: next_WP_loc.alt += -packet.z*100).
 *   - Flight mode hotkeys need two presses: the first opens a confirmation, the
 *     second within modeConfirmTimeout sends it, Esc cancels.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QTimer>

class QEvent;
class Vehicle;
class OIKeyboardSettings;
class QmlObjectListModel;

Q_DECLARE_LOGGING_CATEGORY(OIKeyboardLog)

/// One "press this key to select that flight mode" row. Owned by the controller's
/// model and persisted under OI/KeyboardModeHotkeys.
class OIModeHotkey : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString key      READ key    WRITE setKey    NOTIFY changed)
    Q_PROPERTY(QString mode     READ mode   WRITE setMode   NOTIFY changed)

public:
    explicit OIModeHotkey(const QString &key = QString(), const QString &mode = QString(),
                          QObject *parent = nullptr);

    QString key() const { return _key; }
    void setKey(const QString &key);
    QString mode() const { return _mode; }
    void setMode(const QString &mode);

signals:
    void changed();

private:
    QString _key;
    QString _mode;
};

class OIKeyboardController : public QObject
{
    Q_OBJECT
    Q_MOC_INCLUDE("QmlObjectListModel.h")

    Q_PROPERTY(bool     armed           READ armed          WRITE setArmed  NOTIFY armedChanged)
    Q_PROPERTY(bool     available       READ available                      NOTIFY stateChanged)
    Q_PROPERTY(QString  statusText      READ statusText                     NOTIFY stateChanged)
    Q_PROPERTY(bool     headingUsable   READ headingUsable                  NOTIFY stateChanged)
    Q_PROPERTY(double   altitudeTarget  READ altitudeTarget                 NOTIFY stateChanged)
    Q_PROPERTY(bool     altitudeTargetValid READ altitudeTargetValid        NOTIFY stateChanged)
    Q_PROPERTY(QString  pendingModeName READ pendingModeName                NOTIFY pendingModeChanged)
    Q_PROPERTY(QmlObjectListModel* modeHotkeys READ modeHotkeys             CONSTANT)
    /// The OIKeyboard settings group. Owned here rather than by SettingsManager,
    /// which would be a src/ change for no gain.
    Q_PROPERTY(QObject* settings            READ settingsObject                 CONSTANT)

public:
    explicit OIKeyboardController(QObject *parent = nullptr);
    ~OIKeyboardController() override;

    bool armed() const { return _armed; }
    void setArmed(bool armed);

    /// True when an armed, flying ArduPlane vehicle is in GUIDED - i.e. keys would do something.
    bool available() const { return _available; }

    /// Why keys are or are not doing anything, for the operator.
    QString statusText() const { return _statusText; }

    /// Heading keys only work in forward flight; in VTOL the command is accepted and ignored.
    bool headingUsable() const { return _headingUsable; }

    double altitudeTarget() const { return _altitudeTarget; }
    bool altitudeTargetValid() const { return _altitudeTargetValid; }

    QString pendingModeName() const { return _pendingModeName; }
    QmlObjectListModel *modeHotkeys() const { return _modeHotkeys; }
    QObject *settingsObject() const;

    Q_INVOKABLE void addModeHotkey(const QString &key, const QString &mode);
    Q_INVOKABLE void removeModeHotkey(int index);
    Q_INVOKABLE void saveModeHotkeys();

    /// Flight mode names the connected vehicle offers, for the hotkey editor.
    Q_INVOKABLE QStringList availableModes() const;

    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void armedChanged();
    void stateChanged();
    void pendingModeChanged();

private slots:
    void _activeVehicleChanged(Vehicle *vehicle);
    void _recomputeState();
    void _confirmTimeout();

private:
    bool _handleKey(int key, Qt::KeyboardModifiers modifiers);
    bool _matches(const QString &settingValue, int key) const;
    void _stepHeading(int direction);
    void _stepAltitude(int direction);
    void _stepGimbalPitch(int direction);
    void _stepGimbalYaw(int direction);
    void _cycleGimbalMode(int direction);
    bool _tryModeHotkey(int key);
    void _sendPendingMode();
    void _clearPendingMode();
    void _reseedAltitudeTarget();
    void _loadModeHotkeys();
    void _setStatus(const QString &text);
    Vehicle *_vehicle() const;

    OIKeyboardSettings *_settings = nullptr;
    QmlObjectListModel *_modeHotkeys = nullptr;
    QPointer<Vehicle> _activeVehicle;

    bool _armed = false;
    bool _available = false;
    bool _headingUsable = false;
    QString _statusText;

    double _headingTarget = 0.0;
    bool _headingTargetValid = false;

    double _altitudeTarget = 0.0;
    bool _altitudeTargetValid = false;

    /// Gimbal angles are tracked here because DO_GIMBAL_MANAGER_PITCHYAW is absolute
    /// and the vehicle does not report a settable target back.
    double _gimbalPitch = 0.0;
    double _gimbalYaw = 0.0;
    int _gimbalModeIndex = 0;

    QString _pendingModeName;
    int _pendingModeKey = 0;
    QTimer _confirmTimer;

    /// Re-seeding the altitude target from the aircraft is only safe once it has
    /// settled; this tracks when the last step was commanded.
    QElapsedTimer _sinceLastAltitudeStep;
};
