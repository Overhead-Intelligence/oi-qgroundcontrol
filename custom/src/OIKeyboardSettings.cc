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

DECLARE_SETTINGSFACT(OIKeyboardSettings, altitudeStep)
DECLARE_SETTINGSFACT(OIKeyboardSettings, turnRate)
DECLARE_SETTINGSFACT(OIKeyboardSettings, turnBankLimit)
DECLARE_SETTINGSFACT(OIKeyboardSettings, gimbalRate)
