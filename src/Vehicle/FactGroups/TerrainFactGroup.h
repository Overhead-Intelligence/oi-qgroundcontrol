#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>

#include "FactGroup.h"

/// Terrain state reported by the vehicle, and whether it can be trusted to fly an
/// above-terrain altitude right now.
///
/// The readiness question has to be answered here rather than by each caller, because
/// the failure it guards against is silent. Measured in SITL against ArduPlane: a
/// command carrying MAV_FRAME_GLOBAL_TERRAIN_ALT sent while the vehicle holds no
/// terrain data is answered MAV_RESULT_ACCEPTED and then flown as height above *home*
/// - `altitude.cpp` falls back to `terrain_following = false` with no NAK and no
/// statustext. An ack therefore proves nothing, and anything offering an above-ground
/// altitude has to decide for itself whether the vehicle will honour it.
class TerrainFactGroup : public FactGroup
{
    Q_OBJECT
    Q_PROPERTY(Fact *blocksPending  READ blocksPending  CONSTANT)
    Q_PROPERTY(Fact *blocksLoaded   READ blocksLoaded   CONSTANT)
    /// Height of the ground under the vehicle, AMSL, as the vehicle sees it.
    Q_PROPERTY(Fact *terrainHeight  READ terrainHeight  CONSTANT)
    /// Height of the vehicle above that ground.
    Q_PROPERTY(Fact *vehicleHeight  READ vehicleHeight  CONSTANT)

    /// True when an above-terrain altitude will actually be honoured: terrain following
    /// configured for the mode, no outstanding blocks, data loaded, a height for where
    /// the vehicle is, and a report recent enough to still mean something.
    Q_PROPERTY(bool     referenceReady      READ referenceReady     NOTIFY referenceReadyChanged)
    /// Empty when ready, otherwise the specific reason it is not, for display.
    Q_PROPERTY(QString  referenceProblem    READ referenceProblem   NOTIFY referenceReadyChanged)

public:
    explicit TerrainFactGroup(QObject *parent = nullptr);

    Fact *blocksPending() { return &_blocksPendingFact; }
    Fact *blocksLoaded() { return &_blocksLoadedFact; }
    Fact *terrainHeight() { return &_terrainHeightFact; }
    Fact *vehicleHeight() { return &_vehicleHeightFact; }

    bool referenceReady() const { return _referenceReady; }
    QString referenceProblem() const { return _referenceProblem; }

    /// Called by TerrainProtocolHandler for every TERRAIN_REPORT.
    void handleTerrainReport(uint16_t pending, uint16_t loaded,
                             float terrainHeightAmsl, float vehicleHeightAboveTerrain);

signals:
    void referenceReadyChanged();

private slots:
    void _recomputeReadiness();

private:
    Fact _blocksPendingFact = Fact(0, QStringLiteral("blocksPending"), FactMetaData::valueTypeDouble);
    Fact _blocksLoadedFact = Fact(0, QStringLiteral("blocksLoaded"), FactMetaData::valueTypeDouble);
    Fact _terrainHeightFact = Fact(0, QStringLiteral("terrainHeight"), FactMetaData::valueTypeDouble);
    Fact _vehicleHeightFact = Fact(0, QStringLiteral("vehicleHeight"), FactMetaData::valueTypeDouble);

    bool _referenceReady = false;
    QString _referenceProblem;

    /// Reports stopping is itself a failure: the vehicle moving out of coverage looks
    /// exactly like the last good report never being replaced. Without this a stale
    /// report keeps vouching for the vehicle indefinitely.
    bool _reportSeen = false;
    QElapsedTimer _sinceReport;
    QTimer _stalenessTimer;
};
