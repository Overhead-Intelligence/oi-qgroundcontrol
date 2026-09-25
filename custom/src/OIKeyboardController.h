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
 * ENABLED vs CAN-ACT
 * Two different things, deliberately kept apart. `enabled` is the operator's
 * standing preference, persisted in settings and changed only from the Keyboard
 * page - a pilot decides between flights whether they want it. `canAct` is derived
 * and moment-to-moment: enabled AND a vehicle that is armed, flying and in Guided.
 * Keys do nothing unless canAct, but losing Guided (or the vehicle, or window
 * focus) does NOT switch the feature off. An earlier version disarmed on every one
 * of those, which meant a fat-fingered mode change silently cost the operator the
 * whole feature until they went back to the settings page.
 *
 * SAFETY CONTRACT
 *   - Keys only ever act when canAct: armed, flying, in Guided.
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
 *     second within modeConfirmTimeout sends it, Esc cancels. Esc cancels only the
 *     confirmation; it does not turn the feature off.
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
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/QVariantList>

class QEvent;
class Fact;
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

    Q_PROPERTY(bool     enabled         READ enabled        WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool     canAct          READ canAct                         NOTIFY stateChanged)
    Q_PROPERTY(QString  statusText      READ statusText                     NOTIFY stateChanged)
    Q_PROPERTY(bool     headingUsable   READ headingUsable                  NOTIFY stateChanged)
    Q_PROPERTY(double   altitudeTarget  READ altitudeTarget                 NOTIFY stateChanged)
    Q_PROPERTY(bool     altitudeTargetValid READ altitudeTargetValid        NOTIFY stateChanged)
    Q_PROPERTY(double   headingTarget   READ headingTarget                  NOTIFY stateChanged)
    Q_PROPERTY(bool     headingTargetValid READ headingTargetValid           NOTIFY stateChanged)
    /// Transient, operator-facing reason a key press did nothing. Clears itself.
    Q_PROPERTY(QString  warningText     READ warningText                    NOTIFY warningChanged)
    Q_PROPERTY(QString  warningDetail   READ warningDetail                  NOTIFY warningChanged)
    Q_PROPERTY(QString  pendingModeName READ pendingModeName                NOTIFY pendingModeChanged)
    Q_PROPERTY(int      pendingSeconds  READ pendingSeconds                 NOTIFY pendingModeChanged)
    /// Human-readable descriptions of keys bound to more than one action. While a
    /// key is in conflict every action using it is disabled.
    Q_PROPERTY(QStringList keyConflicts READ keyConflicts                   NOTIFY keyConflictsChanged)
    Q_PROPERTY(QmlObjectListModel* modeHotkeys READ modeHotkeys             CONSTANT)
    /// The OIKeyboard settings group. Owned here rather than by SettingsManager,
    /// which would be a src/ change for no gain.
    Q_PROPERTY(QObject* settings            READ settingsObject                 CONSTANT)

public:
    explicit OIKeyboardController(QObject *parent = nullptr);
    ~OIKeyboardController() override;

    /// The operator's standing preference. Persisted; only the settings page changes it.
    bool enabled() const;
    void setEnabled(bool enabled);

    /// True when keys would actually do something right now: enabled, and a vehicle
    /// that is armed, flying and in Guided.
    bool canAct() const { return _canAct; }

    /// Why keys are or are not doing anything, for the operator.
    QString statusText() const { return _statusText; }

    /// Heading keys only work in forward flight; in VTOL the command is accepted and ignored.
    bool headingUsable() const { return _headingUsable; }

    double altitudeTarget() const { return _altitudeTarget; }
    bool altitudeTargetValid() const { return _altitudeTargetValid; }

    double headingTarget() const { return _headingTarget; }
    bool headingTargetValid() const { return _headingTargetValid; }
    QString warningText() const { return _warningText; }
    QString warningDetail() const { return _warningDetail; }
    QString pendingModeName() const { return _pendingModeName; }
    int pendingSeconds() const;
    QStringList keyConflicts() const { return _keyConflicts; }

    /// One row per bound action, for the Fly view quick reference:
    /// { "action": ..., "key": ..., "conflict": bool }.
    Q_INVOKABLE QVariantList bindingList() const;
    QmlObjectListModel *modeHotkeys() const { return _modeHotkeys; }
    QObject *settingsObject() const;

    Q_INVOKABLE void addModeHotkey(const QString &key, const QString &mode);
    Q_INVOKABLE void removeModeHotkey(int index);
    Q_INVOKABLE void saveModeHotkeys();

    /// Flight mode names the connected vehicle offers, for the hotkey editor.
    Q_INVOKABLE QStringList availableModes() const;

    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void enabledChanged();
    void stateChanged();
    void pendingModeChanged();
    void warningChanged();
    void keyConflictsChanged();

private slots:
    void _activeVehicleChanged(Vehicle *vehicle);
    void _recomputeState();
    void _confirmTimeout();
    void _rebuildKeyConflicts();

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
    /// Moves a step fact onto the nearest offered value if a previously tuned
    /// value is no longer in the dropdown.
    void _normaliseStep(Fact *fact, const QList<double> &allowed);
    void _setWarning(const QString &text, const QString &detail = QString());
    /// Every bound action as (label, key string). The single source of truth for
    /// both conflict detection and the quick reference, so they cannot disagree.
    QList<QPair<QString, QString>> _bindings() const;
    bool _keyIsConflicted(int key) const;
    Vehicle *_vehicle() const;

    OIKeyboardSettings *_settings = nullptr;
    QmlObjectListModel *_modeHotkeys = nullptr;
    QPointer<Vehicle> _activeVehicle;

    bool _canAct = false;
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

    QString _warningText;
    QString _warningDetail;
    QTimer _warningTimer;

    QStringList _keyConflicts;      ///< human-readable, for the settings page
    QList<int> _conflictedKeys;     ///< resolved key codes that are disabled

    /// Re-seeding the altitude target from the aircraft is only safe once it has
    /// settled; this tracks when the last step was commanded.
    QElapsedTimer _sinceLastAltitudeStep;
};
