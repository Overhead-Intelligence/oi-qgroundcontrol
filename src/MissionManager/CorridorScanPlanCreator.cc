#include "CorridorScanPlanCreator.h"
#include "PlanMasterController.h"
#include "QGCMAVLink.h"
#include "CorridorScanComplexItem.h"

CorridorScanPlanCreator::CorridorScanPlanCreator(PlanMasterController* planMasterController)
    : PlanCreator(planMasterController, CorridorScanComplexItem::tr(CorridorScanComplexItem::canonicalName), QStringLiteral("/qmlimages/PlanCreator/CorridorScanPlanCreator.png"), QGCMAVLink::allVehicleClasses())
{

}

void CorridorScanPlanCreator::createPlan(const QGeoCoordinate& mapCenterCoord)
{
    _planMasterController->removeAll();
    VisualMissionItem* takeoffItem = _missionController->insertTakeoffItem(mapCenterCoord, -1);
    _missionController->insertComplexMissionItem(CorridorScanComplexItem::canonicalName, mapCenterCoord, -1);
    // Return to launch rather than a landing pattern. The fleet lands VTOL, straight down
    // over the launch point, and the fixed-wing pattern upstream adds here describes a
    // different aircraft - it was deleted by hand on every plan.
    _missionController->insertReturnToLaunchItem(mapCenterCoord, -1);
    _missionController->setCurrentPlanViewSeqNum(takeoffItem->sequenceNumber(), true);
}
