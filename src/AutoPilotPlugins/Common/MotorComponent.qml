import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

SetupPage {
    id:             motorPage
    pageComponent:  pageComponent

    property bool userLetterMotorIndices: false

    /// ArduPilot caps a motor test at 30 s (MOTOR_TEST_TIMEOUT_MS_MAX); asking for more is
    /// silently truncated, so the field refuses it instead.
    readonly property int _maxDurationSecs:      30
    readonly property int _defaultDurationSecs:  3

    /// -1 is "could not tell", which is still possible for a vehicle that reports no frame
    /// class at all. The buttons then fall back to a fixed eight, as they always did.
    readonly property int  _motorCount:     controller.vehicle.motorCount
    readonly property bool _countKnown:     _motorCount > 0
    readonly property int  _buttonCount:    _countKnown ? _motorCount : 8

    /// One entry per test sequence: { sequence, motorNumber, clockwise }. Empty for a frame
    /// that is not tabulated, in which case only the sequence is named - a wrong motor number
    /// would be worse than none, since the operator checks it against a physical arm.
    readonly property var _layout: controller.vehicle.motorLayout

    /// True while a sweep started here is still expected to be running. QGC gets no progress
    /// report back from a motor test, so this is timed rather than observed - see sweepTimer.
    readonly property bool _sweepRunning: sweepTimer.running

    /// How long the autopilot will take to walk @p count motors at @p secs each.
    ///
    /// It runs a motor for the full duration, then holds all motors at minimum for half that
    /// again before stepping to the next; the last motor has no trailing gap because the count
    /// has reached one and the test simply stops. So the sweep lasts secs * (1.5n - 0.5) - for
    /// four motors at 3 s, 16.5 s rather than the 12 s a naive count would give.
    function sweepDurationMs(secs, count) {
        return secs * ((1.5 * count) - 0.5) * 1000
    }

    Timer {
        id:         sweepTimer
        repeat:     false
    }

    function motorIndexToString(motorIndex) {
        let asciiA = 65;
        if (userLetterMotorIndices) {
            return String.fromCharCode(asciiA + motorIndex);
        } else {
            return motorIndex + 1;
        }
    }

    FactPanelController {
        id: controller
    }

    Component {
        id: pageComponent

        Column {
            spacing: ScreenTools.defaultFontPixelHeight

            QGCLabel {
                text:       qsTr("Warning: Unable to determine motor count")
                color:      qgcPal.warningText
                visible:    !motorPage._countKnown
            }

            RowLayout {
                enabled:    safetySwitch.checked
                spacing:    ScreenTools.defaultFontPixelWidth * 2

                QGCLabel { text: qsTr("Throttle") }

                QGCTextField {
                    id:                     throttleField
                    Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 8
                    text:                   "0"
                    unitsLabel:             qsTr("%")
                    showUnits:              true
                    inputMethodHints:       Qt.ImhFormattedNumbersOnly
                    validator:              IntValidator { bottom: 0; top: 100 }

                    /// Parsed rather than read raw: an empty or half-typed field must mean "do
                    /// not spin", not NaN into a motor command.
                    function throttlePercent() {
                        var value = parseInt(text)
                        return isNaN(value) ? 0 : Math.max(0, Math.min(100, value))
                    }
                }

                QGCLabel {
                    Layout.leftMargin:  ScreenTools.defaultFontPixelWidth * 2
                    text:               qsTr("Duration")
                }

                QGCTextField {
                    id:                     durationField
                    Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 8
                    text:                   motorPage._defaultDurationSecs.toString()
                    unitsLabel:             qsTr("s")
                    showUnits:              true
                    inputMethodHints:       Qt.ImhFormattedNumbersOnly
                    validator:              IntValidator { bottom: 1; top: motorPage._maxDurationSecs }

                    function durationSecs() {
                        var value = parseInt(text)
                        return isNaN(value) ? motorPage._defaultDurationSecs
                                            : Math.max(1, Math.min(motorPage._maxDurationSecs, value))
                    }
                }
            }

            QGCLabel {
                anchors.left:   parent.left
                anchors.right:  parent.right
                wrapMode:       Text.WordWrap
                text:           qsTr("Make sure you remove all props.")
            }

            RowLayout {
                id:         motorButtons
                enabled:    safetySwitch.checked
                spacing:    ScreenTools.defaultFontPixelWidth * 3

                Repeater {
                    id:     buttonRepeater
                    model:  motorPage._buttonCount

                    ColumnLayout {
                        id:         motorColumn
                        spacing:    ScreenTools.defaultFontPixelHeight / 4

                        // The letter is the test sequence, which is what the button sends. The
                        // motor number beside it is what the operator has to find on the
                        // airframe, and the two are not the same: on a QUAD/X, sequence 2 is
                        // motor 4. Naming only one of them sends people to the wrong arm.
                        readonly property var _entry: index < motorPage._layout.length ? motorPage._layout[index] : null

                        QGCButton {
                            Layout.alignment:   Qt.AlignHCenter
                            text:               motorPage.motorIndexToString(index)
                            // Spinning one motor while the sweep is mid-way through another
                            // just retargets the running test, which leaves the sweep counting
                            // down against a motor nobody asked for.
                            enabled:            !motorPage._sweepRunning
                            onClicked:          controller.vehicle.motorTest(
                                                    index + 1,
                                                    throttleField.throttlePercent(),
                                                    throttleField.throttlePercent() === 0 ? 0 : durationField.durationSecs(),
                                                    true)
                        }

                        QGCLabel {
                            Layout.alignment:   Qt.AlignHCenter
                            font.pointSize:     ScreenTools.smallFontPointSize
                            visible:            motorColumn._entry !== null
                            text:               motorColumn._entry ? qsTr("Motor %1").arg(motorColumn._entry.motorNumber) : ""
                        }

                        QGCLabel {
                            Layout.alignment:   Qt.AlignHCenter
                            font.pointSize:     ScreenTools.smallFontPointSize
                            color:              qgcPal.colorGrey
                            visible:            motorColumn._entry !== null
                            text:               motorColumn._entry ? (motorColumn._entry.clockwise ? qsTr("CW") : qsTr("CCW")) : ""
                        }
                    }
                }

                QGCButton {
                    Layout.alignment:   Qt.AlignTop
                    text:               qsTr("All")
                    enabled:            !motorPage._sweepRunning
                    // One command, not one per motor. The autopilot walks the sequence itself and
                    // keeps the motors armed throughout; the old loop sent a command per motor
                    // back to back, each overwriting the last before the autopilot had run a
                    // cycle, so only the final one was ever tested.
                    onClicked: {
                        var throttle = throttleField.throttlePercent()
                        if (throttle === 0) {
                            // Nothing will spin, and the autopilot stops the test immediately on a
                            // zero timeout, so there is no sweep to lock the buttons against.
                            controller.vehicle.motorTest(1, 0, 0, true)
                            return
                        }
                        var secs = durationField.durationSecs()
                        controller.vehicle.motorTest(1, throttle, secs, true, motorPage._buttonCount)
                        sweepTimer.interval = motorPage.sweepDurationMs(secs, motorPage._buttonCount)
                        sweepTimer.restart()
                    }
                }

                QGCButton {
                    Layout.alignment:   Qt.AlignTop
                    text:               qsTr("Stop")
                    // Never locked out. It is the one control that has to work while something is
                    // already spinning.
                    // A zero timeout ends the running test on the next autopilot cycle, whichever
                    // motor it was on, and disarms. One command does it.
                    onClicked: {
                        sweepTimer.stop()
                        controller.vehicle.motorTest(1, 0, 0, true)
                    }
                }
            }

            Row {
                spacing: ScreenTools.defaultFontPixelWidth

                Switch {
                    id: safetySwitch
                    onClicked: {
                        if (!checked) {
                            // Switching off disables the whole row, Stop included, so it has to
                            // stop the test itself - otherwise it is a way to lock out the one
                            // control that was being kept available on purpose.
                            sweepTimer.stop()
                            controller.vehicle.motorTest(1, 0, 0, true)
                            throttleField.text = "0"
                        }
                    }
                }

                QGCLabel {
                    anchors.verticalCenter: parent.verticalCenter
                    color:  qgcPal.warningText
                    text:   safetySwitch.checked ? qsTr("Careful : Motors are enabled")
                                                : qsTr("Propellers are removed - enable the motor controls")
                }
            }
        } // Column
    } // Component
} // SetupPage
