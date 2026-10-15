import QtQuick
import QtQuick.Shapes

import QGroundControl
import QGroundControl.Controls

/// Rotating bank scale with a fixed roll pointer, for the advanced attitude display.
///
/// The stock scale is `attitudeDial.svg`: 496 bytes, two paths, nine tick marks and no text at
/// all. It rotates under a fixed pointer, so the geometry is sound, but with no labels and no
/// emphasis there is nothing to read a deflection against - 20 and 30 degrees look the same.
///
/// This keeps that arrangement and supplies what was missing. The scale turns with the horizon
/// at -roll, the same rotation the pitch ladder and the stock dial use, and the pointer stays
/// at the top. Mission Planner reads the same way, which is the point: a pilot who learned
/// bank angle there should not have to re-learn which way the display moves.
///
/// The numbers counter-rotate so they stay upright wherever the scale carries them. A rotating
/// scale normally cannot carry labels for exactly that reason - the figures end up sideways at
/// the bank angles worth reading - and undoing the rotation per label buys the labels back
/// without changing how the instrument moves.
///
/// Marks at 10, 20, 30, 45 and 60 either side. Only 30, 45 and 60 are numbered - 10 and 20 are
/// short ticks, as they are on most PFDs, because labelling all five crowds the arc at this
/// size and the small angles are the ones nobody needs a number for.
///
/// 45 is amber and 60 is red. Neither is a limit the aircraft enforces; they are the angles at
/// which load factor stops being incidental - 1.41 g at 45 degrees, 2 g at 60 - and a pilot
/// who has rolled that far on a survey aircraft has usually not meant to.
Item {
    id: control

    property real rollAngle: 0
    property real size:      width

    readonly property real _sizeRatio:    size / (ScreenTools.defaultFontPixelHeight * 10)
    readonly property real _radius:       size / 2
    readonly property real _tickInner:    _radius * 0.84
    readonly property real _shortOuter:   _radius * 0.90
    readonly property real _longOuter:    _radius * 0.94
    readonly property real _labelRadius:  _radius * 0.74
    readonly property real _strokeWidth:  Math.max(1, 2 * _sizeRatio)

    /// Qt measures shape angles from 3 o'clock, and the scale is built around 12 o'clock.
    function pointX(angleDeg, radius) {
        return (width / 2) + Math.cos((angleDeg - 90) * Math.PI / 180) * radius
    }

    function pointY(angleDeg, radius) {
        return (height / 2) + Math.sin((angleDeg - 90) * Math.PI / 180) * radius
    }

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    /// One mark: `angle` is bank in degrees, signed, and `stroke` picks the colour.
    component BankTick: Shape {
        id: tick

        property real   angle:  0
        property bool   long:   false
        property color  stroke: qgcPal.text

        anchors.fill: parent

        ShapePath {
            strokeColor: tick.stroke
            strokeWidth: control._strokeWidth
            fillColor:   "transparent"
            startX:      control.pointX(tick.angle, control._tickInner)
            startY:      control.pointY(tick.angle, control._tickInner)

            PathLine {
                x: control.pointX(tick.angle, tick.long ? control._longOuter : control._shortOuter)
                y: control.pointY(tick.angle, tick.long ? control._longOuter : control._shortOuter)
            }
        }
    }

    component BankLabel: QGCLabel {
        property real  angle: 0

        font.pointSize:         ScreenTools.smallFontPointSize
        horizontalAlignment:    Text.AlignHCenter
        verticalAlignment:      Text.AlignVCenter
        x:                      control.pointX(angle, control._labelRadius) - (width / 2)
        y:                      control.pointY(angle, control._labelRadius) - (height / 2)
        // Undoes the group's rotation, so the figure stays upright while its position rides
        // round with the scale.
        rotation:               control.rollAngle
        transformOrigin:        Item.Center
    }

    /// Everything that turns. Rotated by -roll to match the horizon and the pitch ladder, both
    /// of which the stock attitude widget also rotates by -roll.
    Item {
        anchors.fill:       parent
        rotation:           -control.rollAngle
        transformOrigin:    Item.Center

        // Zero, and the two small angles: short, uncoloured, unnumbered.
        BankTick { angle:   0; long: true }
        BankTick { angle: -10 }
        BankTick { angle:  10 }
        BankTick { angle: -20 }
        BankTick { angle:  20 }

        // Thirty is the one a pilot references, so it is long and numbered but not coloured -
        // nothing is wrong at 30 degrees.
        BankTick { angle: -30; long: true }
        BankTick { angle:  30; long: true }

        BankTick { angle: -45; long: true; stroke: qgcPal.colorOrange }
        BankTick { angle:  45; long: true; stroke: qgcPal.colorOrange }

        BankTick { angle: -60; long: true; stroke: qgcPal.colorRed }
        BankTick { angle:  60; long: true; stroke: qgcPal.colorRed }

        BankLabel { angle: -30; text: "30"; color: qgcPal.text }
        BankLabel { angle:  30; text: "30"; color: qgcPal.text }
        BankLabel { angle: -45; text: "45"; color: qgcPal.colorOrange }
        BankLabel { angle:  45; text: "45"; color: qgcPal.colorOrange }
        BankLabel { angle: -60; text: "60"; color: qgcPal.colorRed }
        BankLabel { angle:  60; text: "60"; color: qgcPal.colorRed }
    }

    /// The pointer, fixed at the top. It marks the aircraft rather than the horizon, so it is
    /// the one thing here that does not move: a right bank carries the scale anticlockwise
    /// beneath it.
    Shape {
        anchors.fill: parent

        ShapePath {
            strokeColor: qgcPal.text
            strokeWidth: control._strokeWidth
            fillColor:   qgcPal.text

            startX: control.width / 2
            startY: (control.height / 2) - control._tickInner

            PathLine { x: (control.width / 2) - (5 * control._sizeRatio); y: (control.height / 2) - control._tickInner + (8 * control._sizeRatio) }
            PathLine { x: (control.width / 2) + (5 * control._sizeRatio); y: (control.height / 2) - control._tickInner + (8 * control._sizeRatio) }
            PathLine { x: control.width / 2;                              y: (control.height / 2) - control._tickInner }
        }
    }
}
