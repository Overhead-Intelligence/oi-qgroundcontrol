import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

import OI.Controls

/// "Map Overlays" section of Settings -> Maps.
///
/// Referenced by name from src/AppSettings/pages/Maps.SettingsUI.json, which the
/// build turns into MapsSettings.qml. That generated page emits section components
/// as bare type names, so this file has to be a type in a QML module the page
/// imports - hence the OI.Settings module in custom/CMakeLists.txt rather than a
/// loose file in custom.qrc.
///
/// Layers are imported here and drawn on the Fly view map only. Toggling is
/// deliberately not available in flight: it is a pre-flight decision.
Item {
    id:             root
    implicitHeight: mainLayout.implicitHeight

    ColumnLayout {
        id:     mainLayout
        width:  parent.width

        SettingsGroupLayout {
            Layout.fillWidth:   true
            heading:            qsTr("Map Overlays")
            headingDescription: qsTr("KML hazard layers drawn on the Fly view map. Files are read where they are, not copied.")

            Repeater {
                model: OIMapOverlays.layers

                RowLayout {
                    Layout.fillWidth:   true
                    spacing:            ScreenTools.defaultFontPixelWidth

                    QGCCheckBox {
                        checked:    object.enabled
                        onClicked:  object.enabled = checked
                    }

                    ColumnLayout {
                        Layout.fillWidth:   true
                        spacing:            0

                        QGCLabel {
                            Layout.fillWidth:   true
                            text:               object.name
                            elide:              Text.ElideMiddle
                        }

                        QGCLabel {
                            Layout.fillWidth:   true
                            font.pointSize:     ScreenTools.smallFontPointSize
                            elide:              Text.ElideMiddle
                            text:               object.errorString !== ""
                                                    ? object.errorString
                                                    : qsTr("%1 point(s)  -  %2").arg(object.pointCount).arg(object.filePath)
                            color:              object.errorString !== "" ? qgcPal.warningText : qgcPal.text
                        }
                    }

                    QGCButton {
                        text:       qsTr("Remove")
                        onClicked:  OIMapOverlays.removeLayer(index)
                    }
                }
            }

            QGCLabel {
                Layout.fillWidth:   true
                text:               qsTr("No overlays imported.")
                visible:            OIMapOverlays.layers.count === 0
            }

            LabelledButton {
                label:      qsTr("Import a KML hazard layer")
                buttonText: qsTr("Import")
                onClicked:  importDialog.openForLoad()
            }

            LabelledButton {
                label:      qsTr("Re-read every layer from disk")
                buttonText: qsTr("Reload")
                enabled:    OIMapOverlays.layers.count > 0
                onClicked:  OIMapOverlays.reloadAll()
            }
        }
    }

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    QGCFileDialog {
        id:             importDialog
        title:          qsTr("Select a KML hazard layer")
        nameFilters:    [ qsTr("KML files (*.kml)"), qsTr("All Files (*)") ]

        onAcceptedForLoad: (file) => {
            close()
            if (!file) {
                return
            }
            if (!OIMapOverlays.addLayer(file)) {
                QGroundControl.showMessageDialog(root, qsTr("Import Overlay"), OIMapOverlays.lastError())
            }
        }
    }
}
