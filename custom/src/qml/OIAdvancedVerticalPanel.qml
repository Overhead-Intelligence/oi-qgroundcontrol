import QtQuick

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlightMap

import OI.Controls

/// Fly view instrument panel in a conventional primary-flight-display layout.
///
/// Offered alongside the stock panels rather than replacing one: it is registered into the
/// `instrumentQmlFile2` enum from OIPlugin::adjustSettingMetaData(), which is why no stock QML
/// or settings file names it. A pilot used to the simpler display keeps it.
///
/// Laid out as the basic T, which is the actual requirement - speed left, attitude centre,
/// altitude right, heading below. An instrument-rated pilot scans that pattern without
/// thinking, and rearranging it costs more than any individual widget gains.
///
/// The background is a plain rectangle rather than the stock rounded capsule because the tapes
/// need square corners to sit against; the capsule's radius is half its width, which curves
/// away exactly where a tape would start.
Rectangle {
    id:     control
    width:  _centreWidth + (_tapeWidth * 2)
    height: _headerHeight + (_outerRadius * 4) + _footerHeight
    color:  QGroundControl.globalPalette.window

    /// Reported back to the Fly view so the telemetry bar lays out beside the panel rather than
    /// under it. The stock panels report their own radius; this one is wider, and saying so is
    /// what keeps the bottom-right row honest.
    property real extraInset:       0
    property real extraValuesWidth: _outerRadius

    property real _centreWidth:  ScreenTools.defaultFontPixelHeight * 10
    /// Measured from the tape's own contents rather than set to a count of font heights.
    /// The widest things it must hold are a footer label and a value with its unit, and how
    /// wide those are depends on the font, the display scaling, the units in force and the
    /// translation - none of which a fixed multiple of the font height tracks. See the
    /// Measure block near the bottom of this file.
    property real _tapeWidth:    Math.max(speedLabelWidth.width, altLabelWidth.width,
                                          speedValueWidth.width, altValueWidth.width) +
                                 (ScreenTools.defaultFontPixelWidth * 2)
    property real _headerHeight: ScreenTools.defaultFontPixelHeight * 1.5
    property real _footerHeight: ScreenTools.defaultFontPixelHeight * 1.3
    property real _outerMargin:  (_centreWidth * 0.05) / 2
    property real _outerRadius:  _centreWidth / 2
    property real _innerRadius:  _outerRadius - _outerMargin

    property var _vehicle:         globals.activeVehicle
    property var _units:           QGroundControl.unitsConversion
    property var _flyViewSettings: QGroundControl.settingsManager.flyViewSettings

    // Airspeed is the stall-relevant number and the reason the tape exists, but only an airframe
    // with a pitot reports one. Keyed on the airframe class rather than on the reading: a
    // multirotor with no sensor publishes a steady zero, which no amount of range-checking
    // distinguishes from a genuine zero, while a quadplane sitting in a hover reports a true
    // zero that should be shown as such. Falling back to ground speed and *saying which* beats
    // a confident zero - the same reasoning that has the altitude tape name its datum.
    property bool   _airspeedUsable: _vehicle ? (_vehicle.fixedWing || _vehicle.vtol) : false
    property real   _speedMetersSec: _vehicle ? (_airspeedUsable ? _vehicle.airSpeed.rawValue
                                                                 : _vehicle.groundSpeed.rawValue) : 0
    property real   _speedDisplay:   _units.metersSecondToAppSettingsSpeedUnits(_speedMetersSec)
    // Mission Planner's names, deliberately. Most of the fleet's pilots read that HUD first,
    // and a different word for the same quantity is a tax on every one of them.
    property string _speedLabel:     _airspeedUsable ? qsTr("Airspeed") : qsTr("Groundspeed")

    // Taken from the singleton rather than from the vehicle directly: reading it here would
    // bind to a Q_INVOKABLE and evaluate once, before the parameters exist.
    property real   _minAirspeed:      OIFlightAngles.minimumAirspeed
    property bool   _minAirspeedKnown: !isNaN(_minAirspeed) && (_minAirspeed > 0)

    // Follows the altitude reference the rest of the build already uses rather than inventing a
    // fourth notion of altitude. AGL falls back to relative when terrain height is unavailable,
    // exactly as guided control does, and the label says which is in force.
    property bool   _wantAGL:    _flyViewSettings.guidedAltitudeFrame.rawValue === 1
    property bool   _aglUsable:  _wantAGL && _vehicle && !isNaN(_vehicle.altitudeAboveTerr.rawValue)
    property real   _altMeters:  _vehicle ? (_aglUsable ? _vehicle.altitudeAboveTerr.rawValue
                                                        : _vehicle.altitudeRelative.rawValue) : 0
    property real   _altDisplay: _units.metersToAppSettingsVerticalDistanceUnits(_altMeters)
    property string _altLabel:   _aglUsable ? qsTr("Altitude (AGL)") : qsTr("Altitude (Rel)")

    // Prevent all clicks from going through to lower layers
    DeadMouseArea {
        anchors.fill: parent
    }

    OIAoaIndexer {
        id:             headerArea
        anchors.top:    parent.top
        anchors.left:   parent.left
        anchors.right:  parent.right
        height:         _headerHeight
        vehicle:        control._vehicle
    }

    OIVerticalTape {
        id:             speedTape
        anchors.top:    headerArea.bottom
        anchors.bottom: footerArea.top
        anchors.left:   parent.left
        width:          _tapeWidth
        ticksOnRight:   true
        value:          control._speedDisplay
        valueText:      control._vehicle
                            ? control._speedDisplay.toFixed(1) + " " + control._units.appSettingsSpeedUnitsString
                            : "--"
        // Converted from a physical span so the window shows the same amount of speed whatever
        // the units: 26 m/s, which is Mission Planner's.
        span:           control._units.metersSecondToAppSettingsSpeedUnits(26)

        // Commanded airspeed. Magenta is the navigation-target colour, the same as the compass
        // pointer to the next waypoint - this is that same kind of thing on another axis.
        Rectangle {
            width:   speedTape.width * 0.5
            height:  Math.max(2, ScreenTools.defaultFontPixelHeight * 0.14)
            color:   "magenta"
            visible: control._flyViewSettings.showAdditionalIndicatorsAirspeed.rawValue &&
                     control._airspeedUsable && (control._vehicle.airSpeedSetpoint.rawValue > 0)
            x:       0
            y:       speedTape.yForValue(control._units.metersSecondToAppSettingsSpeedUnits(
                         control._vehicle ? control._vehicle.airSpeedSetpoint.rawValue : 0)) - (height / 2)
        }

        // The airframe's own minimum. Red because below it the wing stops flying: the one speed
        // on this tape that is a limit rather than a target.
        Rectangle {
            width:   speedTape.width * 0.35
            height:  Math.max(2, ScreenTools.defaultFontPixelHeight * 0.12)
            color:   QGroundControl.globalPalette.colorRed
            visible: control._flyViewSettings.showAdditionalIndicatorsAirspeed.rawValue &&
                     control._airspeedUsable && control._minAirspeedKnown
            x:       0
            y:       speedTape.yForValue(control._units.metersSecondToAppSettingsSpeedUnits(
                         control._minAirspeed)) - (height / 2)
        }
    }

    OIVerticalTape {
        id:             altTape
        anchors.top:    headerArea.bottom
        anchors.bottom: footerArea.top
        anchors.right:  parent.right
        width:          _tapeWidth
        ticksOnRight:   false
        value:          control._altDisplay
        valueText:      control._vehicle
                            ? control._altDisplay.toFixed(1) + " " + control._units.appSettingsVerticalDistanceUnitsString
                            : "--"
        span:           control._units.metersToAppSettingsVerticalDistanceUnits(40)

        // Commanded altitude, cyan: the operator asked for this, which is what cyan means and
        // what the commanded heading on the compass already uses.
        Rectangle {
            width:          altTape.width * 0.5
            height:         Math.max(2, ScreenTools.defaultFontPixelHeight * 0.14)
            color:          "cyan"
            anchors.right:  parent.right
            // Gated on the two frames agreeing, not just on there being a target. The keyboard
            // controller frames its target from the guided-altitude setting alone, while this
            // tape also needs terrain height to have arrived; when they differ the number is
            // real but measured from a different datum, and a bug drawn from it would be off
            // by the terrain elevation.
            visible:        control._flyViewSettings.showAdditionalIndicatorsAltitude.rawValue &&
                            OIKeyboard.enabled && OIKeyboard.altitudeTargetValid &&
                            (OIKeyboard.altitudeFrameAGL === control._aglUsable)
            y:              altTape.yForValue(control._units.metersToAppSettingsVerticalDistanceUnits(
                                OIKeyboard.altitudeTarget)) - (height / 2)
        }
    }

    // A bar growing from the centre rather than a scale with a pointer: at this width a pointer
    // would be a few pixels and the sign is the thing being read. Linear to +-5 m/s, pegging
    // visibly past that, because a quadplane in a VTOL climb will sit at the stop regularly and
    // a bar that silently saturates is worse than one that says so. The exact figure is not
    // lost - it is one of the values the telemetry bar can carry.
    Item {
        id:             vsiBar
        anchors.top:    altTape.top
        anchors.bottom: altTape.bottom
        anchors.right:  altTape.left
        width:          ScreenTools.defaultFontPixelHeight * 0.5

        readonly property real _range:   control._units.metersSecondToAppSettingsSpeedUnits(5)
        readonly property real _rate:    control._vehicle
                                             ? control._units.metersSecondToAppSettingsSpeedUnits(control._vehicle.climbRate.rawValue)
                                             : 0
        readonly property real _clamped: Math.max(-_range, Math.min(_range, _rate))
        readonly property bool _pegged:  Math.abs(_rate) >= _range

        Rectangle {
            anchors.fill: parent
            color:        QGroundControl.globalPalette.window
            opacity:      0.55
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width:                    parent.width
            height:                   Math.max(1, ScreenTools.defaultFontPixelHeight * 0.06)
            y:                        (parent.height / 2) - (height / 2)
            color:                    QGroundControl.globalPalette.text
            opacity:                  0.6
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width:                    parent.width * 0.6
            color:                    vsiBar._pegged ? Qt.rgba(0.3, 0.6, 1, 0.5) : Qt.rgba(0.3, 0.6, 1, 1)
            visible:                  control._vehicle !== null
            height:                   vsiBar._range > 0
                                          ? Math.abs(vsiBar._clamped) / vsiBar._range * (parent.height / 2)
                                          : 0
            y:                        vsiBar._clamped >= 0 ? (parent.height / 2) - height
                                                           : (parent.height / 2)
        }
    }

    OIAttitudeWidget {
        id:                       attitude
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin:        _outerMargin
        anchors.top:              headerArea.bottom
        size:                     _innerRadius * 2
        vehicle:                  control._vehicle
    }

    OICompassWidget {
        id:                       compass
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin:        _outerMargin * 2
        anchors.top:              attitude.bottom
        size:                     _innerRadius * 2
        vehicle:                  control._vehicle
    }

    // Under each tape rather than beside it. The centre column is two tangent circles with no
    // room to spare, and the source name is static - it only has to be found once, so it does
    // not need to be in the scan path. The unit travels with the number instead, in the box.
    Item {
        id:             footerArea
        anchors.left:   parent.left
        anchors.right:  parent.right
        anchors.bottom: parent.bottom
        height:         _footerHeight

        /// Centred over a tape by matching its geometry, not by anchoring to it. A tape is
        /// this row's *sibling*, which makes it an uncle to these labels, and QML drops an
        /// anchor to anything that is not a parent or a sibling - silently, apart from one
        /// console warning. Both labels landed at x = 0, on top of each other.
        component FooterLabel: QGCLabel {
            anchors.verticalCenter: parent.verticalCenter
            width:                  control._tapeWidth
            horizontalAlignment:    Text.AlignHCenter
            // Long in some languages and after a units change; shrinking beats overflowing
            // into the centre column.
            fontSizeMode:           Text.HorizontalFit
            font.pointSize:         ScreenTools.smallFontPointSize
            minimumPointSize:       ScreenTools.smallFontPointSize * 0.75
            color:                  QGroundControl.globalPalette.text
        }

        FooterLabel {
            anchors.left:   parent.left
            text:           control._speedLabel
        }

        FooterLabel {
            anchors.right:  parent.right
            text:           control._altLabel
        }
    }
    /// Sizes the tapes. Deliberately measured against worst-case *sample* text and not against
    /// the live value: binding the width to what is on screen would make the whole panel change
    /// width as the altitude crosses a thousand, and everything to the left of it move.
    component Measure: TextMetrics {
        font.family: ScreenTools.normalFontFamily
    }

    Measure {
        id:             speedLabelWidth
        font.pointSize: ScreenTools.smallFontPointSize
        text:           qsTr("Groundspeed")
    }

    Measure {
        id:             altLabelWidth
        font.pointSize: ScreenTools.smallFontPointSize
        text:           qsTr("Altitude (AGL)")
    }

    Measure {
        id:             speedValueWidth
        font.pointSize: ScreenTools.defaultFontPointSize
        font.bold:      true
        text:           "-888.8 " + control._units.appSettingsSpeedUnitsString
    }

    Measure {
        id:             altValueWidth
        font.pointSize: ScreenTools.defaultFontPointSize
        font.bold:      true
        text:           "-8888.8 " + control._units.appSettingsVerticalDistanceUnitsString
    }
}
