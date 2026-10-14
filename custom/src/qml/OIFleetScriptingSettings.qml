import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

import OI.Controls

/// "Scripting" section of Settings -> Fleet.
///
/// Referenced by name from src/AppSettings/pages/Fleet.SettingsUI.json, which the
/// build turns into FleetSettings.qml. That generated page emits section components
/// as bare type names, so this file has to be a type in a QML module the page
/// imports - hence the OI.Settings module in custom/src/qml/CMakeLists.txt rather
/// than a loose file in custom.qrc.
///
/// The status line follows the *active* vehicle while the sync itself runs per
/// vehicle, so switching focus never interrupts an upload - it only changes which
/// aircraft this page is describing. OIFleetScripts.activeVehicleName names it, so
/// the operator is never guessing which one they are reading about.
///
/// Layout note: SettingsGroupLayout gives its content a fixed width, and a RowLayout
/// will not shrink a child below its implicit width - it overflows the group border
/// instead. So rows stay short and anything long is fillWidth with minimumWidth 0.
Item {
    id:             root
    implicitHeight: mainLayout.implicitHeight

    readonly property var _sync: OIFleetScripts.activeSync

    /// Mirrors OIFleetVehicleSync::StatusLevel. Red is reserved for things that are
    /// actually broken - no vehicle and armed are ordinary states, not faults.
    function _statusColor(level) {
        switch (level) {
        case 1:  return qgcPal.text             // busy
        case 2:  return qgcPal.colorGreen       // in sync
        case 3:  return qgcPal.warningText      // blocked, fixable
        case 4:  return qgcPal.colorRed         // broken
        default: return qgcPal.colorGrey        // nothing to say
        }
    }

    ColumnLayout {
        id:     mainLayout
        width:  parent.width

        SettingsGroupLayout {
            Layout.fillWidth:   true
            heading:            qsTr("Scripting")
            headingDescription: qsTr("If a connected drone is disarmed, scripts checked here are automatically uploaded to it if not present in its SD card's scripts folder. Scripting will need to be enabled to use this feature (SCR_ENABLE = 1), and any changes will be applied immediately after uploads are finished.")

            // ------------------------------------------------------------ status
            QGCLabel {
                Layout.fillWidth:       true
                Layout.minimumWidth:    0
                wrapMode:               Text.WordWrap
                color:                  root._statusColor(_sync ? _sync.statusLevel : 0)
                text: {
                    if (!_sync) {
                        return qsTr("No vehicle connected")
                    }
                    return qsTr("%1: %2").arg(OIFleetScripts.activeVehicleName).arg(_sync.statusText)
                }
            }

            // Scripts the aircraft is carrying that this list does not know about.
            // Reported only - nothing here ever deletes from a vehicle.
            QGCLabel {
                Layout.fillWidth:       true
                Layout.minimumWidth:    0
                wrapMode:               Text.WordWrap
                font.pointSize:         ScreenTools.smallFontPointSize
                visible:                _sync && _sync.unmanagedCount > 0
                text:                   qsTr("%1 script(s) on the vehicle are not in this list: %2")
                                            .arg(_sync ? _sync.unmanagedCount : 0)
                                            .arg(_sync ? _sync.unmanagedScripts.join(", ") : "")
            }

            // ----------------------------------------------------------- the set
            Repeater {
                model: OIFleetScripts.scripts

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
                        color:                  object.valid ? qgcPal.text : qgcPal.warningText
                        text:                   object.valid
                                                    ? qsTr("%1  (%2 KB)").arg(object.fileName).arg((object.sizeBytes / 1024).toFixed(1))
                                                    : qsTr("%1  (file missing)").arg(object.fileName)
                    }

                    QGCButton {
                        text:       qsTr("Remove")
                        onClicked:  OIFleetScripts.removeScript(index)
                    }
                }
            }

            QGCLabel {
                Layout.fillWidth:   true
                text:               qsTr("No scripts imported.")
                visible:            OIFleetScripts.scripts.count === 0
            }

            LabelledButton {
                label:      qsTr("Import a Lua script")
                buttonText: qsTr("Import")
                onClicked:  importDialog.openForLoad()
            }

            LabelledButton {
                label:      qsTr("Re-read scripts from disk and re-check the vehicle")
                buttonText: qsTr("Refresh")
                enabled:    OIFleetScripts.scripts.count > 0
                onClicked:  OIFleetScripts.refreshAll()
            }

            QGCLabel {
                Layout.fillWidth:       true
                Layout.minimumWidth:    0
                wrapMode:               Text.WordWrap
                font.pointSize:         ScreenTools.smallFontPointSize
                text:                   qsTr("Imported scripts are copied to %1").arg(OIFleetScripts.scriptsFolder)
            }
        }
    }

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    QGCFileDialog {
        id:             importDialog
        title:          qsTr("Select a Lua script")
        nameFilters:    [ qsTr("Lua Scripts (*.lua)"), qsTr("All Files (*)") ]

        onAcceptedForLoad: (file) => {
            close()
            if (!file) {
                return
            }
            if (!OIFleetScripts.importScript(file)) {
                QGroundControl.showMessageDialog(root, qsTr("Import Script"), OIFleetScripts.lastError())
            }
        }
    }
}
