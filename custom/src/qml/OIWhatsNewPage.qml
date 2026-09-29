import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

/// "What's New" section of Settings -> What's New.
///
/// Release notes for operators, in their words rather than the changelog's. `CHANGELOG.md`
/// explains changes to whoever maintains the code; this explains them to whoever flies with
/// it, and names the tab or window each one is reached from so it can be found without
/// being hunted for.
///
/// ---------------------------------------------------------------------------------------
/// THIS PAGE IS PART OF CUTTING A RELEASE. Replace `releaseVersion`, `features` and `fixes`
/// below with the new release's, wholesale - it describes one release, not a running
/// history. `releaseVersion` must match `custom/VERSION`. See the release steps in
/// README.md and the release conventions in CLAUDE.md.
/// ---------------------------------------------------------------------------------------
///
/// Referenced by name from src/AppSettings/pages/WhatsNew.SettingsUI.json, which the build
/// turns into WhatsNewSettings.qml. That generated page emits section components as bare
/// type names, so this has to be a type in a QML module the page imports - hence the
/// OI.Settings module rather than a loose file in custom.qrc.
///
/// Descriptions are `Text.StyledText`, so `<b>` works and a literal `<` or `&` would have to
/// be escaped. Layout note: SettingsGroupLayout gives its content a fixed width and a
/// RowLayout will not shrink a child below its implicit width, so anything that wraps is
/// fillWidth with minimumWidth 0.
Item {
    id:             root
    implicitHeight: mainLayout.implicitHeight

    readonly property string releaseVersion: "1.1.0"

    readonly property var features: [
        {
            name: qsTr("Keyboard Control"),
            text: qsTr("With this option enabled, you can steer your aircraft, adjust its gimbal, and swap flight modes with fully configurable keyboard inputs. Additional settings allow you to fine-tune control and adjust safety measures. You can configure this feature from the new <b>Keyboard</b> tab in the App Settings.")
        },
        {
            name: qsTr("Map Overlays"),
            text: qsTr("Import KML and FAA Obstacle files to display on your map with configurable visibility around your aircraft. You can configure this feature under <b>Map Overlays</b> in the App Settings.")
        },
        {
            name: qsTr("Custom Actions Rework"),
            text: qsTr("You can now import multiple MAVLink Custom Actions files and toggle them separately for both Fly View and Joystick control. You can configure this feature under <b>Fly View</b> in the App Settings.")
        },
        {
            name: qsTr("Altitude Reference Selector"),
            text: qsTr("Change the altitude frame used by commands sent from the GCS for safer operations over uneven terrain. You can use this feature under <b>Fly View</b> in the App Settings.")
        },
        {
            name: qsTr("Drone GUI Quick Link"),
            text: qsTr("Your fleet's saved UDP connections now also display an accompanying option to open each aircraft's onboard computer configuration page. Aircraft with multiple computers can be set up with multiple links to access each one's page separately. You can use this feature under <b>Comm Links</b> in the App Settings.")
        },
        {
            name: qsTr("Onboard Files"),
            text: qsTr("View your aircraft's file system to freely download/upload files in any location. You can find this feature under the <b>Analyze</b> window.")
        },
        {
            name: qsTr("GCS Location Config"),
            text: qsTr("With an aircraft connected, you can now click on the map to move your GCS.")
        }
    ]

    readonly property var fixes: [
        qsTr("The <b>Flight Modes</b> tab in the Vehicle Configuration window now correctly reads out the full range of an aircraft's configured RC inputs and marks the channel currently mapped to the flight mode switch."),
        qsTr("Mission prompts not initiated by the user no longer block map commands."),
        qsTr("PX4-related settings are no longer visible.")
    ]

    ColumnLayout {
        id:             mainLayout
        anchors.left:   parent.left
        anchors.right:  parent.right
        spacing:        ScreenTools.defaultFontPixelHeight / 2

        QGCLabel {
            Layout.fillWidth:       true
            Layout.minimumWidth:    0
            wrapMode:               Text.WordWrap
            font.pointSize:         ScreenTools.mediumFontPointSize
            font.bold:              true
            text:                   qsTr("New in this release — v%1").arg(root.releaseVersion)
        }

        SettingsGroupLayout {
            Layout.fillWidth:   true
            heading:            qsTr("Features")

            Repeater {
                model: root.features

                ColumnLayout {
                    Layout.fillWidth:       true
                    Layout.minimumWidth:    0
                    spacing:                ScreenTools.defaultFontPixelHeight / 8

                    QGCLabel {
                        Layout.fillWidth:       true
                        Layout.minimumWidth:    0
                        wrapMode:               Text.WordWrap
                        font.bold:              true
                        text:                   modelData.name
                    }

                    QGCLabel {
                        Layout.fillWidth:       true
                        Layout.minimumWidth:    0
                        wrapMode:               Text.WordWrap
                        textFormat:             Text.StyledText
                        text:                   modelData.text
                    }
                }
            }
        }

        SettingsGroupLayout {
            Layout.fillWidth:   true
            heading:            qsTr("Fixes")

            Repeater {
                model: root.fixes

                QGCLabel {
                    Layout.fillWidth:       true
                    Layout.minimumWidth:    0
                    wrapMode:               Text.WordWrap
                    textFormat:             Text.StyledText
                    text:                   modelData
                }
            }
        }
    }
}
