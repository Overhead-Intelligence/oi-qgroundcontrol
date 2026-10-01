import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

/// "MAVLink Actions" section of Settings -> Fly View.
///
/// Every JSON file in the MavlinkActions folder is listed with a tick box for the Fly View
/// action menu and one for joystick button assignment. More than one file can be on at a
/// time; the settings value is the `;`-separated list of file names that MavlinkActionManager
/// loads, so ticking a box takes effect immediately in both the Fly View menu and the
/// joystick button dropdown without a restart.
///
/// Keeping each set of actions in its own file is the point: actions that only work on some
/// airframes - a servo the platform may not have, a Lua script it may not be running - do not
/// have to be merged into one list that is then wrong for every other aircraft.
///
/// Layout note: SettingsGroupLayout gives its content a fixed width and a RowLayout will not
/// shrink a child below that child's implicit width, so each file gets two short lines rather
/// than one long one, and the file name is fillWidth with minimumWidth 0 so it can elide
/// instead of pushing the buttons through the group border.
SettingsGroupLayout {
    id:                     root
    Layout.fillWidth:       true
    Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 35
    heading:                qsTr("MAVLink Actions")
    headingDescription:     qsTr("Action JSON files in the '%1' folder. Tick the ones each menu should offer.").arg(_savePath)

    property var    _mavlinkActionsSettings:    QGroundControl.settingsManager.mavlinkActionsSettings
    property string _savePath:                  QGroundControl.settingsManager.appSettings.mavlinkActionsSavePath
    property var    _actionFiles:               []

    Component.onCompleted: _refresh()

    function _refresh() {
        _actionFiles = QGCFileDialogController.getFiles(_savePath, ["*.json"])
    }

    /// The enabled file names held by a settings fact. Written with ";" but "," is read too,
    /// because a hand-edited settings override file is likely to use it.
    function _enabledFiles(fact) {
        return String(fact.rawValue).split(/[;,]/).map(function(name) { return name.trim() })
                                                  .filter(function(name) { return name.length > 0 })
    }

    function _isEnabled(fact, fileName) {
        return _enabledFiles(fact).indexOf(fileName) !== -1
    }

    function _setEnabled(fact, fileName, enable) {
        var names = _enabledFiles(fact)
        var index = names.indexOf(fileName)
        if (enable && index === -1) {
            names.push(fileName)
        } else if (!enable && index !== -1) {
            names.splice(index, 1)
        }
        fact.rawValue = names.join(";")
    }

    /// ";" and "," separate names in the settings value, so a file carrying either in its name
    /// could never be switched off again once it was on.
    function _destinationName(path) {
        var base = path.substring(Math.max(path.lastIndexOf("/"), path.lastIndexOf("\\")) + 1)
        return base.replace(/[;,]/g, "_")
    }

    function _import(sourcePath) {
        if (!QGCFileDialogController.copyFile(sourcePath, _savePath + "/" + _destinationName(sourcePath))) {
            QGroundControl.showMessageDialog(root, qsTr("Import Actions"),
                                             qsTr("Could not copy the file into %1.").arg(_savePath))
            return
        }
        _refresh()
    }

    Repeater {
        model: _actionFiles

        ColumnLayout {
            Layout.fillWidth:   true
            spacing:            ScreenTools.defaultFontPixelHeight * 0.25

            property string fileName: modelData

            // Line 1: the file, and getting rid of it.
            RowLayout {
                Layout.fillWidth:   true
                spacing:            ScreenTools.defaultFontPixelWidth

                QGCLabel {
                    Layout.fillWidth:       true
                    Layout.minimumWidth:    0
                    elide:                  Text.ElideMiddle
                    text:                   fileName
                }

                QGCButton {
                    text: qsTr("Remove")
                    onClicked: {
                        QGroundControl.showMessageDialog(
                            root,
                            qsTr("Remove Actions File"),
                            qsTr("Delete %1 from the MavlinkActions folder?").arg(fileName),
                            Dialog.Ok | Dialog.Cancel,
                            function() {
                                QGCFileDialogController.deleteFile(_savePath + "/" + fileName)
                                _setEnabled(_mavlinkActionsSettings.flyViewActionsFile, fileName, false)
                                _setEnabled(_mavlinkActionsSettings.joystickActionsFile, fileName, false)
                                _refresh()
                            })
                    }
                }
            }

            // Line 2: where its actions show up.
            RowLayout {
                Layout.fillWidth:       true
                Layout.leftMargin:      ScreenTools.defaultFontPixelWidth * 2
                Layout.bottomMargin:    ScreenTools.defaultFontPixelHeight * 0.3
                spacing:                ScreenTools.defaultFontPixelWidth * 2

                QGCCheckBox {
                    text:       qsTr("Fly View")
                    checked:    _isEnabled(_mavlinkActionsSettings.flyViewActionsFile, fileName)
                    onClicked:  _setEnabled(_mavlinkActionsSettings.flyViewActionsFile, fileName, checked)
                }

                QGCCheckBox {
                    text:       qsTr("Joystick")
                    checked:    _isEnabled(_mavlinkActionsSettings.joystickActionsFile, fileName)
                    onClicked:  _setEnabled(_mavlinkActionsSettings.joystickActionsFile, fileName, checked)
                }

                Item { Layout.fillWidth: true }
            }
        }
    }

    QGCLabel {
        Layout.fillWidth:       true
        Layout.minimumWidth:    0
        wrapMode:               Text.WordWrap
        font.pointSize:         ScreenTools.smallFontPointSize
        visible:                _actionFiles.length === 0
        text:                   qsTr("No action files yet. Import one, or drop a JSON file into the folder above.")
    }

    LabelledButton {
        label:      qsTr("Add an actions file")
        buttonText: qsTr("Import")
        onClicked:  importDialog.openForLoad()
    }

    QGCFileDialog {
        id:             importDialog
        title:          qsTr("Select a MAVLink actions file")
        nameFilters:    [ qsTr("Actions files (*.json)"), qsTr("All Files (*)") ]

        onAcceptedForLoad: (file) => {
            close()
            if (!file) {
                return
            }

            if (QGCFileDialogController.fileExists(_savePath + "/" + _destinationName(file))) {
                QGroundControl.showMessageDialog(
                    root,
                    qsTr("Import Actions"),
                    qsTr("%1 is already in the MavlinkActions folder. Replace it?").arg(_destinationName(file)),
                    Dialog.Ok | Dialog.Cancel,
                    function() { _import(file) })
                return
            }
            _import(file)
        }
    }
}
