import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

import OI.Controls

/// Fly view toolbar indicator for keyboard gimbal control.
///
/// Deliberately a second indicator rather than two more rows on OIKeyboardIndicator. The
/// heading, altitude and mode-confirmation behaviour in that one has been tuned over
/// several flights and is the part a pilot is reading while flying; bolting the gimbal
/// onto it would mean a refused pan could blank the altitude target, and most of the fleet
/// has no gimbal at all. Keeping them apart leaves the flying readout exactly as it was.
///
/// The controller carries a matching pair of warning channels for the same reason -
/// `gimbalWarningText` here, `warningText` there - so neither can clear the other.
///
/// Registered through QGCCorePlugin::toolBarIndicators(), a supported hook, so no stock
/// QML is overridden.
Item {
    id:             control
    anchors.top:    parent.top
    anchors.bottom: parent.bottom
    width:          rowLayout.width

    // Normally only shown when there is a gimbal to point. The warning is included so a
    // gimbal key pressed on an aircraft without one still says so, rather than doing
    // nothing with nowhere to report it.
    property bool showIndicator: OIKeyboard.enabled &&
                                 (OIKeyboard.gimbalPresent || OIKeyboard.zoomAvailable ||
                                  OIKeyboard.gimbalWarningText !== "")

    property bool _warning: OIKeyboard.gimbalWarningText !== ""

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    RowLayout {
        id:             rowLayout
        anchors.top:    parent.top
        anchors.bottom: parent.bottom
        spacing:        ScreenTools.defaultFontPixelWidth

        ColumnLayout {
            Layout.alignment:   Qt.AlignVCenter
            spacing:            0

            QGCLabel {
                font.pointSize: ScreenTools.smallFontPointSize
                color:          control._warning ? qgcPal.warningText : qgcPal.text
                text: {
                    if (control._warning) {
                        return OIKeyboard.gimbalWarningText
                    }
                    return qsTr("Tilt %1").arg(OIKeyboard.gimbalTargetValid
                                                   ? OIKeyboard.gimbalPitchTarget.toFixed(0) + "°"
                                                   : "--")
                }
            }

            QGCLabel {
                font.pointSize: ScreenTools.smallFontPointSize
                color:          control._warning ? qgcPal.warningText
                                                 : (OIKeyboard.gimbalPresent ? qgcPal.text
                                                                             : qgcPal.colorGrey)
                text: {
                    if (control._warning) {
                        return OIKeyboard.gimbalWarningDetail
                    }
                    return qsTr("Pan %1").arg(OIKeyboard.gimbalTargetValid
                                                  ? OIKeyboard.gimbalYawTarget.toFixed(0) + "°"
                                                  : "--")
                }
            }
        }

        // Zoom reads from the camera, not from a target held here, so it is right even
        // when something else moved it. Shown only when there is a camera that can zoom.
        ColumnLayout {
            Layout.alignment:   Qt.AlignVCenter
            spacing:            0
            visible:            OIKeyboard.zoomAvailable && !control._warning

            QGCLabel {
                font.pointSize: ScreenTools.smallFontPointSize
                color:          qgcPal.text
                text:           qsTr("Zoom")
            }

            QGCLabel {
                font.pointSize: ScreenTools.smallFontPointSize
                color:          qgcPal.text
                text:           OIKeyboard.zoomTarget.toFixed(0) + "%"
            }
        }
    }

    MouseArea {
        anchors.fill:   parent
        onClicked:      mainWindow.showIndicatorDrawer(gimbalPopup, control)
    }

    Component {
        id: gimbalPopup

        ToolIndicatorPage {
            showExpand: false

            contentComponent: Component {
                ColumnLayout {
                    spacing: ScreenTools.defaultFontPixelHeight / 2

                    SettingsGroupLayout {
                        Layout.fillWidth:   true
                        heading:            qsTr("Gimbal Control")

                        LabelledLabel {
                            label:      qsTr("Mode")
                            labelText:  OIKeyboard.gimbalModeName
                        }

                        LabelledLabel {
                            label:      qsTr("Tilt target")
                            labelText:  OIKeyboard.gimbalTargetValid
                                            ? OIKeyboard.gimbalPitchTarget.toFixed(0) + "°" : qsTr("not set")
                        }

                        LabelledLabel {
                            label:      qsTr("Pan target")
                            labelText:  OIKeyboard.gimbalTargetValid
                                            ? OIKeyboard.gimbalYawTarget.toFixed(0) + "°" : qsTr("not set")
                        }

                        LabelledLabel {
                            label:      qsTr("Zoom target")
                            visible:    OIKeyboard.zoomAvailable
                            labelText:  OIKeyboard.zoomTarget.toFixed(0) + "%"
                        }
                    }

                    SettingsGroupLayout {
                        Layout.fillWidth:   true
                        heading:            qsTr("Gimbal and Camera Keys")
                        headingDescription: qsTr("These work in any flight mode, armed or not - pointing a camera cannot move the aircraft.")

                        Repeater {
                            model: OIKeyboard.gimbalBindingList()

                            RowLayout {
                                Layout.fillWidth:   true
                                spacing:            ScreenTools.defaultFontPixelWidth

                                QGCLabel {
                                    Layout.fillWidth:       true
                                    Layout.minimumWidth:    0
                                    elide:                  Text.ElideRight
                                    color:                  modelData.conflict ? qgcPal.warningText : qgcPal.text
                                    text:                   modelData.action
                                }

                                QGCLabel {
                                    color:  modelData.conflict ? qgcPal.warningText : qgcPal.text
                                    text:   modelData.conflict
                                                ? qsTr("%1 (conflict)").arg(modelData.key)
                                                : modelData.key
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
