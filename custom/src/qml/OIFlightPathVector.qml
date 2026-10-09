import QtQuick

import QGroundControl
import QGroundControl.Controls

import OI.Controls

/// Flight path vector: where the aircraft is going, as opposed to where it is pointing.
///
/// Displaced from the aircraft reference by angle of attack vertically and sideslip
/// horizontally, so the gap between this symbol and the crosshair *is* the AOA and the slip.
/// That is the whole value of it - a conventional PFD shows the difference as a distance
/// rather than as two numbers to subtract.
///
/// Scaled against the pitch ladder on purpose. QGCPitchIndicator steps `_reticleSlot` pixels
/// per 5 degrees, so matching that puts this symbol on the ladder at the real flight path
/// angle rather than at an arbitrary offset that merely looks proportional.
///
/// **Both values are EKF estimates, not measurements**, derived from velocity minus wind.
/// Near zero airspeed that geometry is ill-conditioned and the wind estimate is at its worst,
/// so this hides itself below a sensible airspeed rather than drawing a confident symbol on
/// arithmetic that has stopped meaning anything. In a VTOL hover it should not be on screen.
Item {
    id: control

    property var  vehicle:      null
    property real attitudeSize: 0
    property real rollAngle:    0

    /// The pitch ladder is built at half the attitude widget's size, and steps one slot per
    /// 5 degrees. Mirrored here rather than guessed so the two agree as the widget is resized.
    readonly property real _ladderSize:   attitudeSize * 0.5
    readonly property real _reticleSlot:  (_ladderSize * 0.15) + 1
    readonly property real _pixelsPerDeg: _reticleSlot / 5

    /// Sign conventions, named so they are a one-character change if a flight says otherwise.
    /// Reasoned rather than copied: positive angle of attack means the nose sits above the
    /// flight path, so the symbol belongs *below* the crosshair, and positive sideslip means
    /// the relative wind is from the right, putting the path to the right of the nose.
    /// Mission Planner negates both, but it draws inside an already-transformed context, so
    /// its signs are not directly comparable. Worth confirming in the air.
    readonly property real _aoaSign: 1
    readonly property real _ssaSign: 1

    /// Below this the estimate is not worth drawing. Taken from the airframe where it knows -
    /// a stall indication tied to a hardcoded speed would be wrong on any other aircraft.
    /// NaN when the airframe has no AIRSPEED_MIN, which is a real case rather than an error:
    /// ArduPilot only computes these angles on Plane builds anyway. Tested rather than compared
    /// against, so the symbol is hidden by a decision instead of by NaN semantics.
    readonly property real _minAirspeed:      vehicle ? vehicle.minimumEquivalentAirspeed() : NaN
    readonly property bool _minAirspeedKnown: !isNaN(_minAirspeed) && (_minAirspeed > 0)
    readonly property real _airspeed:         vehicle ? vehicle.airSpeed.rawValue : 0

    visible: vehicle && OIFlightAngles.valid && _minAirspeedKnown && (_airspeed >= _minAirspeed)

    // Rotated with the ladder, because the displacement is in the aircraft's frame: slip is
    // sideways relative to the wings, not relative to the horizon.
    rotation:           -rollAngle
    transformOrigin:    Item.Center

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    Item {
        id:     symbol
        width:  control.attitudeSize * 0.09
        height: width

        x: (control.width / 2) - (width / 2)
            + (OIFlightAngles.sideslip * control._ssaSign * control._pixelsPerDeg)
        y: (control.height / 2) - (height / 2)
            + (OIFlightAngles.angleOfAttack * control._aoaSign * control._pixelsPerDeg)

        // Body, and the three whiskers that make it read as an aircraft rather than a target.
        Rectangle {
            anchors.fill:   parent
            radius:         width / 2
            color:          "transparent"
            border.color:   qgcPal.colorGreen
            border.width:   Math.max(1, parent.width * 0.12)
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right:          parent.left
            width:                  parent.width * 0.6
            height:                 Math.max(1, parent.width * 0.12)
            color:                  qgcPal.colorGreen
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left:           parent.right
            width:                  parent.width * 0.6
            height:                 Math.max(1, parent.width * 0.12)
            color:                  qgcPal.colorGreen
        }

        Rectangle {
            anchors.horizontalCenter:   parent.horizontalCenter
            anchors.bottom:             parent.top
            width:                      Math.max(1, parent.width * 0.12)
            height:                     parent.width * 0.5
            color:                      qgcPal.colorGreen
        }
    }
}
