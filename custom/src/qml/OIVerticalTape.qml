import QtQuick

import QGroundControl
import QGroundControl.Controls

/// A scrolling vertical tape, used for both airspeed and altitude.
///
/// Values are in **display units** - the caller converts before handing them over, because only
/// the caller knows whether it is holding a speed or a distance. The tape's job is the scale.
///
/// **The step is chosen, not fixed.** A tape labelled every 5 metres becomes one labelled every
/// 16.4 feet the moment an operator switches units, and nobody reads altitude in sixteens. So
/// the span is converted first and a round step picked to suit it: metres give 5s, feet give
/// 20s or 50s, knots give 5s or 10s. No per-unit table to keep in step, and it survives any
/// unit QGC adds later.
Item {
    id: control

    /// Current value, in display units. The tape centres on this.
    property real value: 0
    /// How much of the scale is visible top to bottom, in display units.
    property real span: 60
    /// Drawn on the right edge for a left-hand tape, and the left edge for a right-hand one, so
    /// the ticks always face the instrument they belong to.
    property bool ticksOnRight: true
    /// Roughly how many numbered steps should fit in the span.
    property int targetLabelCount: 6
    /// How far a tick reaches in from its edge. Public so a bug in the overlay can be drawn to
    /// the same length: a marker wider than the ticks it sits opposite runs under the numbers.
    readonly property real tickLength: width * 0.25

    readonly property real _pixelsPerUnit: span > 0 ? height / span : 0
    readonly property real _step:          niceStep(span / targetLabelCount)
    readonly property int  _tickCount:     Math.ceil(span / _step) + 2
    readonly property real _firstTick:     Math.floor((value - (span / 2)) / _step) * _step

    /// Round the raw interval up to something a person would choose. Covers the range a speed
    /// or altitude tape needs in any unit QGC offers.
    function niceStep(raw) {
        var candidates = [1, 2, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000]
        for (var i = 0; i < candidates.length; i++) {
            if (candidates[i] >= raw) {
                return candidates[i]
            }
        }
        return candidates[candidates.length - 1]
    }

    /// Where a value sits on the tape. Larger values are higher, so the scale moves down as the
    /// aircraft speeds up or climbs - the direction every tape instrument moves.
    function yForValue(tapeValue) {
        return (height / 2) - ((tapeValue - value) * _pixelsPerUnit)
    }

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    Rectangle {
        anchors.fill:   parent
        color:          qgcPal.window
        opacity:        0.55
    }

    Item {
        anchors.fill:   parent
        clip:           true

        Repeater {
            model: control._tickCount

            Item {
                readonly property real _tickValue: control._firstTick + (index * control._step)

                x:      0
                y:      control.yForValue(_tickValue) - (height / 2)
                width:  control.width
                height: ScreenTools.defaultFontPixelHeight

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right:          control.ticksOnRight ? parent.right : undefined
                    anchors.left:           control.ticksOnRight ? undefined : parent.left
                    width:                  control.tickLength
                    height:                 Math.max(1, ScreenTools.defaultFontPixelHeight * 0.08)
                    color:                  qgcPal.text
                }

                QGCLabel {
                    anchors.verticalCenter:     parent.verticalCenter
                    anchors.rightMargin:        control.tickLength * 1.4
                    anchors.leftMargin:         control.tickLength * 1.4
                    anchors.right:              control.ticksOnRight ? parent.right : undefined
                    anchors.left:               control.ticksOnRight ? undefined : parent.left
                    font.pointSize:             ScreenTools.smallFontPointSize
                    color:                      qgcPal.text
                    text:                       parent._tickValue.toFixed(0)
                }
            }
        }
    }

    /// Anything the caller wants pinned to the scale - speed bugs, a target altitude - goes
    /// here, positioned with yForValue(). Declared last so it draws over the ticks.
    default property alias overlayContent: overlay.data

    Item {
        id:             overlay
        anchors.fill:   parent
        clip:           true
    }

    /// Current value, boxed on the centre line. This is the conventional place for it and the
    /// reason the tape is wide enough to hold it: the number and the scale it sits on are read
    /// together, so splitting them puts the operator's eye in two places. Empty hides the box.
    ///
    /// Declared last, so it covers the ticks and any bug at the present value - which is what
    /// should happen, since the box is already showing that value more precisely.
    property string valueText: ""

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width:                  parent.width
        height:                 ScreenTools.defaultFontPixelHeight * 1.5
        visible:                control.valueText !== ""
        color:                  qgcPal.window
        border.color:           qgcPal.text
        border.width:           1

        QGCLabel {
            anchors.centerIn:       parent
            width:                  parent.width - (ScreenTools.defaultFontPixelWidth * 0.8)
            horizontalAlignment:    Text.AlignHCenter
            // The box is a fixed width but its contents are not: altitude in feet runs to five
            // digits, and the unit string rides along with the number. Shrinking to fit keeps
            // the one value that must always be readable readable.
            fontSizeMode:           Text.HorizontalFit
            font.pointSize:         ScreenTools.defaultFontPointSize
            minimumPointSize:       ScreenTools.smallFontPointSize * 0.75
            font.bold:              true
            color:                  qgcPal.text
            text:                   control.valueText
        }
    }
}
