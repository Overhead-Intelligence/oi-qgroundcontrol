/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * Hazard overlays: point layers drawn on the Fly view map. Two formats are
 * read - KML (operator drawn, from Google Earth and friends) and the FAA
 * Digital Obstacle File (.Dat), which is the one OI actually flies against.
 *
 * QGC parses KML already (src/Utilities/Geo/Formats/KMLHelper), but
 * loadPointsFromFile() keeps only coordinates - it never reads the parent
 * Placemark's <name>, so every marker would be an anonymous pin - and it fails
 * outright on a KML with no <Point> node. It knows nothing about DOF at all.
 * So OI reads both formats itself.
 *
 * A DOF is fixed-width text covering a whole US state: Florida is 43,893
 * obstacles, 13,725 of them utility poles, median height 43 ft. Drawing all of
 * them would be useless and ruinous - every marker is a QObject plus a QML
 * MapQuickItem. A layer therefore carries a minimum-AGL filter, and the manager
 * draws nothing at all rather than an arbitrary subset when the result still
 * exceeds kMaxMarkers: for hazard data, silently hiding some obstacles is worse
 * than drawing none and saying why.
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
#include <QtCore/qnumeric.h>
#include <QtPositioning/QGeoCoordinate>

#include "QmlComponentInfo.h"

class QIODevice;
class QmlObjectListModel;

/// One hazard marker. QGCCorePlugin::customMapItems() documents that entries derive
/// from QmlComponentInfo and set `url`; CustomMapItems.qml reads that url to know
/// which QML to build, then hands the whole object to it as `customMapObject`.
/// The obstacle description rides along as the inherited `title`.
class OIMapOverlayItem : public QmlComponentInfo
{
    Q_OBJECT

    Q_PROPERTY(QGeoCoordinate coordinate READ coordinate CONSTANT)
    Q_PROPERTY(double heightAglMeters READ heightAglMeters CONSTANT)  ///< NaN when the source carries no height

public:
    OIMapOverlayItem(const QGeoCoordinate &coordinate, const QString &label, double heightAglMeters,
                     QObject *parent = nullptr);

    QGeoCoordinate coordinate() const { return _coordinate; }
    double heightAglMeters() const { return _heightAglMeters; }

private:
    const QGeoCoordinate _coordinate;
    const double _heightAglMeters;
};

/// One imported file. Referenced where the operator put it, not copied: hazard
/// sets are per mission and change between trips.
class OIMapOverlayLayer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString filePath READ filePath CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString formatName READ formatName NOTIFY loadedChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int pointCount READ pointCount NOTIFY shownCountChanged)      ///< after both filters
    Q_PROPERTY(int totalPointCount READ totalPointCount NOTIFY loadedChanged) ///< as read from the file
    Q_PROPERTY(bool supportsHeightFilter READ supportsHeightFilter NOTIFY loadedChanged)
    Q_PROPERTY(int minHeightFt READ minHeightFt WRITE setMinHeightFt NOTIFY filterChanged)
    Q_PROPERTY(double radiusKm READ radiusKm WRITE setRadiusKm NOTIFY filterChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY loadedChanged)

public:
    /// A DOF holds a whole state. 200 ft is deliberate: OI's guided floor is 50 m
    /// (164 ft), so nothing shorter can be struck at the lowest altitude the fleet
    /// flies, and it is what clears the ~25,000 poles out of a state file.
    static constexpr int kDefaultMinHeightFt = 200;

    /// Height alone is not enough. Measured on 12-FL.Dat: 43,893 obstacles total,
    /// still 4,734 at 200 ft - well past kMaxMarkers, so a whole state would draw
    /// nothing. Obstacles only matter near where the aircraft actually is, so a
    /// layer is also clipped to a radius around the reference point. 0 = no limit.
    static constexpr double kDefaultRadiusKm = 25.0;

    OIMapOverlayLayer(const QString &filePath, bool enabled, int minHeightFt, double radiusKm,
                      QObject *parent = nullptr);

    QString filePath() const { return _filePath; }
    QString name() const;
    QString formatName() const { return _formatName; }
    bool enabled() const { return _enabled; }
    void setEnabled(bool enabled);

    /// Only a DOF carries obstacle heights; the filter is inert on a KML layer.
    bool supportsHeightFilter() const { return _supportsHeightFilter; }
    int minHeightFt() const { return _minHeightFt; }
    void setMinHeightFt(int minHeightFt);

    double radiusKm() const { return _radiusKm; }
    void setRadiusKm(double radiusKm);

    /// How many points survived both filters at the last rebuild. Cached because
    /// the radius test needs the manager's reference point, which the layer does
    /// not own; the manager writes it back after every rebuild.
    int pointCount() const { return _shownCount; }
    void setShownCount(int shownCount);

    int totalPointCount() const { return static_cast<int>(_points.size()); }
    QString errorString() const { return _errorString; }

    struct Point {
        QGeoCoordinate coordinate;
        QString label;
        double heightAglMeters = qQNaN();   ///< NaN when the format carries no height
        int heightAglFt = -1;               ///< -1 when unknown; what the filter compares
    };

    const QList<Point> &points() const { return _points; }

    /// Both filters. `reference` is the manager's anchor; an invalid one, or a
    /// radius of 0, disables the distance test.
    bool passesFilter(const Point &point, const QGeoCoordinate &reference) const;

    /// Re-reads the file from disk. Safe to call on a file that has gone away:
    /// the layer reports the error and contributes no markers.
    void reload();

signals:
    void enabledChanged();
    void filterChanged();
    void shownCountChanged();
    void loadedChanged();

private:
    bool _readKml(QIODevice &file);
    bool _readDof(QIODevice &file);

    const QString _filePath;
    bool _enabled;
    int _minHeightFt;
    double _radiusKm;
    int _shownCount = 0;
    bool _supportsHeightFilter = false;
    QString _formatName;
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
    Q_PROPERTY(int markerCount READ markerCount NOTIFY markersChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY markersChanged)
    Q_PROPERTY(bool overCap READ overCap NOTIFY markersChanged)
    Q_PROPERTY(int maxMarkers READ maxMarkers CONSTANT)
    Q_PROPERTY(QString referenceDescription READ referenceDescription NOTIFY markersChanged)

public:
    /// Past this the map stops being usable. Hitting it draws nothing at all
    /// rather than an arbitrary subset - see the note at the top of this file.
    static constexpr int kMaxMarkers = 2000;

    explicit OIMapOverlayManager(QObject *parent = nullptr);

    QmlObjectListModel *layers() const { return _layers; }
    QmlObjectListModel *markers() const { return _markers; }

    int markerCount() const;
    int selectedCount() const { return _selectedCount; }   ///< what the filters chose, before the cap

    /// Where the radius filter is measured from, in words, for the settings UI.
    QString referenceDescription() const { return _referenceDescription; }
    bool overCap() const { return _overCap; }
    int maxMarkers() const { return kMaxMarkers; }

    /// Imports a layer. Accepts a local path or a file:// URL (QGCFileDialog hands
    /// back the latter). Returns false and sets lastError() if the file cannot be
    /// read or is neither KML nor DOF; a file that parses to zero points is still
    /// imported, so the operator can see that it held nothing usable.
    Q_INVOKABLE bool addLayer(const QString &fileUrlOrPath);

    Q_INVOKABLE void removeLayer(int index);

    /// Re-reads every layer from disk (files change between missions).
    Q_INVOKABLE void reloadAll();

    Q_INVOKABLE QString lastError() const { return _lastError; }

signals:
    void markersChanged();

private slots:
    void _rebuildMarkers();

private:
    void _load();
    void _save() const;
    void _connectLayer(OIMapOverlayLayer *layer);

    /// The active vehicle's home when there is one, otherwise the map position QGC
    /// persists between runs. Either way the operator gets obstacles near where
    /// they are working rather than a whole state.
    QGeoCoordinate _referenceCoordinate();

    QmlObjectListModel *_layers = nullptr;
    QmlObjectListModel *_markers = nullptr;
    QString _lastError;
    QString _referenceDescription;
    int _selectedCount = 0;
    bool _overCap = false;
};
