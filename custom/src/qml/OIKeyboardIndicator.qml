import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

import OI.Controls

/// Fly view toolbar indicator for keyboard guided control.
///
/// Registered through QGCCorePlugin::toolBarIndicators(), which is a supported hook,
/// so no stock QML is overridden to place it.
///
/// Two rows of text, matching the other indicators: the heading target and the
/// altitude target the GCS is holding. Without this there is no way to tell what the
/// aircraft has been asked to do after a key press - the first flight test made that
/// the main complaint. Clicking opens the key bindings as a quick reference.
///
/// It also carries the flight-mode confirmation prompt. That cannot live in a modal
/// dialog: the operator is looking at the map, and a dialog that steals focus would
/// swallow the very second key press it is asking for.
Item {
    id:             control
    anchors.top:    parent.top
    anchors.bottom: parent.bottom
    width:          rowLayout.width

    // Hidden entirely when the feature is off, so operators who do not use it never
    // see it. Anything else would be clutter on a screen that is short of room.
    property bool showIndicator: OIKeyboard.enabled

    property bool _confirming: OIKeyboard.pendingModeName !== ""
    property bool _warning:    OIKeyboard.warningText !== ""

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    RowLayout {
        id:         rowLayout
        anchors.top:    parent.top
        anchors.bottom: parent.bottom
        spacing:        ScreenTools.defaultFontPixelWidth

        ColumnLayout {
            Layout.alignment:   Qt.AlignVCenter
            spacing:            0

            QGCLabel {
                font.pointSize: ScreenTools.smallFontPointSize
                color:          control._confirming ? qgcPal.warningText
                                                    : (control._warning ? qgcPal.warningText
                                                                        : qgcPal.text)
                text: {
                    if (control._confirming) {
                        return qsTr("Press again: %1 (%2s)")
                                    .arg(OIKeyboard.pendingModeName).arg(OIKeyboard.pendingSeconds)
                    }
                    if (control._warning) {
                        return OIKeyboard.warningText
                    }
                    return qsTr("Hdg %1").arg(OIKeyboard.headingTargetValid
                                                  ? OIKeyboard.headingTarget.toFixed(0) + "°"
                                                  : "--")
                }
            }

            QGCLabel {
                font.pointSize: ScreenTools.smallFontPointSize
                color:          control._confirming ? qgcPal.warningText
                                                    : (OIKeyboard.canAct ? qgcPal.text
                                                                         : qgcPal.colorGrey)
                text: {
                    if (control._confirming) {
                        return qsTr("Esc cancels")
                    }
                    if (control._warning) {
                        return OIKeyboard.statusText
                    }
                    return qsTr("Alt %1").arg(OIKeyboard.altitudeTargetValid
                                                  ? OIKeyboard.altitudeTarget.toFixed(0) + " m"
                                                  : "--")
                }
            }
        }
    }

    MouseArea {
        anchors.fill:   parent
        onClicked:      mainWindow.showIndicatorDrawer(bindingsPopup, control)
    }

    Component {
        id: bindingsPopup

        ToolIndicatorPage {
            showExpand: false

            contentComponent: Component {
                ColumnLayout {
                    spacing: ScreenTools.defaultFontPixelHeight / 2

                    SettingsGroupLayout {
                        Layout.fillWidth:   true
                        heading:            qsTr("Keyboard Control")

                        QGCLabel {
                            Layout.fillWidth:       true
                            Layout.minimumWidth:    0
                            wrapMode:               Text.WordWrap
                            color:                  OIKeyboard.canAct ? qgcPal.text : qgcPal.warningText
                            text:                   OIKeyboard.statusText
                        }

                        LabelledLabel {
                            label:      qsTr("Heading target")
                            labelText:  OIKeyboard.headingTargetValid
                                            ? OIKeyboard.headingTarget.toFixed(0) + "°" : qsTr("not set")
                        }

                        LabelledLabel {
                            label:      qsTr("Altitude target")
                            labelText:  OIKeyboard.altitudeTargetValid
                                            ? OIKeyboard.altitudeTarget.toFixed(0) + " m" : qsTr("not set")
                        }
                    }

                    SettingsGroupLayout {
                        Layout.fillWidth:   true
                        heading:            qsTr("Key Bindings")
                        headingDescription: qsTr("Gimbal and flight mode keys work in any mode. Heading and altitude keys need the vehicle flying in Guided.")

                        Repeater {
                            model: OIKeyboard.bindingList()

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
