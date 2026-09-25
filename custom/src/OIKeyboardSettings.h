/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtQmlIntegration/QtQmlIntegration>

#include "SettingsGroup.h"

/// Key bindings and step sizes for OIKeyboardController.
///
/// Every binding is a string holding one QKeySequence-portable key name ("A", "Up",
/// "PgUp"). Steps are deliberate rather than rates: ArduPlane's guided altitude slew
/// does not honour the rate it is given (measured in SITL on 4.6.3), and the gimbal
/// rate path did not move a servo mount at all, so discrete absolute steps are the
/// only primitives that behave predictably on the fleet firmware.
class OIKeyboardSettings : public SettingsGroup
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

public:
    OIKeyboardSettings(QObject *parent = nullptr);

    DEFINE_SETTING_NAME_GROUP()

    DEFINE_SETTINGFACT(enabled)

    DEFINE_SETTINGFACT(headingLeftKey)
    DEFINE_SETTINGFACT(headingRightKey)
    DEFINE_SETTINGFACT(headingStep)
    DEFINE_SETTINGFACT(headingBankLimit)

    DEFINE_SETTINGFACT(altitudeUpKey)
    DEFINE_SETTINGFACT(altitudeDownKey)
    DEFINE_SETTINGFACT(altitudeStep)
    DEFINE_SETTINGFACT(altitudeLead)

    DEFINE_SETTINGFACT(gimbalPitchUpKey)
    DEFINE_SETTINGFACT(gimbalPitchDownKey)
    DEFINE_SETTINGFACT(gimbalPitchStep)
    DEFINE_SETTINGFACT(gimbalYawLeftKey)
    DEFINE_SETTINGFACT(gimbalYawRightKey)
    DEFINE_SETTINGFACT(gimbalYawStep)
    DEFINE_SETTINGFACT(gimbalNextModeKey)
    DEFINE_SETTINGFACT(gimbalPrevModeKey)

    DEFINE_SETTINGFACT(modeHotkeysEnabled)
    DEFINE_SETTINGFACT(modeConfirmTimeout)
};
