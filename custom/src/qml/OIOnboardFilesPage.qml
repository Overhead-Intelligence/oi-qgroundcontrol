import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.AnalyzeView
import QGroundControl.Controls

// OI addition (not an override of a stock file): a MAVLink FTP browser for the
// vehicle's onboard storage, registered as an Analyze page by
// OIPlugin::analyzePages(). That registration passes requiresVehicle = true, so
// AnalyzeView shows "Requires a connected vehicle" until one is connected and
// tears the page down again when it disconnects.
//
// None of the transfer work lives here. FTPController (src/Vehicle/FTPController.h)
// is upstream's QML-facing wrapper around FTPManager and resolves the active
// vehicle itself; this file is only the browser on top of it. The same controller
// drives the Lua script list in src/AutoPilotPlugins/Common/ScriptingComponent.qml,
// which is where the directory-entry parsing below comes from.
AnalyzePage {
    id:                 onboardFilesPage
    pageComponent:      _pageComponent
    pageDescription:    qsTr("Browse the vehicle's onboard storage over MAVLink FTP: scripts, terrain data and logs. Open a folder to list it, or select a file to download or delete it.")

    Component {
        id: _pageComponent

        ColumnLayout {
            id:         root
            width:      availableWidth
            height:     availableHeight
            spacing:    ScreenTools.defaultFontPixelHeight * 0.5

            readonly property string rootPath: "/"

            property string browsePath:     rootPath
            property string selectedName:   ""
            property bool   selectedIsDir:  false
            property string statusMessage:  ""

            // MAVLink FTP list entries arrive as "D<name>" for a directory and
            // "F<name>\t<size>" for a file ("S" marks a skipped entry). Directories
            // sort first. "." and ".." are dropped in favour of the Up button.
            readonly property var entries: {
                var dirs = []
                var files = []
                var raw = ftpController.directoryEntries
                for (var i = 0; i < raw.length; i++) {
                    var entry = raw[i]
                    if (!entry || entry.length < 2) {
                        continue
                    }
                    var kind = entry.charAt(0)
                    var fields = entry.slice(1).split("\t")
                    var name = fields[0]
                    if (name === "." || name === "..") {
                        continue
                    }
                    if (kind === "D") {
                        dirs.push({ name: name, isDir: true, size: "" })
                    } else if (kind === "F") {
                        files.push({ name: name, isDir: false, size: fields.length > 1 ? fields[1] : "" })
                    }
                }
                dirs.sort(function(a, b) { return a.name.localeCompare(b.name) })
                files.sort(function(a, b) { return a.name.localeCompare(b.name) })
                return dirs.concat(files)
            }

            function joinPath(dir, name) {
                return dir.endsWith("/") ? dir + name : dir + "/" + name
            }

            function parentPath(path) {
                var trimmed = (path.length > 1 && path.endsWith("/")) ? path.slice(0, -1) : path
                var index = trimmed.lastIndexOf("/")
                return index <= 0 ? "/" : trimmed.slice(0, index + 1)
            }

            function openDirectory(path) {
                root.selectedName = ""
                root.statusMessage = ""
                root.browsePath = path.endsWith("/") ? path : path + "/"
                ftpController.listDirectory(root.browsePath)
            }

            function refresh() {
                openDirectory(root.browsePath)
            }

            function sizeText(bytes) {
                var value = parseInt(bytes, 10)
                if (isNaN(value)) {
                    return ""
                }
                if (value < 1024) {
                    return qsTr("%1 B").arg(value)
                }
                if (value < 1024 * 1024) {
                    return qsTr("%1 KiB").arg((value / 1024).toFixed(1))
                }
                return qsTr("%1 MiB").arg((value / (1024 * 1024)).toFixed(1))
            }

            function reportFailure(title, fallback) {
                var detail = ftpController.errorString.length > 0 ? ftpController.errorString : fallback
                QGroundControl.showMessageDialog(onboardFilesPage, title, detail)
            }

            Component.onCompleted: openDirectory(rootPath)

            QGCPalette { id: qgcPal; colorGroupEnabled: true }

            FTPController {
                id: ftpController

                onUploadComplete: (remotePath, error) => {
                    if (error.length > 0) {
                        QGroundControl.showMessageDialog(onboardFilesPage, qsTr("Upload"), error)
                    } else {
                        root.refresh()
                    }
                }

                onDeleteComplete: (remotePath, error) => {
                    if (error.length > 0) {
                        QGroundControl.showMessageDialog(onboardFilesPage, qsTr("Delete"), error)
                    } else {
                        root.refresh()
                    }
                }

                onDownloadComplete: (filePath, error) => {
                    if (error.length > 0) {
                        QGroundControl.showMessageDialog(onboardFilesPage, qsTr("Download"), error)
                    } else {
                        root.statusMessage = qsTr("Downloaded to %1").arg(filePath)
                    }
                }
            }

            // ---- Path bar and actions ------------------------------------------------

            RowLayout {
                Layout.fillWidth:   true
                spacing:            ScreenTools.defaultFontPixelWidth

                QGCButton {
                    text:       qsTr("Up")
                    enabled:    !ftpController.busy && root.browsePath !== root.rootPath
                    onClicked:  root.openDirectory(root.parentPath(root.browsePath))
                }

                QGCButton {
                    text:       qsTr("Refresh")
                    enabled:    !ftpController.busy
                    onClicked:  root.refresh()
                }

                QGCLabel {
                    Layout.fillWidth:   true
                    text:               root.browsePath
                    elide:              Text.ElideMiddle
                }

                QGCButton {
                    text:       qsTr("Upload here...")
                    enabled:    !ftpController.busy
                    onClicked: {
                        uploadDialog.folder = QGroundControl.settingsManager.appSettings.missionSavePath
                        uploadDialog.openForLoad()
                    }
                }
            }

            // ---- Directory listing ---------------------------------------------------

            QGCFlickable {
                id:                 entryFlickable
                Layout.fillWidth:   true
                Layout.fillHeight:  true
                contentWidth:       entryColumn.width
                contentHeight:      entryColumn.height
                clip:               true

                ColumnLayout {
                    id:         entryColumn
                    width:      entryFlickable.width
                    spacing:    0

                    Repeater {
                        model: root.entries

                        Rectangle {
                            Layout.fillWidth:       true
                            Layout.preferredHeight: entryRow.height + ScreenTools.defaultFontPixelHeight * 0.4
                            color:                  root.selectedName === modelData.name ? qgcPal.buttonHighlight : "transparent"

                            RowLayout {
                                id:                     entryRow
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left:           parent.left
                                anchors.right:          parent.right
                                anchors.leftMargin:     ScreenTools.defaultFontPixelWidth
                                anchors.rightMargin:    ScreenTools.defaultFontPixelWidth
                                spacing:                ScreenTools.defaultFontPixelWidth

                                QGCColoredImage {
                                    // Layout.preferred* rather than width/height: this item is
                                    // managed by the RowLayout, which makes setting them directly
                                    // undefined behaviour (Quick.layout-positioning).
                                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight
                                    Layout.preferredWidth:  ScreenTools.defaultFontPixelHeight
                                    sourceSize.height:      ScreenTools.defaultFontPixelHeight
                                    fillMode:               Image.PreserveAspectFit
                                    source:                 modelData.isDir ? "/InstrumentValueIcons/folder.svg" : "/InstrumentValueIcons/document.svg"
                                    color:                  root.selectedName === modelData.name ? qgcPal.buttonHighlightText : qgcPal.text
                                }

                                QGCLabel {
                                    Layout.fillWidth:   true
                                    text:               modelData.name
                                    elide:              Text.ElideMiddle
                                    color:              root.selectedName === modelData.name ? qgcPal.buttonHighlightText : qgcPal.text
                                }

                                QGCLabel {
                                    text:       modelData.isDir ? "" : root.sizeText(modelData.size)
                                    color:      root.selectedName === modelData.name ? qgcPal.buttonHighlightText : qgcPal.text
                                }
                            }

                            QGCMouseArea {
                                fillItem:   parent
                                enabled:    !ftpController.busy
                                onClicked: {
                                    root.selectedName = modelData.name
                                    root.selectedIsDir = modelData.isDir
                                }
                                onDoubleClicked: {
                                    if (modelData.isDir) {
                                        root.openDirectory(root.joinPath(root.browsePath, modelData.name))
                                    }
                                }
                            }
                        }
                    }

                    QGCLabel {
                        Layout.fillWidth:       true
                        Layout.topMargin:       ScreenTools.defaultFontPixelHeight
                        horizontalAlignment:    Text.AlignHCenter
                        text:                   qsTr("Empty folder")
                        visible:                root.entries.length === 0 && !ftpController.busy
                    }
                }
            }

            // ---- Selection actions and transfer status -------------------------------

            RowLayout {
                Layout.fillWidth:   true
                spacing:            ScreenTools.defaultFontPixelWidth

                QGCButton {
                    text:       qsTr("Open")
                    visible:    root.selectedName !== "" && root.selectedIsDir
                    enabled:    !ftpController.busy
                    onClicked:  root.openDirectory(root.joinPath(root.browsePath, root.selectedName))
                }

                QGCButton {
                    text:       qsTr("Download")
                    visible:    root.selectedName !== "" && !root.selectedIsDir
                    enabled:    !ftpController.busy
                    onClicked: {
                        downloadDialog.title = qsTr("Download %1").arg(root.selectedName)
                        downloadDialog.folder = QGroundControl.settingsManager.appSettings.missionSavePath
                        downloadDialog.openForLoad()
                    }
                }

                QGCButton {
                    text:       qsTr("Delete")
                    visible:    root.selectedName !== "" && !root.selectedIsDir
                    enabled:    !ftpController.busy
                    onClicked: {
                        var remotePath = root.joinPath(root.browsePath, root.selectedName)
                        var confirm = qsTr("Are you sure you want to delete \"%1\" from the vehicle? This action cannot be undone.").arg(remotePath)
                        QGroundControl.showMessageDialog(onboardFilesPage, qsTr("Delete File"), confirm, Dialog.Ok | Dialog.Cancel, function() {
                            if (!ftpController.deleteFile(remotePath)) {
                                root.reportFailure(qsTr("Delete"), qsTr("Delete failed"))
                            }
                        })
                    }
                }

                QGCButton {
                    text:       qsTr("Cancel Operation")
                    visible:    ftpController.busy
                    onClicked:  ftpController.cancelActiveOperation()
                }

                Item { Layout.fillWidth: true }

                QGCLabel {
                    text:       qsTr("Transferring... %1%").arg(Math.round(ftpController.progress * 100))
                    visible:    ftpController.busy && (ftpController.downloadInProgress || ftpController.uploadInProgress)
                }

                // An operation's error wins over the last success message; openDirectory()
                // clears the success message so it never outlives the folder it belongs to.
                QGCLabel {
                    text:       ftpController.errorString.length > 0 ? ftpController.errorString : root.statusMessage
                    color:      ftpController.errorString.length > 0 ? qgcPal.warningText : qgcPal.text
                    elide:      Text.ElideMiddle
                }
            }

            // ---- File dialogs --------------------------------------------------------

            QGCFileDialog {
                id:             uploadDialog
                title:          qsTr("Select a file to upload to %1").arg(root.browsePath)
                nameFilters:    [ qsTr("All Files (*)") ]

                onAcceptedForLoad: (file) => {
                    close()
                    if (!file) {
                        return
                    }
                    var normalized = file.replace(/\\/g, "/")
                    var parts = normalized.split("/")
                    var fileName = parts[parts.length - 1]
                    if (fileName.length === 0) {
                        return
                    }
                    if (!ftpController.uploadFile(file, root.joinPath(root.browsePath, fileName))) {
                        root.reportFailure(qsTr("Upload"), qsTr("Upload failed"))
                    }
                }
            }

            QGCFileDialog {
                id:             downloadDialog
                selectFolder:   true

                onAcceptedForLoad: (folder) => {
                    close()
                    if (!folder) {
                        return
                    }
                    var remotePath = root.joinPath(root.browsePath, root.selectedName)
                    if (!ftpController.downloadFile(remotePath, folder, root.selectedName)) {
                        root.reportFailure(qsTr("Download"), qsTr("Download failed"))
                    }
                }
            }
        }
    }
}
