#include "slicer_core/reports/TextureFillPartitionReport.h"

#include "slicer_core/diagnostics/TextureFillPartitionClosureAdapter.h"
#include "slicer_core/diagnostics/TextureFillPartitionFullClosureAdapter.h"
#include "slicer_core/geometry/OpenVdbAdapter.h"
#include "slicer_core/materials/texture_application/TextureFillPartitionAdmission.h"
#include "slicer_core/materials/texture_application/TextureFillPartitionTextureTransfer.h"
#include "slicer_core/pipeline/TextureFillPartitionDiagnosticComposer.h"
#include "slicer_core/raster/TextureFillPartitionRasterMapper.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace
{

using slicer_core::GlobalTextureFillPartitionResult;
using slicer_core::Json;
using slicer_core::SliceConfig;
using slicer_core::TextureFillPartitionConformanceResult;
using slicer_core::TextureFillPartitionClosureAdapterResult;
using slicer_core::TextureFillPartitionDiagnosticComposerResult;
using slicer_core::TextureFillPartitionFullClosureAdapterResult;
using slicer_core::TextureFillPartitionRasterMappingResult;
using slicer_core::TextureFillPartitionStats;
using slicer_core::TextureFillPartitionTextureTransferResult;
using slicer_core::TextureFillPartitionWidthSweepResult;
using slicer_core::TextureFillPartitionWidthSweepSample;

Json NullableDouble(const bool available, const double value)
{
    return available && std::isfinite(value) ? Json{value} : Json{nullptr};
}

Json BuildGrid(const GlobalTextureFillPartitionResult& result)
{
    return Json::object({
        {"width", result.grid.width},
        {"height", result.grid.height},
        {"depth", result.grid.depth},
        {"originXMm", result.grid.originXMm},
        {"originYMm", result.grid.originYMm},
        {"originZMm", result.grid.originZMm},
        {"spacingXMm", result.grid.spacingXMm},
        {"spacingYMm", result.grid.spacingYMm},
        {"spacingZMm", result.grid.spacingZMm},
    });
}

Json BuildWidth(const GlobalTextureFillPartitionResult& result)
{
    const double quantizationErrorMm =
        result.widthMetrics.effectiveWidthMm
        - result.options.requestedWidthMm;
    const double epsilon = std::max(
        result.widthMetrics.epsilonMm,
        1.0e-12);
    return Json::object({
        {"requestedWidthMm", result.options.requestedWidthMm},
        {"widthStepMm", result.options.widthStepMm},
        {"baseMinimumWidthMm", result.options.baseMinimumWidthMm},
        {"classificationResolutionMm", result.widthMetrics.classificationResolutionMm},
        {"effectiveMinimumWidthMm", result.widthMetrics.effectiveMinimumWidthMm},
        {"effectiveWidthMm", result.widthMetrics.effectiveWidthMm},
        {"maxInteriorDistanceMm", result.widthMetrics.maxInteriorDistanceMm},
        {"allTextureThresholdMm", result.widthMetrics.allTextureThresholdMm},
        {"allTexture", result.widthMetrics.allTexture},
        {"quantizationErrorMm", quantizationErrorMm},
        {"clamped", std::abs(quantizationErrorMm) > epsilon},
    });
}

Json BuildPartition(const GlobalTextureFillPartitionResult& result)
{
    const double modelVoxels = static_cast<double>(result.stats.modelVoxels);
    const double textureRatio = result.stats.modelVoxels == 0U
        ? 0.0
        : static_cast<double>(result.stats.textureSurfaceVoxels) / modelVoxels;
    const double fillRatio = result.stats.modelVoxels == 0U
        ? 0.0
        : static_cast<double>(result.stats.modelFillVoxels) / modelVoxels;
    return Json::object({
        {"modelVoxels", result.stats.modelVoxels},
        {"textureSurfaceVoxels", result.stats.textureSurfaceVoxels},
        {"modelFillVoxels", result.stats.modelFillVoxels},
        {"overlapTextureFillVoxels", result.stats.overlapTextureFillVoxels},
        {"unassignedModelVoxels", result.stats.unassignedModelVoxels},
        {"textureOutsideModelVoxels", result.stats.textureOutsideModelVoxels},
        {"modelFillOutsideModelVoxels", result.stats.modelFillOutsideModelVoxels},
        {"modelPixels", nullptr},
        {"textureSurfacePixels", nullptr},
        {"modelFillPixels", nullptr},
        {"overlapTextureFillPixels", nullptr},
        {"unassignedModelPixels", nullptr},
        {"textureCoverageRatio", textureRatio},
        {"modelFillCoverageRatio", fillRatio},
        {"thinRegionMergedVoxels", 0},
        {"medialAxisTieCount", 0},
        {"partitionPass", result.partitionPass},
    });
}

Json BuildPerformance(
    const GlobalTextureFillPartitionResult& result,
    const TextureFillPartitionTextureTransferResult* transfer = nullptr,
    const TextureFillPartitionRasterMappingResult* rasterMapping = nullptr,
    const TextureFillPartitionFullClosureAdapterResult* fullClosure = nullptr)
{
    return Json::object({
        {"preflightMs", result.performance.topologyMs},
        {"topologyMs", result.performance.topologyMs},
        {"levelSetMs", result.performance.levelSetMs},
        {"gridSampleMs", result.performance.gridSampleMs},
        {"occupancyMs", result.performance.occupancyBuildMs},
        {"distanceMs", result.performance.distanceQueryMs},
        {"partitionMs", result.performance.partitionMs},
        {"textureTransferMs",
         transfer == nullptr || !transfer->available
             ? Json{nullptr}
             : Json{transfer->stats.transferMs}},
        {"rasterMappingMs",
         rasterMapping == nullptr || !rasterMapping->available
             ? Json{nullptr}
             : Json{rasterMapping->stats.mappingMs}},
        {"fullClosureMs",
         fullClosure == nullptr || !fullClosure->available
             ? Json{nullptr}
             : Json{fullClosure->analysisMs}},
        {"totalCoreMs", result.performance.totalCoreMs},
        {"gridVoxelCount", result.performance.gridVoxelCount},
        {"maskBytes", result.performance.maskBytes},
        {"closestReferenceBytes", result.performance.closestReferenceBytes},
        {"occupancyQueryBytes", result.performance.occupancyQueryBytes},
        {"nearestQueryBytes", result.performance.nearestQueryBytes},
        {"openVdbGridBytes", result.performance.openVdbGridBytes},
        {"workingSetBytes",
         NullableDouble(
             result.performance.processMemoryAvailable,
             static_cast<double>(result.performance.processWorkingSetBytes))},
        {"peakWorkingSetBytes",
         NullableDouble(
             result.performance.processMemoryAvailable,
             static_cast<double>(result.performance.processPeakWorkingSetBytes))},
    });
}

Json BuildTextureTransfer(
    const TextureFillPartitionTextureTransferResult* transfer)
{
    if (transfer == nullptr)
    {
        return Json::object({
            {"availability", "unavailable"},
            {"status", "not_evaluated"},
            {"productionAcceptance", "not_evaluated"},
            {"textureSurfaceVoxels", 0},
            {"sampledTextureCount", 0},
            {"materialDiffuseCount", 0},
            {"fallbackCount", 0},
            {"missingUvCount", 0},
            {"missingTextureCount", 0},
            {"textureSampleFailureCount", 0},
            {"uvOutOfRangeCount", 0},
            {"outsideColoredCount", 0},
            {"reusedReferenceCount", 0},
            {"nearestQueryCount", 0},
            {"maxTransferDistanceMm", nullptr},
            {"medialAxisTieCount", 0},
            {"loadedTextureCount", 0},
            {"textureCacheHits", 0},
            {"textureCacheMisses", 0},
            {"textureCacheBytes", 0},
            {"issues", Json::array({})},
        });
    }
    return Json::object({
        {"availability", transfer->available ? "available" : "unavailable"},
        {"status", transfer->status},
        {"productionAcceptance", transfer->productionAcceptance},
        {"textureSurfaceVoxels", transfer->stats.textureSurfaceVoxels},
        {"sampledTextureCount", transfer->stats.sampledTextureCount},
        {"materialDiffuseCount", transfer->stats.materialDiffuseCount},
        {"fallbackCount", transfer->stats.fallbackCount},
        {"missingUvCount", transfer->stats.missingUvCount},
        {"missingTextureCount", transfer->stats.missingTextureCount},
        {"textureSampleFailureCount", transfer->stats.textureSampleFailureCount},
        {"uvOutOfRangeCount", transfer->stats.uvOutOfRangeCount},
        {"outsideColoredCount", transfer->stats.outsideColoredCount},
        {"reusedReferenceCount", transfer->stats.reusedReferenceCount},
        {"nearestQueryCount", transfer->stats.nearestQueryCount},
        {"maxTransferDistanceMm",
         NullableDouble(
             transfer->stats.reusedReferenceCount > 0U,
             transfer->stats.maxTransferDistanceMm)},
        {"medialAxisTieCount", transfer->stats.medialAxisTieCount},
        {"loadedTextureCount", transfer->stats.loadedTextureCount},
        {"textureCacheHits", transfer->stats.textureCacheHits},
        {"textureCacheMisses", transfer->stats.textureCacheMisses},
        {"textureCacheBytes", transfer->stats.textureCacheBytes},
        {"issues", slicer_core::ValidationIssuesToJson(transfer->issues)},
    });
}

Json BuildDiagnosticComposer(
    const TextureFillPartitionDiagnosticComposerResult* composer)
{
    if (composer == nullptr)
    {
        return Json::object({
            {"availability", "unavailable"},
            {"status", "not_evaluated"},
            {"productionAcceptance", "not_evaluated"},
            {"width", 0},
            {"height", 0},
            {"depth", 0},
            {"layerCount", 0},
            {"channelOrder", Json::array({"R", "G", "B", "W", "S", "V"})},
            {"textureSurfaceVoxels", 0},
            {"modelFillVoxels", 0},
            {"modelFillWhiteVoxels", 0},
            {"modelFillVarnishVoxels", 0},
            {"modelFillRgbVoxels", 0},
            {"supportPrintVoxels", 0},
            {"emptyVoxels", 0},
            {"issues", Json::array({})},
        });
    }
    Json::Array channelOrder;
    for (const std::string& channel : composer->channelOrder)
    {
        channelOrder.emplace_back(channel);
    }
    return Json::object({
        {"availability", composer->available ? "available" : "unavailable"},
        {"status", composer->status},
        {"productionAcceptance", composer->productionAcceptance},
        {"width", composer->width},
        {"height", composer->height},
        {"depth", composer->depth},
        {"layerCount", static_cast<std::uint64_t>(composer->layers.size())},
        {"channelOrder", Json{std::move(channelOrder)}},
        {"textureSurfaceVoxels", composer->stats.textureSurfaceVoxels},
        {"modelFillVoxels", composer->stats.modelFillVoxels},
        {"modelFillWhiteVoxels", composer->stats.modelFillWhiteVoxels},
        {"modelFillVarnishVoxels", composer->stats.modelFillVarnishVoxels},
        {"modelFillRgbVoxels", composer->stats.modelFillRgbVoxels},
        {"supportPrintVoxels", composer->stats.supportPrintVoxels},
        {"emptyVoxels", composer->stats.emptyVoxels},
        {"issues", slicer_core::ValidationIssuesToJson(composer->issues)},
    });
}

Json BuildClosureLinkage(
    const TextureFillPartitionClosureAdapterResult* closure)
{
    if (closure == nullptr)
    {
        return Json::object({
            {"availability", "unavailable"},
            {"status", "not_evaluated"},
            {"scope", "texture_model_fill_only"},
            {"source", "unavailable"},
            {"confidence", "unavailable"},
            {"productionAcceptance", "not_evaluated"},
            {"allTexture", false},
            {"colorFillApplicability", "not_evaluated"},
            {"allTextureReason", nullptr},
            {"colorFillGapVoxels", 0},
            {"modelDomainGapVoxels", 0},
            {"supportClosureStatus", "not_evaluated"},
            {"varnishClosureStatus", "not_evaluated"},
            {"repairAttempted", false},
            {"productionOutputWritten", false},
            {"layerCount", 0},
            {"layers", Json::array({})},
            {"issues", Json::array({})},
        });
    }

    Json::Array layers;
    layers.reserve(closure->layers.size());
    for (const slicer_core::TextureFillPartitionClosureLayerResult& layer :
         closure->layers)
    {
        layers.push_back(Json::object({
            {"layerIndex", layer.layerIndex},
            {"zMm", layer.zMm},
            {"colorFillGapVoxels", layer.colorFillGapVoxels},
            {"modelDomainGapVoxels", layer.modelDomainGapVoxels},
        }));
    }
    return Json::object({
        {"availability", closure->available ? "available" : "unavailable"},
        {"status", closure->status},
        {"scope", closure->scope},
        {"source", closure->source},
        {"confidence", closure->confidence},
        {"productionAcceptance", closure->productionAcceptance},
        {"allTexture", closure->allTexture},
        {"colorFillApplicability", closure->colorFillApplicability},
        {"allTextureReason",
         closure->allTextureReason.empty()
             ? Json{nullptr}
             : Json{closure->allTextureReason}},
        {"colorFillGapVoxels", closure->totalColorFillGapVoxels},
        {"modelDomainGapVoxels", closure->totalModelDomainGapVoxels},
        {"supportClosureStatus", closure->supportClosureStatus},
        {"varnishClosureStatus", closure->varnishClosureStatus},
        {"repairAttempted", closure->repairAttempted},
        {"productionOutputWritten", closure->productionOutputWritten},
        {"layerCount", static_cast<std::uint64_t>(closure->layers.size())},
        {"layers", Json{std::move(layers)}},
        {"issues", slicer_core::ValidationIssuesToJson(closure->issues)},
    });
}

Json BuildRasterMapping(
    const TextureFillPartitionRasterMappingResult* mapping)
{
    if (mapping == nullptr)
    {
        return Json::object({
            {"availability", "unavailable"},
            {"status", "not_evaluated"},
            {"productionAcceptance", "not_evaluated"},
            {"mappingMethod", "world_space_cell_containment"},
            {"allTexture", false},
            {"productionOutputWritten", false},
            {"grid", Json::object({
                 {"width", nullptr},
                 {"height", nullptr},
                 {"depth", nullptr},
                 {"originXMm", nullptr},
                 {"originYMm", nullptr},
                 {"originZMm", nullptr},
                 {"pixelPitchXMm", nullptr},
                 {"pixelPitchYMm", nullptr},
                 {"layerThicknessMm", nullptr},
             })},
            {"rasterVoxelCount", 0},
            {"mappedSourceGridVoxels", 0},
            {"outsideSourceGridVoxels", 0},
            {"uniqueSourceVoxelCount", 0},
            {"reusedSourceVoxelCount", 0},
            {"modelRasterVoxels", 0},
            {"textureSurfaceRasterVoxels", 0},
            {"modelFillRasterVoxels", 0},
            {"textureRgbRasterVoxels", 0},
            {"overlapRasterVoxels", 0},
            {"unassignedModelRasterVoxels", 0},
            {"sourceModelCoverage", nullptr},
            {"rasterModelCoverage", nullptr},
            {"modelCoverageDelta", nullptr},
            {"maxCenterQuantizationErrorMm", nullptr},
            {"mappingMs", nullptr},
            {"partitionPass", false},
            {"layerCount", 0},
            {"layers", Json::array({})},
            {"issues", Json::array({})},
        });
    }

    Json::Array layers;
    layers.reserve(mapping->layers.size());
    for (const slicer_core::TextureFillPartitionRasterLayer& layer : mapping->layers)
    {
        const auto countPrinted = [](const std::vector<std::uint8_t>& mask)
        {
            return static_cast<std::uint64_t>(std::count(
                mask.begin(),
                mask.end(),
                static_cast<std::uint8_t>(1U)));
        };
        layers.push_back(Json::object({
            {"layerIndex", layer.layerIndex},
            {"zMm", layer.zMm},
            {"modelRasterVoxels", countPrinted(layer.modelMask)},
            {"textureSurfaceRasterVoxels", countPrinted(layer.textureSurfaceMask)},
            {"modelFillRasterVoxels", countPrinted(layer.modelFillMask)},
        }));
    }
    return Json::object({
        {"availability", mapping->available ? "available" : "unavailable"},
        {"status", mapping->status},
        {"productionAcceptance", mapping->productionAcceptance},
        {"mappingMethod", mapping->mappingMethod},
        {"allTexture", mapping->allTexture},
        {"productionOutputWritten", mapping->productionOutputWritten},
        {"grid", Json::object({
             {"width", mapping->grid.width},
             {"height", mapping->grid.height},
             {"depth", mapping->grid.depth},
             {"originXMm", mapping->grid.originXMm},
             {"originYMm", mapping->grid.originYMm},
             {"originZMm", mapping->grid.originZMm},
             {"pixelPitchXMm", mapping->grid.pixelPitchXMm},
             {"pixelPitchYMm", mapping->grid.pixelPitchYMm},
             {"layerThicknessMm", mapping->grid.layerThicknessMm},
         })},
        {"rasterVoxelCount", mapping->stats.rasterVoxelCount},
        {"mappedSourceGridVoxels", mapping->stats.mappedSourceGridVoxels},
        {"outsideSourceGridVoxels", mapping->stats.outsideSourceGridVoxels},
        {"uniqueSourceVoxelCount", mapping->stats.uniqueSourceVoxelCount},
        {"reusedSourceVoxelCount", mapping->stats.reusedSourceVoxelCount},
        {"modelRasterVoxels", mapping->stats.modelRasterVoxels},
        {"textureSurfaceRasterVoxels", mapping->stats.textureSurfaceRasterVoxels},
        {"modelFillRasterVoxels", mapping->stats.modelFillRasterVoxels},
        {"textureRgbRasterVoxels", mapping->stats.textureRgbRasterVoxels},
        {"overlapRasterVoxels", mapping->stats.overlapRasterVoxels},
        {"unassignedModelRasterVoxels", mapping->stats.unassignedModelRasterVoxels},
        {"sourceModelCoverage", mapping->stats.sourceModelCoverage},
        {"rasterModelCoverage", mapping->stats.rasterModelCoverage},
        {"modelCoverageDelta", mapping->stats.modelCoverageDelta},
        {"maxCenterQuantizationErrorMm", mapping->stats.maxCenterQuantizationErrorMm},
        {"mappingMs", mapping->stats.mappingMs},
        {"partitionPass", mapping->stats.partitionPass},
        {"layerCount", static_cast<std::uint64_t>(mapping->layers.size())},
        {"layers", Json{std::move(layers)}},
        {"issues", slicer_core::ValidationIssuesToJson(mapping->issues)},
    });
}

Json BuildFullClosureLinkage(
    const TextureFillPartitionFullClosureAdapterResult* closure)
{
    if (closure == nullptr)
    {
        return Json::object({
            {"availability", "unavailable"},
            {"status", "not_evaluated"},
            {"scope", "full_material_domain"},
            {"source", "unavailable"},
            {"confidence", "unavailable"},
            {"productionAcceptance", "not_evaluated"},
            {"allTexture", false},
            {"colorFillApplicability", "not_evaluated"},
            {"allTextureReason", nullptr},
            {"modelClosureStatus", "not_evaluated"},
            {"supportClosureStatus", "not_evaluated"},
            {"varnishClosureStatus", "not_evaluated"},
            {"fullClosurePass", false},
            {"repairAttempted", false},
            {"productionOutputWritten", false},
            {"expectedDomainGapPixels", 0},
            {"modelDomainGapPixels", 0},
            {"supportRequiredGapPixels", 0},
            {"outerVarnishGapPixels", 0},
            {"unexpectedOccupiedPixels", 0},
            {"supportChannelMismatchPixels", 0},
            {"varnishChannelMismatchPixels", 0},
            {"semanticChannelMismatchPixels", 0},
            {"colorFillGapPixels", 0},
            {"modelSupportGapPixels", 0},
            {"colorSupportGapPixels", 0},
            {"internalVoidGapPixels", 0},
            {"varnishSupportGapPixels", 0},
            {"analysisMs", nullptr},
            {"layerCount", 0},
            {"layers", Json::array({})},
            {"issues", Json::array({})},
        });
    }

    Json::Array layers;
    layers.reserve(closure->layers.size());
    for (const slicer_core::TextureFillPartitionFullClosureLayerResult& layer :
         closure->layers)
    {
        layers.push_back(Json::object({
            {"layerIndex", layer.layerIndex},
            {"zMm", layer.zMm},
            {"closurePass", layer.closurePass},
            {"expectedDomainGapPixels", layer.expectedDomainGapPixels},
            {"modelDomainGapPixels", layer.modelDomainGapPixels},
            {"supportRequiredGapPixels", layer.supportRequiredGapPixels},
            {"outerVarnishGapPixels", layer.outerVarnishGapPixels},
            {"unexpectedOccupiedPixels", layer.unexpectedOccupiedPixels},
            {"supportChannelMismatchPixels", layer.supportChannelMismatchPixels},
            {"varnishChannelMismatchPixels", layer.varnishChannelMismatchPixels},
            {"colorFillGapPixels", layer.semantic.colorFillGapPixels},
            {"modelSupportGapPixels", layer.semantic.modelSupportGapPixels},
            {"colorSupportGapPixels", layer.semantic.colorSupportGapPixels},
            {"internalVoidGapPixels", layer.semantic.internalVoidGapPixels},
            {"varnishSupportGapPixels", layer.semantic.varnishSupportGapPixels},
        }));
    }
    return Json::object({
        {"availability", closure->available ? "available" : "unavailable"},
        {"status", closure->status},
        {"scope", closure->scope},
        {"source", closure->source},
        {"confidence", closure->confidence},
        {"productionAcceptance", closure->productionAcceptance},
        {"allTexture", closure->allTexture},
        {"colorFillApplicability", closure->colorFillApplicability},
        {"allTextureReason",
         closure->allTextureReason.empty()
             ? Json{nullptr}
             : Json{closure->allTextureReason}},
        {"modelClosureStatus", closure->modelClosureStatus},
        {"supportClosureStatus", closure->supportClosureStatus},
        {"varnishClosureStatus", closure->varnishClosureStatus},
        {"fullClosurePass", closure->fullClosurePass},
        {"repairAttempted", closure->repairAttempted},
        {"productionOutputWritten", closure->productionOutputWritten},
        {"expectedDomainGapPixels", closure->totalExpectedDomainGapPixels},
        {"modelDomainGapPixels", closure->totalModelDomainGapPixels},
        {"supportRequiredGapPixels", closure->totalSupportRequiredGapPixels},
        {"outerVarnishGapPixels", closure->totalOuterVarnishGapPixels},
        {"unexpectedOccupiedPixels", closure->totalUnexpectedOccupiedPixels},
        {"supportChannelMismatchPixels",
         closure->totalSupportChannelMismatchPixels},
        {"varnishChannelMismatchPixels",
         closure->totalVarnishChannelMismatchPixels},
        {"semanticChannelMismatchPixels",
         closure->totalSemanticChannelMismatchPixels},
        {"colorFillGapPixels", closure->totalColorFillGapPixels},
        {"modelSupportGapPixels", closure->totalModelSupportGapPixels},
        {"colorSupportGapPixels", closure->totalColorSupportGapPixels},
        {"internalVoidGapPixels", closure->totalInternalVoidGapPixels},
        {"varnishSupportGapPixels", closure->totalVarnishSupportGapPixels},
        {"analysisMs", closure->analysisMs},
        {"layerCount", static_cast<std::uint64_t>(closure->layers.size())},
        {"layers", Json{std::move(layers)}},
        {"issues", slicer_core::ValidationIssuesToJson(closure->issues)},
    });
}

Json BuildQueryStats(const GlobalTextureFillPartitionResult& result)
{
    return Json::object({
        {"occupancyQueryCount", result.queryStats.occupancyQueryCount},
        {"occupancyVisitedNodes", result.queryStats.occupancyVisitedNodes},
        {"occupancyTestedTriangles", result.queryStats.occupancyTestedTriangles},
        {"occupancyFallbackRayCount", result.queryStats.occupancyFallbackRayCount},
        {"occupancyAmbiguousRayCount", result.queryStats.occupancyAmbiguousRayCount},
        {"occupancyBoundaryPointCount", result.queryStats.occupancyBoundaryPointCount},
        {"sdfSampleCount", result.queryStats.sdfSampleCount},
        {"sdfActiveSampleCount", result.queryStats.sdfActiveSampleCount},
        {"sdfBackgroundSampleCount", result.queryStats.sdfBackgroundSampleCount},
        {"nearestQueryCount", result.queryStats.nearestQueryCount},
        {"nearestVisitedNodes", result.queryStats.nearestVisitedNodes},
        {"nearestTestedTriangles", result.queryStats.nearestTestedTriangles},
    });
}

Json BuildLayers(const GlobalTextureFillPartitionResult& result)
{
    if (result.grid.width <= 0 || result.grid.height <= 0 || result.grid.depth <= 0)
    {
        throw std::invalid_argument(
            "texture/fill partition report requires positive grid dimensions");
    }

    const std::size_t expectedCount = static_cast<std::size_t>(result.grid.width)
        * static_cast<std::size_t>(result.grid.height)
        * static_cast<std::size_t>(result.grid.depth);
    if (result.modelMask.values.size() != expectedCount
        || result.textureSurfaceMask.values.size() != expectedCount
        || result.modelFillMask.values.size() != expectedCount)
    {
        throw std::invalid_argument(
            "texture/fill partition report requires validated same-grid masks");
    }

    const std::size_t layerArea = static_cast<std::size_t>(result.grid.width)
        * static_cast<std::size_t>(result.grid.height);
    Json::Array layers;
    layers.reserve(static_cast<std::size_t>(result.grid.depth));
    for (int layerIndex{0}; layerIndex < result.grid.depth; ++layerIndex)
    {
        TextureFillPartitionStats stats;
        const std::size_t begin = static_cast<std::size_t>(layerIndex)
            * layerArea;
        const std::size_t end = begin + layerArea;
        for (std::size_t index{begin}; index < end; ++index)
        {
            const bool model = result.modelMask.values.at(index) != 0U;
            const bool texture = result.textureSurfaceMask.values.at(index) != 0U;
            const bool fill = result.modelFillMask.values.at(index) != 0U;
            stats.modelVoxels += model ? 1U : 0U;
            stats.textureSurfaceVoxels += texture ? 1U : 0U;
            stats.modelFillVoxels += fill ? 1U : 0U;
            stats.overlapTextureFillVoxels += texture && fill ? 1U : 0U;
            stats.unassignedModelVoxels += model && !texture && !fill ? 1U : 0U;
            stats.textureOutsideModelVoxels += texture && !model ? 1U : 0U;
            stats.modelFillOutsideModelVoxels += fill && !model ? 1U : 0U;
        }
        const bool partitionPass = stats.overlapTextureFillVoxels == 0U
            && stats.unassignedModelVoxels == 0U
            && stats.textureOutsideModelVoxels == 0U
            && stats.modelFillOutsideModelVoxels == 0U
            && stats.textureSurfaceVoxels + stats.modelFillVoxels
                == stats.modelVoxels;
        layers.emplace_back(Json::object({
            {"layerIndex", layerIndex},
            {"zMm",
             result.grid.originZMm
                 + (static_cast<double>(layerIndex) + 0.5)
                    * result.grid.spacingZMm},
            {"modelVoxels", stats.modelVoxels},
            {"textureSurfaceVoxels", stats.textureSurfaceVoxels},
            {"modelFillVoxels", stats.modelFillVoxels},
            {"overlapTextureFillVoxels", stats.overlapTextureFillVoxels},
            {"unassignedModelVoxels", stats.unassignedModelVoxels},
            {"partitionPass", partitionPass},
        }));
    }
    return Json{std::move(layers)};
}

Json BuildConformance(const TextureFillPartitionConformanceResult& conformance)
{
    return Json::object({
        {"cpuAvailable", conformance.cpuAvailable},
        {"openVdbAvailable", conformance.openVdbAvailable},
        {"sameGrid", conformance.sameGrid},
        {"cpuPartitionInvariantPass", conformance.cpuPartitionInvariantPass},
        {"openVdbPartitionInvariantPass", conformance.openVdbPartitionInvariantPass},
        {"cpuStatus", conformance.cpuStatus},
        {"openVdbStatus", conformance.openVdbStatus},
        {"cpuBackendRole", conformance.cpuBackendRole},
        {"openVdbBackendRole", conformance.openVdbBackendRole},
        {"status", conformance.conformanceStatus},
        {"productionAcceptance", conformance.productionAcceptance},
        {"modelOnlyCpuVoxels", conformance.modelOnlyCpuVoxels},
        {"modelOnlyOpenVdbVoxels", conformance.modelOnlyOpenVdbVoxels},
        {"textureOnlyCpuVoxels", conformance.textureOnlyCpuVoxels},
        {"textureOnlyOpenVdbVoxels", conformance.textureOnlyOpenVdbVoxels},
        {"fillOnlyCpuVoxels", conformance.fillOnlyCpuVoxels},
        {"fillOnlyOpenVdbVoxels", conformance.fillOnlyOpenVdbVoxels},
        {"commonDistanceSamples", conformance.commonDistanceSamples},
        {"maxDistanceDeltaMm", conformance.maxDistanceDeltaMm},
        {"meanDistanceDeltaMm", conformance.meanDistanceDeltaMm},
        {"allTextureThresholdDeltaMm", conformance.allTextureThresholdDeltaMm},
        {"openVdbToCpuCoreTimeRatio",
         NullableDouble(
             conformance.openVdbToCpuCoreTimeRatio > 0.0,
             conformance.openVdbToCpuCoreTimeRatio)},
        {"openVdbToCpuPeakMemoryRatio",
         NullableDouble(
             conformance.openVdbToCpuPeakMemoryRatio > 0.0,
             conformance.openVdbToCpuPeakMemoryRatio)},
        {"issues", slicer_core::ValidationIssuesToJson(conformance.issues)},
    });
}

Json BuildConfigSnapshot(
    const SliceConfig& config,
    const GlobalTextureFillPartitionResult& result)
{
    const slicer_core::OpenVdbStatus openVdb = slicer_core::GetOpenVdbStatus();
    return Json::object({
        {"textureEnabled", config.texture.enabled},
        {"textureApplyMode", config.texture.apply_mode},
        {"geometryMode", config.texture.surface_shell.geometry_mode},
        {"widthMm", config.texture.surface_shell.width_mm},
        {"widthStepMm", config.texture.surface_shell.width_step_mm},
        {"minimumWidthPolicy", config.texture.surface_shell.minimum_width_policy},
        {"surfaceScope", config.texture.surface_shell.surface_scope},
        {"fullTextureAtModelLimit", config.texture.surface_shell.full_texture_at_model_limit},
        {"modelFillEnabled", config.model_fill.enabled},
        {"modelFillMaterial", config.model_fill.material},
        {"modelFillScope", config.model_fill.scope},
        {"modelFillValue", static_cast<int>(config.model_fill.value)},
        {"grid", BuildGrid(result)},
        {"pixelPitchXMm",
         NullableDouble(config.output.dpi_x > 0, 25.4 / config.output.dpi_x)},
        {"pixelPitchYMm",
         NullableDouble(config.output.dpi_y > 0, 25.4 / config.output.dpi_y)},
        {"layerThicknessMm", config.output.layer_thickness_mm},
        {"openVdbCompiled", openVdb.compiled_with_openvdb},
        {"openVdbRuntimeAvailable", openVdb.runtime_available},
    });
}

Json BuildSweepSample(const TextureFillPartitionWidthSweepSample& sample)
{
    return Json::object({
        {"requestedWidthMm", sample.requestedWidthMm},
        {"effectiveWidthMm", sample.effectiveWidthMm},
        {"allTexture", sample.allTexture},
        {"status", sample.status},
        {"partitionPass", sample.partitionPass},
        {"modelVoxels", sample.stats.modelVoxels},
        {"textureSurfaceVoxels", sample.stats.textureSurfaceVoxels},
        {"modelFillVoxels", sample.stats.modelFillVoxels},
        {"overlapTextureFillVoxels", sample.stats.overlapTextureFillVoxels},
        {"unassignedModelVoxels", sample.stats.unassignedModelVoxels},
        {"totalCoreMs", sample.performance.totalCoreMs},
    });
}

}  // namespace

namespace slicer_core
{

TextureFillPartitionReportData BuildTextureFillPartitionUnavailableReportData(const SliceConfig& config)
{
    TextureFillPartitionReportData report;
    report.enabled = IsGlobalTextureFillPartitionRequested(config);
    report.options.requestedWidthMm = config.texture.surface_shell.width_mm;
    report.options.widthStepMm = config.texture.surface_shell.width_step_mm;
    report.options.surfaceScope = config.texture.surface_shell.surface_scope;
    if (report.enabled)
    {
        report.issues.push_back(MakeValidationIssue(
            TextureFillPartitionErrorCodeName(
                TextureFillPartitionErrorCode::PartitionBackendUnavailable),
            ValidationSeverity::Error,
            "global 3D texture/fill partition backend is unavailable"));
    }
    return report;
}

Json BuildTextureFillPartitionReportSkeleton(const SliceConfig& config)
{
    const TextureFillPartitionReportData report =
        BuildTextureFillPartitionUnavailableReportData(config);
    const OpenVdbStatus openVdb = GetOpenVdbStatus();
    return Json::object({
        {"schema", "slicesoft.texture_fill_partition.12e.1"},
        {"packageProtocol", "p0.rgbwsv.2"},
        {"enabled", report.enabled},
        {"strategy", report.strategy},
        {"availability", report.availability},
        {"status", report.status},
        {"productionAcceptance", report.productionAcceptance},
        {"geometryMode", config.texture.surface_shell.geometry_mode},
        {"surfaceScope", report.options.surfaceScope},
        {"backend", report.backend},
        {"backendRole", report.backendRole},
        {"grid", Json::object({
             {"width", nullptr},
             {"height", nullptr},
             {"depth", nullptr},
             {"originXMm", nullptr},
             {"originYMm", nullptr},
             {"originZMm", nullptr},
             {"spacingXMm", nullptr},
             {"spacingYMm", nullptr},
             {"spacingZMm", nullptr},
         })},
        {"width",
         Json::object({
             {"requestedWidthMm", report.options.requestedWidthMm},
             {"widthStepMm", report.options.widthStepMm},
             {"baseMinimumWidthMm", report.options.baseMinimumWidthMm},
             {"classificationResolutionMm", nullptr},
             {"effectiveMinimumWidthMm", nullptr},
             {"effectiveWidthMm", nullptr},
             {"maxInteriorDistanceMm", nullptr},
             {"allTextureThresholdMm", nullptr},
             {"allTexture", false},
             {"quantizationErrorMm", nullptr},
             {"clamped", false},
         })},
        {"partition", Json::object({
             {"modelVoxels", 0},
             {"textureSurfaceVoxels", 0},
             {"modelFillVoxels", 0},
             {"overlapTextureFillVoxels", 0},
             {"unassignedModelVoxels", 0},
             {"textureOutsideModelVoxels", 0},
             {"modelFillOutsideModelVoxels", 0},
             {"modelPixels", nullptr},
             {"textureSurfacePixels", nullptr},
             {"modelFillPixels", nullptr},
             {"overlapTextureFillPixels", nullptr},
             {"unassignedModelPixels", nullptr},
             {"textureCoverageRatio", 0.0},
             {"modelFillCoverageRatio", 0.0},
             {"thinRegionMergedVoxels", 0},
             {"medialAxisTieCount", 0},
             {"partitionPass", false},
        })},
        {"textureTransfer", BuildTextureTransfer(nullptr)},
        {"diagnosticComposer", BuildDiagnosticComposer(nullptr)},
        {"closureLinkage", BuildClosureLinkage(nullptr)},
        {"rasterMapping", BuildRasterMapping(nullptr)},
        {"fullClosureLinkage", BuildFullClosureLinkage(nullptr)},
        {"performance", Json::object({
             {"preflightMs", nullptr},
             {"topologyMs", nullptr},
             {"levelSetMs", nullptr},
             {"gridSampleMs", nullptr},
             {"occupancyMs", nullptr},
             {"distanceMs", nullptr},
             {"partitionMs", nullptr},
             {"textureTransferMs", nullptr},
             {"rasterMappingMs", nullptr},
             {"fullClosureMs", nullptr},
             {"totalCoreMs", nullptr},
             {"gridVoxelCount", nullptr},
             {"maskBytes", nullptr},
             {"closestReferenceBytes", nullptr},
             {"occupancyQueryBytes", nullptr},
             {"nearestQueryBytes", nullptr},
             {"openVdbGridBytes", nullptr},
             {"workingSetBytes", nullptr},
             {"peakWorkingSetBytes", nullptr},
         })},
        {"queryStats", Json::object({
             {"occupancyQueryCount", 0},
             {"occupancyVisitedNodes", 0},
             {"occupancyTestedTriangles", 0},
             {"occupancyFallbackRayCount", 0},
             {"occupancyAmbiguousRayCount", 0},
             {"occupancyBoundaryPointCount", 0},
             {"sdfSampleCount", 0},
             {"sdfActiveSampleCount", 0},
             {"sdfBackgroundSampleCount", 0},
             {"nearestQueryCount", 0},
             {"nearestVisitedNodes", 0},
             {"nearestTestedTriangles", 0},
         })},
        {"layers", Json::array({})},
        {"issues", ValidationIssuesToJson(report.issues)},
        {"configSnapshot", Json::object({
             {"textureEnabled", config.texture.enabled},
             {"textureApplyMode", config.texture.apply_mode},
             {"geometryMode", config.texture.surface_shell.geometry_mode},
             {"widthMm", config.texture.surface_shell.width_mm},
             {"widthStepMm", config.texture.surface_shell.width_step_mm},
             {"minimumWidthPolicy", config.texture.surface_shell.minimum_width_policy},
             {"surfaceScope", config.texture.surface_shell.surface_scope},
             {"fullTextureAtModelLimit", config.texture.surface_shell.full_texture_at_model_limit},
             {"modelFillEnabled", config.model_fill.enabled},
             {"modelFillMaterial", config.model_fill.material},
             {"modelFillScope", config.model_fill.scope},
             {"modelFillValue", static_cast<int>(config.model_fill.value)},
             {"pixelPitchXMm",
              NullableDouble(config.output.dpi_x > 0, 25.4 / config.output.dpi_x)},
             {"pixelPitchYMm",
              NullableDouble(config.output.dpi_y > 0, 25.4 / config.output.dpi_y)},
             {"layerThicknessMm", config.output.layer_thickness_mm},
             {"openVdbCompiled", openVdb.compiled_with_openvdb},
             {"openVdbRuntimeAvailable", openVdb.runtime_available},
         })},
    });
}

Json BuildTextureFillPartitionReport(
    const SliceConfig& config,
    const GlobalTextureFillPartitionResult& result,
    const TextureFillPartitionConformanceResult* conformance,
    const TextureFillPartitionTextureTransferResult* transfer,
    const TextureFillPartitionDiagnosticComposerResult* composer,
    const TextureFillPartitionClosureAdapterResult* closure,
    const TextureFillPartitionRasterMappingResult* rasterMapping,
    const TextureFillPartitionFullClosureAdapterResult* fullClosure)
{
    Json::Object report;
    report["schema"] = "slicesoft.texture_fill_partition.12e.1";
    report["packageProtocol"] = "p0.rgbwsv.2";
    report["enabled"] = IsGlobalTextureFillPartitionRequested(config);
    report["strategy"] = "global_surface_shell";
    report["availability"] = result.available ? "available" : "unavailable";
    report["status"] = result.status;
    report["productionAcceptance"] = result.productionAcceptance;
    report["geometryMode"] = config.texture.surface_shell.geometry_mode;
    report["surfaceScope"] = result.options.surfaceScope;
    report["backend"] = result.backend;
    report["backendRole"] = result.backendRole;
    report["grid"] = BuildGrid(result);
    report["width"] = BuildWidth(result);
    report["partition"] = BuildPartition(result);
    report["textureTransfer"] = BuildTextureTransfer(transfer);
    report["diagnosticComposer"] = BuildDiagnosticComposer(composer);
    report["closureLinkage"] = BuildClosureLinkage(closure);
    report["rasterMapping"] = BuildRasterMapping(rasterMapping);
    report["fullClosureLinkage"] = BuildFullClosureLinkage(fullClosure);
    report["performance"] = BuildPerformance(
        result,
        transfer,
        rasterMapping,
        fullClosure);
    report["queryStats"] = BuildQueryStats(result);
    report["layers"] = result.available ? BuildLayers(result) : Json::array({});
    report["issues"] = ValidationIssuesToJson(result.issues);
    report["configSnapshot"] = BuildConfigSnapshot(config, result);
    if (conformance != nullptr)
    {
        report["conformance"] = BuildConformance(*conformance);
    }
    return Json{std::move(report)};
}

Json BuildTextureFillPartitionWidthSweepSummary(
    const TextureFillPartitionWidthSweepResult& sweep)
{
    Json::Array samples;
    samples.reserve(sweep.samples.size());
    for (const TextureFillPartitionWidthSweepSample& sample : sweep.samples)
    {
        samples.push_back(BuildSweepSample(sample));
    }
    return Json::object({
        {"backend", sweep.backend},
        {"backendRole", sweep.backendRole},
        {"availability", sweep.available ? "available" : "unavailable"},
        {"status", sweep.status},
        {"productionAcceptance", sweep.productionAcceptance},
        {"minimumWidthMm", sweep.minimumWidthMm},
        {"maximumWidthMm", sweep.maximumWidthMm},
        {"widthStepMm", sweep.widthStepMm},
        {"sampleCount", static_cast<std::uint64_t>(sweep.samples.size())},
        {"monotonic", sweep.monotonicPass},
        {"endpoint", sweep.endpointPass},
        {"totalCandidateCoreMs", sweep.totalCandidateCoreMs},
        {"samples", Json{std::move(samples)}},
        {"issues", ValidationIssuesToJson(sweep.issues)},
    });
}

}  // namespace slicer_core
