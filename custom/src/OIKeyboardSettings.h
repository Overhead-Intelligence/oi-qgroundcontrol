/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * Tunables for the keyboard guided control. Metadata (defaults, limits,
 * units) lives in custom/res/json/OIKeyboard.SettingsGroup.json; values are
 * persisted in the [OIKeyboard] group of the QGC settings file.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "SettingsGroup.h"

class OIKeyboardSettings : public SettingsGroup
{
    Q_OBJECT

public:
    explicit OIKeyboardSettings(QObject *parent = nullptr);

    DEFINE_SETTING_NAME_GROUP()

    DEFINE_SETTINGFACT(altitudeStep)    ///< metres per W/S press ("vertical m", shown in the operator's units)
    DEFINE_SETTINGFACT(turnRate)        ///< deg/s of heading change while A/D is held
    DEFINE_SETTINGFACT(turnBankLimit)   ///< deg, bank ceiling ArduPlane may use for the turn
    DEFINE_SETTINGFACT(gimbalRate)      ///< deg/s of gimbal pan/tilt while an arrow key is held
};
