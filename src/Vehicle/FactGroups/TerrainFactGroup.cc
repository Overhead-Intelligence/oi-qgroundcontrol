#include "TerrainFactGroup.h"

#include "ParameterManager.h"
#include "Vehicle.h"

namespace {

/// A TERRAIN_REPORT older than this stops vouching for the vehicle. The message rides
/// in STREAM_EXTRA3, so a live link produces them continuously; a gap this long means
/// the link or the terrain subsystem has stopped, not that nothing has changed.
constexpr int kReportStaleMs = 10000;

/// TERRAIN_FOLLOW is a bitmask of modes. Bit 0 enables every mode; bit 6 is Guided.
/// Without one of these the vehicle flies above home in Guided whatever frame the
/// command carries.
constexpr int kTerrainFollowAllModes = 1 << 0;
constexpr int kTerrainFollowGuided = 1 << 6;

} // namespace

TerrainFactGroup::TerrainFactGroup(QObject *parent)
    : FactGroup(1000, QStringLiteral(":/json/Vehicle/TerrainFactGroup.json"), parent)
{
    _addFact(&_blocksPendingFact);
    _addFact(&_blocksLoadedFact);
    _addFact(&_terrainHeightFact);
    _addFact(&_vehicleHeightFact);

    // Readiness has to expire on its own, not only when a new report arrives, or a
    // vehicle that stops reporting stays "ready" forever.
    _stalenessTimer.setInterval(kReportStaleMs / 2);
    (void) connect(&_stalenessTimer, &QTimer::timeout, this, &TerrainFactGroup::_recomputeReadiness);
    _stalenessTimer.start();

    _recomputeReadiness();
}

void TerrainFactGroup::handleTerrainReport(uint16_t pending, uint16_t loaded,
                                           float terrainHeightAmsl, float vehicleHeightAboveTerrain)
{
    _blocksPendingFact.setRawValue(pending);
    _blocksLoadedFact.setRawValue(loaded);
    _terrainHeightFact.setRawValue(terrainHeightAmsl);
    _vehicleHeightFact.setRawValue(vehicleHeightAboveTerrain);

    _reportSeen = true;
    _sinceReport.start();

    _recomputeReadiness();
}

void TerrainFactGroup::_recomputeReadiness()
{
    QString problem;

    Vehicle *const vehicle = qobject_cast<Vehicle*>(parent());
    if (!vehicle) {
        problem = tr("no vehicle");
    } else if (ParameterManager *const params = vehicle->parameterManager()) {
        const int compId = vehicle->defaultComponentId();

        // Configuration first: without this the vehicle cannot honour an above-terrain
        // altitude however good its data is.
        if (!params->parameterExists(compId, QStringLiteral("TERRAIN_ENABLE"))) {
            problem = tr("this firmware has no terrain support");
        } else if (params->getParameter(compId, QStringLiteral("TERRAIN_ENABLE"))->rawValue().toInt() != 1) {
            problem = tr("TERRAIN_ENABLE is off");
        } else if (params->parameterExists(compId, QStringLiteral("TERRAIN_FOLLOW"))) {
            const int follow = params->getParameter(compId, QStringLiteral("TERRAIN_FOLLOW"))->rawValue().toInt();
            if ((follow & (kTerrainFollowAllModes | kTerrainFollowGuided)) == 0) {
                problem = tr("TERRAIN_FOLLOW does not include Guided");
            }
        }

        // Then the data, for where the vehicle actually is.
        if (problem.isEmpty()) {
            if (!_reportSeen) {
                problem = tr("no terrain report from the vehicle yet");
            } else if (!_sinceReport.isValid() || (_sinceReport.elapsed() > kReportStaleMs)) {
                problem = tr("terrain reports have stopped");
            } else if (_blocksPendingFact.rawValue().toInt() > 0) {
                problem = tr("vehicle is still waiting on %1 terrain blocks")
                              .arg(_blocksPendingFact.rawValue().toInt());
            } else if (_blocksLoadedFact.rawValue().toInt() <= 0) {
                problem = tr("vehicle holds no terrain data");
            } else if (qFuzzyIsNull(_terrainHeightFact.rawValue().toFloat()) &&
                       qFuzzyIsNull(_vehicleHeightFact.rawValue().toFloat())) {
                // Both read exactly zero when the vehicle has no terrain for its
                // position, which is how the failure presents in practice.
                problem = tr("vehicle has no terrain height for its position");
            }
        }
    } else {
        problem = tr("vehicle parameters not available");
    }

    const bool ready = problem.isEmpty();
    if ((ready != _referenceReady) || (problem != _referenceProblem)) {
        _referenceReady = ready;
        _referenceProblem = problem;
        emit referenceReadyChanged();
    }
}
