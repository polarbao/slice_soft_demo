#include "slicer_core/pipeline/LegacySceneLayerAdapter.h"

#include "slicer_core/slicer.h"

#include <exception>
#include <string>
#include <utility>

namespace slicer_core
{
namespace
{

void BlockLegacyAdapter(
    SceneRasterAdapterResult& result,
    const LegacySceneLayerAdapterRequest& request,
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

bool HasValidIdentity(const SceneRasterIdentity& identity)
{
    return !identity.sceneid.empty()
        && !identity.modelid.empty()
        && !identity.instanceid.empty()
        && identity.visible
        && identity.admitted
        && identity.transformrevision
            == identity.admittedtransformrevision
        && !identity.transformhash.empty()
        && identity.transformhash
            == identity.admittedtransformhash;
}

bool MatchesInstance(
    const LegacySceneLayerAdapterRequest& request)
{
    const ModelTransformHashResult transformHash =
        ComputeModelTransformHash(
            request.instance.transform,
            request.instance.sourcetransformidentity,
            request.instance.instanceid,
            request.instance.modelid);
    return ValidateModelInstance(request.instance) == std::nullopt
        && request.instance.instanceid
            == request.identity.instanceid
        && request.instance.modelid == request.identity.modelid
        && request.instance.transformrevision
            == request.identity.transformrevision
        && transformHash.IsValid()
        && transformHash.hash == request.identity.transformhash;
}

SceneInstanceRaster BuildIdentityRaster(
    const SceneRasterIdentity& identity)
{
    SceneInstanceRaster raster;
    raster.sceneid = identity.sceneid;
    raster.modelid = identity.modelid;
    raster.instanceid = identity.instanceid;
    raster.scenerevision = identity.scenerevision;
    raster.transformrevision = identity.transformrevision;
    raster.admittedtransformrevision =
        identity.admittedtransformrevision;
    raster.transformhash = identity.transformhash;
    raster.admittedtransformhash =
        identity.admittedtransformhash;
    raster.visible = identity.visible;
    raster.admitted = identity.admitted;
    raster.effectivepipelinemode =
        identity.effectivepipelinemode;
    raster.protocol = FixedSceneRasterProtocol();
    return raster;
}

}  // namespace

SceneRasterAdapterResult AdaptLegacySceneLayers(
    const LegacySceneLayerAdapterRequest& request)
{
    SceneRasterAdapterResult result;
    result.raster = BuildIdentityRaster(request.identity);
    if (request.configpath.empty()
        || !HasValidIdentity(request.identity)
        || !MatchesInstance(request))
    {
        BlockLegacyAdapter(
            result,
            request,
            SceneRasterErrorCode::InstanceIdentityInvalid,
            "request",
            "Legacy scene-layer adapter requires an admitted visible identity and config path");
        return result;
    }
    if (request.identity.effectivepipelinemode
        != SlicePipelineMode::Legacy)
    {
        BlockLegacyAdapter(
            result,
            request,
            SceneRasterErrorCode::PipelineModeMismatch,
            "identity.effectivepipelinemode",
            "Legacy scene-layer adapter only accepts legacy mode");
        return result;
    }

    SliceRunOptions options;
    options.write_tiff_layers = false;
    options.write_preview_files = false;
    options.write_reports = false;
    options.instanceoverride = request.instance;
    options.modelreportoverride = request.modelreportoverride;
    options.progress_callback = request.progresscallback;
    if (!request.modelpathoverride.empty())
    {
        options.inputoverride = SliceRunInputOverride{
            request.modelpathoverride,
            request.modelformatoverride};
    }
    bool gridReceived{false};
    options.gridcallback =
        [&result, &gridReceived](const SliceRunRasterGrid& grid)
        {
            result.raster.localgrid.widthpx = grid.widthpx;
            result.raster.localgrid.heightpx = grid.heightpx;
            result.raster.localgrid.layercount = grid.layercount;
            result.raster.localgrid.originxmm = grid.originxmm;
            result.raster.localgrid.originymm = grid.originymm;
            result.raster.localgrid.originzmm = grid.originzmm;
            result.raster.localgrid.pitchxmm = grid.pixelsizexmm;
            result.raster.localgrid.pitchymm = grid.pixelsizeymm;
            result.raster.localgrid.layerthicknessmm =
                grid.layerthicknessmm;
            gridReceived = true;
        };
    options.layercallback =
        [&result](
            const RgbwsvProductionLayer& output,
            const MaterialClosureSemanticLayerInput& semantic)
        {
            SceneInstanceRasterLayer layer;
            layer.layerindex = output.layerIndex;
            layer.zmm = output.zMm;
            layer.output = output;
            layer.modelownership = semantic.modelMaterialMask;
            layer.modelvarnishownership =
                BuildModelVarnishOwnership(
                    output,
                    layer.modelownership,
                    result.raster.protocol);
            layer.outervarnishownership =
                semantic.outerVarnishShellMask;
            layer.supportownership = semantic.supportFillMask;
            result.raster.layers.push_back(std::move(layer));
        };

    try
    {
        const SliceRunResult run =
            run_slicer(request.configpath, options);
        if (run.effective_pipeline_mode != "legacy")
        {
            BlockLegacyAdapter(
                result,
                request,
                SceneRasterErrorCode::PipelineModeMismatch,
                "run.effective_pipeline_mode",
                "Legacy adapter producer returned a non-Legacy effective mode");
            return result;
        }
    }
    catch (const std::exception& exception)
    {
        BlockLegacyAdapter(
            result,
            request,
            SceneRasterErrorCode::ProducerFailed,
            "configpath",
            exception.what());
        return result;
    }

    if (!gridReceived
        || !result.raster.localgrid.IsValid()
        || result.raster.layers.size()
            != static_cast<std::size_t>(
                result.raster.localgrid.layercount))
    {
        BlockLegacyAdapter(
            result,
            request,
            SceneRasterErrorCode::LayerSequenceMismatch,
            "producer.layers",
            "Legacy producer did not return one complete aligned layer list");
        return result;
    }

    result.available = true;
    result.status = "ready_for_composer";
    return result;
}

}  // namespace slicer_core
