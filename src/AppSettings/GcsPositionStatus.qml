import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

SettingsGroupLayout {
    heading: qsTr("GCS Position")
    visible: _gcsPosition.isValid

    property var  _gcsPosition: QGroundControl.qgcPositionManger.gcsPosition
    property real _gcsHDOP:     QGroundControl.qgcPositionManger.gcsPositionHorizontalAccuracy
    property bool _manual:      QGroundControl.qgcPositionManger.gcsPositionManual

    LabelledLabel {
        Layout.fillWidth: true
        label:     qsTr("Source")
        labelText: _manual ? qsTr("Set by hand on the map") : qsTr("Position source")
    }

    LabelledLabel {
        Layout.fillWidth: true
        label:     qsTr("Latitude")
        labelText: _gcsPosition.isValid ? _gcsPosition.latitude.toFixed(7) : qsTr("N/A")
    }

    LabelledLabel {
        Layout.fillWidth: true
        label:     qsTr("Longitude")
        labelText: _gcsPosition.isValid ? _gcsPosition.longitude.toFixed(7) : qsTr("N/A")
    }

    LabelledLabel {
        Layout.fillWidth: true
        label:     qsTr("HDOP")
        // A hand-placed point has no meaningful accuracy figure to report.
        labelText: _manual ? qsTr("n/a (manual)") : (_gcsHDOP > 0 ? _gcsHDOP.toFixed(1) + " m" : qsTr("N/A"))
    }

    LabelledButton {
        Layout.fillWidth: true
        visible:    _manual
        label:      qsTr("Hand back to this machine's position source")
        buttonText: qsTr("Clear")
        onClicked:  QGroundControl.qgcPositionManger.clearManualGCSPosition()
    }
}
