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
            heading:            qsTr("Keyboard Control")
            headingDescription: qsTr("Enable/disable the use of the hotkeys below. The line below this toggle indicates if there any missing conditions for use of the Heading and Altitude keys.")

            // Always available: turning the feature on or off is a decision the pilot
            // makes between flights, not something that needs a vehicle present.
            FactCheckBoxSlider {
                Layout.fillWidth:   true
                text:               qsTr("Keyboard control enabled")
                fact:               _settings.enabled
            }

            QGCLabel {
                Layout.fillWidth:       true
                Layout.minimumWidth:    0
                wrapMode:               Text.WordWrap
                color:                  OIKeyboard.canAct ? qgcPal.text : qgcPal.warningText
                text:                   OIKeyboard.statusText
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
            headingDescription: qsTr("Each press shifts the heading target by one step, respecting the set turn bank limit. However, ROLL_LIMIT_DEG still determines the maximum bank these commands can force.")

            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.headingLeftKey
                label:                      _settings.headingLeftKey.label
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.headingRightKey
                label:                      _settings.headingRightKey.label
            }
            LabelledFactComboBox {
                Layout.fillWidth:   true
                fact:               _settings.headingStep
                label:              _settings.headingStep.label
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.headingBankLimit
                label:                      _settings.headingBankLimit.label
            }
        }

        // ---------------------------------------------------------- altitude
        SettingsGroupLayout {
            Layout.fillWidth:   true
            heading:            qsTr("Altitude")
            headingDescription: qsTr("Each press shifts the altitude target by one step. The maximum target lead determines how large of an altitude shift can be commanded before altitude commands are blocked until the drone reaches its target. The target can never exceed your Fly View's configured Minimum & Maximum altitudes (currently %1m and %2m).")
                                    .arg(QGroundControl.settingsManager.flyViewSettings.guidedMinimumAltitude.value)
                                    .arg(QGroundControl.settingsManager.flyViewSettings.guidedMaximumAltitude.value)

            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.altitudeUpKey
                label:                      _settings.altitudeUpKey.label
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.altitudeDownKey
                label:                      _settings.altitudeDownKey.label
            }
            LabelledFactComboBox {
                Layout.fillWidth:   true
                fact:               _settings.altitudeStep
                label:              _settings.altitudeStep.label
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.altitudeLead
                label:                      _settings.altitudeLead.label
            }

        }

        // ------------------------------------------------------------ gimbal
        SettingsGroupLayout {
            Layout.fillWidth:   true
            heading:            qsTr("Gimbal")
            headingDescription: qsTr("Each press moves the gimbal by one step, sent as an absolute angle. Mode cycles through Follow, Lock, Retract and Neutral. Gimbal keys work in any flight mode, including on the ground.")

            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.gimbalPitchUpKey
                label:                      _settings.gimbalPitchUpKey.label
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.gimbalPitchDownKey
                label:                      _settings.gimbalPitchDownKey.label
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.gimbalPitchStep
                label:                      _settings.gimbalPitchStep.label
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.gimbalYawLeftKey
                label:                      _settings.gimbalYawLeftKey.label
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.gimbalYawRightKey
                label:                      _settings.gimbalYawRightKey.label
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.gimbalYawStep
                label:                      _settings.gimbalYawStep.label
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.gimbalNextModeKey
                label:                      _settings.gimbalNextModeKey.label
            }
            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.gimbalPrevModeKey
                label:                      _settings.gimbalPrevModeKey.label
            }
        }

        // ----------------------------------------------------- mode hotkeys
        SettingsGroupLayout {
            Layout.fillWidth:   true
            heading:            qsTr("Flight Mode Hotkeys")
            headingDescription: qsTr("For each configured hotkey in the list below, a confirmation will appear upon the first press for the configured timeout duration. Press again to confirm, or press ESC to cancel.")

            FactCheckBoxSlider {
                Layout.fillWidth:   true
                text:               qsTr("Enable flight mode hotkeys")
                fact:               _settings.modeHotkeysEnabled
            }

            LabelledFactTextField {
                Layout.fillWidth:           true
                textFieldPreferredWidth:    _fieldWidth
                fact:                       _settings.modeConfirmTimeout
                label:                      _settings.modeConfirmTimeout.label
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

        // ------------------------------------------------------ key conflicts
        SettingsGroupLayout {
            Layout.fillWidth:   true
            visible:            OIKeyboard.keyConflicts.length > 0
            heading:            qsTr("Key Conflicts")

            Repeater {
                model: OIKeyboard.keyConflicts

                QGCLabel {
                    Layout.fillWidth:       true
                    Layout.minimumWidth:    0
                    wrapMode:               Text.WordWrap
                    color:                  qgcPal.warningText
                    text:                   modelData
                }
            }
        }
    }
}
