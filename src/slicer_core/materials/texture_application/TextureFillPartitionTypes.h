#pragma once

#include "slicer_core/diagnostics/ValidationIssue.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace slicer_core
{

struct TriangleMeshData;

/**
 * @brief Stable errors for the Stage 12E texture/fill partition contract.
 */
enum class TextureFillPartitionErrorCode
{
    SurfaceShellWidthInvalid,
    SurfaceShellPartitionModeUnsupported,
    SurfaceShellStepUnsupported,
    SurfaceShellGeometryModeUnsupported,
    SurfaceShellMinimumPolicyUnsupported,
    SurfaceScopeUnsupported,
    FullTextureAtModelLimitRequired,
    TextureFillScopeMismatch,
    ModelFillRequired,
    PartitionBackendUnavailable,
    PartitionBackendFailed,
    PartitionGridInvalid,
    PartitionMaskSizeMismatch,
    PartitionMaskNonBinary,
    TextureOutsideModel,
    ModelFillOutsideModel,
    TextureFillOverlap,
    ModelVoxelUnassigned,
    CpuMeshMissing,
    CpuGridInvalid,
    CpuTopologyBlocked,
    CpuOccupancyFailed,
    CpuNearestSurfaceFailed,
    OpenVdbBackendUnavailable,
    OpenVdbTopologyBlocked,
    OpenVdbLevelSetFailed,
    OpenVdbGridSampleFailed,
    OpenVdbDistanceIncomplete,
    BackendConformanceFailed,
    WidthSweepEmpty,
    WidthSweepSampleFailed,
    WidthSweepModelChanged,
    WidthSweepTextureNonMonotonic,
    WidthSweepFillNonMonotonic,
    WidthSweepEndpointInvalid,
    TextureTransferInputInvalid,
    TextureReferenceMissing,
    TextureTriangleOutOfRange,
    TextureMissingUv,
    TextureMissingResource,
    TextureSampleFailed,
    DiagnosticComposerInputInvalid,
    DiagnosticComposerPartitionInvalid,
    ClosureAdapterInputInvalid,
    ClosureLayerOrderInvalid,
    ClosureMaskInvalid,
    ClosureModelDomainGap,
    ClosureColorFillGap,
    ClosureChannelOrderInvalid,
    RasterMappingInputInvalid,
    RasterMappingGridInvalid,
    RasterMappingPartitionInvalid,
    RasterMappingTransferInvalid,
    RasterMappingInvariantFailed,
    FullClosureInputInvalid,
    FullClosureLayerOrderInvalid,
    FullClosureMaskInvalid,
    FullClosureChannelOrderInvalid,
    FullClosurePriorityConflict,
    FullClosureSemanticMismatch,
    FullClosureGapDetected,
    FullClosureUnexpectedMaterial,
    SurfaceShellWidthBelowEffectiveMinimum,
    AllTextureThresholdUnavailable,
};

/**
 * @brief Convert a Stage 12E error code to its stable machine-readable name.
 * @param code Error code.
 * @return Stable error name.
 */
std::string TextureFillPartitionErrorCodeName(TextureFillPartitionErrorCode code);

/**
 * @brief Exception carrying a stable Stage 12E error code.
 */
class TextureFillPartitionError : public std::runtime_error
{
public:
    /**
     * @brief Construct a Stage 12E contract error.
     * @param code Stable error code.
     * @param message Human-readable diagnostic message.
     */
    TextureFillPartitionError(TextureFillPartitionErrorCode code, const std::string& message);

    /**
     * @brief Return the stable Stage 12E error code.
     * @return Error code associated with this exception.
     */
    TextureFillPartitionErrorCode Code() const noexcept;

private:
    TextureFillPartitionErrorCode m_code;
};

/**
 * @brief Backend-neutral options for a future global 3D texture/fill partition.
 */
struct GlobalTextureFillPartitionOptions
{
    double requestedWidthMm{0.10};
    double widthStepMm{0.01};
    double baseMinimumWidthMm{0.10};
    std::string surfaceScope{"all_closed_surfaces"};
    bool forceAllTexture{false};
};

/**
 * @brief Grid shared by all backend-neutral Stage 12E three-dimensional masks.
 */
struct TextureFillPartitionGridSpec
{
    int width{0};
    int height{0};
    int depth{0};
    double originXMm{0.0};
    double originYMm{0.0};
    double originZMm{0.0};
    double spacingXMm{0.0};
    double spacingYMm{0.0};
    double spacingZMm{0.0};
};

/**
 * @brief Binary mask aligned to a Stage 12E three-dimensional grid.
 */
struct TextureFillPartitionMask3D
{
    TextureFillPartitionGridSpec grid;
    std::vector<std::uint8_t> values;
};

/**
 * @brief Statistics recomputed by the partition service rather than trusted from a backend.
 */
struct TextureFillPartitionStats
{
    std::uint64_t modelVoxels{0U};
    std::uint64_t textureSurfaceVoxels{0U};
    std::uint64_t modelFillVoxels{0U};
    std::uint64_t overlapTextureFillVoxels{0U};
    std::uint64_t unassignedModelVoxels{0U};
    std::uint64_t textureOutsideModelVoxels{0U};
    std::uint64_t modelFillOutsideModelVoxels{0U};
};

/**
 * @brief Dynamic width values calculated from one model and classification grid.
 */
struct TextureFillPartitionWidthMetrics
{
    double classificationResolutionMm{0.0};
    double epsilonMm{0.0};
    double effectiveMinimumWidthMm{0.0};
    double effectiveWidthMm{0.0};
    double maxInteriorDistanceMm{0.0};
    double allTextureThresholdMm{0.0};
    bool allTexture{false};
};

/**
 * @brief Stable closest-surface evidence for one model voxel.
 */
struct TextureFillClosestSurfaceReference
{
    bool valid{false};
    std::size_t triangleIndex{0U};
    std::array<double, 3> barycentric{0.0, 0.0, 0.0};
    double distanceMm{0.0};
    std::uint64_t tieCandidateCount{0U};
};

/**
 * @brief Backend-neutral query counters for occupancy and nearest-surface work.
 */
struct TextureFillPartitionQueryStats
{
    std::uint64_t occupancyQueryCount{0U};
    std::uint64_t occupancyVisitedNodes{0U};
    std::uint64_t occupancyTestedTriangles{0U};
    std::uint64_t occupancyFallbackRayCount{0U};
    std::uint64_t occupancyAmbiguousRayCount{0U};
    std::uint64_t occupancyBoundaryPointCount{0U};
    std::uint64_t sdfSampleCount{0U};
    std::uint64_t sdfActiveSampleCount{0U};
    std::uint64_t sdfBackgroundSampleCount{0U};
    std::uint64_t nearestQueryCount{0U};
    std::uint64_t nearestVisitedNodes{0U};
    std::uint64_t nearestTestedTriangles{0U};
};

/**
 * @brief Core-only timing and memory evidence for one diagnostic partition run.
 */
struct TextureFillPartitionPerformance
{
    double topologyMs{0.0};
    double levelSetMs{0.0};
    double gridSampleMs{0.0};
    double occupancyBuildMs{0.0};
    double distanceQueryMs{0.0};
    double partitionMs{0.0};
    double totalCoreMs{0.0};
    std::uint64_t gridVoxelCount{0U};
    std::uint64_t maskBytes{0U};
    std::uint64_t closestReferenceBytes{0U};
    std::uint64_t occupancyQueryBytes{0U};
    std::uint64_t nearestQueryBytes{0U};
    std::uint64_t openVdbGridBytes{0U};
    bool processMemoryAvailable{false};
    std::uint64_t processWorkingSetBytes{0U};
    std::uint64_t processPeakWorkingSetBytes{0U};
};

/**
 * @brief Request passed to a backend-neutral global texture/fill partition backend.
 */
struct GlobalTextureFillPartitionRequest
{
    const TriangleMeshData* mesh{nullptr};
    TextureFillPartitionGridSpec grid;
    GlobalTextureFillPartitionOptions options;
};

/**
 * @brief Candidate masks returned by a diagnostic Stage 12E backend.
 */
struct GlobalTextureFillPartitionCandidate
{
    bool available{false};
    bool blocked{false};
    std::string backend{"none"};
    std::string backendRole{"unavailable"};
    TextureFillPartitionMask3D modelMask;
    TextureFillPartitionMask3D textureSurfaceMask;
    TextureFillPartitionMask3D modelFillMask;
    TextureFillPartitionWidthMetrics widthMetrics;
    TextureFillPartitionQueryStats queryStats;
    TextureFillPartitionPerformance performance;
    std::vector<TextureFillClosestSurfaceReference> closestSurfaceReferences;
    std::vector<ValidationIssue> issues;
};

/**
 * @brief Validated backend-neutral result from the Stage 12E partition service.
 */
struct GlobalTextureFillPartitionResult
{
    bool available{false};
    bool partitionPass{false};
    std::string status{"blocked"};
    std::string productionAcceptance{"not_evaluated"};
    std::string backend{"none"};
    std::string backendRole{"unavailable"};
    GlobalTextureFillPartitionOptions options;
    TextureFillPartitionGridSpec grid;
    TextureFillPartitionMask3D modelMask;
    TextureFillPartitionMask3D textureSurfaceMask;
    TextureFillPartitionMask3D modelFillMask;
    TextureFillPartitionStats stats;
    TextureFillPartitionWidthMetrics widthMetrics;
    TextureFillPartitionQueryStats queryStats;
    TextureFillPartitionPerformance performance;
    std::vector<TextureFillClosestSurfaceReference> closestSurfaceReferences;
    std::vector<ValidationIssue> issues;
};

/**
 * @brief Backend-neutral diagnostic comparison between CPU and OpenVDB candidates.
 */
struct TextureFillPartitionConformanceResult
{
    bool cpuAvailable{false};
    bool openVdbAvailable{false};
    bool sameGrid{false};
    bool cpuPartitionInvariantPass{false};
    bool openVdbPartitionInvariantPass{false};
    std::string cpuStatus{"blocked"};
    std::string openVdbStatus{"blocked"};
    std::string cpuBackendRole{"unavailable"};
    std::string openVdbBackendRole{"unavailable"};
    std::string conformanceStatus{"unavailable"};
    std::string productionAcceptance{"not_evaluated"};
    std::uint64_t modelOnlyCpuVoxels{0U};
    std::uint64_t modelOnlyOpenVdbVoxels{0U};
    std::uint64_t textureOnlyCpuVoxels{0U};
    std::uint64_t textureOnlyOpenVdbVoxels{0U};
    std::uint64_t fillOnlyCpuVoxels{0U};
    std::uint64_t fillOnlyOpenVdbVoxels{0U};
    std::uint64_t commonDistanceSamples{0U};
    double maxDistanceDeltaMm{0.0};
    double meanDistanceDeltaMm{0.0};
    double allTextureThresholdDeltaMm{0.0};
    double openVdbToCpuCoreTimeRatio{0.0};
    double openVdbToCpuPeakMemoryRatio{0.0};
    std::vector<ValidationIssue> issues;
};

/**
 * @brief Controls representative or explicit full-step Stage 12E width scans.
 */
struct TextureFillPartitionWidthSweepOptions
{
    bool fullStepScan{false};
    int representativeIntermediateCount{3};
    std::size_t maxSamples{10000U};
};

/**
 * @brief Stable summary of one validated width-sweep candidate.
 */
struct TextureFillPartitionWidthSweepSample
{
    double requestedWidthMm{0.0};
    double effectiveWidthMm{0.0};
    bool allTexture{false};
    bool partitionPass{false};
    std::string status{"blocked"};
    TextureFillPartitionStats stats;
    TextureFillPartitionPerformance performance;
};

/**
 * @brief Backend-neutral monotonic width-sweep evidence.
 */
struct TextureFillPartitionWidthSweepResult
{
    bool available{false};
    bool monotonicPass{false};
    bool endpointPass{false};
    std::string status{"unavailable"};
    std::string productionAcceptance{"not_evaluated"};
    std::string backend{"none"};
    std::string backendRole{"unavailable"};
    double minimumWidthMm{0.0};
    double maximumWidthMm{0.0};
    double widthStepMm{0.0};
    double totalCandidateCoreMs{0.0};
    std::vector<double> requestedAnchorWidthsMm;
    std::vector<TextureFillPartitionWidthSweepSample> samples;
    std::vector<ValidationIssue> issues;
};

/**
 * @brief Backend-neutral report state used before partition evidence exists.
 */
struct TextureFillPartitionReportData
{
    bool enabled{false};
    std::string strategy{"global_surface_shell"};
    std::string availability{"unavailable"};
    std::string status{"blocked"};
    std::string productionAcceptance{"not_evaluated"};
    std::string backend{"none"};
    std::string backendRole{"unavailable"};
    GlobalTextureFillPartitionOptions options;
    std::vector<ValidationIssue> issues;
};

}  // namespace slicer_core
