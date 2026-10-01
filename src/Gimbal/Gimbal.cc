#include "Gimbal.h"
#include "GimbalController.h"
#include "QGCLoggingCategory.h"

#include <QtCore/QtMath>

#include <cmath>

QGC_LOGGING_CATEGORY(GimbalLog, "Gimbal.Gimbal")

Gimbal::Gimbal(GimbalController *parent)
    : FactGroup(1000, QStringLiteral(":/json/Vehicle/GimbalFact.json"), parent)
{
    qCDebug(GimbalLog) << this;

    _initFacts();
}

Gimbal::Gimbal(const Gimbal &other)
    : FactGroup(1000, QStringLiteral(":/json/Vehicle/GimbalFact.json"), other.parent())
{
    _initFacts();
    *this = other;
}

Gimbal::~Gimbal()
{
    // qCDebug(GimbalLog) << Q_FUNC_INFO << this;
}

const Gimbal &Gimbal::operator=(const Gimbal &other)
{
    _requestInformationRetries = other._requestInformationRetries;
    _requestStatusRetries = other._requestStatusRetries;
    _requestAttitudeRetries = other._requestAttitudeRetries;
    _receivedGimbalManagerInformation = other._receivedGimbalManagerInformation;
    _receivedGimbalManagerStatus = other._receivedGimbalManagerStatus;
    _receivedGimbalDeviceAttitudeStatus = other._receivedGimbalDeviceAttitudeStatus;
    _isComplete = other._isComplete;
    _retracted = other._retracted;
    _neutral = other._neutral;
    _haveControl = other._haveControl;
    _othersHaveControl = other._othersHaveControl;
    _absoluteRollFact = other._absoluteRollFact;
    _absolutePitchFact = other._absolutePitchFact;
    _bodyYawFact = other._bodyYawFact;
    _absoluteYawFact = other._absoluteYawFact;
    _deviceIdFact = other._deviceIdFact;
    _yawLock = other._yawLock;
    _haveControl = other._haveControl;
    _othersHaveControl = other._othersHaveControl;

    return *this;
}

void Gimbal::_initFacts()
{
    _addFact(&_absoluteRollFact);
    _addFact(&_absolutePitchFact);
    _addFact(&_bodyYawFact);
    _addFact(&_absoluteYawFact);
    _addFact(&_deviceIdFact);
    _addFact(&_managerCompidFact);

    _absoluteRollFact.setRawValue(0.0f);
    _absolutePitchFact.setRawValue(0.0f);
    _bodyYawFact.setRawValue(0.0f);
    _absoluteYawFact.setRawValue(0.0f);
    _deviceIdFact.setRawValue(0);
    _managerCompidFact.setRawValue(0);
}

void Gimbal::setAngleLimits(float pitchMinRad, float pitchMaxRad, float yawMinRad, float yawMaxRad)
{
    // These fields are optional in practice: gimbals that do not publish travel limits send
    // zeros, and some send NaN. Accepting those would clamp every commanded angle to zero,
    // which is worse than having no limits at all - so a range has to be finite and
    // non-empty before it counts as known.
    const auto usable = [](float minRad, float maxRad) {
        return std::isfinite(minRad) && std::isfinite(maxRad) && (maxRad > minRad);
    };

    const bool known = usable(pitchMinRad, pitchMaxRad) && usable(yawMinRad, yawMaxRad);
    const float pitchMin = known ? qRadiansToDegrees(pitchMinRad) : 0.f;
    const float pitchMax = known ? qRadiansToDegrees(pitchMaxRad) : 0.f;
    const float yawMin = known ? qRadiansToDegrees(yawMinRad) : 0.f;
    const float yawMax = known ? qRadiansToDegrees(yawMaxRad) : 0.f;

    if ((known != _angleLimitsKnown) || (pitchMin != _pitchMin) || (pitchMax != _pitchMax) ||
        (yawMin != _yawMin) || (yawMax != _yawMax)) {
        _angleLimitsKnown = known;
        _pitchMin = pitchMin;
        _pitchMax = pitchMax;
        _yawMin = yawMin;
        _yawMax = yawMax;
        qCDebug(GimbalLog) << "angle limits" << (known ? "known" : "not reported")
                           << "pitch" << _pitchMin << _pitchMax << "yaw" << _yawMin << _yawMax;
        emit angleLimitsChanged();
    }
}

void Gimbal::setCapabilityFlags(uint32_t flags)
{
    if (_capabilityFlags != flags) {
        _capabilityFlags = flags;
        emit capabilityFlagsChanged();
    }
}
