#pragma once

#include "PlanCreator.h"

/// Takeoff, hold over the launch point indefinitely, then return.
///
/// The loiter is placed on the takeoff point and follows it while the plan is edited, so the
/// hold stays over launch without the operator lining up two markers by hand. It stops
/// following the moment the loiter is dragged somewhere else deliberately.
///
/// It is a plain coordinate, not ArduPilot's zero-lat/lng "loiter on the spot". That does
/// track the aircraft - set_next_WP() substitutes the current position for a zero coordinate -
/// but QGC marks MAV_CMD_NAV_LOITER_UNLIM as specifying a coordinate, so a zeroed item lands
/// at 0N 0E on the map and takes the plan's distance and terrain profile with it.
class LoiterOverHomePlanCreator : public PlanCreator
{
    Q_OBJECT

public:
    LoiterOverHomePlanCreator(PlanMasterController* planMasterController);

    Q_INVOKABLE void createPlan(const QGeoCoordinate& mapCenterCoord) final;
};
