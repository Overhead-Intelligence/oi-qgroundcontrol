import QtQuick
import QtLocation
import QtPositioning

import QGroundControl
import QGroundControl.Controls

// One hazard marker on the Fly view map.
//
// Built by src/FlightMap/MapItems/CustomMapItems.qml, which reads `url` off each
// entry of QGroundControl.corePlugin.customMapItems, creates this component, sets
// `customMapObject` to that entry and calls map.addMapItem() on the result. The
// entry is an OIMapOverlayItem (custom/src/OIMapOverlays.h), a QmlComponentInfo
// subclass whose inherited `title` carries the KML Placemark name.
//
// Modelled on src/FlightMap/MapItems/ADSBVehicleMapItem.qml, QGC's own "hazard at
// a location" marker: icon with a label underneath. It deliberately does not use
// QGCMapLabel, which binds map.isSatelliteMap and so needs a map reference that
// CustomMapItems.qml does not pass; an outlined label reads on any basemap anyway.
MapQuickItem {
    id: _root

    property var customMapObject    ///< OIMapOverlayItem, set at creation

    readonly property real _iconSize: ScreenTools.defaultFontPixelHeight * 1.8

    coordinate:     customMapObject ? customMapObject.coordinate : QtPositioning.coordinate()
    visible:        coordinate.isValid
    z:              QGroundControl.zOrderMapItems

    // Anchor on the icon rather than the whole item: the label hangs below and
    // must not drag the icon off the hazard it marks.
    anchorPoint.x:  markerColumn.width / 2
    anchorPoint.y:  hazardIcon.height / 2

    sourceItem: Column {
        id:         markerColumn
        spacing:    ScreenTools.defaultFontPixelHeight * 0.1

        QGCPalette { id: qgcPal; colorGroupEnabled: true }

        QGCColoredImage {
            id:                         hazardIcon
            anchors.horizontalCenter:   parent.horizontalCenter
            width:                      _root._iconSize
            height:                     _root._iconSize
            sourceSize.height:          _root._iconSize
            fillMode:                   Image.PreserveAspectFit
            source:                     "/InstrumentValueIcons/exclamation-solid.svg"
            color:                      qgcPal.colorOrange
        }

        // A DOF obstacle gets its height appended in the operator's own units; a
        // KML point has none and shows just its Placemark name.
        QGCLabel {
            anchors.horizontalCenter:   parent.horizontalCenter
            visible:                    text !== ""
            font.pointSize:             ScreenTools.smallFontPointSize
            color:                      qgcPal.colorOrange
            style:                      Text.Outline
            styleColor:                 qgcPal.window
            text: {
                if (!customMapObject) {
                    return ""
                }
                var name = customMapObject.title
                var agl = customMapObject.heightAglMeters
                if (isNaN(agl)) {
                    return name
                }
                var height = QGroundControl.unitsConversion.metersToAppSettingsVerticalDistanceUnitsString(agl, 0)
                return name === "" ? height : name + " " + height
            }
        }
    }
}
