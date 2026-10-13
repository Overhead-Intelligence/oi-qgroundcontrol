import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls
import QGroundControl.FlightMap

Rectangle {
    id:         _root
    height:     childrenRect.y + childrenRect.height + _margin
    width:      availableWidth
    color:      qgcPal.windowShadeDark
    radius:     _radius

    required property var missionItem
    required property real availableWidth

    property bool   transectAreaDefinitionComplete: true
    property string transectAreaDefinitionHelp:     _internalError
    property string transectValuesHeaderName:       _internalError
    property var    transectValuesComponent:        undefined
    property var    presetsTransectValuesComponent: undefined

    // The shape tools. These used to be a PlanEditToolbar floating over the top of the map,
    // which operators were not finding - the panel said "use the Polygon Tools" and the tools
    // were a row of small buttons somewhere else entirely. They are a section in this panel now.
    property var    transectAreaObject:             undefined   ///< QGCMapPolygon or QGCMapPolyline
    property string transectAreaToolsName:          _internalError
    property bool   transectAreaSupportsCircle:     false
    property var    transectAreaLoadDialog:         undefined

    readonly property string _internalError: "Internal Error"

    property var    _missionItem:               missionItem
    property real   _margin:                    ScreenTools.defaultFontPixelWidth / 2
    property real   _fieldWidth:                ScreenTools.defaultFontPixelWidth * 10.5
    property var    _vehicle:                   QGroundControl.multiVehicleManager.activeVehicle ? QGroundControl.multiVehicleManager.activeVehicle : QGroundControl.multiVehicleManager.offlineEditingVehicle
    property real   _cameraMinTriggerInterval:  _missionItem.cameraCalc.minTriggerInterval.rawValue
    property string _doneAdjusting:             qsTr("Done")
    property bool   _presetsAvailable:          _missionItem.presetNames.length !== 0
    property bool   _tracing:                   transectAreaObject ? transectAreaObject.traceMode : false
    // Everything except the tools is hidden until there is an area to configure, and while a
    // trace is in progress - the same as the old behaviour, which hid the panel behind a help
    // line until the polygon existed.
    property bool   _areaReady:                 transectAreaDefinitionComplete && !_missionItem.wizardMode && !_tracing

    function polygonCaptureStarted() {
        _missionItem.clearPolygon()
    }

    function polygonCaptureFinished(coordinates) {
        for (var i=0; i<coordinates.length; i++) {
            _missionItem.addPolygonCoordinate(coordinates[i])
        }
    }

    function polygonAdjustVertex(vertexIndex, vertexCoordinate) {
        _missionItem.adjustPolygonCoordinate(vertexIndex, vertexCoordinate)
    }

    function polygonAdjustStarted() { }
    function polygonAdjustFinished() { }

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    Component {
        id: transectAreaToolsComponent

        ColumnLayout {
            spacing: _margin

            SectionHeader {
                id:                 toolsHeader
                Layout.fillWidth:   true
                text:               transectAreaToolsName
            }

            ColumnLayout {
                Layout.fillWidth:   true
                spacing:            _margin
                // Forced open while tracing, otherwise a collapsed section would take the
                // "Done Tracing" button away with it and leave no way to finish.
                visible:            toolsHeader.checked || _tracing

                QGCLabel {
                    Layout.fillWidth:   true
                    wrapMode:           Text.WordWrap
                    text:               _tracing ? qsTr("Click in the map to add vertices. Click 'Done Tracing' when finished.")
                                                 : transectAreaDefinitionHelp
                    visible:            _tracing || !transectAreaDefinitionComplete
                }

                QGCButton {
                    Layout.fillWidth:   true
                    text:               qsTr("Basic")
                    visible:            !_tracing
                    onClicked:          transectAreaObject.requestBasicShape()
                }

                QGCButton {
                    Layout.fillWidth:   true
                    text:               qsTr("Circular")
                    visible:            !_tracing && transectAreaSupportsCircle
                    onClicked:          transectAreaObject.requestCircularShape()
                }

                QGCButton {
                    Layout.fillWidth:   true
                    text:               _tracing ? qsTr("Done Tracing") : qsTr("Trace")
                    // Setting the property is the whole gesture: the map visuals save the
                    // existing vertices on the way in and restore them if the trace is
                    // abandoned, so there is nothing to coordinate from here.
                    onClicked:          transectAreaObject.traceMode = !transectAreaObject.traceMode
                }

                QGCButton {
                    Layout.fillWidth:   true
                    text:               qsTr("Load KML/SHP...")
                    visible:            !_tracing
                    onClicked:          transectAreaLoadDialog.openForLoad()
                }
            }
        }
    }

    ColumnLayout {
        id:                 editorColumn
        anchors.margins:    _margin
        anchors.top:        parent.top
        anchors.left:       parent.left
        anchors.right:      parent.right

        // Before there is an area - and while one is being traced - the tools are the only
        // thing in the panel. Once there is one they move down next to Statistics, below.
        Loader {
            Layout.fillWidth:   true
            active:             !_areaReady
            sourceComponent:    transectAreaToolsComponent
        }

        ColumnLayout {
            Layout.fillWidth:   true
            spacing:            _margin
            visible:            _areaReady

            TransectStyleComplexItemTabBar {
                id:                 tabBar
                Layout.fillWidth:   true
            }

            // Grid tab
            ColumnLayout {
                Layout.fillWidth:   true
                spacing:            _margin
                visible:            tabBar.currentIndex === 0

                QGCLabel {
                    Layout.fillWidth:   true
                    text:               qsTr("WARNING: Photo interval is below minimum interval (%1 secs) supported by camera.").arg(_cameraMinTriggerInterval.toFixed(1))
                    wrapMode:           Text.WordWrap
                    color:              qgcPal.warningText
                    visible:            _missionItem.cameraShots > 0 && _cameraMinTriggerInterval !== 0 && _cameraMinTriggerInterval > _missionItem.timeBetweenShots
                }

                CameraCalcGrid {
                    Layout.fillWidth:               true
                    cameraCalc:                     _missionItem.cameraCalc
                    vehicleFlightIsFrontal:         true
                    distanceToSurfaceLabel:         qsTr("Altitude")
                    frontalDistanceLabel:           qsTr("Trigger Dist")
                    sideDistanceLabel:              qsTr("Spacing")
                }

                SectionHeader {
                    id:                 transectValuesHeader
                    Layout.fillWidth:   true
                    text:               transectValuesHeaderName
                }

                Loader {
                    Layout.fillWidth:   true
                    visible:            transectValuesHeader.checked
                    sourceComponent:    transectValuesComponent

                    property bool forPresets: false
                }

                QGCButton {
                    Layout.alignment:   Qt.AlignHCenter
                    text:               qsTr("Rotate Entry Point")
                    onClicked:          _missionItem.rotateEntryPoint()
                    visible:            transectValuesHeader.checked
                }

                Loader {
                    Layout.fillWidth:   true
                    active:             _areaReady
                    sourceComponent:    transectAreaToolsComponent
                }

                SectionHeader {
                    id:                 statsHeader
                    Layout.fillWidth:   true
                    text:               qsTr("Statistics")
                }

                TransectStyleComplexItemStats {
                    Layout.fillWidth:   true
                    visible:            statsHeader.checked
                }
            } // Grid Column

            // Camera Tab
            CameraCalcCamera {
                Layout.fillWidth:   true
                visible:            tabBar.currentIndex === 1
                cameraCalc:         _missionItem.cameraCalc
            }

            // Terrain Tab
            TransectStyleComplexItemTerrainFollow {
                Layout.fillWidth:   true
                spacing:            _margin
                visible:            tabBar.currentIndex === 2
                missionItem:        _missionItem
            }

            // Presets Tab
            ColumnLayout {
                Layout.fillWidth:   true
                spacing:            _margin
                visible:            tabBar.currentIndex === 3

                QGCLabel {
                    Layout.fillWidth:   true
                    text:               qsTr("Presets")
                    wrapMode:           Text.WordWrap
                }

                QGCComboBox {
                    id:                 presetCombo
                    Layout.fillWidth:   true
                    model:              _missionItem.presetNames
                }

                RowLayout {
                    Layout.fillWidth:   true

                    QGCButton {
                        Layout.fillWidth:   true
                        text:               qsTr("Apply Preset")
                        enabled:            _missionItem.presetNames.length != 0
                        onClicked:          _missionItem.loadPreset(presetCombo.textAt(presetCombo.currentIndex))
                    }

                    QGCButton {
                        Layout.fillWidth:   true
                        text:               qsTr("Delete Preset")
                        enabled:            _missionItem.presetNames.length != 0
                        onClicked:          deletePresetDialogFactory.open({ presetName: presetCombo.textAt(presetCombo.currentIndex) })

                        QGCPopupDialogFactory {
                            id: deletePresetDialogFactory

                            dialogComponent: deletePresetDialog
                        }

                        Component {
                            id: deletePresetDialog

                            QGCSimpleMessageDialog {
                                title:      qsTr("Delete Preset")
                                text:       qsTr("Are you sure you want to delete '%1' preset?").arg(presetName)
                                buttons:    Dialog.Yes | Dialog.No

                                property string presetName

                                onAccepted: { _missionItem.deletePreset(presetName) }
                            }
                        }
                    }
                }

                Item { height: ScreenTools.defaultFontPixelHeight; width: 1 }

                QGCButton {
                    Layout.alignment:   Qt.AlignCenter
                    Layout.fillWidth:   true
                    text:               qsTr("Save Settings As New Preset")
                    onClicked:          savePresetDialogFactory.open()
                }

                SectionHeader {
                    id:                 presectsTransectValuesHeader
                    Layout.fillWidth:   true
                    text:               transectValuesHeaderName
                    visible:            !!presetsTransectValuesComponent
                }

                Loader {
                    Layout.fillWidth:   true
                    visible:            presectsTransectValuesHeader.checked && !!presetsTransectValuesComponent
                    sourceComponent:    presetsTransectValuesComponent

                    property bool forPresets: true
                }

                SectionHeader {
                    id:                 presetsStatsHeader
                    Layout.fillWidth:   true
                    text:               qsTr("Statistics")
                }

                TransectStyleComplexItemStats {
                    Layout.fillWidth:   true
                    visible:            presetsStatsHeader.checked
                }
            } // Main editing column
        } // Top level  Column

        QGCPopupDialogFactory {
            id: savePresetDialogFactory

            dialogComponent: savePresetDialog
        }

        Component {
            id: savePresetDialog

            QGCPopupDialog {
                id:         popupDialog
                title:      qsTr("Save Preset")
                buttons:    Dialog.Save | Dialog.Cancel

                onAccepted: {
                    if (presetNameField.text != "") {
                        _missionItem.savePreset(presetNameField.text.trim())
                    } else {
                        preventClose = true
                    }
                }

                ColumnLayout {
                    width:      ScreenTools.defaultFontPixelWidth * 30
                    spacing:    ScreenTools.defaultFontPixelHeight

                    QGCLabel {
                        Layout.fillWidth:   true
                        text:               qsTr("Save the current settings as a named preset.")
                        wrapMode:           Text.WordWrap
                    }

                    QGCLabel {
                        text: qsTr("Preset Name")
                    }

                    QGCTextField {
                        id:                 presetNameField
                        Layout.fillWidth:   true
                        placeholderText:    qsTr("Enter preset name")

                        Component.onCompleted:  validateText(presetNameField.text)
                        onTextChanged:          validateText(text)

                        function validateText(text) {
                            if (text.trim() === "") {
                                nameError.text = qsTr("Preset name cannot be blank.")
                                popupDialog.acceptButtonEnabled = false
                            } else if (text.includes("/")) {
                                nameError.text = qsTr("Preset name cannot include the \"/\" character.")
                                popupDialog.acceptButtonEnabled = false
                            } else {
                                nameError.text = ""
                                popupDialog.acceptButtonEnabled = true
                            }
                        }
                    }

                    QGCLabel {
                        id:                 nameError
                        Layout.fillWidth:   true
                        wrapMode:           Text.WordWrap
                        color:              QGroundControl.globalPalette.warningText
                        visible:            text !== ""
                    }
                }
            }
        }
    }
}
