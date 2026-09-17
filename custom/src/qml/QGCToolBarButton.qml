import QtQuick
import QtQuick.Controls

import QGroundControl
import QGroundControl.Controls

// OI override of src/QmlControls/QGCToolBarButton.qml.
//
// Stock QGC draws its multi-colour logo untinted through a vector renderer. The OI
// logo mark is monochrome, so it is tinted like every other toolbar icon and follows
// the light/dark palette. Everything else is identical to the stock control.
//
// Important Note: Toolbar buttons must manage their checked state manually in order to support
// view switch prevention. This means they can't be checkable or autoExclusive.

Button {
    id:                 button
    height:             ScreenTools.defaultFontPixelHeight * 3
    leftPadding:        _horizontalMargin
    rightPadding:       _horizontalMargin
    checkable:          false

    property bool logo: false

    property real _horizontalMargin: ScreenTools.defaultFontPixelWidth

    onCheckedChanged: checkable = false

    background: Rectangle {
        anchors.fill:   parent
        color:          button.checked ? qgcPal.buttonHighlight : Qt.rgba(0,0,0,0)
        border.color:   "red"
        border.width:   QGroundControl.corePlugin.showTouchAreas ? 3 : 0
    }

    contentItem: Row {
        spacing:                ScreenTools.defaultFontPixelWidth
        anchors.verticalCenter: button.verticalCenter

        // QGCColoredImage goes through the coloredsvg image provider, which reads the
        // resource directly and bypasses the URL interceptor, so the OI mark is named
        // explicitly here instead of relying on the /res/QGCLogoFull.svg override.
        QGCColoredImage {
            height:                 ScreenTools.defaultFontPixelHeight * 2
            width:                  height
            sourceSize.height:      parent.height
            fillMode:               Image.PreserveAspectFit
            color:                  button.checked ? qgcPal.buttonHighlightText : qgcPal.buttonText
            source:                 button.logo ? "/custom/OILogoMark.svg" : button.icon.source
            anchors.verticalCenter: parent.verticalCenter
        }

        Label {
            id:                     _label
            visible:                text !== ""
            text:                   button.text
            color:                  button.checked ? qgcPal.buttonHighlightText : qgcPal.buttonText
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
