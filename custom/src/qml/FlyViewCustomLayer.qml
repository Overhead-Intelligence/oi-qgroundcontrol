import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import QtLocation
import QtPositioning
import QtQuick.Window
import QtQml.Models

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls
import QGroundControl.FlyView
import QGroundControl.FlightMap

import OI.Controls

// OI override of src/FlyView/FlyViewCustomLayer.qml.
//
// Adds the keyboard guided-control panel at the bottom centre of the Fly view.
// OIKeyboard is the C++ singleton (custom/src/OIKeyboardController.cc) that owns
// the key handling; this file is only its switch, status line and settings.

Item {
    id: _root

    property var parentToolInsets               // Screen real estate the stock controls leave free
    property var totalToolInsets:   _toolInsets // Insets after this overlay's additions
    property var mapControl

    property var    _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle
    property real   _margin:        ScreenTools.defaultFontPixelWidth * 0.75
    property bool   _setupOpen:     false

    QGCToolInsets {
        id:                     _toolInsets
        leftEdgeTopInset:       parentToolInsets.leftEdgeTopInset
        leftEdgeCenterInset:    parentToolInsets.leftEdgeCenterInset
        leftEdgeBottomInset:    parentToolInsets.leftEdgeBottomInset
        rightEdgeTopInset:      parentToolInsets.rightEdgeTopInset
        rightEdgeCenterInset:   parentToolInsets.rightEdgeCenterInset
        rightEdgeBottomInset:   parentToolInsets.rightEdgeBottomInset
        topEdgeLeftInset:       parentToolInsets.topEdgeLeftInset
        topEdgeCenterInset:     parentToolInsets.topEdgeCenterInset
        topEdgeRightInset:      parentToolInsets.topEdgeRightInset
        bottomEdgeLeftInset:    parentToolInsets.bottomEdgeLeftInset
        bottomEdgeCenterInset:  keyboardPanel.visible ? _root.height - keyboardPanel.y : parentToolInsets.bottomEdgeCenterInset
        bottomEdgeRightInset:   parentToolInsets.bottomEdgeRightInset
    }

    Rectangle {
        id:                         keyboardPanel
        anchors.horizontalCenter:   parent.horizontalCenter
        anchors.bottom:             parent.bottom
        anchors.bottomMargin:       parentToolInsets.bottomEdgeCenterInset + _margin
        width:                      panelColumn.width + _margin * 2
        height:                     panelColumn.height + _margin * 2
        radius:                     ScreenTools.defaultFontPixelHeight / 4
        color:                      qgcPal.window
        opacity:                    0.9
        visible:                    _activeVehicle ? true : false

        ColumnLayout {
            id:                 panelColumn
            anchors.centerIn:   parent
            spacing:            _margin / 2

            RowLayout {
                spacing: _margin

                QGCCheckBox {
                    id:         enableBox
                    text:       qsTr("Keyboard")
                    checked:    OIKeyboard.enabled
                    onClicked:  OIKeyboard.enabled = checked

                    Connections {
                        target: OIKeyboard
                        function onEnabledChanged() { enableBox.checked = OIKeyboard.enabled }
                    }
                }

                QGCLabel {
                    text:       OIKeyboard.statusText
                    color:      OIKeyboard.warning ? qgcPal.warningText : qgcPal.text
                    visible:    text !== ""
                }

                QGCButton {
                    text:       qsTr("Release")
                    visible:    OIKeyboard.headingHoldActive
                    onClicked:  OIKeyboard.releaseHeadingHold()
                }

                QGCButton {
                    text:       _setupOpen ? qsTr("Close") : qsTr("Setup")
                    visible:    OIKeyboard.enabled
                    onClicked:  _setupOpen = !_setupOpen
                }
            }

            QGCLabel {
                text:           OIKeyboard.legendText
                font.pointSize: ScreenTools.smallFontPointSize
                visible:        OIKeyboard.enabled
            }

            GridLayout {
                columns:        4
                columnSpacing:  _margin
                rowSpacing:     _margin / 2
                visible:        OIKeyboard.enabled && _setupOpen

                QGCLabel { text: qsTr("Altitude step") }
                FactTextField { fact: OIKeyboard.settings.altitudeStep; Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 10 }
                QGCLabel { text: qsTr("Turn rate (deg/s)") }
                FactTextField { fact: OIKeyboard.settings.turnRate; Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 10 }
                QGCLabel { text: qsTr("Turn bank limit (deg)") }
                FactTextField { fact: OIKeyboard.settings.turnBankLimit; Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 10 }
                QGCLabel { text: qsTr("Gimbal rate (deg/s)") }
                FactTextField { fact: OIKeyboard.settings.gimbalRate; Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 10 }
            }
        }
    }
}
