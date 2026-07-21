#include "slicer_core/diagnostics/TextureFillPartitionReleaseBenchmark.h"

#include "slicer_core/diagnostics/ValidationIssue.h"
#include "slicer_core/geometry/OpenVdbAdapter.h"
#include "slicer_core/geometry/TriangleMeshData.h"
#include "slicer_core/materials/texture_application/GlobalTextureFillPartitionService.h"
#include "slicer_core/materials/texture_application/LegacyCpuGlobalDistanceBackend.h"
#include "slicer_core/materials/texture_application/TextureFillPartitionTextureTransfer.h"
#include "slicer_core/raster/TextureFillPartitionRasterMapper.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

using Clock = std::chrono::steady_clock;

constexpr std::size_t kMaterialChannelCount{6U};
constexpr std::size_t kWhiteChannel{3U};

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
    const TextureFillPartitionTextureTransferResult& transfer,
    const TextureFillPartitionRasterMappingResult& rasterMapping,
    const TextureFillPartitionFullClosureAdapterResult& fullClosure,
    const double benchmarkWallMs)
{
    const double globalCoreMs = performance.totalCoreMs
        + (transfer.available ? transfer.stats.transferMs : 0.0)
        + (rasterMapping.available ? rasterMapping.stats.mappingMs : 0.0)
        + (fullClosure.available ? fullClosure.analysisMs : 0.0);
    return Json::object({
        {"configLoadMs", request.configLoadMs},
        {"modelLoadMs", request.modelLoadMs},
        {"meshAdaptMs", request.meshAdaptMs},
        {"topologyMs", performance.topologyMs},
        {"occupancyBuildMs", performance.occupancyBuildMs},
        {"distanceQueryMs", performance.distanceQueryMs},
        {"partitionMs", performance.partitionMs},
        {"totalCoreMs", performance.totalCoreMs},
        {"globalCoreMs", globalCoreMs},
        {"benchmarkWallMs", benchmarkWallMs},
        {"textureTransferMs",
         transfer.available ? Json{transfer.stats.transferMs} : Json{nullptr}},
        {"rasterMappingMs",
         rasterMapping.available ? Json{rasterMapping.stats.mappingMs} : Json{nullptr}},
        {"fullClosureMs",
         fullClosure.available ? Json{fullClosure.analysisMs} : Json{nullptr}},
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

Json BuildTextureTransferJson(
    const TextureFillPartitionTextureTransferResult& result)
{
    return Json::object({
        {"available", result.available},
        {"status", result.status},
        {"productionAcceptance", result.productionAcceptance},
        {"textureSurfaceVoxels", result.stats.textureSurfaceVoxels},
        {"sampledTextureCount", result.stats.sampledTextureCount},
        {"materialDiffuseCount", result.stats.materialDiffuseCount},
        {"fallbackCount", result.stats.fallbackCount},
        {"missingUvCount", result.stats.missingUvCount},
        {"missingTextureCount", result.stats.missingTextureCount},
        {"textureSampleFailureCount", result.stats.textureSampleFailureCount},
        {"outsideColoredCount", result.stats.outsideColoredCount},
        {"issues", ValidationIssuesToJson(result.issues)},
    });
}

Json BuildRasterMappingJson(
    const TextureFillPartitionRasterMappingResult& result)
{
    return Json::object({
        {"available", result.available},
        {"status", result.status},
        {"productionAcceptance", result.productionAcceptance},
        {"productionOutputWritten", result.productionOutputWritten},
        {"mappingMethod", result.mappingMethod},
        {"layerCount", result.layers.size()},
        {"modelRasterVoxels", result.stats.modelRasterVoxels},
        {"textureSurfaceRasterVoxels", result.stats.textureSurfaceRasterVoxels},
        {"modelFillRasterVoxels", result.stats.modelFillRasterVoxels},
        {"overlapRasterVoxels", result.stats.overlapRasterVoxels},
        {"unassignedModelRasterVoxels", result.stats.unassignedModelRasterVoxels},
        {"partitionPass", result.stats.partitionPass},
        {"issues", ValidationIssuesToJson(result.issues)},
    });
}

Json BuildFullClosureJson(
    const TextureFillPartitionFullClosureAdapterResult& result)
{
    return Json::object({
        {"available", result.available},
        {"status", result.status},
        {"scope", result.scope},
        {"source", result.source},
        {"confidence", result.confidence},
        {"fullClosurePass", result.fullClosurePass},
        {"repairAttempted", result.repairAttempted},
        {"productionOutputWritten", result.productionOutputWritten},
        {"productionAcceptance", result.productionAcceptance},
        {"layerCount", result.layers.size()},
        {"totalExpectedDomainGapPixels", result.totalExpectedDomainGapPixels},
        {"totalModelDomainGapPixels", result.totalModelDomainGapPixels},
        {"totalSemanticChannelMismatchPixels",
         result.totalSemanticChannelMismatchPixels},
        {"issues", ValidationIssuesToJson(result.issues)},
    });
}

TextureFillPartitionRasterGridSpec BuildRasterGrid(
    const TextureFillPartitionGridSpec& grid)
{
    TextureFillPartitionRasterGridSpec rasterGrid;
    rasterGrid.width = grid.width;
    rasterGrid.height = grid.height;
    rasterGrid.depth = grid.depth;
    rasterGrid.originXMm = grid.originXMm;
    rasterGrid.originYMm = grid.originYMm;
    rasterGrid.originZMm = grid.originZMm;
    rasterGrid.pixelPitchXMm = grid.spacingXMm;
    rasterGrid.pixelPitchYMm = grid.spacingYMm;
    rasterGrid.layerThicknessMm = grid.spacingZMm;
    return rasterGrid;
}

std::vector<TextureFillPartitionFullClosureLayerEvidence>
BuildDiagnosticClosureEvidence(
    const TextureFillPartitionRasterMappingResult& rasterMapping)
{
    std::vector<TextureFillPartitionFullClosureLayerEvidence> evidenceLayers;
    evidenceLayers.reserve(rasterMapping.layers.size());
    const std::size_t pixelCount = static_cast<std::size_t>(rasterMapping.grid.width)
        * static_cast<std::size_t>(rasterMapping.grid.height);

    for (const TextureFillPartitionRasterLayer& rasterLayer : rasterMapping.layers)
    {
        TextureFillPartitionFullClosureLayerEvidence evidence;
        evidence.layerIndex = rasterLayer.layerIndex;
        evidence.zMm = rasterLayer.zMm;
        evidence.widthPx = rasterMapping.grid.width;
        evidence.heightPx = rasterMapping.grid.height;
        evidence.supportFillMask.assign(pixelCount, 0U);
        evidence.internalVoidSupportMask.assign(pixelCount, 0U);
        evidence.surfaceVarnishMask.assign(pixelCount, 0U);
        evidence.outerVarnishShellMask.assign(pixelCount, 0U);
        evidence.modelEnvelopeMask = rasterLayer.modelMask;
        evidence.supportRequiredMask.assign(pixelCount, 0U);
        evidence.channels.assign(pixelCount * kMaterialChannelCount, 255U);

        for (std::size_t pixelIndex{0U}; pixelIndex < pixelCount; ++pixelIndex)
        {
            if (rasterLayer.textureSurfaceMask.at(pixelIndex) != 0U)
            {
                const std::array<std::uint8_t, 3>& rgb =
                    rasterLayer.textureRgb.at(pixelIndex);
                evidence.channels.at(pixelIndex * kMaterialChannelCount) = rgb[0];
                evidence.channels.at(pixelIndex * kMaterialChannelCount + 1U) = rgb[1];
                evidence.channels.at(pixelIndex * kMaterialChannelCount + 2U) = rgb[2];
                if (rgb[0] == 255U && rgb[1] == 255U && rgb[2] == 255U)
                {
                    evidence.channels.at(
                        pixelIndex * kMaterialChannelCount + kWhiteChannel) = 0U;
                }
            }
            else if (rasterLayer.modelFillMask.at(pixelIndex) != 0U)
            {
                evidence.channels.at(
                    pixelIndex * kMaterialChannelCount + kWhiteChannel) = 0U;
            }
        }
        evidenceLayers.push_back(std::move(evidence));
    }
    return evidenceLayers;
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
    if (request.adaptedMesh != nullptr
        && &request.adaptedMesh->mesh != request.mesh)
    {
        throw std::invalid_argument(
            "benchmark adapted mesh must own the classification mesh");
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
    if (benchmark.partition.partitionPass && request.adaptedMesh != nullptr)
    {
        TextureFillPartitionTextureTransferRequest transferRequest;
        transferRequest.adaptedMesh = request.adaptedMesh;
        transferRequest.partition = &benchmark.partition;
        transferRequest.textureSample = request.textureSample;
        transferRequest.fallbackRgb = request.fallbackRgb;
        transferRequest.missingTexturePolicy = request.missingTexturePolicy;
        benchmark.textureTransfer = TransferTextureFillPartition(transferRequest);

        if (benchmark.textureTransfer.available)
        {
            TextureFillPartitionRasterMappingRequest rasterRequest;
            rasterRequest.partition = &benchmark.partition;
            rasterRequest.transfer = &benchmark.textureTransfer;
            rasterRequest.rasterGrid = BuildRasterGrid(grid);
            benchmark.rasterMapping = MapTextureFillPartitionToRaster(rasterRequest);
        }

        if (benchmark.rasterMapping.available)
        {
            const std::vector<TextureFillPartitionFullClosureLayerEvidence>
                closureEvidence = BuildDiagnosticClosureEvidence(
                    benchmark.rasterMapping);
            TextureFillPartitionFullClosureAdapterRequest closureRequest;
            closureRequest.rasterMapping = &benchmark.rasterMapping;
            closureRequest.layers = &closureEvidence;
            benchmark.fullClosure = AdaptTextureFillPartitionFullClosure(
                closureRequest);
        }
    }
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
        {"textureTransfer", BuildTextureTransferJson(benchmark.textureTransfer)},
        {"rasterMapping", BuildRasterMappingJson(benchmark.rasterMapping)},
        {"fullClosure", BuildFullClosureJson(benchmark.fullClosure)},
        {"timingsMs",
         BuildPerformanceJson(
             request,
             benchmark.partition.performance,
             benchmark.textureTransfer,
             benchmark.rasterMapping,
             benchmark.fullClosure,
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
             {"status",
              benchmark.fullClosure.fullClosurePass
                  ? "measured_not_frozen"
                  : "blocked"},
             {"reason",
              benchmark.fullClosure.fullClosurePass
                  ? "real-model budget must be frozen from the complete 12E-08C matrix"
                  : "global diagnostic chain was blocked before budget admission"},
         })},
    });
    return benchmark;
}

}  // namespace slicer_core
