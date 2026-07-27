#include "slicer_core/pipeline/GlobalSceneLayerAdapter.h"

#include <cstddef>
#include <exception>
#include <string>
#include <utility>

namespace slicer_core
{
namespace
{

void BlockGlobalAdapter(
    SceneRasterAdapterResult& result,
    const GlobalSceneLayerAdapterRequest& request,
    const SceneRasterErrorCode code,
    const std::string& field,
    const std::string& message)
{
    result.available = false;
    result.status = "blocked";
    result.raster.layers.clear();
    SceneRasterError error;
    error.code = code;
    error.sceneid = request.identity.sceneid;
    error.modelid = request.identity.modelid;
    error.instanceid = request.identity.instanceid;
    error.field = field;
    error.message = message;
    result.error = std::move(error);
}

SceneInstanceRaster BuildIdentityRaster(
    const GlobalSceneLayerAdapterRequest& request)
{
    SceneInstanceRaster raster;
    raster.sceneid = request.identity.sceneid;
    raster.modelid = request.identity.modelid;
    raster.instanceid = request.identity.instanceid;
    raster.scenerevision = request.identity.scenerevision;
    raster.transformrevision = request.identity.transformrevision;
    raster.admittedtransformrevision =
        request.identity.admittedtransformrevision;
    raster.transformhash = request.identity.transformhash;
    raster.admittedtransformhash =
        request.identity.admittedtransformhash;
    raster.visible = request.identity.visible;
    raster.admitted = request.identity.admitted;
    raster.effectivepipelinemode =
        request.identity.effectivepipelinemode;
    raster.localgrid = request.localgrid;
    raster.protocol = FixedSceneRasterProtocol();
    return raster;
}

}  // namespace

SceneRasterAdapterResult AdaptGlobalSceneLayers(
    const GlobalSceneLayerAdapterRequest& request)
{
    SceneRasterAdapterResult result;
    result.raster = BuildIdentityRaster(request);
    if (request.source == nullptr
        || request.identity.sceneid.empty()
        || request.identity.modelid.empty()
        || request.identity.instanceid.empty()
        || !request.identity.visible
        || !request.identity.admitted
        || request.identity.transformrevision
            != request.identity.admittedtransformrevision
        || request.identity.transformhash.empty()
        || request.identity.transformhash
            != request.identity.admittedtransformhash)
    {
        BlockGlobalAdapter(
            result,
            request,
            SceneRasterErrorCode::InstanceIdentityInvalid,
            "request",
            "Global scene-layer adapter requires admitted identity, grid, and source layers");
        return result;
    }
    if (request.identity.effectivepipelinemode
        != SlicePipelineMode::GlobalSurfaceShell)
    {
        BlockGlobalAdapter(
            result,
            request,
            SceneRasterErrorCode::PipelineModeMismatch,
            "identity.effectivepipelinemode",
            "Global scene-layer adapter only accepts global_surface_shell mode");
        return result;
    }
    const GlobalSurfaceShellProductionLayerAdapterResult& source =
        *request.source;
    if (!request.localgrid.IsValid()
        || !source.available
        || !source.fullClosurePass
        || source.productionOutputWritten
        || source.status != "ready_for_writer"
        || source.widthPx != request.localgrid.widthpx
        || source.heightPx != request.localgrid.heightpx
        || source.layerCount != request.localgrid.layercount
        || source.layers.size()
            != static_cast<std::size_t>(
                request.localgrid.layercount))
    {
        BlockGlobalAdapter(
            result,
            request,
            SceneRasterErrorCode::LayerSequenceMismatch,
            "source.layers",
            "Global source must be complete, closure-passing, and aligned to the local grid");
        return result;
    }
    if (source.protocol.schema != result.raster.protocol.schema
        || source.protocol.channel_order
            != result.raster.protocol.channel_order
        || source.protocol.bit_depth
            != result.raster.protocol.bit_depth
        || source.protocol.polarity
            != result.raster.protocol.polarity
        || source.protocol.print_value
            != result.raster.protocol.print_value
        || source.protocol.empty_value
            != result.raster.protocol.empty_value)
    {
        BlockGlobalAdapter(
            result,
            request,
            SceneRasterErrorCode::ProtocolMismatch,
            "source.protocol",
            "Global source protocol must remain p0.rgbwsv.2 RGBWSV uint8 black_is_print");
        return result;
    }

    result.raster.layers.reserve(source.layers.size());
    for (const GlobalSurfaceShellProductionLayer& sourceLayer :
         source.layers)
    {
        SceneInstanceRasterLayer layer;
        layer.layerindex = sourceLayer.output.layerIndex;
        layer.zmm = sourceLayer.output.zMm;
        layer.output = sourceLayer.output;
        layer.modelownership =
            sourceLayer.semantic.modelMaterialMask;
        try
        {
            layer.modelvarnishownership =
                BuildModelVarnishOwnership(
                    sourceLayer.output,
                    layer.modelownership,
                    result.raster.protocol);
        }
        catch (const std::exception& exception)
        {
            BlockGlobalAdapter(
                result,
                request,
                SceneRasterErrorCode::LayerSizeInvalid,
                "source.layers",
                exception.what());
            return result;
        }
        layer.outervarnishownership =
            sourceLayer.semantic.outerVarnishShellMask;
        layer.supportownership =
            sourceLayer.semantic.supportFillMask;
        result.raster.layers.push_back(std::move(layer));
    }

    result.available = true;
    result.status = "ready_for_composer";
    if (!result.IsValid())
    {
        BlockGlobalAdapter(
            result,
            request,
            SceneRasterErrorCode::LayerSizeInvalid,
            "source.layers",
            "Global source layer dimensions, sequence, bytes, or masks are invalid");
    }
    return result;
}

}  // namespace slicer_core
