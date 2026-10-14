#include "TakeoffMissionItem.h"
#include "MissionCommandTree.h"
#include "QGroundControlQmlGlobal.h"
#include "SettingsManager.h"
#include "PlanViewSettings.h"
#include "PlanMasterController.h"
#include "MissionSettingsItem.h"
#include "MultiVehicleManager.h"
#include "Vehicle.h"

TakeoffMissionItem::TakeoffMissionItem(PlanMasterController* masterController, bool flyView, MissionSettingsItem* settingsItem, bool forLoad)
    : SimpleMissionItem(masterController, flyView, forLoad)
    , _settingsItem(settingsItem)
{
    _init(forLoad);
}

TakeoffMissionItem::TakeoffMissionItem(MAV_CMD takeoffCmd, PlanMasterController* masterController, bool flyView, MissionSettingsItem* settingsItem, bool forLoad)
    : SimpleMissionItem(masterController, flyView, false /* forLoad */)
    , _settingsItem(settingsItem)
{
    setCommand(takeoffCmd);
    _init(forLoad);
}

TakeoffMissionItem::TakeoffMissionItem(const MissionItem& missionItem, PlanMasterController* masterController, bool flyView, MissionSettingsItem* settingsItem, bool forLoad)
    : SimpleMissionItem(masterController, flyView, missionItem)
    , _settingsItem(settingsItem)
{
    _init(forLoad);
}

TakeoffMissionItem::~TakeoffMissionItem()
{

}

void TakeoffMissionItem::_init(bool forLoad)
{
    _editorQml = QStringLiteral("qrc:/qml/QGroundControl/PlanView/SimpleItemEditor.qml");

    connect(_settingsItem, &MissionSettingsItem::coordinateChanged, this, &TakeoffMissionItem::launchCoordinateChanged);

    if (_flyView) {
        _initLaunchTakeoffAtSameLocation();
        return;
    }

    QGeoCoordinate homePosition = _settingsItem->coordinate();
    if (!homePosition.isValid()) {
        Vehicle* activeVehicle = MultiVehicleManager::instance()->activeVehicle();
        if (activeVehicle) {
            homePosition = activeVehicle->homePosition();
            if (homePosition.isValid()) {
                _settingsItem->setCoordinate(homePosition);
            }
        }
    }

    if (forLoad) {
        // Load routines will set the rest up after load
        return;
    }

    _initLaunchTakeoffAtSameLocation();
    if (_launchTakeoffAtSameLocation && homePosition.isValid()) {
        SimpleMissionItem::setCoordinate(homePosition);
    }

    // Wizard mode is set only when the launch position is missing, which genuinely needs a
    // map click before the item means anything.
    //
    // Upstream also raises it for every fixed wing, to warn about adjusting the climb out
    // position. That warning is dropped here: it is purely informational - the item stays
    // draggable afterwards - and it does not describe this fleet's workflow, where the takeoff
    // point snaps to the aircraft's own position on upload. All it did was put a Done button
    // between the operator and the takeoff settings on every new plan.
    if (!homePosition.isValid()) {
        _wizardMode = true;
    }

    setDirty(false);
}

void TakeoffMissionItem::_setLaunchTakeoffAtSameLocation(bool launchTakeoffAtSameLocation)
{
    if (launchTakeoffAtSameLocation != _launchTakeoffAtSameLocation) {
        _launchTakeoffAtSameLocation = launchTakeoffAtSameLocation;
        if (_launchTakeoffAtSameLocation) {
            _setLaunchCoordinate(coordinate());
        }
        emit launchTakeoffAtSameLocationChanged(_launchTakeoffAtSameLocation);
        setDirty(true);
    }
}

QGeoCoordinate TakeoffMissionItem::launchCoordinate(void) const
{
    return _settingsItem->coordinate();
}

void TakeoffMissionItem::setCoordinate(const QGeoCoordinate& coordinate)
{
    if (coordinate != this->coordinate()) {
        SimpleMissionItem::setCoordinate(coordinate);
        if (_launchTakeoffAtSameLocation) {
            _settingsItem->setCoordinate(coordinate);
        }
    }
}

bool TakeoffMissionItem::isTakeoffCommand(MAV_CMD command)
{
    return MissionCommandTree::instance()->isTakeoffCommand(command);
}

void TakeoffMissionItem::_initLaunchTakeoffAtSameLocation(void)
{
    if (specifiesCoordinate()) {
        if (_controllerVehicle->fixedWing() || _controllerVehicle->vtol()) {
            _setLaunchTakeoffAtSameLocation(false);
        } else {
            // PX4 specifies a coordinate for takeoff even for multi-rotor. But it makes more sense to not have a coordinate
            // from and end user standpoint. So even for PX4 we try to keep launch and takeoff at the same position. Unless the
            // user has moved/loaded launch at a different location than takeoff.
            if (coordinate().isValid() && _settingsItem->coordinate().isValid()) {
                _setLaunchTakeoffAtSameLocation(coordinate().latitude() == _settingsItem->coordinate().latitude() && coordinate().longitude() == _settingsItem->coordinate().longitude());
            } else {
                _setLaunchTakeoffAtSameLocation(true);
            }

        }
    } else {
        _setLaunchTakeoffAtSameLocation(true);
    }
}

bool TakeoffMissionItem::load(QTextStream &loadStream)
{
    bool success = SimpleMissionItem::load(loadStream);
    if (success) {
        _initLaunchTakeoffAtSameLocation();
    }
    _wizardMode = false; // Always be off for loaded items
    return success;
}

bool TakeoffMissionItem::load(const QJsonObject& json, int sequenceNumber, QString& errorString)
{
    bool success = SimpleMissionItem::load(json, sequenceNumber, errorString);
    if (success) {
        _initLaunchTakeoffAtSameLocation();
    }
    _wizardMode = false; // Always be off for loaded items
    return success;
}

void TakeoffMissionItem::_setLaunchCoordinate(const QGeoCoordinate& launchCoordinate)
{
    if (!launchCoordinate.isValid()) {
        return;
    }

    _settingsItem->setCoordinate(launchCoordinate);

    if (!coordinate().isValid()) {
        QGeoCoordinate takeoffCoordinate;
        if (_launchTakeoffAtSameLocation) {
            takeoffCoordinate = launchCoordinate;
        } else {
            double distance = SettingsManager::instance()->planViewSettings()->vtolTransitionDistance()->rawValue().toDouble(); // Default distance is VTOL transition to takeoff point distance
            if (_controllerVehicle->fixedWing()) {
                double altitude = this->altitude()->rawValue().toDouble();

                if (altitudeFrame() == QGroundControlQmlGlobal::AltitudeFrameRelative) {
                    // Offset for fixed wing climb out of 30 degrees to specified altitude
                    if (altitude != 0.0) {
                        distance = altitude / tan(qDegreesToRadians(30.0));
                    }
                } else {
                    distance = altitude * 1.5;
                }
            }
            takeoffCoordinate = launchCoordinate.atDistanceAndAzimuth(distance, 0);
        }
        SimpleMissionItem::setCoordinate(takeoffCoordinate);
    }
}
