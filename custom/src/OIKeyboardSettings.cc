/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#include "OIKeyboardSettings.h"

DECLARE_SETTINGGROUP(OIKeyboard, "OIKeyboard")
{
}

DECLARE_SETTINGSFACT(OIKeyboardSettings, headingLeftKey)
DECLARE_SETTINGSFACT(OIKeyboardSettings, headingRightKey)
DECLARE_SETTINGSFACT(OIKeyboardSettings, headingStep)
DECLARE_SETTINGSFACT(OIKeyboardSettings, headingBankLimit)

DECLARE_SETTINGSFACT(OIKeyboardSettings, altitudeUpKey)
DECLARE_SETTINGSFACT(OIKeyboardSettings, altitudeDownKey)
DECLARE_SETTINGSFACT(OIKeyboardSettings, altitudeStep)
DECLARE_SETTINGSFACT(OIKeyboardSettings, altitudeLead)

DECLARE_SETTINGSFACT(OIKeyboardSettings, gimbalPitchUpKey)
DECLARE_SETTINGSFACT(OIKeyboardSettings, gimbalPitchDownKey)
DECLARE_SETTINGSFACT(OIKeyboardSettings, gimbalPitchStep)
DECLARE_SETTINGSFACT(OIKeyboardSettings, gimbalYawLeftKey)
DECLARE_SETTINGSFACT(OIKeyboardSettings, gimbalYawRightKey)
DECLARE_SETTINGSFACT(OIKeyboardSettings, gimbalYawStep)
DECLARE_SETTINGSFACT(OIKeyboardSettings, gimbalNextModeKey)
DECLARE_SETTINGSFACT(OIKeyboardSettings, gimbalPrevModeKey)

DECLARE_SETTINGSFACT(OIKeyboardSettings, modeHotkeysEnabled)
DECLARE_SETTINGSFACT(OIKeyboardSettings, modeConfirmTimeout)
