import QtQuick
import QtQuick.Shapes

import QGroundControl
import QGroundControl.Controls

import OI.Controls

/// Slip/skid ball, on an arc at the bottom of the attitude display.
///
/// Mirrors the bank scale deliberately: bank at the top, coordination at the bottom, the two
/// halves of the same question about a turn. There is no Mission Planner equivalent to copy, so
/// the reference is the mechanical instrument - a filled tube, a ball riding inside it, and two
/// cage marks a ball-width apart. Step on the ball.
///
/// **It does not move with the horizon.** A real inclinometer is fixed to the panel, and so is
/// the roll pointer above it; between them they are the aircraft, and the scale behind them is
/// the world. Rotating this with the sky would be the one arrangement that makes it unreadable.
///
/// **This is lateral acceleration, not the sideslip the flight path vector shows.** Sideslip is
/// an aerodynamic angle and this is a force balance; they disagree routinely, and over the
/// 2026-10-09 sortie sideslip ran a median +3.4 deg while this ran -0.036 g. Driving the ball
/// from sideslip would redraw a number already on screen, and redraw it wrong.
Item {
    id: control

    property real size: width

    readonly property real _radius:     size / 2
    readonly property real _arcRadius:  _radius * 0.85
    readonly property real _sizeRatio:  size / (ScreenTools.defaultFontPixelHeight * 10)
    readonly property real _ballRadius: size * 0.032

    /// The tube is drawn as a thick stroke along the same arc the ball rides, so the ball sits
    /// inside it by construction rather than by a second set of tuned numbers.
    readonly property real _tubeWidth:  _ballRadius * 2.7
    readonly property real _cageWidth:  Math.max(1, 2 * _sizeRatio)

    /// Full deflection. Measured flight ran to +-0.17 g, so 0.2 uses most of the arc without
    /// sitting on the stop; a ball pegged half the sortie tells a pilot nothing.
    readonly property real _fullScaleG:   0.2
    /// Half the arc the ball travels over, each side of centre.
    readonly property real _halfSweepDeg: 22
    /// A little tube beyond the ball's travel, so a pegged ball still reads as inside it.
    readonly property real _tubeEndPadDeg: 4

    /// Named so a flight can flip it in one character, as the angle of attack signs are.
    ///
    /// Reasoned, then checked: RAW_IMU's yacc is positive toward the right wing (confirmed by
    /// regression against a steady-turn model on the 2026-10-09 flight), and the ball is a
    /// pendulous mass, so it hangs *opposite* the specific force. Force to the right puts the
    /// ball left. Hence the negation.
    readonly property real _ballSign: -1

    readonly property bool _usable: OIFlightAngles.lateralAccelerationValid

    /// Where the ball sits, in degrees along the arc, positive to the right.
    readonly property real _ballAngle: {
        var raw = _ballSign * (OIFlightAngles.lateralAcceleration / _fullScaleG) * _halfSweepDeg
        return Math.max(-_halfSweepDeg, Math.min(_halfSweepDeg, raw))
    }

    /// Half the cage gap, as an arc angle, so the two marks sit exactly one ball apart.
    readonly property real _cageHalfDeg: (_ballRadius / _arcRadius) * (180 / Math.PI)

    /// Zero is the bottom of the circle and positive runs right; Qt measures from 3 o'clock.
    function pointX(offsetDeg, radius) {
        return (width / 2) + Math.cos((90 - offsetDeg) * Math.PI / 180) * radius
    }

    function pointY(offsetDeg, radius) {
        return (height / 2) + Math.sin((90 - offsetDeg) * Math.PI / 180) * radius
    }

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    opacity: _usable ? 1 : 0.35

    // Fixed white and black rather than palette colours, for the same reason the angle of
    // attack indexer uses a black pointer: this sits on the horizon, whose sky blue and ground
    // brown do not follow the theme, so a palette colour would track the wrong background. It
    // is also what the instrument has always looked like.
    Shape {
        anchors.fill: parent

        ShapePath {
            strokeColor: "white"
            strokeWidth: control._tubeWidth
            fillColor:   "transparent"
            capStyle:    ShapePath.FlatCap

            PathAngleArc {
                centerX:    control.width / 2
                centerY:    control.height / 2
                radiusX:    control._arcRadius
                radiusY:    control._arcRadius
                // Qt sweeps clockwise from 3 o'clock, so the arc starts on the left of the
                // bottom and runs across to the right of it.
                startAngle: 90 - control._halfSweepDeg - control._tubeEndPadDeg
                sweepAngle: (control._halfSweepDeg + control._tubeEndPadDeg) * 2
            }
        }
    }

    /// One cage mark: crosses the tube, and stops there.
    ///
    /// The angle is reached through this id rather than through `parent`. ShapePath is not an
    /// Item - `parent` is an Item property - so inside it `parent` is undefined, both endpoints
    /// evaluate to NaN and the mark is drawn from (0,0) to (0,0), which is to say not at all.
    /// Nothing reports this: qmllint passes and the scene renders. OIBankScale's tick uses an
    /// id for the same reason.
    component CageMark: Shape {
        id: mark

        property real angle: 0

        anchors.fill: parent

        ShapePath {
            strokeColor: "black"
            strokeWidth: control._cageWidth
            fillColor:   "transparent"
            startX:      control.pointX(mark.angle, control._arcRadius - (control._tubeWidth / 2))
            startY:      control.pointY(mark.angle, control._arcRadius - (control._tubeWidth / 2))

            PathLine {
                x: control.pointX(mark.angle, control._arcRadius + (control._tubeWidth / 2))
                y: control.pointY(mark.angle, control._arcRadius + (control._tubeWidth / 2))
            }
        }
    }

    CageMark { angle: -control._cageHalfDeg }
    CageMark { angle:  control._cageHalfDeg }

    // The ball. Drawn last so it passes over the cage marks, which is what the real one does -
    // the marks are on the glass and the ball is behind it.
    //
    // Not interpolated between samples. RAW_IMU arrives at 2 Hz against the panel's 4.2, so
    // smoothing it was possible, but it made this the only thing on the display that glided
    // while everything beside it stepped. Matching the panel beats matching an ideal.
    Rectangle {
        width:   control._ballRadius * 2
        height:  width
        radius:  width / 2
        color:   "black"
        visible: control._usable

        x: control.pointX(control._ballAngle, control._arcRadius) - (width / 2)
        y: control.pointY(control._ballAngle, control._arcRadius) - (height / 2)
    }
}
