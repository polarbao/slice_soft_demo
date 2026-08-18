#include "slicer_core/pipeline/TranslatedSceneRasterReuse.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{

void Require(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

slicer_core::ModelInstance MakeInstance(
    const std::string& instanceId,
    const double x,
    const double y)
{
    slicer_core::ModelInstance instance;
    instance.instanceid = instanceId;
    instance.modelid = "model";
    instance.sourcetransformidentity = "source";
    instance.transform.translatexmm = x;
    instance.transform.translateymm = y;
    instance.transformrevision = 3U;
    instance.sourcebboxmm = {{0.0, 0.0, 0.0}, {2.0, 3.0, 1.0}};
    instance.effectivebboxmm = {
        {x, y, 0.0},
        {x + 2.0, y + 3.0, 1.0}};
    return instance;
}

slicer_core::SceneRasterIdentity MakeIdentity(
    const slicer_core::ModelInstance& instance)
{
    const slicer_core::ModelTransformHashResult hash =
        slicer_core::ComputeModelTransformHash(
            instance.transform,
            instance.sourcetransformidentity,
            instance.instanceid,
            instance.modelid);
    Require(hash.IsValid(), "fixture transform hash must be valid");

    slicer_core::SceneRasterIdentity identity;
    identity.sceneid = "scene";
    identity.modelid = instance.modelid;
    identity.instanceid = instance.instanceid;
    identity.scenerevision = 7U;
    identity.transformrevision = instance.transformrevision;
    identity.admittedtransformrevision =
        instance.transformrevision;
    identity.transformhash = hash.hash;
    identity.admittedtransformhash = hash.hash;
    identity.visible = true;
    identity.admitted = true;
    identity.effectivepipelinemode =
        slicer_core::SlicePipelineMode::Legacy;
    return identity;
}

slicer_core::SceneInstanceRaster MakePrototype(
    const slicer_core::ModelInstance& instance)
{
    const slicer_core::SceneRasterIdentity identity =
        MakeIdentity(instance);
    slicer_core::SceneInstanceRaster raster;
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
    raster.visible = true;
    raster.admitted = true;
    raster.effectivepipelinemode =
        slicer_core::SlicePipelineMode::Legacy;
    raster.protocol = slicer_core::FixedSceneRasterProtocol();
    raster.localgrid.widthpx = 2;
    raster.localgrid.heightpx = 1;
    raster.localgrid.layercount = 1;
    raster.localgrid.originxmm = instance.transform.translatexmm;
    raster.localgrid.originymm = instance.transform.translateymm;
    raster.localgrid.originzmm = 0.0;
    raster.localgrid.pitchxmm = 0.2;
    raster.localgrid.pitchymm = 0.2;
    raster.localgrid.layerthicknessmm = 0.1;

    slicer_core::SceneInstanceRasterLayer layer;
    layer.layerindex = 0;
    layer.zmm = 0.05;
    layer.output.layerIndex = 0;
    layer.output.zMm = 0.05;
    layer.output.widthPx = 2;
    layer.output.heightPx = 1;
    layer.output.channels = {
        0U, 255U, 255U, 255U, 255U, 255U,
        255U, 255U, 255U, 255U, 255U, 255U};
    layer.modelownership = {1U, 0U};
    layer.modelvarnishownership = {0U, 0U};
    layer.outervarnishownership = {0U, 0U};
    layer.supportownership = {0U, 0U};
    raster.layers.push_back(std::move(layer));
    return raster;
}

void ReusesPureTranslationAndRebindsIdentity()
{
    const slicer_core::ModelInstance prototypeInstance =
        MakeInstance("prototype", 1.0, 2.0);
    const slicer_core::SceneInstanceRaster prototype =
        MakePrototype(prototypeInstance);
    const slicer_core::ModelInstance target =
        MakeInstance("target", 21.0, 32.0);

    slicer_core::TranslatedSceneRasterReuseRequest request;
    request.prototype = &prototype;
    request.prototypeinstance = prototypeInstance;
    request.targetidentity = MakeIdentity(target);
    request.targetinstance = target;
    const slicer_core::SceneRasterAdapterResult result =
        slicer_core::ReuseTranslatedSceneRaster(request);

    Require(result.IsValid(), "pure XY translation should be reusable");
    Require(
        result.raster.instanceid == "target"
            && result.raster.transformhash
                == request.targetidentity.transformhash,
        "target identity should replace prototype identity");
    Require(
        result.raster.localgrid.originxmm == 21.0
            && result.raster.localgrid.originymm == 32.0,
        "local grid origin should follow target translation");
    Require(
        result.raster.layers.front().output.channels
            == prototype.layers.front().output.channels,
        "writer-ready bytes should remain unchanged");
    Require(
        prototype.instanceid == "prototype"
            && prototype.localgrid.originxmm == 1.0,
        "prototype should remain immutable");
}

void RejectsNonTranslationAndStaleIdentity()
{
    const slicer_core::ModelInstance prototypeInstance =
        MakeInstance("prototype", 0.0, 0.0);
    const slicer_core::SceneInstanceRaster prototype =
        MakePrototype(prototypeInstance);
    slicer_core::ModelInstance rotated =
        MakeInstance("rotated", 20.0, 0.0);
    rotated.transform.rotatezdeg = 90.0;

    slicer_core::TranslatedSceneRasterReuseRequest request;
    request.prototype = &prototype;
    request.prototypeinstance = prototypeInstance;
    request.targetidentity = MakeIdentity(rotated);
    request.targetinstance = rotated;
    const slicer_core::SceneRasterAdapterResult rotatedResult =
        slicer_core::ReuseTranslatedSceneRaster(request);
    Require(
        rotatedResult.error.has_value()
            && rotatedResult.error->code
                == slicer_core::SceneRasterErrorCode::ProducerFailed,
        "rotation should require a new producer run");

    slicer_core::ModelInstance lifted =
        MakeInstance("lifted", 20.0, 0.0);
    lifted.transform.translatezmm = 1.0;
    request.targetinstance = lifted;
    request.targetidentity = MakeIdentity(lifted);
    const slicer_core::SceneRasterAdapterResult liftedResult =
        slicer_core::ReuseTranslatedSceneRaster(request);
    Require(
        liftedResult.error.has_value()
            && liftedResult.error->code
                == slicer_core::SceneRasterErrorCode::ProducerFailed,
        "Z translation should require a new producer run");

    request.targetinstance = MakeInstance("stale", 20.0, 0.0);
    request.targetidentity = MakeIdentity(request.targetinstance);
    request.targetidentity.transformhash = "stale";
    const slicer_core::SceneRasterAdapterResult staleResult =
        slicer_core::ReuseTranslatedSceneRaster(request);
    Require(
        staleResult.error.has_value()
            && staleResult.error->code
                == slicer_core::SceneRasterErrorCode::
                    InstanceIdentityInvalid,
        "stale target identity should fail closed");
}

void EnforcesHalfPixelQuantizationBound()
{
    const slicer_core::ModelInstance prototypeInstance =
        MakeInstance("prototype", 0.0, 0.0);
    const slicer_core::SceneInstanceRaster prototype =
        MakePrototype(prototypeInstance);
    const slicer_core::ModelInstance target =
        MakeInstance("target", 0.101, 0.0);

    slicer_core::TranslatedSceneRasterReuseRequest request;
    request.prototype = &prototype;
    request.prototypeinstance = prototypeInstance;
    request.targetidentity = MakeIdentity(target);
    request.targetinstance = target;
    request.quantizationtolerance = 0.49;
    const slicer_core::SceneRasterAdapterResult result =
        slicer_core::ReuseTranslatedSceneRaster(request);
    Require(
        result.error.has_value()
            && result.error->code
                == slicer_core::SceneRasterErrorCode::
                    OffsetNotIntegral,
        "translation beyond configured quantization tolerance should fail");
}

}  // namespace

int main()
{
    ReusesPureTranslationAndRebindsIdentity();
    RejectsNonTranslationAndStaleIdentity();
    EnforcesHalfPixelQuantizationBound();
    std::cout << "translated_scene_raster_reuse_unit_tests passed\n";
    return 0;
}
