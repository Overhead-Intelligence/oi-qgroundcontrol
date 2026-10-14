import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls

/// Fly view toolbar indicator: a shortcut to the video source and its connection URL.
///
/// Registered through QGCCorePlugin::toolBarIndicators(), which is a supported hook, so no
/// stock QML is overridden to place it. Everything in that list is drawn before the active
/// vehicle's own indicators, so this sits left of anything that appears on connection.
///
/// Reconfiguring video is a routine part of a flight here rather than a one-off setup step,
/// and reaching it meant leaving the Fly view for the settings pages and coming back. The
/// two fields that actually get changed are the source and its URL, so those are edited in
/// place; anything rarer is one button away on the full settings page.
///
/// Icon only, with no readout beside it. The toolbar is short of room and the stream's state
/// is already visible in the video pane itself - a second copy in the toolbar would cost
/// space to say what the operator can see.
Item {
    id:             control
    anchors.top:    parent.top
    anchors.bottom: parent.bottom
    width:          iconRow.width

    /// Hidden when the build has no video support, matching the Video settings page's own
    /// visibility test - a shortcut to a page that is not there would be worse than nothing.
    property bool showIndicator: _videoSettings.userVisible

    property var    _videoSettings: QGroundControl.settingsManager.videoSettings
    property string _source:        _videoSettings.videoSource.rawValue

    /// A MAVLink camera supplies its own stream settings, and the fields below are then
    /// read-only rather than hidden, so the operator can still see what is in use.
    property bool _autoStream: QGroundControl.videoManager.autoStreamConfigured

    /// The URL fact belonging to the selected source, or null for a source that has none -
    /// Disabled, or a USB camera. Mirrors the mapping in Video.SettingsUI.json; keep the two
    /// in step if upstream adds a source.
    property var _urlFact: {
        if (_source === _videoSettings.rtspVideoSource) {
            return _videoSettings.rtspUrl
        }
        if (_source === _videoSettings.tcpVideoSource) {
            return _videoSettings.tcpUrl
        }
        if (_source === _videoSettings.udp264VideoSource ||
            _source === _videoSettings.udp265VideoSource ||
            _source === _videoSettings.mpegtsVideoSource) {
            return _videoSettings.udpUrl
        }
        return null
    }

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    Row {
        id:             iconRow
        anchors.top:    parent.top
        anchors.bottom: parent.bottom

        QGCColoredImage {
            width:              height
            anchors.top:        parent.top
            anchors.bottom:     parent.bottom
            sourceSize.height:  height
            // The same icon the Video settings page uses, so the shortcut and the place it
            // leads to are recognisably the same thing.
            source:             "/InstrumentValueIcons/camera.svg"
            fillMode:           Image.PreserveAspectFit
            color:              qgcPal.buttonText
        }
    }

    MouseArea {
        anchors.fill:   parent
        onClicked:      mainWindow.showIndicatorDrawer(videoPopup, control)
    }

    Component {
        id: videoPopup

        ToolIndicatorPage {
            showExpand: false

            contentComponent: Component {
                ColumnLayout {
                    spacing: ScreenTools.defaultFontPixelHeight / 2

                    SettingsGroupLayout {
                        Layout.fillWidth:   true
                        heading:            qsTr("Video")
                        headingDescription: control._autoStream
                                                ? qsTr("The stream is configured by the vehicle.")
                                                : ""
                        enabled:            !control._autoStream

                        LabelledFactComboBox {
                            label:  qsTr("Source")
                            fact:   control._videoSettings.videoSource
                        }

                        LabelledFactTextField {
                            // Hidden for a source that carries no URL. The fact still has to
                            // resolve to something while hidden, hence the fallback - binding
                            // null here logs a warning on every rebuild of the drawer.
                            visible:    control._urlFact !== null
                            label:      qsTr("Connection URL")
                            fact:       control._urlFact !== null ? control._urlFact
                                                                  : control._videoSettings.rtspUrl
                        }
                    }

                    SettingsGroupLayout {
                        Layout.fillWidth: true

                        LabelledButton {
                            label:      qsTr("Video Settings")
                            buttonText: qsTr("Config")
                            onClicked: {
                                // Untranslated page key from SettingsPages.json - do not qsTr()
                                mainWindow.showSettingsTool("Video")
                                mainWindow.closeIndicatorDrawer()
                            }
                        }
                    }
                }
            }
        }
    }
}
