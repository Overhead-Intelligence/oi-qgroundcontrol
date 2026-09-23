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
            property string selectedSize:   ""
            property string statusMessage:  ""
            property bool   cancelRequested: false

            /// Anything past this prompts first. MAVLink FTP over a telemetry link
            /// runs at a few KB/s, so a megabyte is already minutes of transfer that
            /// cannot be called off - see the note on the confirmation below.
            readonly property int largeDownloadBytes: 1024 * 1024

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
                root.selectedSize = ""
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

            function startDownload() {
                downloadDialog.title = qsTr("Download %1").arg(root.selectedName)
                downloadDialog.folder = QGroundControl.settingsManager.appSettings.missionSavePath
                downloadDialog.openForLoad()
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
                    root.cancelRequested = false
                    if (error.length > 0) {
                        QGroundControl.showMessageDialog(onboardFilesPage, qsTr("Upload"), error)
                    } else {
                        root.refresh()
                    }
                }

                onDeleteComplete: (remotePath, error) => {
                    root.cancelRequested = false
                    if (error.length > 0) {
                        QGroundControl.showMessageDialog(onboardFilesPage, qsTr("Delete"), error)
                    } else {
                        root.refresh()
                    }
                }

                onDownloadComplete: (filePath, error) => {
                    var wasCancelled = root.cancelRequested
                    root.cancelRequested = false
                    if (wasCancelled) {
                        // Not a failure - the operator asked for this. The vehicle may
                        // still be draining its burst for a few seconds.
                        root.statusMessage = qsTr("Download cancelled. The vehicle may keep sending briefly; wait before the next action.")
                        return
                    }
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
                                    root.selectedSize = modelData.size
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
                        var bytes = parseInt(root.selectedSize, 10)
                        if (!isNaN(bytes) && bytes >= root.largeDownloadBytes) {
                            // Cancelling a burst does not stop the vehicle sending: ArduPilot
                            // streams a burst read from a blocking loop and will not service a
                            // TerminateSession until it finishes. Say so before they commit.
                            QGroundControl.showMessageDialog(
                                onboardFilesPage,
                                qsTr("Large Download"),
                                qsTr("%1 is %2.\n\n").arg(root.selectedName).arg(root.sizeText(root.selectedSize)) +
                                qsTr("Over a telemetry link MAVLink FTP moves a few KB per second, so this can take many minutes.\n\n") +
                                qsTr("Cancelling will stop saving the file but will NOT stop the vehicle sending it, ") +
                                qsTr("and the file list stays unusable until the transfer drains.\n\n") +
                                qsTr("Download anyway?"),
                                Dialog.Ok | Dialog.Cancel,
                                function() { root.startDownload() })
                            return
                        }
                        root.startDownload()
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
                    text:       qsTr("Cancel")
                    visible:    ftpController.busy
                    enabled:    !root.cancelRequested
                    onClicked: {
                        root.cancelRequested = true
                        ftpController.cancelActiveOperation()
                    }
                }

                Item { Layout.fillWidth: true }

                // The panel disables itself while an operation runs, so it has to say
                // why or it reads as a hang. The cancelling case is the important one:
                // ArduPilot streams a burst read from a blocking loop and will not
                // service a TerminateSession until that loop ends, so a cancelled
                // download keeps arriving for several seconds after the click.
                QGCLabel {
                    Layout.fillWidth:       true
                    Layout.minimumWidth:    0
                    elide:                  Text.ElideRight
                    visible:                text !== ""
                    color:                  root.cancelRequested ? qgcPal.warningText : qgcPal.text
                    text: {
                        if (root.cancelRequested) {
                            return qsTr("Cancelling - the vehicle is still sending, waiting for it to stop...")
                        }
                        if (ftpController.downloadInProgress) {
                            return qsTr("Downloading... %1%").arg(Math.round(ftpController.progress * 100))
                        }
                        if (ftpController.uploadInProgress) {
                            return qsTr("Uploading... %1%").arg(Math.round(ftpController.progress * 100))
                        }
                        if (ftpController.listInProgress) {
                            return qsTr("Listing %1 ...").arg(root.browsePath)
                        }
                        if (ftpController.deleteInProgress) {
                            return qsTr("Deleting...")
                        }
                        return ""
                    }
                }

                // An operation's error wins over the last success message; openDirectory()
                // clears the success message so it never outlives the folder it belongs to.
                QGCLabel {
                    Layout.fillWidth:       true
                    Layout.minimumWidth:    0
                    elide:                  Text.ElideMiddle
                    text:                   (ftpController.errorString.length > 0 && !root.cancelRequested)
                                                ? ftpController.errorString : root.statusMessage
                    color:                  (ftpController.errorString.length > 0 && !root.cancelRequested)
                                                ? qgcPal.warningText : qgcPal.text
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
