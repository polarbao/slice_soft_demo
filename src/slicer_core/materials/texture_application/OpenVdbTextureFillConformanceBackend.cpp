#include "slicer_core/materials/texture_application/OpenVdbTextureFillConformanceBackend.h"

#include "slicer_core/geometry/MeshRobustnessDiagnostics.h"
#include "slicer_core/geometry/MeshScaleTolerance.h"
#include "slicer_core/geometry/MeshTopologyDiagnostics.h"
#include "slicer_core/geometry/NearestTriangleQuery.h"
#include "slicer_core/geometry/OpenVdbAdapter.h"
#include "slicer_core/geometry/OpenVdbLevelSetBuilder.h"
#include "slicer_core/geometry/TriangleMeshData.h"
#include "slicer_core/system/ProcessMemoryStats.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

namespace slicer_core
{
namespace
{

using Clock = std::chrono::steady_clock;

double ElapsedMilliseconds(const Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(
        Clock::now() - start).count();
}

ValidationIssue MakeOpenVdbIssue(
    const TextureFillPartitionErrorCode code,
    const std::string& message)
{
    return MakeValidationIssue(
        TextureFillPartitionErrorCodeName(code),
        ValidationSeverity::Error,
        message);
}

void BlockCandidate(
    GlobalTextureFillPartitionCandidate& candidate,
    const TextureFillPartitionErrorCode code,
    const std::string& message)
{
    candidate.blocked = true;
    candidate.issues.push_back(MakeOpenVdbIssue(code, message));
}

bool IsFinitePositive(const double value)
{
    return std::isfinite(value) && value > 0.0;
}

std::optional<std::size_t> GridVoxelCount(
    const TextureFillPartitionGridSpec& grid)
{
    if (grid.width <= 0 || grid.height <= 0 || grid.depth <= 0
        || !std::isfinite(grid.originXMm)
        || !std::isfinite(grid.originYMm)
        || !std::isfinite(grid.originZMm)
        || !IsFinitePositive(grid.spacingXMm)
        || !IsFinitePositive(grid.spacingYMm)
        || !IsFinitePositive(grid.spacingZMm))
    {
        return std::nullopt;
    }

    const std::size_t width = static_cast<std::size_t>(grid.width);
    const std::size_t height = static_cast<std::size_t>(grid.height);
    const std::size_t depth = static_cast<std::size_t>(grid.depth);
    const std::size_t maximum = std::numeric_limits<std::size_t>::max();
    if (width > maximum / height)
    {
        return std::nullopt;
    }
    const std::size_t area = width * height;
    if (area > maximum / depth)
    {
        return std::nullopt;
    }
    return area * depth;
}

std::size_t GridIndex(
    const TextureFillPartitionGridSpec& grid,
    const int x,
    const int y,
    const int z)
{
    return
        (static_cast<std::size_t>(z) * static_cast<std::size_t>(grid.height)
         + static_cast<std::size_t>(y))
            * static_cast<std::size_t>(grid.width)
        + static_cast<std::size_t>(x);
}

Vec3 GridCellCenter(
    const TextureFillPartitionGridSpec& grid,
    const int x,
    const int y,
    const int z)
{
    return {
        grid.originXMm + (static_cast<double>(x) + 0.5) * grid.spacingXMm,
        grid.originYMm + (static_cast<double>(y) + 0.5) * grid.spacingYMm,
        grid.originZMm + (static_cast<double>(z) + 0.5) * grid.spacingZMm};
}

double CeilToStep(const double value, const double step)
{
    return std::ceil(value / step - 1.0e-12) * step;
}

void InitializeMasks(
    GlobalTextureFillPartitionCandidate& candidate,
    const TextureFillPartitionGridSpec& grid,
    const std::size_t voxelCount)
{
    candidate.modelMask.grid = grid;
    candidate.textureSurfaceMask.grid = grid;
    candidate.modelFillMask.grid = grid;
    candidate.modelMask.values.assign(voxelCount, 0U);
    candidate.textureSurfaceMask.values.assign(voxelCount, 0U);
    candidate.modelFillMask.values.assign(voxelCount, 0U);
    candidate.closestSurfaceReferences.assign(
        voxelCount,
        TextureFillClosestSurfaceReference{});
}

std::string ValidateStrictMesh(
    const TriangleMeshData& mesh,
    const double classificationResolutionMm,
    double& topologyMs)
{
    const Clock::time_point start = Clock::now();
    const std::string meshError = ValidateTriangleMesh(mesh);
    if (!meshError.empty())
    {
        topologyMs = ElapsedMilliseconds(start);
        return meshError;
    }

    const MeshTopologyReport topology = AnalyzeMeshTopology(mesh);
    const std::string topologyError = ValidateMeshTopology(
        topology,
        MeshValidationPolicy::StrictClosed);
    if (!topologyError.empty())
    {
        topologyMs = ElapsedMilliseconds(start);
        return topologyError;
    }

    MeshRobustnessOptions robustnessOptions;
    robustnessOptions.tolerance = MakeMeshScaleTolerance(
        mesh.bbox_mm,
        classificationResolutionMm);
    const MeshRobustnessReport robustness = AnalyzeMeshRobustness(
        mesh,
        robustnessOptions);
    if (robustness.self_intersection_check_sampled)
    {
        topologyMs = ElapsedMilliseconds(start);
        return "strict_closed rejected incomplete self-intersection audit";
    }
    const std::string robustnessError = ValidateMeshRobustness(
        robustness,
        true);
    topologyMs = ElapsedMilliseconds(start);
    return robustnessError;
}

}  // namespace

GlobalTextureFillPartitionCandidate
OpenVdbTextureFillConformanceBackend::Evaluate(
    const GlobalTextureFillPartitionRequest& request) const
{
    const Clock::time_point totalStart = Clock::now();
    GlobalTextureFillPartitionCandidate candidate;
    candidate.backend = "openvdb_sdf_conformance";
    candidate.backendRole = "conformance_candidate";
    candidate.modelMask.grid = request.grid;
    candidate.textureSurfaceMask.grid = request.grid;
    candidate.modelFillMask.grid = request.grid;

    const OpenVdbStatus openVdbStatus = GetOpenVdbStatus();
    if (!openVdbStatus.compiled_with_openvdb
        || !openVdbStatus.runtime_available)
    {
        candidate.issues.push_back(MakeOpenVdbIssue(
            TextureFillPartitionErrorCode::OpenVdbBackendUnavailable,
            "OpenVDB conformance backend is unavailable because USE_OPENVDB is off or runtime initialization failed"));
        return candidate;
    }
    candidate.available = true;

    if (request.mesh == nullptr)
    {
        BlockCandidate(
            candidate,
            TextureFillPartitionErrorCode::OpenVdbLevelSetFailed,
            "OpenVDB conformance candidate requires a transformed mesh");
        return candidate;
    }
    const std::optional<std::size_t> voxelCount = GridVoxelCount(request.grid);
    if (!voxelCount.has_value())
    {
        BlockCandidate(
            candidate,
            TextureFillPartitionErrorCode::PartitionGridInvalid,
            "OpenVDB conformance candidate requires a finite positive grid");
        return candidate;
    }
    candidate.performance.gridVoxelCount =
        static_cast<std::uint64_t>(voxelCount.value());

    const double classificationResolutionMm = std::max({
        request.grid.spacingXMm,
        request.grid.spacingYMm,
        request.grid.spacingZMm});
    candidate.widthMetrics.classificationResolutionMm =
        classificationResolutionMm;
    candidate.widthMetrics.effectiveMinimumWidthMm = std::max(
        request.options.baseMinimumWidthMm,
        2.0 * classificationResolutionMm);
    const MeshScaleTolerance tolerance = MakeMeshScaleTolerance(
        request.mesh->bbox_mm,
        classificationResolutionMm);
    candidate.widthMetrics.epsilonMm = std::max(
        tolerance.position_epsilon_mm,
        classificationResolutionMm * 1.0e-6);

    if (request.options.surfaceScope != "all_closed_surfaces")
    {
        BlockCandidate(
            candidate,
            TextureFillPartitionErrorCode::SurfaceScopeUnsupported,
            "OpenVDB conformance candidate requires all_closed_surfaces");
        return candidate;
    }
    if (!IsFinitePositive(request.options.widthStepMm)
        || std::abs(request.options.widthStepMm - 0.01) > 1.0e-12)
    {
        BlockCandidate(
            candidate,
            TextureFillPartitionErrorCode::SurfaceShellStepUnsupported,
            "OpenVDB conformance candidate requires a 0.01 mm width step");
        return candidate;
    }
    if (!IsFinitePositive(request.options.requestedWidthMm)
        || !IsFinitePositive(request.options.baseMinimumWidthMm))
    {
        BlockCandidate(
            candidate,
            TextureFillPartitionErrorCode::SurfaceShellWidthInvalid,
            "OpenVDB conformance candidate widths must be finite and positive");
        return candidate;
    }
    if (request.options.requestedWidthMm
        + candidate.widthMetrics.epsilonMm
        < candidate.widthMetrics.effectiveMinimumWidthMm)
    {
        BlockCandidate(
            candidate,
            TextureFillPartitionErrorCode::SurfaceShellWidthBelowEffectiveMinimum,
            "requested surface-shell width is below the effective grid minimum");
        return candidate;
    }

    const std::string topologyError = ValidateStrictMesh(
        *request.mesh,
        classificationResolutionMm,
        candidate.performance.topologyMs);
    if (!topologyError.empty())
    {
        BlockCandidate(
            candidate,
            TextureFillPartitionErrorCode::OpenVdbTopologyBlocked,
            topologyError);
        return candidate;
    }

    OpenVdbLevelSetOptions levelSetOptions;
    levelSetOptions.voxel_size_mm = classificationResolutionMm;
    levelSetOptions.exterior_band_voxels = 3.0;
    levelSetOptions.interior_band_voxels = 3.0;
    levelSetOptions.bbox_padding_voxels = 2;
    levelSetOptions.use_parity_interior_test = true;
    const Clock::time_point levelSetStart = Clock::now();
    const OpenVdbLevelSetResult levelSet = BuildOpenVdbLevelSet(
        *request.mesh,
        levelSetOptions);
    candidate.performance.levelSetMs = ElapsedMilliseconds(levelSetStart);
    if (!levelSet.available || !levelSet.generated || levelSet.grid == nullptr)
    {
        BlockCandidate(
            candidate,
            TextureFillPartitionErrorCode::OpenVdbLevelSetFailed,
            levelSet.error.empty()
                ? "OpenVDB conformance level set generation failed"
                : levelSet.error);
        return candidate;
    }
    candidate.performance.openVdbGridBytes = levelSet.memory_bytes;

    InitializeMasks(candidate, request.grid, voxelCount.value());
    const Clock::time_point sampleStart = Clock::now();
    for (int z{0}; z < request.grid.depth; ++z)
    {
        for (int y{0}; y < request.grid.height; ++y)
        {
            for (int x{0}; x < request.grid.width; ++x)
            {
                const OpenVdbSignedDistanceSample sample =
                    SampleOpenVdbSignedDistanceWorld(
                        levelSet,
                        GridCellCenter(request.grid, x, y, z));
                if (!sample.available)
                {
                    BlockCandidate(
                        candidate,
                        TextureFillPartitionErrorCode::OpenVdbGridSampleFailed,
                        sample.error.empty()
                            ? "OpenVDB world-space grid sample failed"
                            : sample.error);
                    return candidate;
                }
                ++candidate.queryStats.sdfSampleCount;
                candidate.queryStats.sdfActiveSampleCount +=
                    sample.active ? 1U : 0U;
                candidate.queryStats.sdfBackgroundSampleCount +=
                    sample.active ? 0U : 1U;
                if (sample.signedDistanceMm < 0.0)
                {
                    candidate.modelMask.values.at(
                        GridIndex(request.grid, x, y, z)) = 1U;
                }
            }
        }
    }
    candidate.performance.gridSampleMs = ElapsedMilliseconds(sampleStart);
    candidate.performance.occupancyBuildMs =
        candidate.performance.levelSetMs
        + candidate.performance.gridSampleMs;
    candidate.queryStats.occupancyQueryCount =
        candidate.queryStats.sdfSampleCount;
    candidate.performance.occupancyQueryBytes = levelSet.memory_bytes;

    const std::uint64_t modelVoxelCount = static_cast<std::uint64_t>(
        std::count(
            candidate.modelMask.values.begin(),
            candidate.modelMask.values.end(),
            static_cast<std::uint8_t>(1U)));
    if (modelVoxelCount == 0U)
    {
        BlockCandidate(
            candidate,
            TextureFillPartitionErrorCode::OpenVdbGridSampleFailed,
            "OpenVDB request grid contains no negative signed-distance model voxels");
        return candidate;
    }

    try
    {
        const Clock::time_point distanceStart = Clock::now();
        NearestTriangleQuery nearest(*request.mesh);
        NearestTriangleQueryStats nearestStats = nearest.GetBuildStats();
        NearestTriangleQueryOptions nearestOptions;
        nearestOptions.tie_epsilon_mm = tolerance.tie_epsilon_mm;
        for (int z{0}; z < request.grid.depth; ++z)
        {
            for (int y{0}; y < request.grid.height; ++y)
            {
                for (int x{0}; x < request.grid.width; ++x)
                {
                    const std::size_t index = GridIndex(
                        request.grid,
                        x,
                        y,
                        z);
                    if (candidate.modelMask.values.at(index) == 0U)
                    {
                        continue;
                    }
                    const NearestTriangleHit hit = nearest.FindNearestWithStats(
                        GridCellCenter(request.grid, x, y, z),
                        nearestOptions,
                        nearestStats);
                    if (!hit.found || !std::isfinite(hit.distance_mm))
                    {
                        throw std::runtime_error(
                            "nearest surface query returned no finite hit");
                    }
                    TextureFillClosestSurfaceReference& reference =
                        candidate.closestSurfaceReferences.at(index);
                    reference.valid = true;
                    reference.triangleIndex = hit.triangle_index;
                    reference.barycentric = hit.barycentric;
                    reference.distanceMm = hit.distance_mm;
                    candidate.widthMetrics.maxInteriorDistanceMm = std::max(
                        candidate.widthMetrics.maxInteriorDistanceMm,
                        hit.distance_mm);
                }
            }
        }
        candidate.performance.distanceQueryMs =
            ElapsedMilliseconds(distanceStart);
        candidate.queryStats.nearestQueryCount = nearestStats.query_count;
        candidate.queryStats.nearestVisitedNodes = nearestStats.visited_nodes;
        candidate.queryStats.nearestTestedTriangles =
            nearestStats.tested_triangles;
        candidate.performance.nearestQueryBytes =
            static_cast<std::uint64_t>(nearestStats.estimated_bytes);
    }
    catch (const std::exception& error)
    {
        BlockCandidate(
            candidate,
            TextureFillPartitionErrorCode::OpenVdbDistanceIncomplete,
            std::string{"OpenVDB conformance exact-distance query failed: "}
                + error.what());
        return candidate;
    }

    if (!std::isfinite(candidate.widthMetrics.maxInteriorDistanceMm))
    {
        BlockCandidate(
            candidate,
            TextureFillPartitionErrorCode::OpenVdbDistanceIncomplete,
            "OpenVDB conformance candidate could not calculate a complete exact-distance threshold");
        return candidate;
    }
    candidate.widthMetrics.allTextureThresholdMm = CeilToStep(
        std::max(
            candidate.widthMetrics.effectiveMinimumWidthMm,
            candidate.widthMetrics.maxInteriorDistanceMm),
        request.options.widthStepMm);
    candidate.widthMetrics.effectiveWidthMm = std::min(
        request.options.requestedWidthMm,
        candidate.widthMetrics.allTextureThresholdMm);
    candidate.widthMetrics.allTexture =
        candidate.widthMetrics.effectiveWidthMm
            + candidate.widthMetrics.epsilonMm
        >= candidate.widthMetrics.allTextureThresholdMm;

    const Clock::time_point partitionStart = Clock::now();
    for (std::size_t index{0U};
         index < candidate.modelMask.values.size();
         ++index)
    {
        if (candidate.modelMask.values.at(index) == 0U)
        {
            continue;
        }
        const TextureFillClosestSurfaceReference& reference =
            candidate.closestSurfaceReferences.at(index);
        const bool textureSurface = candidate.widthMetrics.allTexture
            || reference.distanceMm
                <= candidate.widthMetrics.effectiveWidthMm
                    + candidate.widthMetrics.epsilonMm;
        candidate.textureSurfaceMask.values.at(index) =
            textureSurface ? 1U : 0U;
        candidate.modelFillMask.values.at(index) =
            textureSurface ? 0U : 1U;
    }
    candidate.performance.partitionMs = ElapsedMilliseconds(partitionStart);
    candidate.performance.maskBytes = static_cast<std::uint64_t>(
        candidate.modelMask.values.size()
        + candidate.textureSurfaceMask.values.size()
        + candidate.modelFillMask.values.size());
    candidate.performance.closestReferenceBytes =
        static_cast<std::uint64_t>(
            candidate.closestSurfaceReferences.size()
            * sizeof(TextureFillClosestSurfaceReference));
    candidate.performance.totalCoreMs = ElapsedMilliseconds(totalStart);
    const ProcessMemoryStats processMemory = CaptureProcessMemoryStats();
    candidate.performance.processMemoryAvailable = processMemory.available;
    candidate.performance.processWorkingSetBytes =
        processMemory.working_set_bytes;
    candidate.performance.processPeakWorkingSetBytes =
        processMemory.peak_working_set_bytes;
    return candidate;
}

}  // namespace slicer_core
