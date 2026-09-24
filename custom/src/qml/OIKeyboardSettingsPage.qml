import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls

import OI.Controls

/// "Keyboard Control" section of Settings -> Keyboard.
///
/// Referenced by name from src/AppSettings/pages/Keyboard.SettingsUI.json, which the
/// build turns into KeyboardSettings.qml. That generated page emits section components
/// as bare type names, so this has to be a type in a QML module the page imports -
/// hence the OI.Settings module rather than a loose file in custom.qrc.
///
/// The page deliberately lives in Application Settings rather than beside the Joystick
/// tab in Vehicle Setup: there is no QGCCorePlugin hook for adding a vehicle component,
/// and binding keys should not require a connected aircraft.
///
/// Layout note: SettingsGroupLayout gives its content a fixed width and a RowLayout will
/// not shrink a child below its implicit width, so rows stay short and anything long is
/// fillWidth with minimumWidth 0.
Item {
    id:             root
    implicitHeight: mainLayout.implicitHeight

    property var  _settings:   OIKeyboard.settings
    property real _fieldWidth: ScreenTools.defaultFontPixelWidth * 12

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    ColumnLayout {
        id:     mainLayout
        width:  parent.width

        // ------------------------------------------------------------ status
        SettingsGroupLayout {
            Layout.fillWidth:   true
            heading:            qsTr("Status")
            headingDescription: qsTr("Keyboard control is always off when QGroundControl starts. It turns itself off if the vehicle leaves Guided, changes, or disconnects, and Esc turns it off at any time.")

            RowLayout {
                Layout.fillWidth:   true
                spacing:            ScreenTools.defaultFontPixelWidth

                QGCLabel {
                    Layout.fillWidth:       true
                    Layout.minimumWidth:    0
                    wrapMode:               Text.WordWrap
                    color:                  OIKeyboard.available ? qgcPal.text : qgcPal.warningText
                    text:                   OIKeyboard.statusText
                }

                QGCButton {
                    text:       OIKeyboard.armed ? qsTr("Turn off") : qsTr("Turn on")
                    enabled:    OIKeyboard.available || OIKeyboard.armed
                    onClicked:  OIKeyboard.armed = !OIKeyboard.armed
                }
            }

            QGCLabel {
                Layout.fillWidth:       true
                Layout.minimumWidth:    0
                wrapMode:               Text.WordWrap
                font.pointSize:         ScreenTools.smallFontPointSize
                visible:                OIKeyboard.altitudeTargetValid
                text:                   qsTr("Commanded altitude target: %1 m above home")
                                            .arg(OIKeyboard.altitudeTarget.toFixed(0))
            }
        }

        // ----------------------------------------------------------- heading
        SettingsGroupLayout {
            Layout.fillWidth:   true
            heading:            qsTr("Heading")
            headingDescription: qsTr("Each press turns the aircraft by one step. Heading control needs forward flight: in a VTOL hover ArduPlane accepts the command and then ignores it, so the keys do nothing there and the status line says so.")

            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.headingLeftKey
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.headingRightKey
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.headingStep
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.headingBankLimit
            }
        }

        // ---------------------------------------------------------- altitude
        SettingsGroupLayout {
            Layout.fillWidth:   true
            heading:            qsTr("Altitude")
            headingDescription: qsTr("Each press moves the target altitude by one step. The target is held here and clamped to the Fly View guided minimum and maximum (currently %1 to %2 m above home), because the autopilot applies no limit of its own.")
                                    .arg(QGroundControl.settingsManager.flyViewSettings.guidedMinimumAltitude.value)
                                    .arg(QGroundControl.settingsManager.flyViewSettings.guidedMaximumAltitude.value)

            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.altitudeUpKey
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.altitudeDownKey
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.altitudeStep
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.altitudeLead
            }

            QGCLabel {
                Layout.fillWidth:       true
                Layout.minimumWidth:    0
                wrapMode:               Text.WordWrap
                font.pointSize:         ScreenTools.smallFontPointSize
                text:                   qsTr("Those limits are measured above the point the aircraft armed, not above the ground beneath it. Over rising terrain the floor can sit below the surface.")
                color:                  qgcPal.warningText
            }
        }

        // ------------------------------------------------------------ gimbal
        SettingsGroupLayout {
            Layout.fillWidth:   true
            heading:            qsTr("Gimbal")
            headingDescription: qsTr("Each press moves the gimbal by one step, sent as an absolute angle. Mode cycles through Follow, Lock, Retract and Neutral.")

            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.gimbalPitchUpKey
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.gimbalPitchDownKey
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.gimbalPitchStep
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.gimbalYawLeftKey
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.gimbalYawRightKey
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.gimbalYawStep
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.gimbalNextModeKey
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.gimbalPrevModeKey
            }
        }

        // ----------------------------------------------------- mode hotkeys
        SettingsGroupLayout {
            Layout.fillWidth:   true
            heading:            qsTr("Flight Mode Hotkeys")
            headingDescription: qsTr("A press opens a confirmation; a second press of the same key within the timeout sends the mode change, and Esc cancels. Changing mode leaves Guided, which turns keyboard control off.")

            FactCheckBoxSlider {
                Layout.fillWidth:   true
                text:               qsTr("Enable flight mode hotkeys")
                fact:               _settings.modeHotkeysEnabled
            }

            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.modeConfirmTimeout
                visible:                    _settings.modeHotkeysEnabled.rawValue
            }

            Repeater {
                model: OIKeyboard.modeHotkeys

                RowLayout {
                    Layout.fillWidth:   true
                    spacing:            ScreenTools.defaultFontPixelWidth
                    visible:            _settings.modeHotkeysEnabled.rawValue

                    QGCLabel { text: qsTr("Press") }

                    QGCTextField {
                        Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 8
                        text:                   object.key
                        onEditingFinished: {
                            object.key = text
                            OIKeyboard.saveModeHotkeys()
                        }
                    }

                    QGCLabel { text: qsTr("for") }

                    QGCLabel {
                        Layout.fillWidth:       true
                        Layout.minimumWidth:    0
                        elide:                  Text.ElideRight
                        text:                   object.mode
                    }

                    QGCButton {
                        text:       qsTr("Remove")
                        onClicked:  OIKeyboard.removeModeHotkey(index)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth:   true
                spacing:            ScreenTools.defaultFontPixelWidth
                visible:            _settings.modeHotkeysEnabled.rawValue

                QGCTextField {
                    id:                     newKeyField
                    Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 8
                    placeholderText:        qsTr("Key")
                }

                QGCComboBox {
                    id:                 newModeCombo
                    Layout.fillWidth:   true
                    Layout.minimumWidth: 0
                    model:              OIKeyboard.availableModes()
                    // The list comes from the connected vehicle, so it is empty
                    // offline; the operator adds hotkeys with a vehicle connected.
                    enabled:            count > 0
                }

                QGCButton {
                    text:       qsTr("Add")
                    enabled:    newKeyField.text !== "" && newModeCombo.count > 0
                    onClicked: {
                        OIKeyboard.addModeHotkey(newKeyField.text, newModeCombo.currentText)
                        newKeyField.text = ""
                    }
                }
            }

            QGCLabel {
                Layout.fillWidth:       true
                Layout.minimumWidth:    0
                wrapMode:               Text.WordWrap
                font.pointSize:         ScreenTools.smallFontPointSize
                visible:                _settings.modeHotkeysEnabled.rawValue &&
                                        OIKeyboard.availableModes().length === 0
                text:                   qsTr("Connect a vehicle to see its flight modes.")
            }
        }
    }
}
