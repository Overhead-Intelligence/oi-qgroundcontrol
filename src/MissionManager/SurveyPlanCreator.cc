#include "SurveyPlanCreator.h"
#include "PlanMasterController.h"
#include "QGCMAVLink.h"
#include "SurveyComplexItem.h"

SurveyPlanCreator::SurveyPlanCreator(PlanMasterController* planMasterController)
    : PlanCreator(planMasterController, SurveyComplexItem::tr(SurveyComplexItem::canonicalName), QStringLiteral("/qmlimages/PlanCreator/SurveyPlanCreator.png"), QGCMAVLink::allVehicleClasses())
{

}

void SurveyPlanCreator::createPlan(const QGeoCoordinate& mapCenterCoord)
{
    _planMasterController->removeAll();
    VisualMissionItem* takeoffItem = _missionController->insertTakeoffItem(mapCenterCoord, -1);
    _missionController->insertComplexMissionItem(SurveyComplexItem::canonicalName, mapCenterCoord, -1);
    // Return to launch rather than a landing pattern. The fleet lands VTOL, straight down
    // over the launch point, and the fixed-wing pattern upstream adds here describes a
    // different aircraft - it was deleted by hand on every plan.
    _missionController->insertReturnToLaunchItem(mapCenterCoord, -1);
    _missionController->setCurrentPlanViewSeqNum(takeoffItem->sequenceNumber(), true);
}
