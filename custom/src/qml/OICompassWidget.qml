import QtQuick
import QtQuick.Shapes

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlightMap

import OI.Controls

/// Compass rose for the advanced instrument panel.
///
/// A copy of QGCCompassWidget rather than a change to it: the stock panels keep the stock
/// compass, and a pilot who selects one of them sees exactly what they saw before.
///
/// Three differences, all of them about reading it at a glance.
///
/// **The colours follow the standard code.** The stock widget draws the bearing to the next
/// waypoint in green and ground track in cyan, which inverts the convention every glass
/// cockpit uses: magenta is where the system is navigating to, cyan is what the operator
/// selected, and green means normal status rather than "go here". Drawn through
/// QGCColoredImage so the colour is a property rather than baked into the SVG - the stock
/// pointers hardcode theirs, which is also why they do not follow the theme.
///
/// **The commanded heading is shown.** Keyboard guided control has had a heading target since
/// it was added, and nothing on the compass has ever said where it was. It is cyan, because it
/// is precisely the "operator selected this" case the convention reserves that colour for.
///
/// **Rate of turn.** Fixed at the top rather than following the card, the way a turn
/// coordinator sits in a fixed position on a panel - it describes the aircraft, not the
/// heading. Detents at the standard rate are the point of it: 3 deg/s is a two-minute turn,
/// and a bar sitting on the detent is a fact a pilot can read without interpreting a number.
Rectangle {
    id:     root
    width:  size
    height: size
    radius: width / 2
    color:  qgcPal.window

    property real size:     ScreenTools.defaultFontPixelHeight * 10
    property var  vehicle:  null

    property real _sizeRatio:                size / (ScreenTools.defaultFontPixelHeight * 10)
    property real _heading:                  vehicle ? vehicle.heading.rawValue : 0
    property real _headingToHome:            vehicle ? vehicle.headingToHome.rawValue : 0
    property real _groundSpeed:              vehicle ? vehicle.groundSpeed.rawValue : 0
    property real _headingToNextWP:          vehicle ? vehicle.headingToNextWP.rawValue : 0
    property real _courseOverGround:         vehicle ? vehicle.gps.courseOverGround.rawValue : 0
    property real _yawRate:                  vehicle ? vehicle.yawRate.rawValue : 0
    property var  _flyViewSettings:          QGroundControl.settingsManager.flyViewSettings
    property bool _showAdditionalIndicators: _flyViewSettings.showAdditionalIndicatorsCompass.value
    property bool _lockNoseUpCompass:        _flyViewSettings.lockNoseUpCompass.value

    /// Rate of turn is clamped to this, and the arc is drawn across the full sweep below.
    /// Mission Planner uses the same +-6, which keeps a reading comparable between the two.
    readonly property real _turnRateRange:      6
    /// A standard-rate turn: 3 deg/s puts the aircraft through 360 degrees in two minutes.
    readonly property real _standardTurnRate:   3
    /// How far around the rim the full range sweeps.
    readonly property real _turnArcSweepDeg:    60

    function showCOG() {
        // Ground track is meaningless at a standstill; below walking pace the GPS course is
        // noise and the pointer would spin.
        return vehicle && _showAdditionalIndicators && (_groundSpeed >= 0.5)
    }

    function showHeadingHome() {
        return vehicle && _showAdditionalIndicators && !isNaN(_headingToHome)
    }

    function showHeadingToNextWP() {
        return vehicle && _showAdditionalIndicators && !isNaN(_headingToNextWP)
    }

    function showCommandedHeading() {
        return vehicle && _showAdditionalIndicators && OIKeyboard.enabled && OIKeyboard.headingTargetValid
    }

    function translateCenterToAngleX(radius, angle) {
        return radius * Math.sin(angle * (Math.PI / 180))
    }

    function translateCenterToAngleY(radius, angle) {
        return -radius * Math.cos(angle * (Math.PI / 180))
    }

    QGCPalette { id: qgcPal; colorGroupEnabled: enabled }

    Item {
        id:             rotationParent
        anchors.fill:   parent
        rotation:       _lockNoseUpCompass ? -_heading : 0

        CompassDial {
            anchors.fill: parent
        }

        CompassHeadingIndicator {
            compassSize:    size
            heading:        _heading
        }

        // Ground track - an actual, measured value, so white rather than the cyan the stock
        // widget uses. Cyan here would claim the operator had selected it.
        QGCColoredImage {
            anchors.fill:       parent
            source:             "/qmlimages/cOGPointer.svg"
            sourceSize.height:  parent.height
            fillMode:           Image.PreserveAspectFit
            mipmap:             true
            color:              qgcPal.text
            visible:            showCOG()
            rotation:           _courseOverGround
        }

        // Where the mission is taking us. Magenta is the convention for a navigation target
        // and the reason "follow the magenta line" means anything.
        QGCColoredImage {
            anchors.fill:       parent
            source:             "/qmlimages/compassDottedLine.svg"
            sourceSize.height:  parent.height
            fillMode:           Image.PreserveAspectFit
            mipmap:             true
            color:              "magenta"
            visible:            showHeadingToNextWP()
            rotation:           _headingToNextWP
        }

        // Where the operator has asked to go. Same artwork as the mission target deliberately:
        // they are the same kind of thing - a heading to fly - and the colour is what separates
        // "the mission wants this" from "I asked for this".
        QGCColoredImage {
            anchors.fill:       parent
            source:             "/qmlimages/compassDottedLine.svg"
            sourceSize.height:  parent.height
            fillMode:           Image.PreserveAspectFit
            mipmap:             true
            color:              "cyan"
            visible:            showCommandedHeading()
            rotation:           OIKeyboard.headingTarget
        }

        // Launch location indicator
        Rectangle {
            width:              Math.max(label.contentWidth, label.contentHeight)
            height:             width
            color:              qgcPal.mapIndicator
            radius:             width / 2
            anchors.centerIn:   parent
            visible:            showHeadingHome()

            QGCLabel {
                id:                 label
                text:               qsTr("L")
                font.bold:          true
                color:              qgcPal.text
                anchors.centerIn:   parent
                rotation:           _lockNoseUpCompass ? _heading : 0
            }

            transform: Translate {
                property double _angle: _headingToHome

                // Inside the rose, on the same ring the other pointers reach rather than hung
                // off the rim. The stock widget puts it half a font height *outside* the edge,
                // where it collides with whatever the panel places beside the compass - here
                // that is the tapes, the vertical speed bar and the rate of turn arc. 0.62 of
                // the radius clears the dial's number ring on the outside and the heading
                // readout in the middle.
                property real _labelOffset: (root.width / 2) * 0.62

                x: translateCenterToAngleX(_labelOffset, _angle)
                y: translateCenterToAngleY(_labelOffset, _angle)
            }
        }
    }

    // Rate of turn, outside the rim and outside rotationParent so it stays at the top whether
    // or not the card is locked nose-up. Inside the rose it would compete with the three
    // pointers above, and those are the ones that must stay unambiguous.
    Shape {
        id:             turnArc
        anchors.fill:   parent
        visible:        vehicle !== null

        readonly property real _radius:  (root.width / 2) + (ScreenTools.defaultFontPixelHeight * 0.35 * _sizeRatio)
        readonly property real _clamped: Math.max(-_turnRateRange, Math.min(_turnRateRange, _yawRate))
        readonly property bool _pegged:  Math.abs(_yawRate) >= _turnRateRange

        // Where a standard-rate turn falls on the sweep, in Qt angle terms (-90 is the top).
        readonly property real _detentDeg: (_standardTurnRate / _turnRateRange) * _turnArcSweepDeg
        readonly property real _tickInner: _radius - (3 * _sizeRatio)
        readonly property real _tickOuter: _radius + (3 * _sizeRatio)

        function tickX(signedDetent, radius) {
            return (width / 2) + Math.cos((signedDetent - 90) * Math.PI / 180) * radius
        }

        function tickY(signedDetent, radius) {
            return (height / 2) + Math.sin((signedDetent - 90) * Math.PI / 180) * radius
        }

        // Standard-rate detents, written out rather than repeated: ShapePath is not an Item, so
        // it cannot be a Repeater delegate.
        ShapePath {
            strokeColor:    qgcPal.text
            strokeWidth:    Math.max(1, 2 * _sizeRatio)
            fillColor:      "transparent"
            startX:         turnArc.tickX(-turnArc._detentDeg, turnArc._tickInner)
            startY:         turnArc.tickY(-turnArc._detentDeg, turnArc._tickInner)

            PathLine {
                x: turnArc.tickX(-turnArc._detentDeg, turnArc._tickOuter)
                y: turnArc.tickY(-turnArc._detentDeg, turnArc._tickOuter)
            }
        }

        ShapePath {
            strokeColor:    qgcPal.text
            strokeWidth:    Math.max(1, 2 * _sizeRatio)
            fillColor:      "transparent"
            startX:         turnArc.tickX(turnArc._detentDeg, turnArc._tickInner)
            startY:         turnArc.tickY(turnArc._detentDeg, turnArc._tickInner)

            PathLine {
                x: turnArc.tickX(turnArc._detentDeg, turnArc._tickOuter)
                y: turnArc.tickY(turnArc._detentDeg, turnArc._tickOuter)
            }
        }

        // The bar itself, growing from the nose in the direction of turn. Blue because it is a
        // measured rate, the same reason the vertical speed bar is blue - the two are the same
        // kind of reading on different axes.
        ShapePath {
            strokeColor:    turnArc._pegged ? Qt.rgba(0.3, 0.6, 1, 0.5) : Qt.rgba(0.3, 0.6, 1, 1)
            strokeWidth:    Math.max(2, 4 * _sizeRatio)
            fillColor:      "transparent"
            capStyle:       ShapePath.FlatCap

            PathAngleArc {
                centerX:    turnArc.width / 2
                centerY:    turnArc.height / 2
                radiusX:    turnArc._radius
                radiusY:    turnArc._radius
                // -90 is the top of the circle in Qt's angle convention, which is where the
                // aircraft nose is on a turn coordinator.
                startAngle: -90
                sweepAngle: (turnArc._clamped / _turnRateRange) * _turnArcSweepDeg
            }
        }
    }

    QGCLabel {
        anchors.horizontalCenter:   parent.horizontalCenter
        y:                          size * 0.74
        text:                       vehicle ? _heading.toFixed(0) + "°" : ""
        horizontalAlignment:        Text.AlignHCenter
    }
}
