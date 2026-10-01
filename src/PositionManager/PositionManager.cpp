#include "PositionManager.h"
#include "AppMessages.h"
#include "QGCCorePlugin.h"
#include "SimulatedPosition.h"
// #include "QGCSensors.h"
#include "QGCLoggingCategory.h"

#include <QtCore/QApplicationStatic>
#include <QtCore/QSettings>
#include <QtCore/QPermissions>
#include <QtPositioning/QNmeaPositionInfoSource>

QGC_LOGGING_CATEGORY(QGCPositionManagerLog, "PositionManager.QGCPositionManager")

Q_APPLICATION_STATIC(QGCPositionManager, _positionManager);

QGCPositionManager::QGCPositionManager(QObject *parent)
    : QObject(parent)
{
    qCDebug(QGCPositionManagerLog) << this;
}

QGCPositionManager::~QGCPositionManager()
{
    qCDebug(QGCPositionManagerLog) << this;
}

QGCPositionManager *QGCPositionManager::instance()
{
    return _positionManager();
}

void QGCPositionManager::init()
{
    if (QGC::runningUnitTests()) {
        _simulatedSource = new SimulatedPosition(this);
        _setPositionSource(QGCPositionSource::Simulated);
    } else {
        _checkPermission();
        // After the sources are set up, so a restored manual pin wins over whatever
        // the platform GPS reports first.
        _loadManualGCSPosition();
    }
}

void QGCPositionManager::_setupPositionSources()
{
    _defaultSource = QGCCorePlugin::instance()->createPositionSource(this);
    if (_defaultSource) {
        _usingPluginSource = true;
    } else {
        qCDebug(QGCPositionManagerLog) << Q_FUNC_INFO << QGeoPositionInfoSource::availableSources();

        _defaultSource = QGeoPositionInfoSource::createDefaultSource(this);
        if (!_defaultSource) {
            qCWarning(QGCPositionManagerLog) << Q_FUNC_INFO << "No default source available";
            return;
        }
    }

    _setPositionSource(QGCPositionSource::InternalGPS);
}

void QGCPositionManager::_handlePermissionStatus(Qt::PermissionStatus permissionStatus)
{
    if (permissionStatus == Qt::PermissionStatus::Granted) {
        _setupPositionSources();
    } else {
        qCWarning(QGCPositionManagerLog) << Q_FUNC_INFO << "Location Permission Denied";
    }
}

void QGCPositionManager::_checkPermission()
{
    QLocationPermission locationPermission;
    locationPermission.setAccuracy(QLocationPermission::Precise);

    const Qt::PermissionStatus permissionStatus = QCoreApplication::instance()->checkPermission(locationPermission);
    if (permissionStatus == Qt::PermissionStatus::Undetermined) {
        QCoreApplication::instance()->requestPermission(locationPermission, this, [this](const QPermission &permission) {
            _handlePermissionStatus(permission.status());
        });
    } else {
        _handlePermissionStatus(permissionStatus);
    }
}

void QGCPositionManager::setNmeaSourceDevice(QIODevice *device)
{
    if (_nmeaSource) {
        _nmeaSource->stopUpdates();
        (void) _nmeaSource->disconnect(this);

        if (_currentSource == _nmeaSource) {
            _currentSource = nullptr;
        }

        delete _nmeaSource;
        _nmeaSource = nullptr;
    }

    _nmeaSource = new QNmeaPositionInfoSource(QNmeaPositionInfoSource::RealTimeMode, this);
    _nmeaSource->setDevice(device);
    _nmeaSource->setUserEquivalentRangeError(5.1);
    _setPositionSource(QGCPositionManager::NmeaGPS);
}

void QGCPositionManager::resetNmeaSourceDevice()
{
    if (!_nmeaSource) {
        return;
    }

    if (_currentSource == _nmeaSource) {
        // Switch away while the NMEA source is still valid so _setPositionSource() can run its
        // usual cleanup (stop updates, disconnect, and reset the stale GCS position/accuracy)
        // before we delete it. Falls back to the platform's default source (e.g. integrated GPS).
        _setPositionSource(QGCPositionManager::InternalGPS);
    } else {
        _nmeaSource->stopUpdates();
        (void) _nmeaSource->disconnect(this);
    }

    delete _nmeaSource;
    _nmeaSource = nullptr;
}

void QGCPositionManager::_positionUpdated(const QGeoPositionInfo &update)
{
    _geoPositionInfo = update;
    _gcsPositioningError = QGeoPositionInfoSource::NoError;

    // A manual position outranks the source. The source is left running so that
    // clearing the pin picks straight back up, and so accuracy reporting stays
    // live, but it must not move gcsPosition out from under the operator.
    if (_gcsPositionManual) {
        emit positionInfoUpdated(update);
        return;
    }

    QGeoCoordinate newGCSPosition(_gcsPosition);

    if (update.hasAttribute(QGeoPositionInfo::HorizontalAccuracy)) {
        if ((qAbs(update.coordinate().latitude()) > 0.001) && (qAbs(update.coordinate().longitude()) > 0.001)) {
            _gcsPositionHorizontalAccuracy = update.attribute(QGeoPositionInfo::HorizontalAccuracy);
            if (_gcsPositionHorizontalAccuracy <= kMinHorizonalAccuracyMeters) {
                newGCSPosition.setLatitude(update.coordinate().latitude());
                newGCSPosition.setLongitude(update.coordinate().longitude());
                // Stamp the local arrival time so consumers can tell how fresh gcsPosition is.
                // Updates rejected by the accuracy gate leave the stamp alone, since they leave
                // the previous coordinate in place as well.
                _gcsPositionTimestamp = QDateTime::currentDateTimeUtc();
            }
            emit gcsPositionHorizontalAccuracyChanged(_gcsPositionHorizontalAccuracy);
        }
    }

    if (update.hasAttribute(QGeoPositionInfo::VerticalAccuracy)) {
        _gcsPositionVerticalAccuracy = update.attribute(QGeoPositionInfo::VerticalAccuracy);
        if (_gcsPositionVerticalAccuracy <= kMinVerticalAccuracyMeters) {
            newGCSPosition.setAltitude(update.coordinate().altitude());
        }
    }

    _gcsPositionAccuracy = sqrt(pow(_gcsPositionHorizontalAccuracy, 2) + pow(_gcsPositionVerticalAccuracy, 2));

    _setGCSPosition(newGCSPosition);

    if (update.hasAttribute(QGeoPositionInfo::DirectionAccuracy)) {
        _gcsDirectionAccuracy = update.attribute(QGeoPositionInfo::DirectionAccuracy);
        if (_gcsDirectionAccuracy <= kMinDirectionAccuracyDegrees) {
            _setGCSHeading(update.attribute(QGeoPositionInfo::Direction));
        }
    } else if (_usingPluginSource) {
        _setGCSHeading(update.attribute(QGeoPositionInfo::Direction));
    }

    emit positionInfoUpdated(update);
}

void QGCPositionManager::_positionError(QGeoPositionInfoSource::Error gcsPositioningError)
{
    qCWarning(QGCPositionManagerLog) << Q_FUNC_INFO << "Positioning error:" << gcsPositioningError;
    _gcsPositioningError = gcsPositioningError;
}

void QGCPositionManager::_setGCSHeading(qreal newGCSHeading)
{
    if (newGCSHeading != _gcsHeading) {
        _gcsHeading = newGCSHeading;
        emit gcsHeadingChanged(_gcsHeading);
    }
}

void QGCPositionManager::_setGCSPosition(const QGeoCoordinate& newGCSPosition)
{
    if (newGCSPosition != _gcsPosition) {
        _gcsPosition = newGCSPosition;
        emit gcsPositionChanged(_gcsPosition);
    }
}

void QGCPositionManager::setManualGCSPosition(const QGeoCoordinate &coordinate)
{
    if (!coordinate.isValid()) {
        return;
    }

    _gcsPositionManual = true;
    // A hand-placed point is exact by definition; leaving a stale source accuracy
    // in place would have consumers gate on a number that no longer describes
    // anything. Same for the timestamp: this is fresh now.
    _gcsPositionHorizontalAccuracy = 0.0;
    _gcsPositionTimestamp = QDateTime::currentDateTimeUtc();

    _setGCSPosition(coordinate);
    _saveManualGCSPosition();

    emit gcsPositionHorizontalAccuracyChanged(_gcsPositionHorizontalAccuracy);
    emit gcsPositionManualChanged(_gcsPositionManual);
}

void QGCPositionManager::clearManualGCSPosition()
{
    if (!_gcsPositionManual) {
        return;
    }

    _gcsPositionManual = false;
    _gcsPositionHorizontalAccuracy = std::numeric_limits<qreal>::infinity();
    _saveManualGCSPosition();

    emit gcsPositionHorizontalAccuracyChanged(_gcsPositionHorizontalAccuracy);
    emit gcsPositionManualChanged(_gcsPositionManual);
    // gcsPosition itself is left where it was until the source produces a fix;
    // blanking it would make the marker disappear rather than simply go stale.
}

void QGCPositionManager::_loadManualGCSPosition()
{
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(kManualPositionGroup));
    const bool wasSet = settings.value(QString::fromLatin1(kManualPositionSetKey), false).toBool();
    const double latitude = settings.value(QString::fromLatin1(kManualPositionLatKey)).toDouble();
    const double longitude = settings.value(QString::fromLatin1(kManualPositionLonKey)).toDouble();
    settings.endGroup();

    if (!wasSet) {
        return;
    }

    const QGeoCoordinate coordinate(latitude, longitude);
    if (coordinate.isValid()) {
        setManualGCSPosition(coordinate);
    }
}

void QGCPositionManager::_saveManualGCSPosition() const
{
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(kManualPositionGroup));
    settings.setValue(QString::fromLatin1(kManualPositionSetKey), _gcsPositionManual);
    if (_gcsPositionManual) {
        settings.setValue(QString::fromLatin1(kManualPositionLatKey), _gcsPosition.latitude());
        settings.setValue(QString::fromLatin1(kManualPositionLonKey), _gcsPosition.longitude());
    }
    settings.endGroup();
}

void QGCPositionManager::_setPositionSource(QGCPositionSource source)
{
    if (_currentSource != nullptr) {
        _currentSource->stopUpdates();
        // Note the receiver-side overload: disconnect(_currentSource) would drop our own signals
        // to the source (of which there are none), leaving source->this connected and duplicating
        // it every time a source is re-selected.
        (void) _currentSource->disconnect(this);

        _geoPositionInfo = QGeoPositionInfo();
        emit positionInfoUpdated(_geoPositionInfo);

        _setGCSPosition(QGeoCoordinate());
        _gcsPositionTimestamp = QDateTime();

        _setGCSHeading(qQNaN());

        _gcsPositionHorizontalAccuracy = std::numeric_limits<qreal>::infinity();
        emit gcsPositionHorizontalAccuracyChanged(_gcsPositionHorizontalAccuracy);
    }

    switch (source) {
    case QGCPositionManager::Log:
        break;
    case QGCPositionManager::Simulated:
        _currentSource = _simulatedSource;
        break;
    case QGCPositionManager::NmeaGPS:
        _currentSource = _nmeaSource;
        break;
    case QGCPositionManager::InternalGPS:
        _currentSource = _defaultSource;
        break;
    case QGCPositionManager::ExternalGPS:
        break;
    default:
        _currentSource = _defaultSource;
        break;
    }

    if (_currentSource != nullptr) {
        _currentSource->setPreferredPositioningMethods(QGeoPositionInfoSource::SatellitePositioningMethods);
        _updateInterval = _currentSource->minimumUpdateInterval();
        #if !defined(Q_OS_DARWIN) && !defined(Q_OS_IOS)
            _currentSource->setUpdateInterval(_updateInterval);
        #endif

        (void) connect(_currentSource, &QGeoPositionInfoSource::positionUpdated, this, &QGCPositionManager::_positionUpdated);
        (void) connect(_currentSource, &QGeoPositionInfoSource::errorOccurred, this, &QGCPositionManager::_positionError);

        // (void) connect(QGCCompass::instance(), &QGCCompass::positionUpdated, this, &QGCPositionManager::_positionUpdated);

        _currentSource->startUpdates();
    }
}
