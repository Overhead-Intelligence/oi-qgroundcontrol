import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls
import QGroundControl.FlightMap

TransectStyleComplexItemEditor {
    transectAreaDefinitionComplete: _missionItem.corridorPolyline.isValid
    transectAreaDefinitionHelp:     qsTr("Create the polyline which defines the corridor.")
    transectValuesHeaderName:       qsTr("Corridor")
    transectAreaObject:             _missionItem.corridorPolyline
    transectAreaToolsName:          qsTr("Polyline Tools")
    // A corridor is a line; there is no circular form of it.
    transectAreaSupportsCircle:     false
    transectAreaLoadDialog:         kmlOrSHPLoadDialog
    transectValuesComponent:        _transectValuesComponent
    presetsTransectValuesComponent: _transectValuesComponent

    property real   _margin:        ScreenTools.defaultFontPixelWidth / 2
    property var    _missionItem:   missionItem

    Component {
        id: _transectValuesComponent

        GridLayout {
            columnSpacing:  _margin
            rowSpacing:     _margin
            columns:        2

            QGCLabel { text: qsTr("Width") }
            FactTextField {
                fact:               _missionItem.corridorWidth
                Layout.fillWidth:   true
            }

            QGCLabel {
                text:       qsTr("Turnaround dist")
                visible:    !forPresets
            }
            FactTextField {
                fact:               _missionItem.turnAroundDistance
                Layout.fillWidth:   true
                visible:            !forPresets
            }

            FactCheckBox {
                Layout.columnSpan:  2
                text:               qsTr("Images in turnarounds")
                fact:               _missionItem.cameraTriggerInTurnAround
                enabled:            _missionItem.hoverAndCaptureAllowed ? !_missionItem.hoverAndCapture.rawValue : true
                visible:            !forPresets
            }
        }
    }

    KMLOrSHPFileDialog {
        id:     kmlOrSHPLoadDialog
        title:  qsTr("Select Polyline File")

        onAcceptedForLoad: (file) => {
            _missionItem.corridorPolyline.loadKMLOrSHPFile(file)
            _missionItem.resetState = false
            close()
        }
    }
}
