#include "LoiterOverHomePlanCreator.h"

#include <memory>

#include "MissionController.h"
#include "MissionItem.h"
#include "PlanMasterController.h"
#include "QGCMAVLink.h"
#include "SimpleMissionItem.h"

/// Hold altitude, for both the takeoff and the loiter. Deliberately not the takeoff default
/// the survey patterns use: there is no transition to make the climb up on here, so the
/// aircraft goes straight to the holding altitude.
static constexpr double kHoldAltitudeMeters = 120.0;

/// Orbit radius. Comfortably outside the airframe's turn radius at cruise, and wide enough
/// that the hold is a recognisable circle on the map rather than a tight spiral.
static constexpr double kLoiterRadiusMeters = 250.0;

LoiterOverHomePlanCreator::LoiterOverHomePlanCreator(PlanMasterController* planMasterController)
    : PlanCreator(planMasterController, tr("Loiter Over Home"), QStringLiteral("/qmlimages/PlanCreator/LoiterOverHomePlanCreator.png"), QGCMAVLink::allVehicleClasses())
{

}

void LoiterOverHomePlanCreator::createPlan(const QGeoCoordinate& mapCenterCoord)
{
    _planMasterController->removeAll();

    VisualMissionItem* takeoffVisual = _missionController->insertTakeoffItem(mapCenterCoord, -1);
    if (SimpleMissionItem* takeoffItem = qobject_cast<SimpleMissionItem*>(takeoffVisual)) {
        takeoffItem->altitude()->setRawValue(kHoldAltitudeMeters);
    }

    VisualMissionItem* loiterVisual = _missionController->insertSimpleMissionItem(mapCenterCoord, -1);
    if (SimpleMissionItem* loiterItem = qobject_cast<SimpleMissionItem*>(loiterVisual)) {
        // Order matters: setCommand() reapplies the defaults for the new command, so altitude
        // and radius have to be written after it rather than before.
        loiterItem->setCommand(MAV_CMD_NAV_LOITER_UNLIM);
        loiterItem->altitude()->setRawValue(kHoldAltitudeMeters);
        loiterItem->missionItem().setParam3(kLoiterRadiusMeters);

        if (takeoffVisual) {
            loiterItem->setCoordinate(takeoffVisual->coordinate());

            // Keep the hold over the takeoff point as that point is moved - either by the
            // operator dragging it, or by the launch position arriving from the vehicle.
            //
            // Tracked against the last coordinate this applied rather than followed blindly:
            // once the loiter has been dragged somewhere of its own, the operator has said
            // where they want it and it stops being pulled back.
            auto lastApplied = std::make_shared<QGeoCoordinate>(takeoffVisual->coordinate());
            connect(takeoffVisual, &VisualMissionItem::coordinateChanged, loiterItem,
                    [loiterItem, lastApplied](const QGeoCoordinate& takeoffCoordinate) {
                        if (loiterItem->coordinate() != *lastApplied) {
                            return;
                        }
                        loiterItem->setCoordinate(takeoffCoordinate);
                        *lastApplied = takeoffCoordinate;
                    });
        }
    }

    _missionController->insertReturnToLaunchItem(mapCenterCoord, -1);

    if (takeoffVisual) {
        _missionController->setCurrentPlanViewSeqNum(takeoffVisual->sequenceNumber(), true);
    }
}
