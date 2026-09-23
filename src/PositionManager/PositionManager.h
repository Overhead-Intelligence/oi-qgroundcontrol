#pragma once

#include <QtCore/QDateTime>
#include <QtCore/QObject>
#include <QtPositioning/QGeoCoordinate>
#include <QtPositioning/QGeoPositionInfo>
#include <QtPositioning/QGeoPositionInfoSource>
#include <QtQmlIntegration/QtQmlIntegration>

class QNmeaPositionInfoSource;
class QGCCompass;

class QGCPositionManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

    Q_PROPERTY(QGeoCoordinate gcsPosition                   READ gcsPosition                    NOTIFY gcsPositionChanged)
    Q_PROPERTY(qreal          gcsHeading                    READ gcsHeading                     NOTIFY gcsHeadingChanged)
    Q_PROPERTY(qreal          gcsPositionHorizontalAccuracy READ gcsPositionHorizontalAccuracy  NOTIFY gcsPositionHorizontalAccuracyChanged)
    Q_PROPERTY(bool           gcsPositionManual             READ gcsPositionManual              NOTIFY gcsPositionManualChanged)

public:
    explicit QGCPositionManager(QObject *parent = nullptr);
    ~QGCPositionManager();

    /// Gets the singleton instance of AudioOutput.
    ///     @return The singleton instance.
    static QGCPositionManager *instance();

    void init();
    QGeoCoordinate gcsPosition() const { return _gcsPosition; }
    qreal gcsHeading() const { return _gcsHeading; }
    qreal gcsPositionHorizontalAccuracy() const { return _gcsPositionHorizontalAccuracy; }
    QGeoPositionInfo geoPositionInfo() const { return _geoPositionInfo; }
    QGeoPositionInfoSource::Error gcsPositioningError() const { return _gcsPositioningError; }

    /// Local arrival time of the last position update which passed the accuracy gates and was
    /// copied into gcsPosition. Invalid until the first such update arrives. This is the local
    /// clock rather than the position source's own timestamp, which on some platforms (e.g.
    /// Android) is offset from the system clock.
    ///     @return Arrival time, in UTC, of the last position update applied to gcsPosition.
    QDateTime gcsPositionTimestamp() const { return _gcsPositionTimestamp; }

    int updateInterval() const { return _updateInterval; }

    /// True while the operator has pinned the GCS position by hand. A manual
    /// position wins over every position source: a GCS on a laptop with no GPS,
    /// or with a GPS that reports the wrong place, is the normal case for OI, and
    /// anything keyed off gcsPosition (the map marker, Remote ID, hazard overlays)
    /// is only as good as that coordinate.
    bool gcsPositionManual() const { return _gcsPositionManual; }

    /// Pins the GCS position. Persisted, so it survives a restart - an operator who
    /// set it at a site should not silently lose it on the next launch.
    Q_INVOKABLE void setManualGCSPosition(const QGeoCoordinate &coordinate);

    /// Releases the pin and hands control back to the active position source.
    Q_INVOKABLE void clearManualGCSPosition();

    void setNmeaSourceDevice(QIODevice *device);
    /// Tears down any active NMEA source and falls back to the platform's default
    /// position source (e.g. the integrated Android GPS).
    void resetNmeaSourceDevice();

signals:
    void gcsPositionChanged(QGeoCoordinate gcsPosition);
    void gcsHeadingChanged(qreal gcsHeading);
    void positionInfoUpdated(QGeoPositionInfo update);
    void gcsPositionHorizontalAccuracyChanged(qreal gcsPositionHorizontalAccuracy);
    void gcsPositionManualChanged(bool gcsPositionManual);

private slots:
    void _positionUpdated(const QGeoPositionInfo &update);
    void _positionError(QGeoPositionInfoSource::Error gcsPositioningError);

private:
    enum QGCPositionSource {
        Simulated,
        InternalGPS,
        Log,
        NmeaGPS,
        ExternalGPS
    };

    void _setPositionSource(QGCPositionSource source);
    void _setupPositionSources();
    void _handlePermissionStatus(Qt::PermissionStatus permissionStatus);
    void _checkPermission();
    void _setGCSHeading(qreal newGCSHeading);
    void _setGCSPosition(const QGeoCoordinate &newGCSPosition);
    void _loadManualGCSPosition();
    void _saveManualGCSPosition() const;

    bool _usingPluginSource = false;
    bool _gcsPositionManual = false;
    int _updateInterval = 0;

    QGeoPositionInfo _geoPositionInfo;
    QGeoPositionInfoSource::Error  _gcsPositioningError = QGeoPositionInfoSource::NoError;

    QGeoCoordinate _gcsPosition;
    QDateTime _gcsPositionTimestamp;
    qreal _gcsHeading = qQNaN();
    qreal _gcsPositionHorizontalAccuracy = std::numeric_limits<qreal>::infinity();
    qreal _gcsPositionVerticalAccuracy = std::numeric_limits<qreal>::infinity();
    qreal _gcsPositionAccuracy = std::numeric_limits<qreal>::infinity();
    qreal _gcsDirectionAccuracy = std::numeric_limits<qreal>::infinity();

    QGeoPositionInfoSource *_currentSource = nullptr;
    QGeoPositionInfoSource *_defaultSource = nullptr;
    QNmeaPositionInfoSource *_nmeaSource = nullptr;
    QGeoPositionInfoSource *_simulatedSource = nullptr;

    QGCCompass *_compass = nullptr;

    static constexpr const char *kManualPositionGroup = "GCSManualPosition";
    static constexpr const char *kManualPositionSetKey = "Set";
    static constexpr const char *kManualPositionLatKey = "Latitude";
    static constexpr const char *kManualPositionLonKey = "Longitude";

    static constexpr qreal kMinHorizonalAccuracyMeters = 100.;
    static constexpr qreal kMinVerticalAccuracyMeters = 10.;
    static constexpr qreal kMinDirectionAccuracyDegrees = 30.;
};
