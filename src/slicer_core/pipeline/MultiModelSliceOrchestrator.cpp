#include "slicer_core/pipeline/MultiModelSliceOrchestrator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>

namespace slicer_core
{
namespace
{

SceneLayerComposeResult BlockOrchestrator(
    const MultiModelLayerComposeRequest& request,
    const SceneRasterErrorCode code,
    const std::string& field,
    const std::string& message,
    const SceneInstanceRaster* instance = nullptr)
{
    SceneLayerComposeResult result;
    result.sceneid = request.admission.sceneid;
    result.scenerevision = request.currentscenerevision;
    result.protocol = FixedSceneRasterProtocol();
    result.effectivepipelinemode =
        request.effectivepipelinemode;
    SceneRasterError error;
    error.code = code;
    error.sceneid = request.admission.sceneid;
    error.field = field;
    error.message = message;
    if (instance != nullptr)
    {
        error.modelid = instance->modelid;
        error.instanceid = instance->instanceid;
    }
    result.error = std::move(error);
    return result;
}

bool SameSampling(
    const double first,
    const double second,
    const double tolerance)
{
    return std::isfinite(first)
        && std::isfinite(second)
        && std::abs(first - second)
            <= std::max(
                tolerance,
                std::max(std::abs(first), std::abs(second))
                    * 1.0e-9);
}

bool QuantizeOffset(
    const double origin,
    const double globalOrigin,
    const double spacing,
    const double tolerance,
    int& offset)
{
    const double raw = (origin - globalOrigin) / spacing;
    const double rounded = std::round(raw);
    if (!std::isfinite(raw)
        || std::abs(raw - rounded) > tolerance
        || rounded < 0.0
        || rounded
            > static_cast<double>(
                std::numeric_limits<int>::max()))
    {
        return false;
    }
    offset = static_cast<int>(rounded);
    return true;
}

bool IsAdmittedInstance(
    const SceneCollisionInstanceResult& admission,
    const SceneInstanceRaster& raster)
{
    return admission.modelid == raster.modelid
        && admission.instanceid == raster.instanceid
        && admission.visible
        && !admission.skippedhidden
        && admission.admissionstatus
            == SceneInstanceAdmissionStatus::Admitted
        && admission.boundsvalid
        && admission.inbounds
        && admission.errors.empty();
}

bool MatchesTransformEvidence(
    const SceneCollisionInstanceResult& admission,
    const SceneInstanceRaster& raster)
{
    return admission.transformrevision == raster.transformrevision
        && admission.transformrevision
            == raster.admittedtransformrevision
        && !admission.transformhash.empty()
        && admission.transformhash == raster.transformhash
        && admission.transformhash
            == raster.admittedtransformhash;
}

bool ExtendExtent(
    const int offset,
    const int size,
    int& extent)
{
    if (offset < 0
        || size <= 0
        || offset > std::numeric_limits<int>::max() - size)
    {
        return false;
    }
    extent = std::max(extent, offset + size);
    return true;
}

}  // namespace

SceneLayerComposeResult ComposeAdmittedSceneRasters(
    const MultiModelLayerComposeRequest& request)
{
    if (!request.admission.IsValid()
        || request.admission.scenestatus
            != SceneCollisionStatus::Passed
        || (!request.admission.functionalallowed
            && !request.admission.productionallowed)
        || request.admission.sceneid.empty())
    {
        return BlockOrchestrator(
            request,
            SceneRasterErrorCode::AdmissionRequired,
            "admission",
            "scene raster requires passing functional or production admission");
    }
    if (request.currentscenerevision
        != request.admission.sourcescenerevision)
    {
        return BlockOrchestrator(
            request,
            SceneRasterErrorCode::RevisionStale,
            "currentscenerevision",
            "scene revision changed after collision admission");
    }

    std::unordered_map<std::string, const SceneCollisionInstanceResult*>
        admittedById;
    for (const SceneCollisionInstanceResult& instance :
         request.admission.instances)
    {
        if (instance.visible)
        {
            const auto inserted =
                admittedById.emplace(
                    instance.instanceid,
                    &instance);
            if (instance.instanceid.empty()
                || !inserted.second)
            {
                return BlockOrchestrator(
                    request,
                    SceneRasterErrorCode::InstanceIdentityInvalid,
                    "admission.instances",
                    "visible admission instance identities must be non-empty and unique");
            }
        }
    }

    std::vector<const SceneInstanceRaster*> visible;
    visible.reserve(request.instances.size());
    for (const SceneInstanceRaster& raster : request.instances)
    {
        if (!raster.visible)
        {
            continue;
        }
        const auto admitted = admittedById.find(raster.instanceid);
        if (admitted == admittedById.end()
            || !IsAdmittedInstance(*admitted->second, raster))
        {
            return BlockOrchestrator(
                request,
                SceneRasterErrorCode::AdmissionRequired,
                "instances.admission",
                "every visible raster must match passing per-instance admission",
                &raster);
        }
        if (!MatchesTransformEvidence(
                *admitted->second,
                raster))
        {
            return BlockOrchestrator(
                request,
                SceneRasterErrorCode::RevisionStale,
                "instances.transform",
                "instance raster transform changed after collision admission",
                &raster);
        }
        if (raster.effectivepipelinemode
            != request.effectivepipelinemode)
        {
            return BlockOrchestrator(
                request,
                SceneRasterErrorCode::PipelineModeMismatch,
                "instances.effectivepipelinemode",
                "one scene cannot mix Legacy and Global rasters",
                &raster);
        }
        visible.push_back(&raster);
    }
    if (visible.empty()
        || visible.size() != admittedById.size())
    {
        return BlockOrchestrator(
            request,
            SceneRasterErrorCode::AdmissionRequired,
            "instances",
            "scene raster requires every admitted visible instance and at least one instance");
    }

    const SceneRasterGrid& reference = visible.front()->localgrid;
    if (!reference.IsValid())
    {
        return BlockOrchestrator(
            request,
            SceneRasterErrorCode::GridInvalid,
            "instances.localgrid",
            "visible instance local grid is invalid",
            visible.front());
    }

    SceneRasterGrid global;
    global.originxmm = reference.originxmm;
    global.originymm = reference.originymm;
    global.originzmm = 0.0;
    global.pitchxmm = reference.pitchxmm;
    global.pitchymm = reference.pitchymm;
    global.layerthicknessmm = reference.layerthicknessmm;
    for (const SceneInstanceRaster* raster : visible)
    {
        if (!raster->localgrid.IsValid()
            || !SameSampling(
                raster->localgrid.pitchxmm,
                reference.pitchxmm,
                request.quantizationtolerance)
            || !SameSampling(
                raster->localgrid.pitchymm,
                reference.pitchymm,
                request.quantizationtolerance)
            || !SameSampling(
                raster->localgrid.layerthicknessmm,
                reference.layerthicknessmm,
                request.quantizationtolerance))
        {
            return BlockOrchestrator(
                request,
                SceneRasterErrorCode::ResolutionMismatch,
                "instances.localgrid.resolution",
                "all visible rasters must share dpiX, dpiY, and layer thickness",
                raster);
        }
        global.originxmm =
            std::min(global.originxmm, raster->localgrid.originxmm);
        global.originymm =
            std::min(global.originymm, raster->localgrid.originymm);
    }

    for (const SceneInstanceRaster* raster : visible)
    {
        int offsetX{0};
        int offsetY{0};
        int offsetZ{0};
        if (!QuantizeOffset(
                raster->localgrid.originxmm,
                global.originxmm,
                global.pitchxmm,
                request.quantizationtolerance,
                offsetX)
            || !QuantizeOffset(
                raster->localgrid.originymm,
                global.originymm,
                global.pitchymm,
                request.quantizationtolerance,
                offsetY)
            || !QuantizeOffset(
                raster->localgrid.originzmm,
                global.originzmm,
                global.layerthicknessmm,
                request.quantizationtolerance,
                offsetZ))
        {
            return BlockOrchestrator(
                request,
                SceneRasterErrorCode::OffsetNotIntegral,
                "instances.localgrid.origin",
                "local grid origin must map to integral global offsets",
                raster);
        }
        if (!ExtendExtent(
                offsetX,
                raster->localgrid.widthpx,
                global.widthpx)
            || !ExtendExtent(
                offsetY,
                raster->localgrid.heightpx,
                global.heightpx)
            || !ExtendExtent(
                offsetZ,
                raster->localgrid.layercount,
                global.layercount))
        {
            return BlockOrchestrator(
                request,
                SceneRasterErrorCode::GridInvalid,
                "globalgrid.extent",
                "aligned visible raster union exceeds supported integer extents",
                raster);
        }
    }
    if (!global.IsValid())
    {
        return BlockOrchestrator(
            request,
            SceneRasterErrorCode::GridInvalid,
            "globalgrid",
            "aligned visible raster union is invalid");
    }

    SceneLayerComposeRequest compose;
    compose.sceneid = request.admission.sceneid;
    compose.currentscenerevision =
        request.currentscenerevision;
    compose.expectedscenerevision =
        request.admission.sourcescenerevision;
    compose.admissionpassed = true;
    compose.effectivepipelinemode =
        request.effectivepipelinemode;
    compose.globalgrid = global;
    compose.protocol = FixedSceneRasterProtocol();
    compose.instances = request.instances;
    compose.quantizationtolerance =
        request.quantizationtolerance;
    return ComposeSceneLayers(compose);
}

}  // namespace slicer_core
