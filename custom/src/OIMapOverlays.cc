/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#include "OIMapOverlays.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QIODevice>
#include <QtCore/QSettings>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <QtCore/QXmlStreamReader>

#include "MultiVehicleManager.h"
#include "QGroundControlQmlGlobal.h"
#include "QmlObjectListModel.h"
#include "Vehicle.h"

namespace {

constexpr const char *kMarkerQml = "qrc:/custom/qml/OIMapOverlayMarker.qml";
constexpr const char *kSettingsArray = "OI/MapOverlays";
constexpr const char *kKeyPath = "path";
constexpr const char *kKeyEnabled = "enabled";
constexpr const char *kKeyMinHeightM = "minHeightM";
/// Pre-metres key, read once so an existing layer keeps its filter.
constexpr const char *kKeyLegacyMinHeightFt = "minHeightFt";
constexpr const char *kKeyRadiusKm = "radiusKm";

constexpr double kFeetToMeters = 0.3048;

/// FAA Digital Obstacle File record layout, 0-based half-open slices. Verified
/// against 12-FL.Dat (2026-08-02 currency), all 43,893 records parsing clean.
/// Columns are fixed width; the file is CRLF and latin-1.
constexpr int kDofMinRecordLen = 100;
constexpr int kDofLatStart = 35, kDofLatLen = 12;   ///< "DD MM SS.SSH"
constexpr int kDofLonStart = 48, kDofLonLen = 13;   ///< "DDD MM SS.SSH"
constexpr int kDofTypeStart = 62, kDofTypeLen = 18;
constexpr int kDofAglStart = 83, kDofAglLen = 5;

/// KML coordinate tuples are "lon,lat[,alt]" and a <coordinates> element may hold
/// several, whitespace separated. A Point has exactly one; take the first and
/// ignore altitude, which for a hazard marker is not ours to interpret.
QGeoCoordinate firstCoordinate(const QString &text)
{
    const QStringList tuples = text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tuples.isEmpty()) {
        return QGeoCoordinate();
    }

    const QStringList parts = tuples.first().split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        return QGeoCoordinate();
    }

    bool lonOk = false;
    bool latOk = false;
    const double lon = parts.at(0).toDouble(&lonOk);
    const double lat = parts.at(1).toDouble(&latOk);
    if (!lonOk || !latOk) {
        return QGeoCoordinate();
    }

    const QGeoCoordinate coordinate(lat, lon);
    return coordinate.isValid() ? coordinate : QGeoCoordinate();
}

/// "DD MM SS.SSH" / "DDD MM SS.SSH" -> signed degrees. Returns NaN if malformed,
/// which the caller treats as "skip this record" rather than as a file error: a
/// DOF occasionally carries a blank coordinate and one bad row must not lose the
/// other forty thousand.
double dofDegrees(const QString &field)
{
    const QString trimmed = field.trimmed();
    if (trimmed.size() < 4) {
        return qQNaN();
    }

    const QChar hemisphere = trimmed.back();
    const QStringList parts = trimmed.left(trimmed.size() - 1).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() != 3) {
        return qQNaN();
    }

    bool dOk = false;
    bool mOk = false;
    bool sOk = false;
    const double degrees = parts.at(0).toDouble(&dOk);
    const double minutes = parts.at(1).toDouble(&mOk);
    const double seconds = parts.at(2).toDouble(&sOk);
    if (!dOk || !mOk || !sOk) {
        return qQNaN();
    }

    const double value = degrees + (minutes / 60.0) + (seconds / 3600.0);
    return ((hemisphere == QLatin1Char('S')) || (hemisphere == QLatin1Char('W'))) ? -value : value;
}

} // namespace

/*===========================================================================*/

OIMapOverlayItem::OIMapOverlayItem(const QGeoCoordinate &coordinate, const QString &label, double heightAglMeters,
                                   QObject *parent)
    : QmlComponentInfo(label, QUrl::fromUserInput(QString::fromLatin1(kMarkerQml)), QUrl(), parent)
    , _coordinate(coordinate)
    , _heightAglMeters(heightAglMeters)
{
}

/*===========================================================================*/

OIMapOverlayLayer::OIMapOverlayLayer(const QString &filePath, bool enabled, int minHeightM, double radiusKm,
                                     QObject *parent)
    : QObject(parent)
    , _filePath(filePath)
    , _enabled(enabled)
    , _minHeightM(minHeightM)
    , _radiusKm(radiusKm)
{
    reload();
}

QString OIMapOverlayLayer::name() const
{
    return QFileInfo(_filePath).completeBaseName();
}

void OIMapOverlayLayer::setEnabled(bool enabled)
{
    if (_enabled == enabled) {
        return;
    }

    _enabled = enabled;
    emit enabledChanged();
}

void OIMapOverlayLayer::setMinHeightM(int minHeightM)
{
    const int clamped = qMax(0, minHeightM);
    if (_minHeightM == clamped) {
        return;
    }

    _minHeightM = clamped;
    emit filterChanged();
}

void OIMapOverlayLayer::setRadiusKm(double radiusKm)
{
    const double clamped = qMax(0.0, radiusKm);
    if (qFuzzyCompare(_radiusKm, clamped)) {
        return;
    }

    _radiusKm = clamped;
    emit filterChanged();
}

void OIMapOverlayLayer::setShownCount(int shownCount)
{
    if (_shownCount == shownCount) {
        return;
    }

    _shownCount = shownCount;
    emit shownCountChanged();
}

bool OIMapOverlayLayer::passesFilter(const Point &point, const QGeoCoordinate &reference) const
{
    // An obstacle with no readable height is kept: unknown is not the same as
    // short, and dropping it would hide a hazard.
    if (_supportsHeightFilter && !qIsNaN(point.heightAglMeters) &&
        (point.heightAglMeters < static_cast<double>(_minHeightM))) {
        return false;
    }

    if ((_radiusKm > 0.0) && reference.isValid()) {
        if (point.coordinate.distanceTo(reference) > (_radiusKm * 1000.0)) {
            return false;
        }
    }

    return true;
}

void OIMapOverlayLayer::reload()
{
    _points.clear();
    _errorString.clear();
    _supportsHeightFilter = false;
    _formatName.clear();

    QFile file(_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        _errorString = tr("Cannot read %1").arg(_filePath);
        emit loadedChanged();
        return;
    }

    // Sniff rather than trust the extension: operators rename these files, and a
    // DOF arrives as .Dat, .DAT or .dat depending on who unzipped it.
    const QByteArray head = file.peek(4096);
    const bool looksXml = head.contains("<kml") || head.contains("<?xml");
    const bool ok = looksXml ? _readKml(file) : _readDof(file);
    Q_UNUSED(ok);

    emit loadedChanged();
    emit filterChanged();
}

bool OIMapOverlayLayer::_readKml(QIODevice &file)
{
    _formatName = tr("KML");

    // Forward-only walk: remember the most recent <name> at Placemark level and
    // pair it with the <coordinates> of the <Point> in the same Placemark. A
    // Placemark holding a polygon or a line is skipped - this is a point-hazard
    // overlay, not a mission shape importer.
    QXmlStreamReader xml(&file);
    QString placemarkName;
    bool inPlacemark = false;
    bool inPoint = false;

    while (!xml.atEnd() && !xml.hasError()) {
        const QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            const QStringView element = xml.name();
            if (element == QLatin1String("Placemark")) {
                inPlacemark = true;
                placemarkName.clear();
            } else if (inPlacemark && (element == QLatin1String("Point"))) {
                inPoint = true;
            } else if (inPlacemark && !inPoint && (element == QLatin1String("name"))) {
                placemarkName = xml.readElementText().trimmed();
            } else if (inPoint && (element == QLatin1String("coordinates"))) {
                const QGeoCoordinate coordinate = firstCoordinate(xml.readElementText());
                if (coordinate.isValid()) {
                    _points.append({coordinate, placemarkName, qQNaN()});
                }
            }
        } else if (token == QXmlStreamReader::EndElement) {
            const QStringView element = xml.name();
            if (element == QLatin1String("Point")) {
                inPoint = false;
            } else if (element == QLatin1String("Placemark")) {
                inPlacemark = false;
            }
        }
    }

    if (xml.hasError()) {
        _points.clear();
        _errorString = tr("Line %1: %2").arg(xml.lineNumber()).arg(xml.errorString());
        return false;
    }

    return true;
}

bool OIMapOverlayLayer::_readDof(QIODevice &file)
{
    _formatName = tr("FAA DOF");
    _supportsHeightFilter = true;

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Latin1);

    int malformed = 0;

    while (!stream.atEnd()) {
        const QString line = stream.readLine();

        // The four header lines and the dashed rule are shorter than a record and
        // fail the OAS-number shape, so no explicit header skip is needed.
        if (line.size() < kDofMinRecordLen) {
            continue;
        }
        if ((line.at(2) != QLatin1Char('-')) || (line.at(9) != QLatin1Char(' '))) {
            continue;
        }

        const double latitude = dofDegrees(line.mid(kDofLatStart, kDofLatLen));
        const double longitude = dofDegrees(line.mid(kDofLonStart, kDofLonLen));
        if (qIsNaN(latitude) || qIsNaN(longitude)) {
            malformed++;
            continue;
        }

        const QGeoCoordinate coordinate(latitude, longitude);
        if (!coordinate.isValid()) {
            malformed++;
            continue;
        }

        // The DOF is in feet; everything past this point is metres.
        bool aglOk = false;
        const int aglFt = line.mid(kDofAglStart, kDofAglLen).trimmed().toInt(&aglOk);

        Point point;
        point.coordinate = coordinate;
        point.label = line.mid(kDofTypeStart, kDofTypeLen).trimmed();
        point.heightAglMeters = aglOk ? (aglFt * kFeetToMeters) : qQNaN();
        _points.append(point);
    }

    if (_points.isEmpty()) {
        _errorString = tr("No obstacle records found - is this an FAA DOF file?");
        return false;
    }

    if (malformed > 0) {
        _errorString = tr("%1 record(s) skipped: unreadable coordinates").arg(malformed);
    }

    return true;
}

/*===========================================================================*/

OIMapOverlayManager::OIMapOverlayManager(QObject *parent)
    : QObject(parent)
    , _layers(new QmlObjectListModel(this))
    , _markers(new QmlObjectListModel(this))
{
    _load();

    // The radius filter is anchored on the vehicle home, so the marker set has to
    // follow it: a vehicle appearing, and that vehicle announcing home after
    // connecting, both change which obstacles are near.
    MultiVehicleManager *const vehicleManager = MultiVehicleManager::instance();
    (void) connect(vehicleManager, &MultiVehicleManager::activeVehicleChanged, this, [this](Vehicle *vehicle) {
        if (vehicle) {
            (void) connect(vehicle, &Vehicle::homePositionChanged, this, &OIMapOverlayManager::_rebuildMarkers,
                           Qt::UniqueConnection);
        }
        _rebuildMarkers();
    });

    // With no vehicle the anchor is the map position, which nothing signals to us.
    // See _checkReferenceMoved().
    _referenceWatchTimer = new QTimer(this);
    _referenceWatchTimer->setInterval(kReferenceWatchMs);
    (void) connect(_referenceWatchTimer, &QTimer::timeout, this, &OIMapOverlayManager::_checkReferenceMoved);
    _referenceWatchTimer->start();

    _rebuildMarkers();
}

void OIMapOverlayManager::_checkReferenceMoved()
{
    if (_layers->count() == 0) {
        return;
    }

    const QGeoCoordinate reference = _referenceCoordinate();
    if (!reference.isValid()) {
        return;
    }

    if (_lastReference.isValid() && (reference.distanceTo(_lastReference) < kReferenceMoveM)) {
        return;
    }

    _rebuildMarkers();
}

int OIMapOverlayManager::markerCount() const
{
    return _markers->count();
}

bool OIMapOverlayManager::addLayer(const QString &fileUrlOrPath)
{
    _lastError.clear();

    const QUrl url(fileUrlOrPath);
    const QString path = url.isLocalFile() ? url.toLocalFile() : fileUrlOrPath;

    if (path.isEmpty()) {
        _lastError = tr("No file selected");
        return false;
    }

    if (!QFileInfo::exists(path)) {
        _lastError = tr("%1 does not exist").arg(path);
        return false;
    }

    for (int i = 0; i < _layers->count(); i++) {
        const OIMapOverlayLayer *existing = qobject_cast<OIMapOverlayLayer *>((*_layers)[i]);
        if (existing && (existing->filePath() == path)) {
            _lastError = tr("%1 is already imported").arg(QFileInfo(path).fileName());
            return false;
        }
    }

    OIMapOverlayLayer *layer = new OIMapOverlayLayer(
        path, true, OIMapOverlayLayer::kDefaultMinHeightM, OIMapOverlayLayer::kDefaultRadiusKm, this);
    if (layer->totalPointCount() == 0) {
        _lastError = layer->errorString().isEmpty() ? tr("No points found in %1").arg(QFileInfo(path).fileName())
                                                    : layer->errorString();
        layer->deleteLater();
        return false;
    }

    _connectLayer(layer);
    _layers->append(layer);
    _save();
    _rebuildMarkers();
    return true;
}

void OIMapOverlayManager::removeLayer(int index)
{
    if ((index < 0) || (index >= _layers->count())) {
        return;
    }

    QObject *removed = _layers->removeAt(index);
    if (removed) {
        removed->deleteLater();
    }

    _save();
    _rebuildMarkers();
}

void OIMapOverlayManager::reloadAll()
{
    for (int i = 0; i < _layers->count(); i++) {
        OIMapOverlayLayer *layer = qobject_cast<OIMapOverlayLayer *>((*_layers)[i]);
        if (layer) {
            layer->reload();
        }
    }

    _rebuildMarkers();
}

QGeoCoordinate OIMapOverlayManager::_referenceCoordinate()
{
    Vehicle *const vehicle = MultiVehicleManager::instance()->activeVehicle();
    if (vehicle) {
        const QGeoCoordinate home = vehicle->homePosition();
        if (home.isValid()) {
            _referenceDescription = tr("vehicle home");
            return home;
        }
    }

    // No vehicle yet: fall back to where QGC last had the map. That is persisted
    // across runs, so planning at the desk still shows local obstacles.
    const QGeoCoordinate mapPosition = QGroundControlQmlGlobal::flightMapPosition();
    if (mapPosition.isValid()) {
        _referenceDescription = tr("last map position");
        return mapPosition;
    }

    _referenceDescription = tr("none yet, radius filter inactive");
    return QGeoCoordinate();
}

void OIMapOverlayManager::_rebuildMarkers()
{
    // Markers are owned by this model, not by the layers: CustomMapItems.qml keys
    // its Instantiator off the model, so replacing the contents wholesale is what
    // makes the map redraw.
    _markers->clearAndDeleteContents();

    const QGeoCoordinate reference = _referenceCoordinate();
    _lastReference = reference;

    // Count first. Past the cap nothing is drawn at all: a partial hazard overlay
    // is more dangerous than an absent one, because it looks complete.
    _selectedCount = 0;
    for (int i = 0; i < _layers->count(); i++) {
        OIMapOverlayLayer *layer = qobject_cast<OIMapOverlayLayer *>((*_layers)[i]);
        if (!layer) {
            continue;
        }

        int shown = 0;
        const QList<OIMapOverlayLayer::Point> points = layer->points();
        for (const OIMapOverlayLayer::Point &point : points) {
            if (layer->passesFilter(point, reference)) {
                shown++;
            }
        }
        layer->setShownCount(shown);

        if (layer->enabled()) {
            _selectedCount += shown;
        }
    }

    _overCap = (_selectedCount > kMaxMarkers);
    if (_overCap) {
        emit markersChanged();
        return;
    }

    for (int i = 0; i < _layers->count(); i++) {
        const OIMapOverlayLayer *layer = qobject_cast<OIMapOverlayLayer *>((*_layers)[i]);
        if (!layer || !layer->enabled()) {
            continue;
        }

        const QList<OIMapOverlayLayer::Point> points = layer->points();
        for (const OIMapOverlayLayer::Point &point : points) {
            if (!layer->passesFilter(point, reference)) {
                continue;
            }
            _markers->append(new OIMapOverlayItem(point.coordinate, point.label, point.heightAglMeters, _markers));
        }
    }

    emit markersChanged();
}

void OIMapOverlayManager::_connectLayer(OIMapOverlayLayer *layer)
{
    connect(layer, &OIMapOverlayLayer::enabledChanged, this, [this]() {
        _save();
        _rebuildMarkers();
    });
    connect(layer, &OIMapOverlayLayer::filterChanged, this, [this]() {
        _save();
        _rebuildMarkers();
    });
}

void OIMapOverlayManager::_load()
{
    QSettings settings;
    const int count = settings.beginReadArray(QString::fromLatin1(kSettingsArray));
    for (int i = 0; i < count; i++) {
        settings.setArrayIndex(i);
        const QString path = settings.value(QString::fromLatin1(kKeyPath)).toString();
        if (path.isEmpty()) {
            continue;
        }

        // The filter moved from feet to metres. Convert a value saved by an older
        // build rather than silently resetting it to the default - this filter
        // decides which hazards are drawn.
        int minHeightM = OIMapOverlayLayer::kDefaultMinHeightM;
        if (settings.contains(QString::fromLatin1(kKeyMinHeightM))) {
            minHeightM = settings.value(QString::fromLatin1(kKeyMinHeightM)).toInt();
        } else if (settings.contains(QString::fromLatin1(kKeyLegacyMinHeightFt))) {
            minHeightM = qRound(settings.value(QString::fromLatin1(kKeyLegacyMinHeightFt)).toInt() * kFeetToMeters);
        }

        // A layer whose file has gone missing is kept rather than dropped: the
        // operator sees why it stopped drawing instead of the row vanishing.
        OIMapOverlayLayer *layer = new OIMapOverlayLayer(
            path,
            settings.value(QString::fromLatin1(kKeyEnabled), true).toBool(),
            minHeightM,
            settings.value(QString::fromLatin1(kKeyRadiusKm), OIMapOverlayLayer::kDefaultRadiusKm).toDouble(),
            this);
        _connectLayer(layer);
        _layers->append(layer);
    }
    settings.endArray();
}

void OIMapOverlayManager::_save() const
{
    QSettings settings;

    // beginWriteArray() writes the new "size" but does not delete the indices
    // above it, so removing a layer used to leave its keys behind forever. Clear
    // the subtree first and write it fresh.
    settings.remove(QString::fromLatin1(kSettingsArray));
    settings.beginWriteArray(QString::fromLatin1(kSettingsArray), _layers->count());
    for (int i = 0; i < _layers->count(); i++) {
        const OIMapOverlayLayer *layer = qobject_cast<OIMapOverlayLayer *>((*_layers)[i]);
        if (!layer) {
            continue;
        }
        settings.setArrayIndex(i);
        settings.setValue(QString::fromLatin1(kKeyPath), layer->filePath());
        settings.setValue(QString::fromLatin1(kKeyEnabled), layer->enabled());
        settings.setValue(QString::fromLatin1(kKeyMinHeightM), layer->minHeightM());
        settings.setValue(QString::fromLatin1(kKeyRadiusKm), layer->radiusKm());
    }
    settings.endArray();
}
