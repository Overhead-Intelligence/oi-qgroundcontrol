/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * Keyboard guided control. An application-wide event filter turns held keys
 * into discrete GUIDED-mode commands for ArduPlane:
 *
 *   W / S        bump the target altitude by the altitude step (repeats while held)
 *   A / D        slew the target heading at the turn rate while held; the
 *                aircraft holds the final heading (ArduPlane guided heading hold)
 *   Arrow keys   pan / tilt the gimbal at the gimbal rate while held
 *   Esc          turn keyboard control off
 *
 * Safety contract (also in CLAUDE.md):
 *   - off at start, off when the active vehicle changes, off on Esc;
 *   - only acts on an ArduPlane vehicle whose flight mode is Guided and never
 *     changes the flight mode itself;
 *   - keys are ignored while a text field has focus;
 *   - losing window focus releases every held key.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QTimer>

class QEvent;
class Vehicle;
class OIKeyboardSettings;

Q_DECLARE_LOGGING_CATEGORY(OIKeyboardLog)

class OIKeyboardController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool     enabled             READ enabled            WRITE setEnabled    NOTIFY enabledChanged)
    Q_PROPERTY(bool     ready               READ ready                                  NOTIFY stateChanged)    ///< vehicle is an ArduPlane in Guided
    Q_PROPERTY(bool     warning             READ warning                                NOTIFY stateChanged)    ///< statusText describes why keys do nothing
    Q_PROPERTY(QString  statusText          READ statusText                             NOTIFY stateChanged)
    Q_PROPERTY(QString  legendText          READ legendText                             NOTIFY stateChanged)
    Q_PROPERTY(bool     headingHoldActive   READ headingHoldActive                      NOTIFY stateChanged)
    Q_PROPERTY(double   headingTarget       READ headingTarget                          NOTIFY stateChanged)
    Q_PROPERTY(int      turnDirection       READ turnDirection                          NOTIFY stateChanged)    ///< -1 left, 0 none, +1 right
    Q_PROPERTY(QObject* settings            READ settingsObject                         CONSTANT)

public:
    explicit OIKeyboardController(QObject *parent = nullptr);
    ~OIKeyboardController() override;

    bool enabled() const { return _enabled; }
    void setEnabled(bool enabled);

    bool ready() const { return _ready; }
    bool warning() const { return _warning; }
    QString statusText() const { return _statusText; }
    QString legendText() const;
    bool headingHoldActive() const { return _headingHoldActive; }
    double headingTarget() const { return _headingTarget; }
    int turnDirection() const { return _turnDirection; }
    OIKeyboardSettings *settings() const { return _settings; }
    QObject *settingsObject() const;

    /// Sends HEADING_TYPE_DEFAULT so ArduPlane drops the heading hold and resumes the guided loiter.
    Q_INVOKABLE void releaseHeadingHold();

    bool eventFilter(QObject *watched, QEvent *event) final;

signals:
    void enabledChanged(bool enabled);
    void stateChanged();

private slots:
    void _activeVehicleChanged(Vehicle *vehicle);
    void _flightModeChanged(const QString &flightMode);
    void _mavCommandResult(int vehicleId, int targetComponent, int command, int ackResult, int failureCode);
    void _turnTick();
    void _altitudeTick();
    void _gimbalTick();

private:
    bool _handleKeyPress(int key);
    bool _handleKeyRelease(int key);
    void _releaseAllKeys();
    void _setVehicle(Vehicle *vehicle);
    bool _vehicleIsGuidedPlane() const;
    void _updateState();
    void _setStatus(const QString &text, bool warning = false);

    void _startTurn(int direction);
    void _stopTurn();
    void _sendHeading();
    void _bumpAltitude();
    void _startGimbal(int pitchDirection, int yawDirection);
    void _stopGimbal();

    static bool _isTrackedKey(int key);
    static bool _textInputHasFocus();
    static double _wrap360(double degrees);

    OIKeyboardSettings *_settings = nullptr;
    QPointer<Vehicle> _vehicle;

    bool _enabled = false;
    bool _ready = false;
    bool _warning = false;
    QString _statusText;

    // Turn (A / D)
    QTimer _turnTimer;
    int _turnDirection = 0;
    double _headingTarget = 0.0;
    bool _headingHoldActive = false;

    // Altitude (W / S)
    QTimer _altitudeTimer;
    int _altitudeDirection = 0;

    // Gimbal (arrow keys)
    QTimer _gimbalTimer;
    int _gimbalPitchDirection = 0;
    int _gimbalYawDirection = 0;
    float _gimbalPitch = 0.0f;
    float _gimbalYaw = 0.0f;
};
