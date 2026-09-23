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
#include <QtCore/QSettings>
#include <QtCore/QStringList>
#include <QtCore/QXmlStreamReader>

#include "OIPlugin.h"
#include "QmlObjectListModel.h"

namespace {

constexpr const char *kMarkerQml = "qrc:/custom/qml/OIMapOverlayMarker.qml";
constexpr const char *kSettingsArray = "OI/MapOverlays";
constexpr const char *kKeyPath = "path";
constexpr const char *kKeyEnabled = "enabled";

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

} // namespace

/*===========================================================================*/

OIMapOverlayItem::OIMapOverlayItem(const QGeoCoordinate &coordinate, const QString &label, QObject *parent)
    : QmlComponentInfo(label, QUrl::fromUserInput(QString::fromLatin1(kMarkerQml)), QUrl(), parent)
    , _coordinate(coordinate)
{
}

/*===========================================================================*/

OIMapOverlayLayer::OIMapOverlayLayer(const QString &filePath, bool enabled, QObject *parent)
    : QObject(parent)
    , _filePath(filePath)
    , _enabled(enabled)
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

void OIMapOverlayLayer::reload()
{
    _points.clear();
    _errorString.clear();

    QFile file(_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        _errorString = tr("Cannot read %1").arg(_filePath);
        emit loadedChanged();
        return;
    }

    // Forward-only walk: remember the most recent <name> at Placemark level, and
    // pair it with the <coordinates> of the <Point> in the same Placemark. A
    // Placemark that holds a polygon or a line rather than a point is skipped -
    // this is a point-hazard overlay, not a mission shape importer.
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
                    _points.append({coordinate, placemarkName});
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
    }

    emit loadedChanged();
}

/*===========================================================================*/

OIMapOverlayManager::OIMapOverlayManager(QObject *parent)
    : QObject(parent)
    , _layers(new QmlObjectListModel(this))
    , _markers(new QmlObjectListModel(this))
{
    _load();
    _rebuildMarkers();
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

    OIMapOverlayLayer *layer = new OIMapOverlayLayer(path, true, this);
    if (!layer->errorString().isEmpty()) {
        _lastError = layer->errorString();
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

void OIMapOverlayManager::_rebuildMarkers()
{
    // The markers are owned by this model, not by the layers: CustomMapItems.qml
    // keys its Instantiator off the model, so replacing the contents wholesale is
    // what makes the map redraw.
    _markers->clearAndDeleteContents();

    for (int i = 0; i < _layers->count(); i++) {
        const OIMapOverlayLayer *layer = qobject_cast<OIMapOverlayLayer *>((*_layers)[i]);
        if (!layer || !layer->enabled()) {
            continue;
        }

        const QList<OIMapOverlayLayer::Point> points = layer->points();
        for (const OIMapOverlayLayer::Point &point : points) {
            _markers->append(new OIMapOverlayItem(point.coordinate, point.label, _markers));
        }
    }
}

void OIMapOverlayManager::_connectLayer(OIMapOverlayLayer *layer)
{
    connect(layer, &OIMapOverlayLayer::enabledChanged, this, [this]() {
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

        // A layer whose file has gone missing is kept rather than dropped: the
        // operator sees why it stopped drawing instead of the row vanishing.
        OIMapOverlayLayer *layer =
            new OIMapOverlayLayer(path, settings.value(QString::fromLatin1(kKeyEnabled), true).toBool(), this);
        _connectLayer(layer);
        _layers->append(layer);
    }
    settings.endArray();
}

void OIMapOverlayManager::_save() const
{
    QSettings settings;
    settings.beginWriteArray(QString::fromLatin1(kSettingsArray), _layers->count());
    for (int i = 0; i < _layers->count(); i++) {
        const OIMapOverlayLayer *layer = qobject_cast<OIMapOverlayLayer *>((*_layers)[i]);
        if (!layer) {
            continue;
        }
        settings.setArrayIndex(i);
        settings.setValue(QString::fromLatin1(kKeyPath), layer->filePath());
        settings.setValue(QString::fromLatin1(kKeyEnabled), layer->enabled());
    }
    settings.endArray();
}
