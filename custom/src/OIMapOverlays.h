/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * Hazard overlays: KML point layers drawn on the Fly view map.
 *
 * QGC parses KML already (src/Utilities/Geo/Formats/KMLHelper), but
 * loadPointsFromFile() keeps only coordinates - it never reads the parent
 * Placemark's <name>, so every marker would be an anonymous pin - and it fails
 * outright on a KML with no <Point> node. OI reads the file itself instead, so
 * a marker can carry the operator-meaningful label the file already has, and a
 * file with no points is reported as "0 points" rather than an error.
 *
 * Rendering goes through QGCCorePlugin::customMapItems(), which
 * src/FlightMap/MapItems/CustomMapItems.qml instantiates - one QML object per
 * model entry, added to the map with map.addMapItem(). That is a Fly view only
 * hook (FlyViewMap.qml is its sole instantiation), which is what we want:
 * hazards are a flight-time concern, not a planning one.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtPositioning/QGeoCoordinate>

#include "QmlComponentInfo.h"

class QmlObjectListModel;

/// One hazard marker. QGCCorePlugin::customMapItems() documents that entries derive
/// from QmlComponentInfo and set `url`; CustomMapItems.qml reads that url to know
/// which QML to build, then hands the whole object to it as `customMapObject`.
/// The Placemark name rides along as the inherited `title`.
class OIMapOverlayItem : public QmlComponentInfo
{
    Q_OBJECT

    Q_PROPERTY(QGeoCoordinate coordinate READ coordinate CONSTANT)

public:
    OIMapOverlayItem(const QGeoCoordinate &coordinate, const QString &label, QObject *parent = nullptr);

    QGeoCoordinate coordinate() const { return _coordinate; }

private:
    const QGeoCoordinate _coordinate;
};

/// One imported KML file. The file is referenced where the operator put it, not
/// copied into the app: hazard sets are per mission and change between trips.
class OIMapOverlayLayer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString filePath READ filePath CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int pointCount READ pointCount NOTIFY loadedChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY loadedChanged)

public:
    OIMapOverlayLayer(const QString &filePath, bool enabled, QObject *parent = nullptr);

    QString filePath() const { return _filePath; }
    QString name() const;
    bool enabled() const { return _enabled; }
    void setEnabled(bool enabled);

    int pointCount() const { return static_cast<int>(_points.size()); }
    QString errorString() const { return _errorString; }

    struct Point {
        QGeoCoordinate coordinate;
        QString label;
    };

    const QList<Point> &points() const { return _points; }

    /// Re-reads the file from disk. Safe to call on a file that has gone away:
    /// the layer reports the error and contributes no markers.
    void reload();

signals:
    void enabledChanged();
    void loadedChanged();

private:
    const QString _filePath;
    bool _enabled;
    QList<Point> _points;
    QString _errorString;
};

/// Owns the imported layers, their persistence, and the flat marker model that
/// OIPlugin::customMapItems() hands to QGC.
class OIMapOverlayManager : public QObject
{
    Q_OBJECT
    Q_MOC_INCLUDE("QmlObjectListModel.h")

    Q_PROPERTY(QmlObjectListModel *layers READ layers CONSTANT)

public:
    explicit OIMapOverlayManager(QObject *parent = nullptr);

    QmlObjectListModel *layers() const { return _layers; }
    QmlObjectListModel *markers() const { return _markers; }

    /// Imports a KML file. Accepts a local path or a file:// URL (QGCFileDialog
    /// hands back the latter). Returns false and sets lastError() if the file
    /// cannot be read; a file that parses to zero points is still imported, so
    /// the operator can see that it held nothing usable.
    Q_INVOKABLE bool addLayer(const QString &fileUrlOrPath);

    Q_INVOKABLE void removeLayer(int index);

    /// Re-reads every layer from disk (files change between missions).
    Q_INVOKABLE void reloadAll();

    Q_INVOKABLE QString lastError() const { return _lastError; }

private slots:
    void _rebuildMarkers();

private:
    void _load();
    void _save() const;
    void _connectLayer(OIMapOverlayLayer *layer);

    QmlObjectListModel *_layers = nullptr;
    QmlObjectListModel *_markers = nullptr;
    QString _lastError;
};
