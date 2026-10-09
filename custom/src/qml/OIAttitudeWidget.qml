import QtQuick
import QtQuick.Effects

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlightMap

/// Attitude display for the advanced instrument panel.
///
/// A copy of QGCAttitudeWidget rather than a change to it, so the stock panels keep the stock
/// instrument. The horizon, pitch ladder, circular mask and border are the stock ones - the
/// difference is the bank scale.
///
/// The stock scale is a rotating SVG with nine unlabelled ticks. This replaces it with a fixed,
/// labelled one and moves the pointer instead; see OIBankScale for why that way round.
Item {
    id: root

    property var  vehicle: null
    property real size

    property real _rollAngle:  vehicle ? vehicle.roll.rawValue  : 0
    property real _pitchAngle: vehicle ? vehicle.pitch.rawValue : 0

    width:  size
    height: size

    QGCPalette { id: qgcPal; colorGroupEnabled: enabled }

    Item {
        id:             instrument
        anchors.fill:   parent
        visible:        false

        QGCArtificialHorizon {
            rollAngle:      _rollAngle
            pitchAngle:     _pitchAngle
            anchors.fill:   parent
        }

        QGCPitchIndicator {
            id:                     pitchWidget
            size:                   root.size * 0.5
            anchors.verticalCenter: parent.verticalCenter
            pitchAngle:             _pitchAngle
            rollAngle:              _rollAngle
            color:                  Qt.rgba(0, 0, 0, 0)
        }

        Image {
            id:                 crossHair
            anchors.centerIn:   parent
            source:             "/qmlimages/crossHair.svg"
            mipmap:             true
            width:              size * 0.75
            sourceSize.width:   width
            fillMode:           Image.PreserveAspectFit
        }
    }

    MultiEffect {
        source:         instrument
        anchors.fill:   instrument
        maskEnabled:    true
        maskSource:     mask
    }

    Item {
        id:             mask
        width:          instrument.width
        height:         instrument.height
        layer.enabled:  true
        visible:        false

        Rectangle {
            width:  parent.width
            height: parent.height
            radius: width / 2
            color:  "black"
        }
    }

    Rectangle {
        id:             borderRect
        anchors.fill:   parent
        radius:         width / 2
        color:          Qt.rgba(0, 0, 0, 0)
        border.color:   qgcPal.text
        border.width:   1
    }

    // Outside the mask so the ticks and numbers sit on top of the horizon rather than being
    // clipped with it. Sibling file under qrc:/custom/qml.
    OIBankScale {
        anchors.fill:   parent
        size:           root.size
        rollAngle:      _rollAngle
    }
}
