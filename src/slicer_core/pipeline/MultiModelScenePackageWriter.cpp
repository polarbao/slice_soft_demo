#include "slicer_core/pipeline/MultiModelScenePackageWriter.h"

#include "slicer_core/config.h"
#include "slicer_core/config/SlicePipelineConfig.h"
#include "slicer_core/reports/MultiModelSceneReport.h"
#include "slicer_core/reports/SceneCapabilitySummary.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace slicer_core
{
namespace
{

bool SameProtocol(
    const RgbwsvProtocol& first,
    const RgbwsvProtocol& second)
{
    return first.schema == second.schema
        && first.channel_order == second.channel_order
        && first.bit_depth == second.bit_depth
        && first.polarity == second.polarity
        && first.print_value == second.print_value
        && first.empty_value == second.empty_value;
}

void ValidateScenePackageRequest(
    const RgbwsvProductionPackageWriteRequest& request,
    const SceneLayerComposeResult& composition,
    const MultiModelScene& scene,
    const SceneCollisionResult& admission,
    const bool compositionAlreadyValidated = false)
{
    const std::string effectiveMode =
        SlicePipelineModeName(
            composition.effectivepipelinemode);
    if (!request.layers.empty()
        || request.scene.has_value()
        || (!compositionAlreadyValidated && !composition.IsValid())
        || composition.sceneid.empty()
        || composition.sceneid != scene.sceneid
        || composition.sceneid != admission.sceneid
        || composition.scenerevision != scene.scenerevision
        || composition.scenerevision
            != admission.sourcescenerevision
        || request.sourceFormat != "scene"
        || request.requestedPipelineMode != effectiveMode
        || request.effectivePipelineMode != effectiveMode
        || !SameProtocol(
            composition.protocol,
            CurrentRgbwsvProtocol())
        || request.grid.dpiX <= 0
        || request.grid.dpiY <= 0
        || !IsOutputPixelSizeConsistent(
            request.grid.dpiX,
            composition.grid.pitchxmm)
        || !IsOutputPixelSizeConsistent(
            request.grid.dpiY,
            composition.grid.pitchymm))
    {
        throw std::invalid_argument(
            "multi-model scene package request is invalid or stale");
    }
}

}  // namespace

RgbwsvProductionPackageWriteResult
WriteMultiModelSceneProductionPackage(
    RgbwsvProductionPackageWriteRequest request,
    SceneLayerComposeResult composition,
    const MultiModelScene& scene,
    const SceneCollisionResult& admission,
    const std::vector<SceneInstanceRaster>& instanceRasters,
    const std::filesystem::path& profileConfigPath)
{
    ValidateScenePackageRequest(
        request,
        composition,
        scene,
        admission);

    request.grid.widthPx = composition.grid.widthpx;
    request.grid.heightPx = composition.grid.heightpx;
    request.grid.layerCount = composition.grid.layercount;
    request.grid.pixelSizeXmm = composition.grid.pitchxmm;
    request.grid.pixelSizeYmm = composition.grid.pitchymm;
    request.grid.layerThicknessMm =
        composition.grid.layerthicknessmm;
    request.grid.originXmm = composition.grid.originxmm;
    request.grid.originYmm = composition.grid.originymm;
    request.grid.originZmm = composition.grid.originzmm;
    request.scene = BuildMultiModelSceneReport(
        scene,
        admission,
        composition,
        request.requestedPipelineMode,
        request.packageDir);
    const auto capabilitySummary = BuildSceneCapabilitySummary(
        scene, instanceRasters, profileConfigPath);
    if (capabilitySummary.has_value())
    {
        request.perinstance = capabilitySummary->perinstance;
        request.profileecho = capabilitySummary->profileecho;
    }
    request.layers = std::move(composition.layers);
    return WriteRgbwsvProductionPackage(request);
}

RgbwsvProductionPackageWriteResult
WriteValidatedMultiModelSceneProductionPackage(
    RgbwsvProductionPackageWriteRequest request,
    ValidatedSceneLayerComposeResult composition,
    const MultiModelScene& scene,
    const SceneCollisionResult& admission,
    const std::filesystem::path& profileConfigPath)
{
    if (!composition.IsValid())
    {
        throw std::invalid_argument(
            "validated multi-model scene composition is unavailable");
    }
    const SceneLayerComposeResult& value = composition.Value();
    ValidateScenePackageRequest(
        request,
        value,
        scene,
        admission,
        true);

    request.grid.widthPx = value.grid.widthpx;
    request.grid.heightPx = value.grid.heightpx;
    request.grid.layerCount = value.grid.layercount;
    request.grid.pixelSizeXmm = value.grid.pitchxmm;
    request.grid.pixelSizeYmm = value.grid.pitchymm;
    request.grid.layerThicknessMm = value.grid.layerthicknessmm;
    request.grid.originXmm = value.grid.originxmm;
    request.grid.originYmm = value.grid.originymm;
    request.grid.originZmm = value.grid.originzmm;
    request.scene = BuildValidatedMultiModelSceneReport(
        scene,
        admission,
        composition,
        request.requestedPipelineMode,
        request.packageDir);
    const auto capabilitySummary = BuildSceneCapabilitySummary(
        scene, value.statistics, profileConfigPath);
    if (capabilitySummary.has_value())
    {
        request.perinstance = capabilitySummary->perinstance;
        request.profileecho = capabilitySummary->profileecho;
    }

    SceneLayerComposeResult owned = std::move(composition).Release();
    request.layerStatistics = std::move(owned.layerstatistics);
    request.layers = std::move(owned.layers);
    return WriteRgbwsvProductionPackage(request);
}

}  // namespace slicer_core
