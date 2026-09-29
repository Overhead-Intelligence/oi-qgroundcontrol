import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls

/// Stock LinkConfigurationManager with the drone GUI controls added to each UDP row.
///
/// Every provisioned OI drone computer serves its own configuration page on port 8088
/// (`drone-gui`), and the hostname it is reached on is already sitting in the link
/// configuration - the operator typed it to make the telemetry link. So the GUI is one
/// click away rather than a hostname retyped into a browser, and a bird with two
/// computers offers both, because a UDP configuration already holds a list of targets.
///
/// This replaces a stock file through the URL interceptor rather than patching `src/`:
/// the row is built inline in the Repeater, so there is nothing smaller to override.
/// Keep the rest of this file in step with upstream on a sync - the only OI content is
/// the `_droneGuiUrls` helper and the three controls after the Connect button.
SettingsGroupLayout {
    id: _root
    heading: qsTr("Links")

    property var _linkManager: QGroundControl.linkManager

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    /// "host:port" targets turned into the GUI address for each one.
    ///
    /// Split on the LAST colon: the port is what we are replacing, and an IPv6 literal
    /// is full of earlier ones. Such a literal also has to be bracketed to sit in a URL,
    /// which is why that is handled here rather than by pasting the string together.
    function _droneGuiUrls(config) {
        if (!config || config.linkType !== LinkConfiguration.TypeUdp || !config.hostList) {
            return []
        }
        var urls = []
        for (var i = 0; i < config.hostList.length; i++) {
            var entry = config.hostList[i]
            var cut = entry.lastIndexOf(":")
            var host = cut === -1 ? entry : entry.substring(0, cut)
            if (host === "") {
                continue
            }
            urls.push(host.indexOf(":") === -1 ? host : "[" + host + "]")
        }
        return urls
    }

    Repeater {
        model: _linkManager.linkConfigurations

        RowLayout {
            id:                 linkRow
            Layout.fillWidth:   true
            visible:            !object.dynamic

            // Recomputed whenever the configuration's own host list changes, so editing a
            // link's addresses updates the dropdown without reopening the page.
            readonly property var _guiHosts: _root._droneGuiUrls(object)
            readonly property bool _showDroneGui: object.linkType === LinkConfiguration.TypeUdp

            QGCLabel {
                Layout.fillWidth:   true
                text:               object.name
            }
            QGCColoredImage {
                height:                 ScreenTools.minTouchPixels
                width:                  height
                sourceSize.height:      height
                fillMode:               Image.PreserveAspectFit
                mipmap:                 true
                smooth:                 true
                color:                  qgcPalEdit.text
                source:                 "/res/pencil.svg"
                enabled:                !object.link

                QGCPalette {
                    id: qgcPalEdit
                    colorGroupEnabled: parent.enabled
                }

                QGCMouseArea {
                    fillItem: parent
                    onClicked: {
                        var editingConfig = _linkManager.startConfigurationEditing(object)
                        linkDialogFactory.open({ editingConfig: editingConfig, originalConfig: object })
                    }
                }
            }
            QGCColoredImage {
                height:                 ScreenTools.minTouchPixels
                width:                  height
                sourceSize.height:      height
                fillMode:               Image.PreserveAspectFit
                mipmap:                 true
                smooth:                 true
                color:                  qgcPalDelete.text
                source:                 "/res/TrashDelete.svg"

                QGCPalette {
                    id: qgcPalDelete
                    colorGroupEnabled: parent.enabled
                }

                QGCMouseArea {
                    fillItem:   parent
                    onClicked:  QGroundControl.showMessageDialog(
                                    _root,
                                    qsTr("Delete Link"),
                                    qsTr("Are you sure you want to delete '%1'?").arg(object.name),
                                    Dialog.Ok | Dialog.Cancel,
                                    function () {
                                        _linkManager.removeConfiguration(object)
                                    })
                }
            }
            QGCButton {
                text:       object.linkActive ? qsTr("Disconnect") : qsTr("Connect")
                onClicked: {
                    if (object.linkActive) {
                        _linkManager.disconnectLinkConfiguration(object)
                    } else {
                        _linkManager.createConnectedLink(object)
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth:  1
                Layout.fillHeight:      true
                Layout.topMargin:       ScreenTools.defaultFontPixelHeight / 4
                Layout.bottomMargin:    ScreenTools.defaultFontPixelHeight / 4
                Layout.leftMargin:      ScreenTools.defaultFontPixelWidth / 2
                Layout.rightMargin:     ScreenTools.defaultFontPixelWidth / 2
                color:                  qgcPal.windowShade
                visible:                linkRow._showDroneGui
            }

            QGCComboBox {
                id:                     droneGuiCombo
                visible:                linkRow._showDroneGui
                // Deliberately capped. A tailnet hostname is long and this row already
                // carries four controls; the dropdown itself shows the full text.
                Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 22
                // Addresses only. The GUI port was shown here at first, on the theory that
                // naming it distinguished it from the telemetry port - but it reads as a
                // link the operator is meant to understand rather than a machine to pick,
                // and the button says what it opens. Which drone is the only question this
                // has to answer.
                model:                  linkRow._guiHosts
                enabled:                linkRow._guiHosts.length > 0
            }

            QGCButton {
                // QGCButton sizes to its text - the background's implicitWidth is only a
                // five character floor - so the wider label needs no explicit width.
                text:       qsTr("Open Device Config")
                visible:    linkRow._showDroneGui
                // Nothing to open with no addresses saved, and nothing sensible to put in
                // the dropdown either, so both go dead rather than the button failing.
                enabled:    linkRow._guiHosts.length > 0
                // Not gated on the link being connected: opening the GUI of a bird that is
                // not talking is one of the times it is most wanted.
                onClicked: {
                    var host = linkRow._guiHosts[droneGuiCombo.currentIndex]
                    if (host) {
                        Qt.openUrlExternally("http://" + host + ":8088")
                    }
                }
            }
        }
    }

    LabelledButton {
        label:      qsTr("Add New Link")
        buttonText: qsTr("Add")

        onClicked: {
            var editingConfig = _linkManager.createConfiguration(ScreenTools.isSerialAvailable ? LinkConfiguration.TypeSerial : LinkConfiguration.TypeUdp, "")
            linkDialogFactory.open({ editingConfig: editingConfig, originalConfig: null })
        }
    }

    QGCPopupDialogFactory {
        id: linkDialogFactory

        dialogComponent: linkDialogComponent
    }

    Component {
        id: linkDialogComponent

        QGCPopupDialog {
            title:                  originalConfig ? qsTr("Edit Link") : qsTr("Add New Link")
            buttons:                Dialog.Save | Dialog.Cancel
            acceptButtonEnabled:    nameField.text !== ""

            property var originalConfig
            property var editingConfig

            onAccepted: {
                linkSettingsLoader.item.saveSettings()
                editingConfig.name = nameField.text
                if (originalConfig) {
                    _linkManager.endConfigurationEditing(originalConfig, editingConfig)
                } else {
                    editingConfig.dynamic = false
                    _linkManager.endCreateConfiguration(editingConfig)
                }
            }

            onRejected: _linkManager.cancelConfigurationEditing(editingConfig)

            ColumnLayout {
                spacing: ScreenTools.defaultFontPixelHeight / 2

                RowLayout {
                    Layout.fillWidth:   true
                    spacing:            ScreenTools.defaultFontPixelWidth

                    QGCLabel { text: qsTr("Name") }
                    QGCTextField {
                        id:                 nameField
                        Layout.fillWidth:   true
                        text:               editingConfig.name
                        placeholderText:    qsTr("Enter name")
                    }
                }

                QGCCheckBoxSlider {
                    Layout.fillWidth:   true
                    text:               qsTr("Automatically Connect on Start")
                    checked:            editingConfig.autoConnect
                    onCheckedChanged:   editingConfig.autoConnect = checked
                }

                QGCCheckBoxSlider {
                    Layout.fillWidth:   true
                    text:               qsTr("High Latency")
                    checked:            editingConfig.highLatency
                    onCheckedChanged:   editingConfig.highLatency = checked
                }

                LabelledComboBox {
                    label:                  qsTr("Type")
                    enabled:                originalConfig == null
                    model:                  _linkManager.linkTypeStrings
                    Component.onCompleted:  comboBox.currentIndex = editingConfig.linkType

                    onActivated: (index) => {
                        if (index !== editingConfig.linkType) {
                            var name = nameField.text
                            editingConfig = _linkManager.createConfiguration(index, name)
                        }
                    }
                }

                Loader {
                    id: linkSettingsLoader

                    // settingsURL is a bare file name - "UdpSettings.qml" - which a Loader
                    // resolves against the directory of the file it is written in. The stock
                    // LinkConfigurationManager.qml lives beside those pages; this override is
                    // served from qrc:/Custom/qml/... where none of them exist, so a relative
                    // source silently loaded nothing and the edit dialog lost every per-type
                    // field below Type. Name the stock directory instead. The interceptor
                    // still gets first refusal on the result, so a /Custom copy of a settings
                    // page would be picked up if one is ever added.
                    readonly property string _stockSettingsDir: "qrc:/qml/QGroundControl/AppSettings/"

                    source: editingConfig && editingConfig.settingsURL
                                ? _stockSettingsDir + editingConfig.settingsURL
                                : ""
                    asynchronous: true

                    property var subEditConfig:         editingConfig
                    property int _firstColumnWidth:     ScreenTools.defaultFontPixelWidth * 12
                    property int _secondColumnWidth:    ScreenTools.defaultFontPixelWidth * 30
                    property int _rowSpacing:           ScreenTools.defaultFontPixelHeight / 2
                    property int _colSpacing:           ScreenTools.defaultFontPixelWidth / 2

                    onStatusChanged: {
                        if (status === Loader.Error) {
                            console.warn("Failed to load link settings page:", source)
                        }
                    }
                }
            }
        }
    }
}
