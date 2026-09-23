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
///
/// Layout note: SettingsGroupLayout gives its content a fixed width, and a
/// RowLayout will not shrink a child below that child's implicit width - it
/// overflows the group border instead. So every row here stays short, anything
/// long (paths, the reference description) is fillWidth with minimumWidth 0 so it
/// can actually shrink, and the two filters sit on their own lines.
Item {
    id:             root
    implicitHeight: mainLayout.implicitHeight

    ColumnLayout {
        id:     mainLayout
        width:  parent.width

        SettingsGroupLayout {
            Layout.fillWidth:   true
            heading:            qsTr("Map Overlays")
            headingDescription: qsTr("FAA obstacle (.Dat) and KML layers drawn on the Fly view map. Files are read where they are, not copied.")

            Repeater {
                model: OIMapOverlays.layers

                ColumnLayout {
                    Layout.fillWidth:   true
                    spacing:            ScreenTools.defaultFontPixelHeight * 0.25

                    // Line 1: on/off, name, remove.
                    RowLayout {
                        Layout.fillWidth:   true
                        spacing:            ScreenTools.defaultFontPixelWidth

                        QGCCheckBox {
                            checked:    object.enabled
                            onClicked:  object.enabled = checked
                        }

                        QGCLabel {
                            Layout.fillWidth:       true
                            Layout.minimumWidth:    0
                            elide:                  Text.ElideMiddle
                            text:                   qsTr("%1  (%2)").arg(object.name).arg(object.formatName)
                        }

                        QGCButton {
                            text:       qsTr("Remove")
                            onClicked:  OIMapOverlays.removeLayer(index)
                        }
                    }

                    // Line 2: counts and path, on their own line so a long path
                    // cannot push the controls out of the group.
                    QGCLabel {
                        Layout.fillWidth:       true
                        Layout.minimumWidth:    0
                        wrapMode:               Text.WordWrap
                        font.pointSize:         ScreenTools.smallFontPointSize
                        color:                  object.errorString !== "" ? qgcPal.warningText : qgcPal.text
                        text: {
                            if (object.errorString !== "") {
                                return object.errorString
                            }
                            if (object.supportsHeightFilter) {
                                return qsTr("%1 of %2 obstacles shown\n%3")
                                        .arg(object.pointCount).arg(object.totalPointCount).arg(object.filePath)
                            }
                            return qsTr("%1 point(s)\n%2").arg(object.pointCount).arg(object.filePath)
                        }
                    }

                    // Line 3: height filter. Only a DOF carries obstacle heights.
                    RowLayout {
                        Layout.fillWidth:   true
                        Layout.leftMargin:  ScreenTools.defaultFontPixelWidth * 2
                        spacing:            ScreenTools.defaultFontPixelWidth
                        visible:            object.supportsHeightFilter

                        QGCLabel {
                            text:           qsTr("Hide below")
                            font.pointSize: ScreenTools.smallFontPointSize
                        }

                        QGCTextField {
                            text:                   object.minHeightM
                            Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 7
                            onEditingFinished:      object.minHeightM = parseInt(text)
                        }

                        QGCLabel {
                            Layout.fillWidth:       true
                            Layout.minimumWidth:    0
                            elide:                  Text.ElideRight
                            text:                   qsTr("m AGL")
                            font.pointSize:         ScreenTools.smallFontPointSize
                        }
                    }

                    // Line 4: radius filter. This is the one that makes a whole
                    // state file drawable at all.
                    RowLayout {
                        Layout.fillWidth:       true
                        Layout.leftMargin:      ScreenTools.defaultFontPixelWidth * 2
                        Layout.bottomMargin:    ScreenTools.defaultFontPixelHeight * 0.3
                        spacing:                ScreenTools.defaultFontPixelWidth

                        QGCLabel {
                            text:           qsTr("Show within")
                            font.pointSize: ScreenTools.smallFontPointSize
                        }

                        QGCTextField {
                            text:                   object.radiusKm
                            Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 7
                            onEditingFinished:      object.radiusKm = parseFloat(text)
                        }

                        QGCLabel {
                            Layout.fillWidth:       true
                            Layout.minimumWidth:    0
                            elide:                  Text.ElideRight
                            font.pointSize:         ScreenTools.smallFontPointSize
                            text:                   qsTr("km of %1  (0 = no limit)").arg(OIMapOverlays.referenceDescription)
                        }
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
                Layout.fillWidth:       true
                Layout.minimumWidth:    0
                wrapMode:               Text.WordWrap
                color:                  qgcPal.warningText
                visible:                OIMapOverlays.overCap
                text:                   qsTr("%1 obstacles selected, over the %2 limit - nothing is being drawn. Raise the height filter or shrink the radius.")
                                            .arg(OIMapOverlays.selectedCount).arg(OIMapOverlays.maxMarkers)
            }

            QGCLabel {
                Layout.fillWidth:       true
                Layout.minimumWidth:    0
                wrapMode:               Text.WordWrap
                font.pointSize:         ScreenTools.smallFontPointSize
                visible:                OIMapOverlays.layers.count > 0 && !OIMapOverlays.overCap
                text:                   qsTr("%1 marker(s) on the Fly view map.").arg(OIMapOverlays.markerCount)
            }

            LabelledButton {
                label:      qsTr("Import an FAA obstacle or KML layer")
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
        title:          qsTr("Select an FAA obstacle or KML file")
        nameFilters:    [ qsTr("Overlay files (*.dat *.Dat *.DAT *.kml)"),
                          qsTr("FAA obstacle files (*.dat *.Dat *.DAT)"),
                          qsTr("KML files (*.kml)"),
                          qsTr("All Files (*)") ]

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
