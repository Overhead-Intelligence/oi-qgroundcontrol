import QtQuick

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlightMap

/// Fly view instrument panel in a conventional primary-flight-display layout.
///
/// Offered alongside the stock panels rather than replacing one: it is registered into the
/// `instrumentQmlFile2` enum from OIPlugin::adjustSettingMetaData(), which is why no stock QML
/// or settings file names it. A pilot used to the simpler display keeps it.
///
/// Built on the stock Large Vertical arrangement - attitude above compass - because that
/// column already matches the vertical half of the basic T. What this adds is the rest of it:
/// speed on the left, altitude on the right, and the two things QGC has never shown at all,
/// a stall margin and a slip indication.
///
/// The background is a plain rectangle rather than the stock rounded capsule because the
/// tapes need square corners to sit against; the capsule's radius is half its width, which
/// curves away exactly where a tape would start.
Rectangle {
    id:     control
    width:  _centreWidth + (_tapeWidth * 2)
    height: _headerHeight + (_outerRadius * 4)
    color:  QGroundControl.globalPalette.window

    /// Reported back to the Fly view so the telemetry bar lays out beside the panel rather
    /// than under it. The stock panels report their own radius; this one is wider, and saying
    /// so is what keeps the bottom-right row honest.
    property real extraInset:       0
    property real extraValuesWidth: _outerRadius

    property real _centreWidth:  ScreenTools.defaultFontPixelHeight * 10
    property real _tapeWidth:    ScreenTools.defaultFontPixelHeight * 3
    property real _headerHeight: ScreenTools.defaultFontPixelHeight * 1.5
    property real _outerMargin:  (_centreWidth * 0.05) / 2
    property real _outerRadius:  _centreWidth / 2
    property real _innerRadius:  _outerRadius - _outerMargin

    // Prevent all clicks from going through to lower layers
    DeadMouseArea {
        anchors.fill: parent
    }

    // Reserved for the angle of attack indexer, which spans the full width as a header: it is
    // the most direct stall indication available and reads as a banner rather than as another
    // instrument competing with the horizon.
    Item {
        id:                 headerArea
        anchors.top:        parent.top
        anchors.left:       parent.left
        anchors.right:      parent.right
        height:             _headerHeight
    }

    // Reserved for the airspeed tape.
    Item {
        id:             leftTapeArea
        anchors.top:    headerArea.bottom
        anchors.bottom: parent.bottom
        anchors.left:   parent.left
        width:          _tapeWidth
    }

    // Reserved for the altitude tape and its vertical speed bar.
    Item {
        id:             rightTapeArea
        anchors.top:    headerArea.bottom
        anchors.bottom: parent.bottom
        anchors.right:  parent.right
        width:          _tapeWidth
    }

    QGCAttitudeWidget {
        id:                         attitude
        anchors.horizontalCenter:   parent.horizontalCenter
        anchors.topMargin:          _outerMargin
        anchors.top:                headerArea.bottom
        size:                       _innerRadius * 2
        vehicle:                    globals.activeVehicle
    }

    // OI's compass rather than the stock one: standard-code colours, the commanded heading,
    // and rate of turn. Resolved as a sibling file - both live under qrc:/custom/qml.
    OICompassWidget {
        id:                         compass
        anchors.horizontalCenter:   parent.horizontalCenter
        anchors.topMargin:          _outerMargin * 2
        anchors.top:                attitude.bottom
        size:                       _innerRadius * 2
        vehicle:                    globals.activeVehicle
    }
}
