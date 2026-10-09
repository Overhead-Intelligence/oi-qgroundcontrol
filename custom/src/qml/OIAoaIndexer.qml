import QtQuick

import QGroundControl
import QGroundControl.Controls

import OI.Controls

/// Angle of attack indexer, laid horizontally as a header across the panel.
///
/// Bands and pointer follow Mission Planner's, so a reading means the same thing in either
/// ground station: blue below 10%, green to 60%, yellow to 90%, red beyond, with the pointer
/// at angle of attack as a fraction of the critical angle. Low AOA at the left, stall at the
/// right.
///
/// This is the most direct stall indication available, and it is better than a marked airspeed
/// tape for the job: stall *speed* moves with bank angle and load factor, critical *angle* does
/// not. A pilot pulling in a turn can be above the marked stall speed and still stalling.
///
/// **It is an estimate.** ArduPilot derives angle of attack from EKF velocity minus wind, with
/// no vane, so below a usable airspeed the arithmetic stops meaning anything. The indexer
/// greys out there rather than parking the pointer in the blue band, which would read as
/// "plenty of margin" at exactly the moment the number is worthless.
Item {
    id: control

    property var vehicle: null

    /// Mission Planner's band edges, as percentages of the bar.
    readonly property real _blueTo:   10
    readonly property real _greenTo:  60
    readonly property real _yellowTo: 90

    readonly property real _minAirspeed: vehicle ? Math.max(vehicle.minimumEquivalentAirspeed(), 1) : 1
    readonly property real _airspeed:    vehicle ? vehicle.airSpeed.rawValue : 0
    readonly property bool _usable:      vehicle && OIFlightAngles.valid && (_airspeed >= _minAirspeed)

    /// Pointer position as a percentage across the bar. Zero AOA sits at the bottom of the
    /// green band and the critical angle at the bottom of the red, which is Mission Planner's
    /// mapping - the blue band is margin below level flight, not part of the usable range.
    readonly property real _pointerPercent:
        Math.max(0, Math.min(100, _blueTo + (OIFlightAngles.criticalFraction * (_yellowTo - _blueTo))))

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    Row {
        anchors.fill:   parent
        opacity:        control._usable ? 1 : 0.35

        Rectangle { width: control.width * control._blueTo / 100;                     height: parent.height; color: "#1f6fd0" }
        Rectangle { width: control.width * (control._greenTo - control._blueTo) / 100; height: parent.height; color: "#1f9d3a" }
        Rectangle { width: control.width * (control._yellowTo - control._greenTo) / 100; height: parent.height; color: "#d8c020" }
        Rectangle { width: control.width * (100 - control._yellowTo) / 100;            height: parent.height; color: "#c8321f" }
    }

    // The pointer. Black against every band, which is why Mission Planner uses it too - there
    // is no single palette colour that stays legible across blue, green, yellow and red.
    Rectangle {
        width:      Math.max(2, control.width * 0.004)
        height:     parent.height
        color:      "black"
        visible:    control._usable
        x:          (control.width * control._pointerPercent / 100) - (width / 2)
    }

    // Says why it is greyed, rather than leaving a dim bar to be read as a dim reading.
    QGCLabel {
        anchors.centerIn:   parent
        font.pointSize:     ScreenTools.smallFontPointSize
        color:              qgcPal.text
        visible:            !control._usable
        text:               qsTr("AOA unavailable")
    }
}
