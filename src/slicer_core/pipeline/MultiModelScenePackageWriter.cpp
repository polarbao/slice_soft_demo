#include "slicer_core/pipeline/MultiModelScenePackageWriter.h"

#include "slicer_core/config.h"
#include "slicer_core/config/SlicePipelineConfig.h"
#include "slicer_core/reports/MultiModelSceneReport.h"

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
    const SceneCollisionResult& admission)
{
    const std::string effectiveMode =
        SlicePipelineModeName(
            composition.effectivepipelinemode);
    if (!request.layers.empty()
        || request.scene.has_value()
        || !composition.IsValid()
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
    const SceneCollisionResult& admission)
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
    request.layers = std::move(composition.layers);
    return WriteRgbwsvProductionPackage(request);
}

}  // namespace slicer_core
