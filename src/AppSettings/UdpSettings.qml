import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

ColumnLayout {
    spacing: _rowSpacing

    function saveSettings() {
        // No need
    }

    QGCLabel {
        Layout.preferredWidth: _secondColumnWidth
        Layout.fillWidth:       true
        font.pointSize:         ScreenTools.smallFontPointSize
        wrapMode:               Text.WordWrap
        text:                   qsTr("Note: For best perfomance, please disable AutoConnect to UDP devices on the General page.")
    }

    RowLayout {
        spacing: _colSpacing

        QGCLabel { text: qsTr("Port") }
        QGCTextField {
            id:                     portField
            text:                   subEditConfig ? subEditConfig.localPort.toString() : ""
            focus:                  true
            Layout.preferredWidth:  _secondColumnWidth
            inputMethodHints:       Qt.ImhFormattedNumbersOnly
            onTextChanged:          { if (subEditConfig) subEditConfig.localPort = parseInt(portField.text) }
        }
    }

    QGCLabel { text: qsTr("Server Addresses") }

    QGCLabel {
        Layout.preferredWidth:  _secondColumnWidth
        Layout.fillWidth:       true
        font.pointSize:         ScreenTools.smallFontPointSize
        wrapMode:               Text.WordWrap
        text:                   qsTr("This link only accepts telemetry from the addresses listed here. Changes apply the next time it connects.")
        visible:                !acceptAnySenderCheckBox.checked
    }

    Repeater {
        model: subEditConfig ? subEditConfig.hostList : []

        delegate: RowLayout {
            spacing: _colSpacing

            QGCLabel {
                Layout.preferredWidth:  _secondColumnWidth
                text:                   modelData
            }

            QGCButton {
                text:       qsTr("Remove")
                onClicked:  subEditConfig.removeHost(modelData)
            }
        }
    }

    RowLayout {
        spacing: _colSpacing

        QGCTextField {
            id:                     hostField
            Layout.preferredWidth:  _secondColumnWidth
            placeholderText:        qsTr("IP or hostname, e.g. 127.0.0.1:14550 or my-drone.local:14550")
        }
        QGCButton {
            text:       qsTr("Add Server")
            enabled:    hostField.text !== ""
            onClicked: {
                subEditConfig.addHost(hostField.text)
                hostField.text = ""
            }
        }
    }

    QGCCheckBox {
        id:                 acceptAnySenderCheckBox
        text:               qsTr("Accept data from any sender")
        checked:            subEditConfig ? subEditConfig.acceptAnySender : false
        onCheckedChanged:   { if (subEditConfig) subEditConfig.acceptAnySender = checked }
    }

    QGCLabel {
        Layout.preferredWidth:  _secondColumnWidth
        Layout.fillWidth:       true
        font.pointSize:         ScreenTools.smallFontPointSize
        wrapMode:               Text.WordWrap
        color:                  qgcPal.warningText
        text:                   qsTr("Any aircraft reaching this port will be shown, including ones belonging to another link or another operator.")
        visible:                acceptAnySenderCheckBox.checked
    }

    QGCPalette { id: qgcPal; colorGroupEnabled: true }
}
