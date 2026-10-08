import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

import OI.Controls

/// Fly view toolbar indicator for EKF health.
///
/// Registered through QGCCorePlugin::toolBarIndicators(), a supported hook, so no stock QML is
/// overridden to place it. Everything in that list draws before the active vehicle's own
/// indicators, and this one is ordered ahead of the gimbal readout - the only other entry that
/// comes and goes with the aircraft - so it holds the same place on screen whatever is fitted.
///
/// Icon only. The number that matters changes several times a second and is meaningless
/// without its name, so a readout in the toolbar would be noise; the colour carries the one
/// bit an operator needs at a glance and the popup carries the rest.
Item {
    id:             control
    anchors.top:    parent.top
    anchors.bottom: parent.bottom
    width:          iconRow.width

    /// Shown whenever there is an aircraft, not only when a report has arrived. A missing EKF
    /// report is itself worth seeing - an indicator that disappears instead looks like one that
    /// was never there.
    property bool showIndicator: _activeVehicle

    property var _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    function severityColor(severity) {
        if (severity === OIEkfStatus.SeverityCritical) {
            return qgcPal.colorRed
        }
        if (severity === OIEkfStatus.SeverityWarning) {
            return qgcPal.colorOrange
        }
        return qgcPal.text
    }

    Row {
        id:             iconRow
        anchors.top:    parent.top
        anchors.bottom: parent.bottom

        QGCColoredImage {
            width:              height
            anchors.top:        parent.top
            anchors.bottom:     parent.bottom
            sourceSize.height:  height
            source:             "/InstrumentValueIcons/stethoscope.svg"
            fillMode:           Image.PreserveAspectFit
            // Grey rather than white with no report: white is the nominal colour, and showing
            // nominal for an estimate nobody has heard from would be a lie.
            color:              OIEkfStatus.valid ? control.severityColor(OIEkfStatus.severity)
                                                  : qgcPal.colorGrey
        }
    }

    MouseArea {
        anchors.fill:   parent
        onClicked:      mainWindow.showIndicatorDrawer(ekfPopup, control)
    }

    Component {
        id: ekfPopup

        ToolIndicatorPage {
            showExpand: false

            contentComponent: Component {
                ColumnLayout {
                    spacing: ScreenTools.defaultFontPixelHeight / 2

                    SettingsGroupLayout {
                        Layout.fillWidth:   true
                        heading:            qsTr("EKF Variances")
                        headingDescription: OIEkfStatus.valid ? OIEkfStatus.summary
                                                              : qsTr("No EKF report from the aircraft.")

                        Repeater {
                            model: OIEkfStatus.variances

                            RowLayout {
                                Layout.fillWidth:   true
                                spacing:            ScreenTools.defaultFontPixelWidth

                                QGCLabel {
                                    Layout.fillWidth:       true
                                    Layout.minimumWidth:    0
                                    elide:                  Text.ElideRight
                                    text:                   modelData.name
                                }

                                QGCLabel {
                                    color:  control.severityColor(modelData.severity)
                                    text:   modelData.value.toFixed(2)
                                }
                            }
                        }
                    }

                    SettingsGroupLayout {
                        Layout.fillWidth:   true
                        heading:            qsTr("EKF Flags")
                        // Nine of these report a capability by being present and three report a
                        // fault the same way, so "set" alone does not say whether a row is good
                        // news. The tick is what the row should read, not what the bit says.
                        headingDescription: qsTr("Shown as healthy or not, since some flags are faults when set.")

                        Repeater {
                            model: OIEkfStatus.flags

                            RowLayout {
                                Layout.fillWidth:   true
                                spacing:            ScreenTools.defaultFontPixelWidth

                                QGCLabel {
                                    Layout.fillWidth:       true
                                    Layout.minimumWidth:    0
                                    elide:                  Text.ElideRight
                                    text:                   modelData.name
                                }

                                QGCLabel {
                                    // An unhealthy flag that carries no severity still reads as a
                                    // cross, just an uncoloured one: the aircraft may simply not
                                    // have the sensor, and that is information rather than alarm.
                                    color:  modelData.healthy ? qgcPal.text
                                                              : control.severityColor(modelData.severity)
                                    text:   modelData.healthy ? qsTr("OK") : qsTr("NO")
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
