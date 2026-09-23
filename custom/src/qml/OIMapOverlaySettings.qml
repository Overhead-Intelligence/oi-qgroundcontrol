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
/// imports - hence the OI.Settings module in custom/src/qml/CMakeLists.txt rather
/// than a loose file in custom.qrc.
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
            headingDescription: qsTr("KML and FAA obstacle (DOF) layers drawn on the Fly view map. Files are read where they are, not copied.")

            Repeater {
                model: OIMapOverlays.layers

                ColumnLayout {
                    Layout.fillWidth:   true
                    spacing:            0

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
                                text:               qsTr("%1  (%2)").arg(object.name).arg(object.formatName)
                                elide:              Text.ElideMiddle
                            }

                            QGCLabel {
                                Layout.fillWidth:   true
                                font.pointSize:     ScreenTools.smallFontPointSize
                                elide:              Text.ElideMiddle
                                color:              object.errorString !== "" ? qgcPal.warningText : qgcPal.text
                                text: {
                                    if (object.errorString !== "") {
                                        return object.errorString
                                    }
                                    if (object.supportsHeightFilter) {
                                        return qsTr("%1 of %2 obstacles shown  -  %3")
                                                .arg(object.pointCount).arg(object.totalPointCount).arg(object.filePath)
                                    }
                                    return qsTr("%1 point(s)  -  %2").arg(object.pointCount).arg(object.filePath)
                                }
                            }
                        }

                        QGCButton {
                            text:       qsTr("Remove")
                            onClicked:  OIMapOverlays.removeLayer(index)
                        }
                    }

                    // A whole state file is tens of thousands of obstacles. Height
                    // alone does not tame it - Florida still has 4,734 above 200 ft
                    // - so the radius is what actually makes it drawable. Both
                    // controls are per layer.
                    RowLayout {
                        Layout.fillWidth:       true
                        Layout.leftMargin:      ScreenTools.defaultFontPixelWidth * 3
                        Layout.bottomMargin:    ScreenTools.defaultFontPixelHeight * 0.4
                        spacing:                ScreenTools.defaultFontPixelWidth

                        QGCLabel {
                            text:           qsTr("Hide below")
                            font.pointSize: ScreenTools.smallFontPointSize
                            visible:        object.supportsHeightFilter
                        }

                        QGCTextField {
                            text:                   object.minHeightFt
                            visible:                object.supportsHeightFilter
                            Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 8
                            onEditingFinished:      object.minHeightFt = parseInt(text)
                        }

                        QGCLabel {
                            text:           qsTr("ft AGL")
                            font.pointSize: ScreenTools.smallFontPointSize
                            visible:        object.supportsHeightFilter
                        }

                        QGCLabel {
                            text:               qsTr("within")
                            font.pointSize:     ScreenTools.smallFontPointSize
                            Layout.leftMargin:  object.supportsHeightFilter ? ScreenTools.defaultFontPixelWidth : 0
                        }

                        QGCTextField {
                            text:                   object.radiusKm
                            Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 8
                            onEditingFinished:      object.radiusKm = parseFloat(text)
                        }

                        QGCLabel {
                            text:           qsTr("km of %1  (0 = no limit)").arg(OIMapOverlays.referenceDescription)
                            font.pointSize: ScreenTools.smallFontPointSize
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }

            QGCLabel {
                Layout.fillWidth:   true
                text:               qsTr("No overlays imported.")
                visible:            OIMapOverlays.layers.count === 0
            }

            // Nothing is drawn past the cap, rather than an arbitrary subset: a
            // hazard overlay that silently omits obstacles looks complete and is
            // more dangerous than no overlay at all.
            QGCLabel {
                Layout.fillWidth:   true
                wrapMode:           Text.WordWrap
                color:              qgcPal.warningText
                visible:            OIMapOverlays.overCap
                text:               qsTr("%1 obstacles selected, over the %2 limit - nothing is being drawn. Raise the height filter or disable a layer.")
                                        .arg(OIMapOverlays.selectedCount).arg(OIMapOverlays.maxMarkers)
            }

            QGCLabel {
                Layout.fillWidth:   true
                font.pointSize:     ScreenTools.smallFontPointSize
                visible:            OIMapOverlays.layers.count > 0 && !OIMapOverlays.overCap
                text:               qsTr("%1 marker(s) on the Fly view map.").arg(OIMapOverlays.markerCount)
            }

            LabelledButton {
                label:      qsTr("Import a KML or FAA obstacle layer")
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
        title:          qsTr("Select a KML or FAA obstacle file")
        nameFilters:    [ qsTr("Overlay files (*.kml *.dat *.Dat *.DAT)"), qsTr("KML files (*.kml)"),
                          qsTr("FAA obstacle files (*.dat *.Dat *.DAT)"), qsTr("All Files (*)") ]

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
