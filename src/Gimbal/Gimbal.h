#pragma once

#include "FactGroup.h"
#include "MAVLinkEnums.h"

class GimbalController;

class Gimbal : public FactGroup
{
    Q_OBJECT
    Q_PROPERTY(Fact     *absoluteRoll           READ absoluteRoll               CONSTANT)
    Q_PROPERTY(Fact     *absolutePitch          READ absolutePitch              CONSTANT)
    Q_PROPERTY(Fact     *bodyYaw                READ bodyYaw                    CONSTANT)
    Q_PROPERTY(Fact     *absoluteYaw            READ absoluteYaw                CONSTANT)
    Q_PROPERTY(Fact     *deviceId               READ deviceId                   CONSTANT)
    Q_PROPERTY(Fact     *managerCompid          READ managerCompid              CONSTANT)
    Q_PROPERTY(float    pitchRate               READ pitchRate                  NOTIFY pitchRateChanged)
    Q_PROPERTY(float    yawRate                 READ yawRate                    NOTIFY yawRateChanged)
    Q_PROPERTY(bool     yawLock                 READ yawLock                    NOTIFY yawLockChanged)
    Q_PROPERTY(bool     retracted               READ retracted                  NOTIFY retractedChanged)
    Q_PROPERTY(bool     gimbalHaveControl       READ gimbalHaveControl          NOTIFY gimbalHaveControlChanged)
    Q_PROPERTY(bool     gimbalOthersHaveControl READ gimbalOthersHaveControl    NOTIFY gimbalOthersHaveControlChanged)
    Q_PROPERTY(bool     supportsRetract         READ supportsRetract            NOTIFY capabilityFlagsChanged)
    Q_PROPERTY(bool     supportsYawLock         READ supportsYawLock            NOTIFY capabilityFlagsChanged)
    /// Hardware travel limits in degrees, as reported in GIMBAL_MANAGER_INFORMATION.
    /// Only meaningful when angleLimitsKnown is true - the fields are optional in
    /// practice and plenty of gimbals send zeros or NaN for them.
    Q_PROPERTY(bool     angleLimitsKnown        READ angleLimitsKnown           NOTIFY angleLimitsChanged)
    Q_PROPERTY(float    pitchMin                READ pitchMin                   NOTIFY angleLimitsChanged)
    Q_PROPERTY(float    pitchMax                READ pitchMax                   NOTIFY angleLimitsChanged)
    Q_PROPERTY(float    yawMin                  READ yawMin                     NOTIFY angleLimitsChanged)
    Q_PROPERTY(float    yawMax                  READ yawMax                     NOTIFY angleLimitsChanged)

    friend class GimbalController;

public:
    Gimbal(GimbalController *parent);
    Gimbal(const Gimbal &other);
    const Gimbal &operator=(const Gimbal &other);
    ~Gimbal();

    Fact *absoluteRoll() { return &_absoluteRollFact; }
    Fact *absolutePitch() { return &_absolutePitchFact; }
    Fact *bodyYaw() { return &_bodyYawFact; }
    Fact *absoluteYaw() { return &_absoluteYawFact; }
    Fact *deviceId() { return &_deviceIdFact; }
    Fact *managerCompid() { return &_managerCompidFact; }

    float pitchRate() const { return _pitchRate; }
    float yawRate() const { return _yawRate; }
    bool yawLock() const { return _yawLock; }
    bool retracted() const { return _retracted; }
    bool gimbalHaveControl() const { return _haveControl; }
    bool gimbalOthersHaveControl() const { return _othersHaveControl; }

    void setAbsoluteRoll(float absRoll) { absoluteRoll()->setRawValue(absRoll); }
    void setAbsolutePitch(float absPitch) { absolutePitch()->setRawValue(absPitch); }
    void setBodyYaw(float yaw) { bodyYaw()->setRawValue(yaw); }
    void setAbsoluteYaw(float absYaw) { absoluteYaw()->setRawValue(absYaw); }
    void setDeviceId(uint id) { deviceId()->setRawValue(id); }
    void setManagerCompid(uint id) { managerCompid()->setRawValue(id); }

    void setPitchRate(float pitchRate) { if (pitchRate != _pitchRate) { _pitchRate = pitchRate; emit pitchRateChanged(); } }
    void setYawRate(float yawRate) { if (yawRate != _yawRate) { _yawRate = yawRate; emit yawRateChanged(); } }
    void setYawLock(bool yawLock) { if (yawLock != _yawLock) { _yawLock = yawLock; emit yawLockChanged(); } }
    void setRetracted(bool retracted) { if (retracted != _retracted) { _retracted = retracted; emit retractedChanged(); } }
    void setGimbalHaveControl(bool set) { if (set != _haveControl) { _haveControl = set; emit gimbalHaveControlChanged(); } }
    void setGimbalOthersHaveControl(bool set) { if (set != _othersHaveControl) { _othersHaveControl = set; emit gimbalOthersHaveControlChanged(); } }

    bool angleLimitsKnown() const { return _angleLimitsKnown; }
    float pitchMin() const { return _pitchMin; }
    float pitchMax() const { return _pitchMax; }
    float yawMin() const { return _yawMin; }
    float yawMax() const { return _yawMax; }

    /// Angles in radians, as they arrive on the wire. A range is only accepted when it is
    /// finite and min is genuinely below max; anything else leaves angleLimitsKnown false
    /// so callers fall back rather than clamping everything to zero.
    void setAngleLimits(float pitchMinRad, float pitchMaxRad, float yawMinRad, float yawMaxRad);

    void setCapabilityFlags(uint32_t flags);
    bool supportsRetract() const { return (_capabilityFlags & GIMBAL_MANAGER_CAP_FLAGS_HAS_RETRACT) != 0; }
    bool supportsYawLock() const { return (_capabilityFlags & GIMBAL_MANAGER_CAP_FLAGS_HAS_YAW_LOCK) != 0; }

signals:
    void pitchRateChanged();
    void yawRateChanged();
    void yawLockChanged();
    void retractedChanged();
    void gimbalHaveControlChanged();
    void gimbalOthersHaveControlChanged();
    void capabilityFlagsChanged();
    void angleLimitsChanged();

private:
    void _initFacts();

    unsigned _requestInformationRetries = 3;
    unsigned _requestStatusRetries = 6;
    unsigned _requestAttitudeRetries = 3;
    bool _receivedGimbalManagerInformation = false;
    bool _receivedGimbalManagerStatus = false;
    bool _receivedGimbalDeviceAttitudeStatus = false;
    bool _isComplete = false;
    bool _neutral = false;
    uint32_t _capabilityFlags = 0; // GIMBAL_MANAGER_CAP_FLAGS

    bool _angleLimitsKnown = false;
    float _pitchMin = 0.f;      ///< degrees
    float _pitchMax = 0.f;
    float _yawMin = 0.f;
    float _yawMax = 0.f;

    Fact _absoluteRollFact = Fact(0, QStringLiteral("gimbalRoll"), FactMetaData::valueTypeFloat);
    Fact _absolutePitchFact = Fact(0, QStringLiteral("gimbalPitch"), FactMetaData::valueTypeFloat);
    Fact _bodyYawFact = Fact(0, QStringLiteral("gimbalYaw"), FactMetaData::valueTypeFloat);
    Fact _absoluteYawFact = Fact(0, QStringLiteral("gimbalAzimuth"), FactMetaData::valueTypeFloat);
    Fact _deviceIdFact = Fact(0, QStringLiteral("deviceId"), FactMetaData::valueTypeUint8); ///< Component ID of gimbal device (or 1-6 for non-MAVLink gimbal)
    Fact _managerCompidFact = Fact(0, QStringLiteral("managerCompid"), FactMetaData::valueTypeUint8);

    float _pitchRate = 0.f;
    float _yawRate = 0.f;
    bool _yawLock = false;
    bool _retracted = false;
    bool _haveControl = false;
    bool _othersHaveControl = false;
};
