/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#include "OIKeyboardController.h"

#include <cmath>

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QEvent>
#include <QtCore/QtMath>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>

#include "Fact.h"
#include "FlyViewSettings.h"
#include "Gimbal.h"
#include "GimbalController.h"
#include "MAVLinkLib.h"
#include "MultiVehicleManager.h"
#include "OIKeyboardSettings.h"
#include "QGCLoggingCategory.h"
#include "SettingsManager.h"
#include "Vehicle.h"

QGC_LOGGING_CATEGORY(OIKeyboardLog, "OI.Keyboard")

namespace {

// ArduPilot-dialect command and its HEADING_TYPE values, spelled out so this file
// does not depend on which MAVLink dialect headers are on the include path.
constexpr int kCmdGuidedChangeHeading = 43002;   // MAV_CMD_GUIDED_CHANGE_HEADING
constexpr float kHeadingTypeHeading = 1.0f;      // HEADING_TYPE_HEADING: compass heading
constexpr float kHeadingTypeDefault = 2.0f;      // HEADING_TYPE_DEFAULT: clear the hold

constexpr int kTurnTickMs = 200;
constexpr int kAltitudeRepeatMs = 600;
constexpr int kGimbalTickMs = 100;

constexpr float kGimbalPitchMin = -90.0f;
constexpr float kGimbalPitchMax = 90.0f;

constexpr qint64 kAltitudeErrorMaxAgeMs = 3000;   // NAV_CONTROLLER_OUTPUT normally arrives several times a second

constexpr double kGravity = 9.80665;

const QString kGuidedModeName = QStringLiteral("Guided");

} // namespace

/*===========================================================================*/

OIKeyboardController::OIKeyboardController(QObject *parent)
    : QObject(parent)
    , _settings(new OIKeyboardSettings(this))
{
    _turnTimer.setInterval(kTurnTickMs);
    _altitudeTimer.setInterval(kAltitudeRepeatMs);
    _gimbalTimer.setInterval(kGimbalTickMs);
    (void) connect(&_turnTimer, &QTimer::timeout, this, &OIKeyboardController::_turnTick);
    (void) connect(&_altitudeTimer, &QTimer::timeout, this, &OIKeyboardController::_altitudeTick);
    (void) connect(&_gimbalTimer, &QTimer::timeout, this, &OIKeyboardController::_gimbalTick);

    MultiVehicleManager *manager = MultiVehicleManager::instance();
    (void) connect(manager, &MultiVehicleManager::activeVehicleChanged, this, &OIKeyboardController::_activeVehicleChanged);
    _setVehicle(manager->activeVehicle());

    QCoreApplication::instance()->installEventFilter(this);
    _updateState();
}

OIKeyboardController::~OIKeyboardController()
{
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->removeEventFilter(this);
    }
}

QObject *OIKeyboardController::settingsObject() const
{
    return _settings;
}

/*===========================================================================*/

void OIKeyboardController::setEnabled(bool enabled)
{
    if (enabled == _enabled) {
        return;
    }

    _enabled = enabled;
    if (!_enabled) {
        _releaseAllKeys();
    }
    qCDebug(OIKeyboardLog) << "keyboard control" << (_enabled ? "ON" : "OFF");
    emit enabledChanged(_enabled);
    _updateState();
}

QString OIKeyboardController::legendText() const
{
    const QString altitude = _settings->altitudeStep()->cookedValueString() + QLatin1Char(' ') + _settings->altitudeStep()->cookedUnits();
    const QString rate = _settings->turnRate()->cookedValueString();
    return tr("W/S alt %1  |  A/D turn %2 deg/s  |  arrows gimbal  |  Esc off").arg(altitude, rate);
}

/*===========================================================================*/

void OIKeyboardController::_activeVehicleChanged(Vehicle *vehicle)
{
    _setVehicle(vehicle);
}

void OIKeyboardController::_setVehicle(Vehicle *vehicle)
{
    if (vehicle == _vehicle) {
        return;
    }

    _releaseAllKeys();
    _headingHoldActive = false;
    _setVehicleAltitudeUnknown();

    if (_vehicle) {
        (void) disconnect(_vehicle.data(), nullptr, this, nullptr);
    }

    _vehicle = vehicle;

    if (_vehicle) {
        (void) connect(_vehicle.data(), &Vehicle::flightModeChanged, this, &OIKeyboardController::_flightModeChanged);
        (void) connect(_vehicle.data(), &Vehicle::mavCommandResult, this, &OIKeyboardController::_mavCommandResult);
    }

    // A new (or no) vehicle always starts with keyboard control off.
    if (_enabled) {
        _enabled = false;
        emit enabledChanged(false);
    }
    _updateState();
}

void OIKeyboardController::_flightModeChanged(const QString &flightMode)
{
    if (flightMode != kGuidedModeName) {
        // Leaving Guided clears ArduPlane's heading hold and makes our timers pointless.
        _releaseAllKeys();
        _headingHoldActive = false;
    }
    _updateState();
}

void OIKeyboardController::_mavCommandResult(int vehicleId, int targetComponent, int command, int ackResult, int failureCode)
{
    Q_UNUSED(vehicleId);
    Q_UNUSED(targetComponent);

    if (command != kCmdGuidedChangeHeading) {
        return;
    }

    // Our own rapid resend while a previous command is still waiting for its ack.
    if (failureCode == Vehicle::MavCmdResultFailureDuplicateCommand) {
        return;
    }

    if (ackResult != MAV_RESULT_ACCEPTED) {
        qCWarning(OIKeyboardLog) << "GUIDED_CHANGE_HEADING rejected, result" << ackResult << "failure code" << failureCode;
        _stopTurn();
        _headingHoldActive = false;
        _setStatus(tr("Heading command rejected (is the aircraft in GUIDED?)"), true);
    }
}

bool OIKeyboardController::_vehicleIsGuidedPlane() const
{
    return _vehicle && (_vehicle->fixedWing() || _vehicle->vtol()) && (_vehicle->flightMode() == kGuidedModeName);
}

/*===========================================================================*/

bool OIKeyboardController::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);

    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease: {
        if (!_enabled) {
            return false;
        }
        auto *keyEvent = static_cast<QKeyEvent*>(event);
        if (_textInputHasFocus()) {
            return false;
        }
        if (keyEvent->isAutoRepeat()) {
            // Holding a key is tracked with our own timers; swallow the OS repeat for our keys.
            return _isTrackedKey(keyEvent->key());
        }
        return (event->type() == QEvent::KeyPress) ? _handleKeyPress(keyEvent->key()) : _handleKeyRelease(keyEvent->key());
    }
    case QEvent::ApplicationStateChange:
        if (QGuiApplication::applicationState() != Qt::ApplicationActive) {
            _releaseAllKeys();
        }
        return false;
    case QEvent::WindowDeactivate:
        _releaseAllKeys();
        return false;
    default:
        return false;
    }
}

bool OIKeyboardController::_isTrackedKey(int key)
{
    switch (key) {
    case Qt::Key_W: case Qt::Key_S: case Qt::Key_A: case Qt::Key_D:
    case Qt::Key_Up: case Qt::Key_Down: case Qt::Key_Left: case Qt::Key_Right:
        return true;
    default:
        return false;
    }
}

bool OIKeyboardController::_handleKeyPress(int key)
{
    switch (key) {
    case Qt::Key_Escape:
        setEnabled(false);
        return true;

    case Qt::Key_W:
    case Qt::Key_S:
        _altitudeDirection = (key == Qt::Key_W) ? 1 : -1;
        _bumpAltitude();
        _altitudeTimer.start();
        return true;

    case Qt::Key_A:
    case Qt::Key_D:
        _startTurn((key == Qt::Key_D) ? 1 : -1);
        return true;

    case Qt::Key_Up:
    case Qt::Key_Down:
        _startGimbal((key == Qt::Key_Up) ? 1 : -1, _gimbalYawDirection);
        return true;

    case Qt::Key_Left:
    case Qt::Key_Right:
        _startGimbal(_gimbalPitchDirection, (key == Qt::Key_Right) ? 1 : -1);
        return true;

    default:
        return false;
    }
}

bool OIKeyboardController::_handleKeyRelease(int key)
{
    switch (key) {
    case Qt::Key_W:
    case Qt::Key_S:
        _altitudeTimer.stop();
        _altitudeDirection = 0;
        return true;

    case Qt::Key_A:
    case Qt::Key_D:
        if (_turnDirection == ((key == Qt::Key_D) ? 1 : -1)) {
            _stopTurn();
        }
        return true;

    case Qt::Key_Up:
    case Qt::Key_Down:
        _startGimbal(0, _gimbalYawDirection);
        return true;

    case Qt::Key_Left:
    case Qt::Key_Right:
        _startGimbal(_gimbalPitchDirection, 0);
        return true;

    default:
        return false;
    }
}

void OIKeyboardController::_releaseAllKeys()
{
    _altitudeTimer.stop();
    _altitudeDirection = 0;
    _stopTurn();
    _stopGimbal();
}

bool OIKeyboardController::_textInputHasFocus()
{
    const QObject *focus = QGuiApplication::focusObject();
    return focus && (focus->inherits("QQuickTextInput") || focus->inherits("QQuickTextEdit"));
}

double OIKeyboardController::_wrap360(double degrees)
{
    double wrapped = std::fmod(degrees, 360.0);
    if (wrapped < 0.0) {
        wrapped += 360.0;
    }
    return wrapped;
}

/*===========================================================================*/

void OIKeyboardController::_startTurn(int direction)
{
    if (!_vehicleIsGuidedPlane()) {
        _updateState();
        return;
    }

    if (_turnDirection == 0 && !_headingHoldActive) {
        // Start slewing from where the nose is now; while a hold is active keep slewing the held target.
        _headingTarget = _wrap360(_vehicle->heading()->rawValue().toDouble());
    }

    _turnDirection = direction;
    _sendHeading();
    _turnTimer.start();
    _updateState();
}

void OIKeyboardController::_stopTurn()
{
    _turnTimer.stop();
    if (_turnDirection != 0) {
        _turnDirection = 0;
        _updateState();
    }
}

void OIKeyboardController::_turnTick()
{
    if (!_vehicleIsGuidedPlane() || _turnDirection == 0) {
        _stopTurn();
        return;
    }

    const double rate = _settings->turnRate()->rawValue().toDouble();
    _headingTarget = _wrap360(_headingTarget + (_turnDirection * rate * kTurnTickMs / 1000.0));
    _sendHeading();
    _updateState();
}

void OIKeyboardController::_sendHeading()
{
    if (!_vehicle) {
        return;
    }

    // ArduPlane turns param3 (m/s^2) into a bank ceiling: bank = atan(param3 / g).
    const double bankLimitDeg = _settings->turnBankLimit()->rawValue().toDouble();
    const float accelLimit = static_cast<float>(kGravity * qTan(qDegreesToRadians(bankLimitDeg)));

    _vehicle->sendMavCommand(
        _vehicle->defaultComponentId(),
        static_cast<MAV_CMD>(kCmdGuidedChangeHeading),
        false,                                      // showError: results are handled in _mavCommandResult
        kHeadingTypeHeading,                        // param1: heading type
        static_cast<float>(_headingTarget),         // param2: target heading (deg, 0..360)
        accelLimit);                                // param3: acceleration limit -> bank ceiling
    _headingHoldActive = true;
}

void OIKeyboardController::releaseHeadingHold()
{
    _stopTurn();
    _headingHoldActive = false;

    if (_vehicle && _vehicleIsGuidedPlane()) {
        _vehicle->sendMavCommand(
            _vehicle->defaultComponentId(),
            static_cast<MAV_CMD>(kCmdGuidedChangeHeading),
            false,
            kHeadingTypeDefault,                    // clears the guided heading hold
            0.0f,
            0.0f);
        qCDebug(OIKeyboardLog) << "heading hold released";
    }
    _updateState();
}

/*===========================================================================*/

void OIKeyboardController::_altitudeTick()
{
    if (_altitudeDirection == 0) {
        _altitudeTimer.stop();
        return;
    }
    _bumpAltitude();
}

void OIKeyboardController::setAltitudeError(Vehicle *vehicle, double altitudeErrorMeters)
{
    if (!vehicle || vehicle != _vehicle || qIsNaN(altitudeErrorMeters)) {
        return;
    }
    _altitudeError = altitudeErrorMeters;
    _altitudeErrorMs = QDateTime::currentMSecsSinceEpoch();
    if (_enabled) {
        emit stateChanged();    // refreshes altitudeTargetText in the panel
    }
}

bool OIKeyboardController::_altitudeTargetValid() const
{
    return _vehicle && (_altitudeErrorMs != 0)
        && ((QDateTime::currentMSecsSinceEpoch() - _altitudeErrorMs) < kAltitudeErrorMaxAgeMs)
        && !qIsNaN(_vehicle->altitudeRelative()->rawValue().toDouble());
}

double OIKeyboardController::_altitudeTargetRelative() const
{
    // The autopilot's target expressed above home: differences are frame free, so this holds
    // whether the guided target is relative, AMSL or terrain based.
    return _vehicle->altitudeRelative()->rawValue().toDouble() + _altitudeError;
}

QString OIKeyboardController::altitudeTargetText() const
{
    if (!_altitudeTargetValid()) {
        return QString();
    }
    return tr("target %1 m").arg(_altitudeTargetRelative(), 0, 'f', 0);
}

void OIKeyboardController::_bumpAltitude()
{
    if (!_vehicleIsGuidedPlane() || _altitudeDirection == 0) {
        _updateState();
        return;
    }

    const double current = _vehicle->altitudeRelative()->rawValue().toDouble();
    if (qIsNaN(current)) {
        _setStatus(tr("Altitude unknown"), true);
        return;
    }

    FlyViewSettings *flyView = SettingsManager::instance()->flyViewSettings();
    const double minAlt = flyView->guidedMinimumAltitude()->rawValue().toDouble();
    const double maxAlt = flyView->guidedMaximumAltitude()->rawValue().toDouble();
    const double step = _settings->altitudeStep()->rawValue().toDouble();

    // ArduPlane applies the offset to its current guided target, so the clamp is done on that
    // target when the autopilot has reported it; otherwise on the current altitude.
    const double base = _altitudeTargetValid() ? _altitudeTargetRelative() : current;
    const double target = qBound(minAlt, base + (_altitudeDirection * step), maxAlt);
    const double delta = target - base;
    if (qAbs(delta) < 0.5) {
        _setStatus(tr("At the guided altitude limit (%1 .. %2 m)").arg(minAlt, 0, 'f', 0).arg(maxAlt, 0, 'f', 0), true);
        return;
    }

    // Relative offset only: no absolute altitude and no frame are sent, so the guided point keeps
    // whatever altitude frame it already has (relative, AMSL or terrain).
    _vehicle->guidedModeChangeAltitude(delta, false /* pauseVehicle */);
    _setStatus(tr("Altitude %1%2 m, target %3 m").arg(delta > 0 ? QStringLiteral("+") : QString()).arg(delta, 0, 'f', 0).arg(target, 0, 'f', 0));
}

/*===========================================================================*/

void OIKeyboardController::_startGimbal(int pitchDirection, int yawDirection)
{
    _gimbalPitchDirection = pitchDirection;
    _gimbalYawDirection = yawDirection;

    if (pitchDirection == 0 && yawDirection == 0) {
        _stopGimbal();
        return;
    }

    if (!_vehicle) {
        return;
    }

    GimbalController *controller = _vehicle->gimbalController();
    Gimbal *gimbal = controller ? controller->activeGimbal() : nullptr;
    if (!gimbal) {
        _setStatus(tr("No gimbal on this vehicle"), true);
        _stopGimbal();
        return;
    }

    if (!_gimbalTimer.isActive()) {
        _gimbalPitch = gimbal->absolutePitch()->rawValue().toFloat();
        _gimbalYaw = gimbal->bodyYaw()->rawValue().toFloat();
        if (qIsNaN(_gimbalPitch)) { _gimbalPitch = 0.0f; }
        if (qIsNaN(_gimbalYaw)) { _gimbalYaw = 0.0f; }
        _gimbalTimer.start();
        _gimbalTick();
    }
}

void OIKeyboardController::_stopGimbal()
{
    _gimbalTimer.stop();
    _gimbalPitchDirection = 0;
    _gimbalYawDirection = 0;
}

void OIKeyboardController::_gimbalTick()
{
    if (!_vehicle || (_gimbalPitchDirection == 0 && _gimbalYawDirection == 0)) {
        _stopGimbal();
        return;
    }

    GimbalController *controller = _vehicle->gimbalController();
    if (!controller || !controller->activeGimbal()) {
        _stopGimbal();
        return;
    }

    const float rate = _settings->gimbalRate()->rawValue().toFloat();
    const float stepDeg = rate * kGimbalTickMs / 1000.0f;

    _gimbalPitch = qBound(kGimbalPitchMin, _gimbalPitch + (_gimbalPitchDirection * stepDeg), kGimbalPitchMax);
    _gimbalYaw = static_cast<float>(_wrap360(_gimbalYaw + (_gimbalYawDirection * stepDeg)));
    if (_gimbalYaw > 180.0f) {
        _gimbalYaw -= 360.0f;    // body yaw is reported and commanded as -180..180
    }

    controller->sendPitchBodyYaw(_gimbalPitch, _gimbalYaw, false /* showError */);
    _setStatus(tr("Gimbal tilt %1 deg, pan %2 deg").arg(_gimbalPitch, 0, 'f', 0).arg(_gimbalYaw, 0, 'f', 0));
}

/*===========================================================================*/

void OIKeyboardController::_setStatus(const QString &text, bool warning)
{
    _statusText = text;
    _warning = warning;
    emit stateChanged();
}

void OIKeyboardController::_updateState()
{
    _ready = _vehicleIsGuidedPlane();

    if (!_enabled) {
        if (_headingHoldActive) {
            _setStatus(tr("Holding heading %1 deg (keyboard off)").arg(_headingTarget, 0, 'f', 0));
        } else {
            _setStatus(QString());
        }
        return;
    }

    if (!_vehicle) {
        _setStatus(tr("No vehicle"), true);
    } else if (!(_vehicle->fixedWing() || _vehicle->vtol())) {
        _setStatus(tr("ArduPlane only"), true);
    } else if (_vehicle->flightMode() != kGuidedModeName) {
        _setStatus(tr("Switch to GUIDED"), true);
    } else if (_turnDirection != 0) {
        _setStatus(tr("Turning %1 to %2 deg").arg(_turnDirection > 0 ? tr("right") : tr("left")).arg(_headingTarget, 0, 'f', 0));
    } else if (_headingHoldActive) {
        _setStatus(tr("Holding heading %1 deg").arg(_headingTarget, 0, 'f', 0));
    } else {
        _setStatus(tr("Ready"));
    }
}

/*===========================================================================*/

void OIKeyboardController::_setVehicleAltitudeUnknown()
{
    _altitudeErrorMs = 0;
    _altitudeError = 0.0;
}
