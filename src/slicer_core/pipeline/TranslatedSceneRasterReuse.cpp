#include "slicer_core/pipeline/TranslatedSceneRasterReuse.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace slicer_core
{
namespace
{

void BlockReuse(
    SceneRasterAdapterResult& result,
    const TranslatedSceneRasterReuseRequest& request,
    const SceneRasterErrorCode code,
    const std::string& field,
    const std::string& message)
{
    result.available = false;
    result.status = "blocked";
    result.raster.layers.clear();
    SceneRasterError error;
    error.code = code;
    error.sceneid = request.targetidentity.sceneid;
    error.modelid = request.targetidentity.modelid;
    error.instanceid = request.targetidentity.instanceid;
    error.field = field;
    error.message = message;
    result.error = std::move(error);
}

bool SameNonTranslationTransform(
    const ModelTransform& first,
    const ModelTransform& second)
{
    return first.rotatezdeg == second.rotatezdeg
        && first.uniformscale == second.uniformscale
        && first.mirrorx == second.mirrorx
        && first.mirrory == second.mirrory;
}

bool HasValidTargetIdentity(
    const TranslatedSceneRasterReuseRequest& request)
{
    const ModelTransformHashResult targetHash =
        ComputeModelTransformHash(
            request.targetinstance.transform,
            request.targetinstance.sourcetransformidentity,
            request.targetinstance.instanceid,
            request.targetinstance.modelid);
    return targetHash.IsValid()
        && !request.targetidentity.sceneid.empty()
        && request.targetidentity.modelid
            == request.targetinstance.modelid
        && request.targetidentity.instanceid
            == request.targetinstance.instanceid
        && request.targetidentity.transformrevision
            == request.targetinstance.transformrevision
        && request.targetidentity.transformrevision
            == request.targetidentity.admittedtransformrevision
        && request.targetidentity.transformhash == targetHash.hash
        && request.targetidentity.admittedtransformhash
            == targetHash.hash
        && request.targetidentity.visible
        && request.targetidentity.admitted;
}

bool HasValidPrototypeIdentity(
    const TranslatedSceneRasterReuseRequest& request)
{
    if (request.prototype == nullptr)
    {
        return false;
    }
    const SceneInstanceRaster& prototype = *request.prototype;
    const ModelTransformHashResult prototypeHash =
        ComputeModelTransformHash(
            request.prototypeinstance.transform,
            request.prototypeinstance.sourcetransformidentity,
            request.prototypeinstance.instanceid,
            request.prototypeinstance.modelid);
    return prototypeHash.IsValid()
        && prototype.sceneid == request.targetidentity.sceneid
        && prototype.modelid == request.prototypeinstance.modelid
        && prototype.instanceid
            == request.prototypeinstance.instanceid
        && prototype.transformrevision
            == request.prototypeinstance.transformrevision
        && prototype.transformrevision
            == prototype.admittedtransformrevision
        && prototype.transformhash == prototypeHash.hash
        && prototype.admittedtransformhash == prototypeHash.hash
        && prototype.visible
        && prototype.admitted
        && prototype.localgrid.IsValid()
        && !prototype.layers.empty();
}

bool IsQuantizable(
    const double delta,
    const double pitch,
    const double tolerance)
{
    if (!std::isfinite(delta)
        || !std::isfinite(pitch)
        || !(pitch > 0.0)
        || !std::isfinite(tolerance)
        || tolerance < 0.0
        || tolerance > 0.500001)
    {
        return false;
    }
    const double pixels = delta / pitch;
    return std::isfinite(pixels)
        && std::abs(pixels - std::round(pixels)) <= tolerance;
}

}  // namespace

SceneRasterAdapterResult ReuseTranslatedSceneRaster(
    const TranslatedSceneRasterReuseRequest& request)
{
    SceneRasterAdapterResult result;
    if (!HasValidPrototypeIdentity(request)
        || !HasValidTargetIdentity(request)
        || request.prototypeinstance.modelid
            != request.targetinstance.modelid
        || request.prototypeinstance.sourcetransformidentity
            != request.targetinstance.sourcetransformidentity)
    {
        BlockReuse(
            result,
            request,
            SceneRasterErrorCode::InstanceIdentityInvalid,
            "request.identity",
            "translated raster reuse requires fresh source and target identities for one model source");
        return result;
    }
    if (!SameNonTranslationTransform(
            request.prototypeinstance.transform,
            request.targetinstance.transform))
    {
        BlockReuse(
            result,
            request,
            SceneRasterErrorCode::ProducerFailed,
            "targetinstance.transform",
            "rotation, scale, or mirror changes require a new local layer producer run");
        return result;
    }

    const double deltaX =
        request.targetinstance.transform.translatexmm
        - request.prototypeinstance.transform.translatexmm;
    const double deltaY =
        request.targetinstance.transform.translateymm
        - request.prototypeinstance.transform.translateymm;
    if (!IsQuantizable(
            deltaX,
            request.prototype->localgrid.pitchxmm,
            request.quantizationtolerance)
        || !IsQuantizable(
            deltaY,
            request.prototype->localgrid.pitchymm,
            request.quantizationtolerance))
    {
        BlockReuse(
            result,
            request,
            SceneRasterErrorCode::OffsetNotIntegral,
            "targetinstance.transform.translation",
            "translated raster origin must map within one-half pixel of the shared scene grid");
        return result;
    }

    result.raster = *request.prototype;
    result.raster.sceneid = request.targetidentity.sceneid;
    result.raster.modelid = request.targetidentity.modelid;
    result.raster.instanceid = request.targetidentity.instanceid;
    result.raster.scenerevision =
        request.targetidentity.scenerevision;
    result.raster.transformrevision =
        request.targetidentity.transformrevision;
    result.raster.admittedtransformrevision =
        request.targetidentity.admittedtransformrevision;
    result.raster.transformhash =
        request.targetidentity.transformhash;
    result.raster.admittedtransformhash =
        request.targetidentity.admittedtransformhash;
    result.raster.visible = request.targetidentity.visible;
    result.raster.admitted = request.targetidentity.admitted;
    result.raster.effectivepipelinemode =
        request.targetidentity.effectivepipelinemode;
    result.raster.localgrid.originxmm += deltaX;
    result.raster.localgrid.originymm += deltaY;
    result.available = true;
    result.status = "ready_for_composer";
    if (!result.IsValid())
    {
        BlockReuse(
            result,
            request,
            SceneRasterErrorCode::LayerSequenceMismatch,
            "prototype.layers",
            "translated raster reuse produced an invalid layer sequence");
    }
    return result;
}

}  // namespace slicer_core

