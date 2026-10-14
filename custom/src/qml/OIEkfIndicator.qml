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

    /// Off unless the operator asked for it. Once on, shown whenever there is an aircraft and
    /// not only when a report has arrived: a missing EKF report is itself worth seeing, and an
    /// indicator that disappears instead looks like one that was never there.
    ///
    /// Hiding here rather than leaving the indicator out of QGCCorePlugin::toolBarIndicators()
    /// is what makes the toggle take effect at once: that list is built once and cached, so a
    /// plugin-side test would need a restart. FlyViewToolBarIndicators.qml binds each Loader's
    /// visible to this, and the Row it sits in closes the gap.
    property bool showIndicator: _activeVehicle &&
                                 QGroundControl.settingsManager.flyViewSettings.showEkfIndicator.rawValue

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
                        headingDescription: OIEkfStatus.valid ? OIEkfStatus.varianceSummary
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
                        headingDescription: qsTr("Status color indicates a flag's current flight risk.")

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
                                    // The word states the bit; the colour states the risk. A flag
                                    // that is unhealthy but carries no severity therefore reads in
                                    // the normal colour - the aircraft may simply not have the
                                    // sensor, which is information rather than alarm.
                                    color:  modelData.healthy ? qgcPal.text
                                                              : control.severityColor(modelData.severity)
                                    text:   modelData.faultWhenSet ? (modelData.set ? qsTr("Yes") : qsTr("No"))
                                                                   : (modelData.set ? qsTr("On") : qsTr("Off"))
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
