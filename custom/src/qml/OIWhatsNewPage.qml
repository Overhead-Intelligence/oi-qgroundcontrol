import QtQuick
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
/// THIS PAGE IS PART OF CUTTING A RELEASE. **Prepend** the new release to `releases` below -
/// do not replace what is there. The page is a browsable history now, and an operator
/// coming to a build several versions late is the normal case rather than the exception:
/// most of the org skipped 1.1.0 entirely, and a customer will skip more than that. The
/// first entry is the one the page opens on, and its `version` must match `custom/VERSION`.
/// See the release steps in README.md and the release conventions in CLAUDE.md.
/// ---------------------------------------------------------------------------------------
///
/// Referenced by name from src/AppSettings/pages/WhatsNew.SettingsUI.json, which the build
/// turns into WhatsNewSettings.qml. That generated page emits section components as bare
/// type names, so this has to be a type in a QML module the page imports - hence the
/// OI.Settings module rather than a loose file in custom.qrc.
///
/// The notes stay in QML rather than moving to a JSON resource so that `qsTr()` keeps
/// reaching them - strings in a data file are invisible to lupdate, and QGC is translated.
///
/// Descriptions are `Text.StyledText`, so `<b>` and `<i>` work and a literal `<` or `&`
/// would have to be escaped. Layout note: SettingsGroupLayout gives its content a fixed
/// width and a RowLayout will not shrink a child below its implicit width, so anything that
/// wraps is fillWidth with minimumWidth 0.
Item {
    id:             root
    implicitHeight: mainLayout.implicitHeight

    // Twice the stock settings column. SettingsPage.qml sizes itself to
    // Math.max(implicitWidth, 50 characters), so asking for more widens this page and
    // leaves every other one alone. These are paragraphs rather than labelled controls,
    // and at the stock width they wrap into a narrow ribbon.
    implicitWidth:  ScreenTools.defaultFontPixelWidth * 100

    /// Newest first. The page opens on entry 0; Older/Newer walk the list.
    readonly property var releases: [
        {
            version: "1.2.0",
            features: [
                {
                    name: qsTr("Fleet-Wide Scripting"),
                    text: qsTr("Import Lua scripts to be safely applied to any connected drone, with detailed status reports and indications when the active drone's script layout doesn't match fleet standard. You can configure this feature from the new <b>Fleet</b> tab in the App Settings.")
                },
                {
                    name: qsTr("Advanced Instrument Panel"),
                    text: qsTr("Turn QGC's native instrument panel into a complete AHRS display, which now reports speed/altitude in their active frame, yaw/pitch rates, slip/skid, angle of attack, and more. You can enable this feature under <b>Fly View</b>'s <i>Instrument Panel</i> section in the App Settings.")
                },
                {
                    name: qsTr("Streamlined Mission Planning"),
                    text: qsTr("Plan View defaults now reflect the OI standard configuration. Templates are automatically configured for the Skylord platform, and a new template quickly sets up a plan to have your drone loiter over its launch point. The UI has also been adjusted for a simpler planning experience.")
                },
                {
                    name: qsTr("EKF Health Checker"),
                    text: qsTr("A new indicator in the Fly View reports EKF health metrics with dynamic coloration to indicate current flight risk. You can enable this indicator under <b>Fly View</b>'s <i>General</i> section in the App Settings.")
                },
                {
                    name: qsTr("Quick Disconnect"),
                    text: qsTr("Disconnect from the active drone at any time from the Fly View, with safety measures for mid-flight attempts.")
                },
                {
                    name: qsTr("Quick Reconnect"),
                    text: qsTr("Drones rebooted from the GCS are automatically reconnected once they become available.")
                },
                {
                    name: qsTr("Quick Video Settings"),
                    text: qsTr("Edit your stream link and format from the Fly View, with a shortcut to the full Video settings tab for advanced configuration. You can enable this indicator under <b>Fly View</b>'s <i>General</i> section in the App Settings.")
                },
                {
                    name: qsTr("Quick Voltage Source Config"),
                    text: qsTr("You can now set the flight controller's voltage source directly within the <b>Battery Failsafe</b> settings of the Vehicle Configuration page.")
                },
                {
                    name: qsTr("Map Overlay Saving"),
                    text: qsTr("Overlay files are now saved within the app folder for better organization.")
                }
            ],
            fixes: [
                qsTr("Several issues affecting UDP connection reliability have been addressed."),
                qsTr("If the GCS saw a drone's launch and is disconnected mid-flight, it will now attempt to resume its flight time and distance track upon reconnect."),
                qsTr("The Motors tab of the Vehicle Configuration page now properly detects the airframe's motor count. The UI has also been reworked with more detail, additional settings, and new safety measures."),
                qsTr("The GCS and app icons have been changed for increased visibility against various backgrounds.")
            ]
        },
        {
            version: "1.1.0",
            features: [
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
            ],
            fixes: [
                qsTr("The <b>Flight Modes</b> tab in the Vehicle Configuration window now correctly reads out the full range of an aircraft's configured RC inputs and marks the channel currently mapped to the flight mode switch."),
                qsTr("Mission prompts not initiated by the user no longer block map commands."),
                qsTr("PX4-related settings are no longer visible.")
            ]
        }
    ]

    property int _index: 0

    readonly property var  _release:  releases[_index]
    readonly property bool _isCurrent: _index === 0

    ColumnLayout {
        id:             mainLayout
        anchors.left:   parent.left
        anchors.right:  parent.right
        spacing:        ScreenTools.defaultFontPixelHeight / 2

        RowLayout {
            Layout.fillWidth:   true
            spacing:            ScreenTools.defaultFontPixelWidth

            QGCLabel {
                Layout.fillWidth:       true
                Layout.minimumWidth:    0
                wrapMode:               Text.WordWrap
                font.pointSize:         ScreenTools.mediumFontPointSize
                font.bold:              true
                // "New in this release" is only true of the newest entry. Saying it over
                // an older one would misdescribe what the operator is looking at.
                text:                   root._isCurrent
                                            ? qsTr("New in this release — v%1").arg(root._release.version)
                                            : qsTr("Release v%1").arg(root._release.version)
            }

            QGCLabel {
                font.pointSize: ScreenTools.smallFontPointSize
                visible:        root.releases.length > 1
                text:           qsTr("%1 of %2").arg(root._index + 1).arg(root.releases.length)
            }

            QGCButton {
                text:       qsTr("Newer")
                visible:    root.releases.length > 1
                enabled:    root._index > 0
                onClicked:  root._index--
            }

            QGCButton {
                text:       qsTr("Older")
                visible:    root.releases.length > 1
                enabled:    root._index < root.releases.length - 1
                onClicked:  root._index++
            }
        }

        SettingsGroupLayout {
            Layout.fillWidth:   true
            heading:            qsTr("Features")
            visible:            root._release.features.length > 0

            Repeater {
                model: root._release.features

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
            visible:            root._release.fixes.length > 0

            Repeater {
                model: root._release.fixes

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
