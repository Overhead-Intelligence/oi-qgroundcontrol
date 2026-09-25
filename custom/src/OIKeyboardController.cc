/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#include "OIKeyboardController.h"
#include "OIKeyboardSettings.h"

#include "FlyViewSettings.h"
#include "Fact.h"
#include "GimbalController.h"
#include "MultiVehicleManager.h"
#include "QGCApplication.h"
#include "QGCLoggingCategory.h"
#include "QmlObjectListModel.h"
#include "SettingsManager.h"
#include "Vehicle.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QMap>
#include <QtCore/QSettings>
#include <QtGui/QKeyEvent>
#include <QtGui/QKeySequence>
#include <QtCore/QtMath>
#include <QtQml/QQmlEngine>

#include "MAVLinkLib.h"

QGC_LOGGING_CATEGORY(OIKeyboardLog, "OI.Keyboard")

namespace {

constexpr const char *kHotkeySettingsArray = "OI/KeyboardModeHotkeys";
constexpr const char *kGuidedModeName = "Guided";

/// ArduPlane accepts GUIDED_CHANGE_HEADING as a COMMAND_INT; QGC's ordinary
/// sendMavCommand sends a COMMAND_LONG, which ArduPilot converts with param1..4
/// carried through unchanged, so the long form is enough for this command.
constexpr int kCmdGuidedChangeHeading = 43002;
constexpr int kHeadingTypeHeading = 1;

/// The gimbal modes the operator cycles through. Kept deliberately short: these
/// are the ones GimbalController exposes directly.
const char *kGimbalModeNames[] = { "Follow", "Lock", "Retract", "Neutral" };
constexpr int kGimbalModeCount = 4;

double wrap360(double deg)
{
    while (deg < 0.0) {
        deg += 360.0;
    }
    while (deg >= 360.0) {
        deg -= 360.0;
    }
    return deg;
}

} // namespace

/*===========================================================================*/

OIModeHotkey::OIModeHotkey(const QString &key, const QString &mode, QObject *parent)
    : QObject(parent)
    , _key(key)
    , _mode(mode)
{
}

void OIModeHotkey::setKey(const QString &key)
{
    if (key != _key) {
        _key = key;
        emit changed();
    }
}

void OIModeHotkey::setMode(const QString &mode)
{
    if (mode != _mode) {
        _mode = mode;
        emit changed();
    }
}

/*===========================================================================*/

OIKeyboardController::OIKeyboardController(QObject *parent)
    : QObject(parent)
    , _settings(new OIKeyboardSettings(this))
    , _modeHotkeys(new QmlObjectListModel(this))
{
    _confirmTimer.setSingleShot(true);
    (void) connect(&_confirmTimer, &QTimer::timeout, this, &OIKeyboardController::_confirmTimeout);

    _loadModeHotkeys();
    _rebuildKeyConflicts();

    _warningTimer.setSingleShot(true);
    _warningTimer.setInterval(6000);
    (void) connect(&_warningTimer, &QTimer::timeout, this, [this]() {
        _warningText.clear();
        emit warningChanged();
    });

    // Any binding change can create or clear a conflict.
    for (Fact *const fact : { _settings->headingLeftKey(), _settings->headingRightKey(),
                              _settings->altitudeUpKey(), _settings->altitudeDownKey(),
                              _settings->gimbalPitchUpKey(), _settings->gimbalPitchDownKey(),
                              _settings->gimbalYawLeftKey(), _settings->gimbalYawRightKey(),
                              _settings->gimbalNextModeKey(), _settings->gimbalPrevModeKey() }) {
        (void) connect(fact, &Fact::rawValueChanged, this, &OIKeyboardController::_rebuildKeyConflicts);
    }

    (void) connect(_settings->enabled(), &Fact::rawValueChanged, this, [this]() {
        _clearPendingMode();
        emit enabledChanged();
        _recomputeState();
    });

    MultiVehicleManager *const mvm = MultiVehicleManager::instance();
    (void) connect(mvm, &MultiVehicleManager::activeVehicleChanged,
                   this, &OIKeyboardController::_activeVehicleChanged);
    _activeVehicleChanged(mvm->activeVehicle());

    // The filter sees every key in the application, so it must be cheap and must
    // refuse to act unless armed. Installed on the application rather than a QML
    // item so the operator does not have to keep a particular panel focused.
    qgcApp()->installEventFilter(this);
}

OIKeyboardController::~OIKeyboardController()
{
}

/*===========================================================================*/

Vehicle *OIKeyboardController::_vehicle() const
{
    return _activeVehicle.data();
}

void OIKeyboardController::_activeVehicleChanged(Vehicle *vehicle)
{
    if (_activeVehicle) {
        (void) disconnect(_activeVehicle, nullptr, this, nullptr);
    }

    // A different aircraft does not change what the operator asked for, but its
    // state is unknown, so the tracked targets are dropped and re-seeded.
    _clearPendingMode();

    _activeVehicle = vehicle;
    if (vehicle) {
        (void) connect(vehicle, &Vehicle::flightModeChanged, this, &OIKeyboardController::_recomputeState);
        (void) connect(vehicle, &Vehicle::armedChanged, this, &OIKeyboardController::_recomputeState);
        (void) connect(vehicle, &Vehicle::flyingChanged, this, &OIKeyboardController::_recomputeState);
        (void) connect(vehicle, &Vehicle::vtolInFwdFlightChanged, this, &OIKeyboardController::_recomputeState);
    }

    _altitudeTargetValid = false;
    _headingTargetValid = false;
    _recomputeState();
}

void OIKeyboardController::_recomputeState()
{
    Vehicle *const vehicle = _vehicle();
    const bool wasCanAct = _canAct;

    QString status;
    bool canAct = false;
    bool headingUsable = false;

    if (!enabled()) {
        status = tr("Keyboard control is off");
    } else if (!vehicle) {
        status = tr("No vehicle connected");
    } else if (!vehicle->armed()) {
        status = tr("Vehicle is disarmed");
    } else if (!vehicle->flying()) {
        status = tr("Vehicle is not flying");
    } else if (vehicle->flightMode() != QString::fromLatin1(kGuidedModeName)) {
        // Deliberately does not offer to switch: changing mode is the operator's call.
        status = tr("Flight mode is %1, not Guided").arg(vehicle->flightMode());
    } else {
        canAct = true;
        // Heading only bites in forward flight. In a VTOL hover ArduPlane acks
        // GUIDED_CHANGE_HEADING and then never reads the target, so the control is
        // disabled here rather than letting the operator press a key that lies.
        headingUsable = vehicle->fixedWing() || (vehicle->vtol() && vehicle->vtolInFwdFlight());
        status = headingUsable ? tr("Ready")
                               : tr("Ready - heading keys need forward flight");
    }

    // Deliberately does NOT touch `enabled`. Leaving Guided means keys stop working,
    // not that the operator has withdrawn their preference.
    if (!canAct) {
        _clearPendingMode();
    }

    if ((canAct != _canAct) || (headingUsable != _headingUsable) || (status != _statusText)) {
        _canAct = canAct;
        _headingUsable = headingUsable;
        _statusText = status;
        emit stateChanged();
    }

    // Becoming actionable is the moment to re-seed: the aircraft may have moved a
    // long way since the last time keys worked.
    if (canAct && !wasCanAct) {
        _reseedAltitudeTarget();
        _headingTargetValid = false;
    }
}

bool OIKeyboardController::enabled() const
{
    return _settings->enabled()->rawValue().toBool();
}

void OIKeyboardController::setEnabled(bool enabled)
{
    // Writing the fact persists it and fires the connection made in the constructor,
    // which recomputes state and emits enabledChanged().
    _settings->enabled()->setRawValue(enabled);
}

void OIKeyboardController::_setWarning(const QString &text)
{
    // Always re-emit, even for the same text: a repeated press should visibly
    // re-trigger rather than look like nothing happened.
    _warningText = text;
    _warningTimer.start();
    emit warningChanged();
    qCDebug(OIKeyboardLog) << "blocked:" << text;
}

int OIKeyboardController::pendingSeconds() const
{
    return _confirmTimer.isActive() ? ((_confirmTimer.remainingTime() + 999) / 1000) : 0;
}

void OIKeyboardController::_setStatus(const QString &text)
{
    if (text != _statusText) {
        _statusText = text;
        emit stateChanged();
    }
}

/*===========================================================================*/

void OIKeyboardController::_reseedAltitudeTarget()
{
    Vehicle *const vehicle = _vehicle();
    if (!vehicle) {
        _altitudeTargetValid = false;
        return;
    }

    const double current = vehicle->altitudeRelative()->rawValue().toDouble();
    if (qIsNaN(current)) {
        _altitudeTargetValid = false;
        return;
    }

    _altitudeTarget = current;
    _altitudeTargetValid = true;
    _sinceLastAltitudeStep.invalidate();
    emit stateChanged();
}

bool OIKeyboardController::_matches(const QString &settingValue, int key) const
{
    if (settingValue.isEmpty()) {
        return false;
    }
    const QKeySequence sequence = QKeySequence::fromString(settingValue, QKeySequence::PortableText);
    if (sequence.isEmpty()) {
        return false;
    }
    return sequence[0].key() == key;
}

bool OIKeyboardController::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::ApplicationDeactivate ||
        event->type() == QEvent::WindowDeactivate) {
        // A confirmation must not survive the operator looking away; the feature
        // itself is untouched.
        _clearPendingMode();
        return QObject::eventFilter(watched, event);
    }

    if (event->type() != QEvent::KeyPress) {
        return QObject::eventFilter(watched, event);
    }

    QKeyEvent *const keyEvent = static_cast<QKeyEvent*>(event);

    // Esc cancels a pending flight mode confirmation. It deliberately does not turn
    // the feature off: that is a settings-page decision, not a keystroke.
    if (keyEvent->key() == Qt::Key_Escape) {
        if (!_pendingModeName.isEmpty()) {
            _clearPendingMode();
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

    if (!_canAct) {
        return QObject::eventFilter(watched, event);
    }

    // Never steal a key that is being typed into a field. QQuickItem text inputs
    // report themselves through this property; anything that accepts text input
    // gets the key instead of the aircraft.
    QObject *const focus = qgcApp()->focusObject();
    if (focus && focus->property("acceptableInput").isValid()) {
        return QObject::eventFilter(watched, event);
    }
    if (focus && focus->inherits("QQuickTextInput")) {
        return QObject::eventFilter(watched, event);
    }
    if (focus && focus->inherits("QQuickTextEdit")) {
        return QObject::eventFilter(watched, event);
    }

    // A modifier means the operator is driving a shortcut, not the aircraft.
    const Qt::KeyboardModifiers mods = keyEvent->modifiers();
    if (mods & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
        return QObject::eventFilter(watched, event);
    }

    if (_handleKey(keyEvent->key(), mods)) {
        return true;
    }

    return QObject::eventFilter(watched, event);
}

bool OIKeyboardController::_handleKey(int key, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers);

    if (!enabled()) {
        return false;
    }

    if (_keyIsConflicted(key)) {
        _setWarning(tr("That key is bound to more than one action. Both are disabled "
                       "until the conflict is resolved in Settings > Keyboard."));
        return true;
    }

    // A pending mode confirmation swallows its own key as the acceptance.
    if (!_pendingModeName.isEmpty()) {
        if (key == _pendingModeKey) {
            _sendPendingMode();
            return true;
        }
        // Any other bound key cancels rather than doing two things at once.
        _clearPendingMode();
    }

    // ---- Gimbal. No flight-state restriction at all: pointing a camera cannot move
    // the aircraft, and being able to check the gimbal on the ground is the point.
    if (_matches(_settings->gimbalPitchUpKey()->rawValue().toString(), key)) {
        _stepGimbalPitch(1);
        return true;
    }
    if (_matches(_settings->gimbalPitchDownKey()->rawValue().toString(), key)) {
        _stepGimbalPitch(-1);
        return true;
    }
    if (_matches(_settings->gimbalYawLeftKey()->rawValue().toString(), key)) {
        _stepGimbalYaw(-1);
        return true;
    }
    if (_matches(_settings->gimbalYawRightKey()->rawValue().toString(), key)) {
        _stepGimbalYaw(1);
        return true;
    }
    if (_matches(_settings->gimbalNextModeKey()->rawValue().toString(), key)) {
        _cycleGimbalMode(1);
        return true;
    }
    if (_matches(_settings->gimbalPrevModeKey()->rawValue().toString(), key)) {
        _cycleGimbalMode(-1);
        return true;
    }

    // ---- Flight mode hotkeys. Also unrestricted: the two-press confirmation is the
    // guard, and a pilot who deliberately confirms a mode change on the ground meant
    // it. Restricting these would only stop legitimate pre-flight use.
    if (_tryModeHotkey(key)) {
        return true;
    }

    // ---- Heading and altitude. These fly the aircraft, so they need Guided.
    const bool headingKey =
        _matches(_settings->headingLeftKey()->rawValue().toString(), key) ||
        _matches(_settings->headingRightKey()->rawValue().toString(), key);
    const bool altitudeKey =
        _matches(_settings->altitudeUpKey()->rawValue().toString(), key) ||
        _matches(_settings->altitudeDownKey()->rawValue().toString(), key);

    if (!headingKey && !altitudeKey) {
        return false;
    }

    if (!_canAct) {
        _setWarning(_statusText);
        return true;
    }

    if (headingKey) {
        _stepHeading(_matches(_settings->headingLeftKey()->rawValue().toString(), key) ? -1 : 1);
    } else {
        _stepAltitude(_matches(_settings->altitudeUpKey()->rawValue().toString(), key) ? 1 : -1);
    }
    return true;
}

/*===========================================================================*/

void OIKeyboardController::_stepHeading(int direction)
{
    Vehicle *const vehicle = _vehicle();
    if (!vehicle) {
        return;
    }
    if (!_headingUsable) {
        _setWarning(tr("Heading keys need forward flight. ArduPlane accepts the command "
                       "in a VTOL hover and then ignores it."));
        return;
    }

    // The command is an absolute heading, so the running target lives here. Seed it
    // from the aircraft the first time, then accumulate: seeding every press would
    // make a second press before the turn started do nothing.
    if (!_headingTargetValid) {
        _headingTarget = vehicle->heading()->rawValue().toDouble();
        _headingTargetValid = true;
    }

    const double step = _settings->headingStep()->rawValue().toDouble();
    _headingTarget = wrap360(_headingTarget + (direction * step));

    // param3 is an acceleration limit that ArduPlane turns into a bank limit and
    // then clamps to ROLL_LIMIT_DEG, so this cannot command more bank than the
    // airframe allows. a = g * tan(bank).
    const double bankLimitDeg = _settings->headingBankLimit()->rawValue().toDouble();
    const float accelLimit = static_cast<float>(9.80665 * qTan(qDegreesToRadians(bankLimitDeg)));

    vehicle->sendMavCommand(
        vehicle->defaultComponentId(),
        static_cast<MAV_CMD>(kCmdGuidedChangeHeading),
        false /* showError */,
        kHeadingTypeHeading,
        static_cast<float>(_headingTarget),
        accelLimit);

    qCDebug(OIKeyboardLog) << "heading ->" << _headingTarget << "bank limit" << bankLimitDeg;
    emit stateChanged();
}

void OIKeyboardController::_stepAltitude(int direction)
{
    Vehicle *const vehicle = _vehicle();
    if (!vehicle) {
        return;
    }

    const double current = vehicle->altitudeRelative()->rawValue().toDouble();
    if (qIsNaN(current)) {
        _setStatus(tr("Altitude not known"));
        return;
    }

    if (!_altitudeTargetValid) {
        _reseedAltitudeTarget();
        if (!_altitudeTargetValid) {
            return;
        }
    }

    FlyViewSettings *const flyView = SettingsManager::instance()->flyViewSettings();
    const double minAlt = flyView->guidedMinimumAltitude()->rawValue().toDouble();
    const double maxAlt = flyView->guidedMaximumAltitude()->rawValue().toDouble();
    const double step = _settings->altitudeStep()->rawValue().toDouble();
    const double lead = _settings->altitudeLead()->rawValue().toDouble();

    const double wanted = _altitudeTarget + (direction * step);

    // Two clamps, and they behave differently on purpose.
    //
    // The guided min/max is a boundary: a step that would cross it lands exactly on
    // it, so the operator can still reach the floor from 2 m above it. Only a press
    // that cannot move at all is refused.
    //
    // The lead is a rate-of-commitment guard, so it refuses the WHOLE step rather
    // than applying a sliver. The first flight showed why: pressing quickly produced
    // steps of 0.4 m, 0.3 m, 0.1 m with no explanation, which reads as the key being
    // broken. Refusing outright and saying why is honest and predictable.
    const double clampedToLimits = qBound(minAlt, wanted, maxAlt);

    if (qAbs(clampedToLimits - _altitudeTarget) < 0.01) {
        _setWarning(direction > 0
                        ? tr("Altitude target is at the Fly View maximum (%1 m). Cannot climb further.")
                              .arg(maxAlt, 0, 'f', 0)
                        : tr("Altitude target is at the Fly View minimum (%1 m). Cannot descend further.")
                              .arg(minAlt, 0, 'f', 0));
        return;
    }

    if ((clampedToLimits > current + lead) || (clampedToLimits < current - lead)) {
        _setWarning(tr("Altitude target is %1 m from the current altitude, the maximum lead. "
                       "Please wait for the aircraft to catch up.").arg(lead, 0, 'f', 0));
        return;
    }

    const double delta = clampedToLimits - _altitudeTarget;
    _altitudeTarget = clampedToLimits;
    _sinceLastAltitudeStep.start();

    // Uses the stock guided path so behaviour matches the altitude slider exactly.
    // Safe here only because _canAct has already established that the vehicle is
    // in Guided: guidedModeChangeAltitude() would otherwise switch it there itself.
    vehicle->guidedModeChangeAltitude(delta, false /* pauseVehicle */);

    qCDebug(OIKeyboardLog) << "altitude target ->" << _altitudeTarget << "delta" << delta;
    emit stateChanged();
}

/*===========================================================================*/

void OIKeyboardController::_stepGimbalPitch(int direction)
{
    Vehicle *const vehicle = _vehicle();
    if (!vehicle || !vehicle->gimbalController()) {
        _setWarning(tr("No vehicle with a gimbal is connected"));
        return;
    }

    const double step = _settings->gimbalPitchStep()->rawValue().toDouble();
    _gimbalPitch = qBound(-90.0, _gimbalPitch + (direction * step), 90.0);
    vehicle->gimbalController()->sendPitchBodyYaw(static_cast<float>(_gimbalPitch),
                                                 static_cast<float>(_gimbalYaw),
                                                 false /* showError */);
    qCDebug(OIKeyboardLog) << "gimbal pitch ->" << _gimbalPitch;
}

void OIKeyboardController::_stepGimbalYaw(int direction)
{
    Vehicle *const vehicle = _vehicle();
    if (!vehicle || !vehicle->gimbalController()) {
        _setWarning(tr("No vehicle with a gimbal is connected"));
        return;
    }

    const double step = _settings->gimbalYawStep()->rawValue().toDouble();
    _gimbalYaw = qBound(-180.0, _gimbalYaw + (direction * step), 180.0);
    vehicle->gimbalController()->sendPitchBodyYaw(static_cast<float>(_gimbalPitch),
                                                 static_cast<float>(_gimbalYaw),
                                                 false /* showError */);
    qCDebug(OIKeyboardLog) << "gimbal yaw ->" << _gimbalYaw;
}

void OIKeyboardController::_cycleGimbalMode(int direction)
{
    Vehicle *const vehicle = _vehicle();
    if (!vehicle || !vehicle->gimbalController()) {
        _setWarning(tr("No vehicle with a gimbal is connected"));
        return;
    }

    GimbalController *const gimbal = vehicle->gimbalController();
    _gimbalModeIndex = (_gimbalModeIndex + direction + kGimbalModeCount) % kGimbalModeCount;

    switch (_gimbalModeIndex) {
    case 0:                                     // Follow: yaw tracks the airframe
        gimbal->setGimbalRetract(false);
        gimbal->setGimbalYawLock(false);
        break;
    case 1:                                     // Lock: yaw holds an earth-frame heading
        gimbal->setGimbalRetract(false);
        gimbal->setGimbalYawLock(true);
        break;
    case 2:                                     // Retract
        gimbal->setGimbalRetract(true);
        break;
    case 3:                                     // Neutral: centred, stowed forward
        gimbal->setGimbalRetract(false);
        gimbal->centerGimbal();
        _gimbalPitch = 0.0;
        _gimbalYaw = 0.0;
        break;
    default:
        break;
    }

    _setStatus(tr("Gimbal mode: %1").arg(QString::fromLatin1(kGimbalModeNames[_gimbalModeIndex])));
    qCDebug(OIKeyboardLog) << "gimbal mode ->" << kGimbalModeNames[_gimbalModeIndex];
}

/*===========================================================================*/

bool OIKeyboardController::_tryModeHotkey(int key)
{
    if (!_settings->modeHotkeysEnabled()->rawValue().toBool()) {
        return false;
    }
    Vehicle *const vehicle = _vehicle();

    for (int i = 0; i < _modeHotkeys->count(); i++) {
        const OIModeHotkey *const hotkey = _modeHotkeys->value<OIModeHotkey*>(i);
        if (!hotkey || !_matches(hotkey->key(), key)) {
            continue;
        }
        if (!vehicle) {
            _setWarning(tr("No vehicle connected, cannot change flight mode"));
            return true;
        }
        if (!vehicle->flightModes().contains(hotkey->mode())) {
            _setWarning(tr("%1 is not a flight mode this vehicle offers").arg(hotkey->mode()));
            return true;
        }

        // First press only arms the change. A flight mode is too consequential to
        // hang on one keystroke that could have been meant for something else.
        _pendingModeName = hotkey->mode();
        _pendingModeKey = key;
        _confirmTimer.start(static_cast<int>(
            _settings->modeConfirmTimeout()->rawValue().toDouble() * 1000.0));
        emit pendingModeChanged();
        return true;
    }

    return false;
}

void OIKeyboardController::_sendPendingMode()
{
    Vehicle *const vehicle = _vehicle();
    const QString mode = _pendingModeName;
    _clearPendingMode();

    if (!vehicle || mode.isEmpty()) {
        return;
    }

    qCDebug(OIKeyboardLog) << "flight mode ->" << mode;
    vehicle->setFlightMode(mode);
    _setStatus(tr("Flight mode: %1").arg(mode));

    // Changing mode leaves Guided, which disarms the keyboard through
    // _recomputeState(). That is intended: the operator re-arms deliberately.
}

void OIKeyboardController::_clearPendingMode()
{
    _confirmTimer.stop();
    if (!_pendingModeName.isEmpty()) {
        _pendingModeName.clear();
        _pendingModeKey = 0;
        emit pendingModeChanged();
    }
}

void OIKeyboardController::_confirmTimeout()
{
    if (!_pendingModeName.isEmpty()) {
        qCDebug(OIKeyboardLog) << "flight mode confirmation timed out";
        _clearPendingMode();
    }
}

/*===========================================================================*/

QList<QPair<QString, QString>> OIKeyboardController::_bindings() const
{
    QList<QPair<QString, QString>> rows;
    const auto add = [&rows](const QString &label, Fact *fact) {
        const QString key = fact->rawValue().toString().trimmed();
        if (!key.isEmpty()) {
            rows.append(qMakePair(label, key));
        }
    };
    add(tr("Turn left"),            _settings->headingLeftKey());
    add(tr("Turn right"),           _settings->headingRightKey());
    add(tr("Climb"),                _settings->altitudeUpKey());
    add(tr("Descend"),              _settings->altitudeDownKey());
    add(tr("Gimbal up"),            _settings->gimbalPitchUpKey());
    add(tr("Gimbal down"),          _settings->gimbalPitchDownKey());
    add(tr("Gimbal left"),          _settings->gimbalYawLeftKey());
    add(tr("Gimbal right"),         _settings->gimbalYawRightKey());
    add(tr("Next gimbal mode"),     _settings->gimbalNextModeKey());
    add(tr("Previous gimbal mode"), _settings->gimbalPrevModeKey());

    for (int i = 0; i < _modeHotkeys->count(); i++) {
        const OIModeHotkey *const hotkey = _modeHotkeys->value<OIModeHotkey*>(i);
        if (hotkey && !hotkey->key().trimmed().isEmpty()) {
            rows.append(qMakePair(tr("Flight mode: %1").arg(hotkey->mode()), hotkey->key().trimmed()));
        }
    }
    return rows;
}

void OIKeyboardController::_rebuildKeyConflicts()
{
    // Group by resolved key code, not by the typed string: "a" and "A" are the
    // same key and must still be caught.
    QMap<int, QStringList> byKey;
    for (const auto &row : _bindings()) {
        const QKeySequence sequence = QKeySequence::fromString(row.second, QKeySequence::PortableText);
        if (sequence.isEmpty()) {
            continue;
        }
        byKey[sequence[0].key()].append(row.first);
    }

    QStringList conflicts;
    QList<int> conflicted;
    for (auto it = byKey.constBegin(); it != byKey.constEnd(); ++it) {
        if (it.value().count() > 1) {
            conflicted.append(it.key());
            conflicts.append(tr("%1 - all disabled until this is resolved")
                                 .arg(it.value().join(QStringLiteral(", "))));
        }
    }

    if ((conflicts != _keyConflicts) || (conflicted != _conflictedKeys)) {
        _keyConflicts = conflicts;
        _conflictedKeys = conflicted;
        emit keyConflictsChanged();
    }
}

bool OIKeyboardController::_keyIsConflicted(int key) const
{
    return _conflictedKeys.contains(key);
}

QVariantList OIKeyboardController::bindingList() const
{
    QVariantList rows;
    for (const auto &row : _bindings()) {
        const QKeySequence sequence = QKeySequence::fromString(row.second, QKeySequence::PortableText);
        QVariantMap entry;
        entry[QStringLiteral("action")] = row.first;
        entry[QStringLiteral("key")] = row.second;
        entry[QStringLiteral("conflict")] =
            !sequence.isEmpty() && _conflictedKeys.contains(sequence[0].key());
        rows.append(entry);
    }
    return rows;
}

QObject *OIKeyboardController::settingsObject() const
{
    return _settings;
}

QStringList OIKeyboardController::availableModes() const
{
    Vehicle *const vehicle = _vehicle();
    return vehicle ? vehicle->flightModes() : QStringList();
}

void OIKeyboardController::addModeHotkey(const QString &key, const QString &mode)
{
    OIModeHotkey *const hotkey = new OIModeHotkey(key, mode, this);
    QQmlEngine::setObjectOwnership(hotkey, QQmlEngine::CppOwnership);
    (void) _modeHotkeys->append(hotkey);
    saveModeHotkeys();
    _rebuildKeyConflicts();
}

void OIKeyboardController::removeModeHotkey(int index)
{
    if ((index < 0) || (index >= _modeHotkeys->count())) {
        return;
    }
    QObject *const hotkey = _modeHotkeys->removeAt(index);
    if (hotkey) {
        hotkey->deleteLater();
    }
    saveModeHotkeys();
    _rebuildKeyConflicts();
}

void OIKeyboardController::saveModeHotkeys()
{
    QSettings settings;
    // beginWriteArray writes the new size but leaves higher indices behind, so a
    // shrinking list would keep reading stale rows on the next start.
    settings.remove(QString::fromLatin1(kHotkeySettingsArray));
    settings.beginWriteArray(QString::fromLatin1(kHotkeySettingsArray));
    for (int i = 0; i < _modeHotkeys->count(); i++) {
        const OIModeHotkey *const hotkey = _modeHotkeys->value<OIModeHotkey*>(i);
        if (!hotkey) {
            continue;
        }
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("key"), hotkey->key());
        settings.setValue(QStringLiteral("mode"), hotkey->mode());
    }
    settings.endArray();
}

void OIKeyboardController::_loadModeHotkeys()
{
    QSettings settings;
    const int count = settings.beginReadArray(QString::fromLatin1(kHotkeySettingsArray));
    for (int i = 0; i < count; i++) {
        settings.setArrayIndex(i);
        const QString key = settings.value(QStringLiteral("key")).toString();
        const QString mode = settings.value(QStringLiteral("mode")).toString();
        if (key.isEmpty() || mode.isEmpty()) {
            continue;
        }
        OIModeHotkey *const hotkey = new OIModeHotkey(key, mode, this);
        QQmlEngine::setObjectOwnership(hotkey, QQmlEngine::CppOwnership);
        (void) _modeHotkeys->append(hotkey);
    }
    settings.endArray();
}
