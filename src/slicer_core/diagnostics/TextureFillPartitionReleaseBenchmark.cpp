#include "slicer_core/diagnostics/TextureFillPartitionReleaseBenchmark.h"

#include "slicer_core/diagnostics/ValidationIssue.h"
#include "slicer_core/geometry/OpenVdbAdapter.h"
#include "slicer_core/geometry/TriangleMeshData.h"
#include "slicer_core/materials/texture_application/GlobalTextureFillPartitionService.h"
#include "slicer_core/materials/texture_application/LegacyCpuGlobalDistanceBackend.h"

#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace slicer_core
{
namespace
{

using Clock = std::chrono::steady_clock;

int CalculateGridAxis(
    const double minimum,
    const double maximum,
    const double voxelMm,
    const int paddingVoxels)
{
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || maximum < minimum)
    {
        throw std::invalid_argument("benchmark bounding box is invalid");
    }

    const double cells = std::ceil((maximum - minimum) / voxelMm - 1.0e-12)
        + 2.0 * static_cast<double>(paddingVoxels);
    if (cells < 1.0 || cells > static_cast<double>(std::numeric_limits<int>::max()))
    {
        throw std::invalid_argument("benchmark grid axis exceeds the supported range");
    }
    return static_cast<int>(cells);
}

Json BuildGridJson(const TextureFillPartitionGridSpec& grid)
{
    return Json::object({
        {"width", grid.width},
        {"height", grid.height},
        {"depth", grid.depth},
        {"originXMm", grid.originXMm},
        {"originYMm", grid.originYMm},
        {"originZMm", grid.originZMm},
        {"spacingXMm", grid.spacingXMm},
        {"spacingYMm", grid.spacingYMm},
        {"spacingZMm", grid.spacingZMm},
    });
}

Json BuildPerformanceJson(
    const TextureFillPartitionReleaseBenchmarkRequest& request,
    const TextureFillPartitionPerformance& performance,
    const double benchmarkWallMs)
{
    return Json::object({
        {"configLoadMs", request.configLoadMs},
        {"modelLoadMs", request.modelLoadMs},
        {"meshAdaptMs", request.meshAdaptMs},
        {"topologyMs", performance.topologyMs},
        {"occupancyBuildMs", performance.occupancyBuildMs},
        {"distanceQueryMs", performance.distanceQueryMs},
        {"partitionMs", performance.partitionMs},
        {"totalCoreMs", performance.totalCoreMs},
        {"benchmarkWallMs", benchmarkWallMs},
        {"textureTransferMs", Json{nullptr}},
        {"rasterMappingMs", Json{nullptr}},
        {"outputWriteMs", 0.0},
    });
}

Json BuildMemoryJson(const TextureFillPartitionPerformance& performance)
{
    return Json::object({
        {"gridVoxelCount", performance.gridVoxelCount},
        {"maskBytes", performance.maskBytes},
        {"closestReferenceBytes", performance.closestReferenceBytes},
        {"occupancyQueryBytes", performance.occupancyQueryBytes},
        {"nearestQueryBytes", performance.nearestQueryBytes},
        {"processMemoryAvailable", performance.processMemoryAvailable},
        {"workingSetBytes",
         performance.processMemoryAvailable
             ? Json{performance.processWorkingSetBytes}
             : Json{nullptr}},
        {"peakWorkingSetBytes",
         performance.processMemoryAvailable
             ? Json{performance.processPeakWorkingSetBytes}
             : Json{nullptr}},
    });
}

Json BuildPartitionJson(const GlobalTextureFillPartitionResult& result)
{
    return Json::object({
        {"available", result.available},
        {"partitionPass", result.partitionPass},
        {"status", result.status},
        {"productionAcceptance", result.productionAcceptance},
        {"modelVoxels", result.stats.modelVoxels},
        {"textureSurfaceVoxels", result.stats.textureSurfaceVoxels},
        {"modelFillVoxels", result.stats.modelFillVoxels},
        {"overlapTextureFillVoxels", result.stats.overlapTextureFillVoxels},
        {"unassignedModelVoxels", result.stats.unassignedModelVoxels},
        {"textureOutsideModelVoxels", result.stats.textureOutsideModelVoxels},
        {"modelFillOutsideModelVoxels", result.stats.modelFillOutsideModelVoxels},
        {"effectiveMinimumWidthMm", result.widthMetrics.effectiveMinimumWidthMm},
        {"effectiveWidthMm", result.widthMetrics.effectiveWidthMm},
        {"allTextureThresholdMm", result.widthMetrics.allTextureThresholdMm},
        {"allTexture", result.widthMetrics.allTexture},
    });
}

}  // namespace

TextureFillPartitionGridSpec BuildTextureFillPartitionBenchmarkGrid(
    const BoundingBox& bbox,
    const double voxelMm,
    const int paddingVoxels)
{
    if (!std::isfinite(voxelMm) || voxelMm <= 0.0)
    {
        throw std::invalid_argument("benchmark voxel size must be finite and positive");
    }
    if (paddingVoxels < 0)
    {
        throw std::invalid_argument("benchmark padding must not be negative");
    }

    TextureFillPartitionGridSpec grid;
    grid.width = CalculateGridAxis(bbox.min.x, bbox.max.x, voxelMm, paddingVoxels);
    grid.height = CalculateGridAxis(bbox.min.y, bbox.max.y, voxelMm, paddingVoxels);
    grid.depth = CalculateGridAxis(bbox.min.z, bbox.max.z, voxelMm, paddingVoxels);
    grid.originXMm = bbox.min.x - static_cast<double>(paddingVoxels) * voxelMm;
    grid.originYMm = bbox.min.y - static_cast<double>(paddingVoxels) * voxelMm;
    grid.originZMm = bbox.min.z - static_cast<double>(paddingVoxels) * voxelMm;
    grid.spacingXMm = voxelMm;
    grid.spacingYMm = voxelMm;
    grid.spacingZMm = voxelMm;
    return grid;
}

TextureFillPartitionReleaseBenchmarkResult RunTextureFillPartitionReleaseBenchmark(
    const TextureFillPartitionReleaseBenchmarkRequest& request)
{
    if (request.mesh == nullptr)
    {
        throw std::invalid_argument("benchmark mesh is required");
    }

    const TextureFillPartitionGridSpec grid =
        BuildTextureFillPartitionBenchmarkGrid(
            request.mesh->bbox_mm,
            request.voxelMm,
            request.paddingVoxels);

    LegacyCpuGlobalDistanceBackend backend;
    GlobalTextureFillPartitionService service(&backend);
    GlobalTextureFillPartitionRequest partitionRequest;
    partitionRequest.mesh = request.mesh;
    partitionRequest.grid = grid;
    partitionRequest.options.requestedWidthMm = request.widthMm;

    const Clock::time_point benchmarkStart = Clock::now();
    TextureFillPartitionReleaseBenchmarkResult benchmark;
    benchmark.partition = service.Evaluate(partitionRequest);
    const double benchmarkWallMs = std::chrono::duration<double, std::milli>(
        Clock::now() - benchmarkStart).count();
    benchmark.evidenceCollected = true;
    benchmark.productionAdmitted = false;

    const OpenVdbStatus openVdbStatus = GetOpenVdbStatus();
    benchmark.report = Json::object({
        {"schema", "slicesoft.texture_fill_partition.release_evidence.12e_08c.1"},
        {"caseName", request.caseName},
        {"buildType", request.buildType},
        {"diagnosticOnly", true},
        {"productionOutputWritten", false},
        {"productionAdmitted", false},
        {"backend", benchmark.partition.backend},
        {"backendRole", benchmark.partition.backendRole},
        {"build",
         Json::object({
             {"useOpenVdb", openVdbStatus.compiled_with_openvdb},
             {"openVdbRuntimeAvailable", openVdbStatus.runtime_available},
         })},
        {"input",
         Json::object({
             {"configPath", request.configPath},
             {"modelPath", request.modelPath},
             {"vertexCount", request.mesh->vertices.size()},
             {"triangleCount", request.mesh->triangles.size()},
             {"voxelMm", request.voxelMm},
             {"widthMm", request.widthMm},
             {"paddingVoxels", request.paddingVoxels},
         })},
        {"topology",
         Json::object({
             {"sourceTriangles", request.sourceTriangles},
             {"acceptedTriangles", request.acceptedTriangles},
             {"degenerateTriangles", request.degenerateTriangles},
             {"boundaryEdges", request.boundaryEdges},
             {"nonManifoldEdges", request.nonManifoldEdges},
         })},
        {"grid", BuildGridJson(grid)},
        {"partition", BuildPartitionJson(benchmark.partition)},
        {"timingsMs",
         BuildPerformanceJson(
             request,
             benchmark.partition.performance,
             benchmarkWallMs)},
        {"memory", BuildMemoryJson(benchmark.partition.performance)},
        {"queries",
         Json::object({
             {"occupancyQueryCount", benchmark.partition.queryStats.occupancyQueryCount},
             {"occupancyVisitedNodes", benchmark.partition.queryStats.occupancyVisitedNodes},
             {"occupancyTestedTriangles", benchmark.partition.queryStats.occupancyTestedTriangles},
             {"nearestQueryCount", benchmark.partition.queryStats.nearestQueryCount},
             {"nearestVisitedNodes", benchmark.partition.queryStats.nearestVisitedNodes},
             {"nearestTestedTriangles", benchmark.partition.queryStats.nearestTestedTriangles},
         })},
        {"issues", ValidationIssuesToJson(benchmark.partition.issues)},
        {"budget",
         Json::object({
             {"evaluated", false},
             {"status", benchmark.partition.partitionPass ? "measured_not_frozen" : "blocked"},
             {"reason",
              benchmark.partition.partitionPass
                  ? "real-model budget must be frozen from the complete 12E-08C matrix"
                  : "partition candidate was blocked before budget admission"},
         })},
    });
    return benchmark;
}

}  // namespace slicer_core
