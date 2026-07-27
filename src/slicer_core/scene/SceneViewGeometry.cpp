#include "slicer_core/scene/SceneViewGeometry.h"

#include "slicer_core/geometry/TransformedModelAdapter.h"
#include "slicer_core/system/Sha256.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string_view>
#include <utility>

namespace slicer_core
{
namespace
{

SceneViewGeometryError MakeError(
    const SceneViewGeometryErrorCode code,
    const SceneViewGeometryRequest& request,
    const std::string_view field,
    const std::string_view message)
{
    SceneViewGeometryError error;
    error.code = code;
    error.sceneid = request.sceneid;
    error.modelid = request.instance.modelid;
    error.instanceid = request.instance.instanceid;
    error.field = field;
    error.message = message;
    return error;
}

bool IsFinite(const SceneViewPoint& point)
{
    return std::isfinite(point.xmm) && std::isfinite(point.ymm);
}

std::string ComputeGeometryHash(const SceneViewGeometry& geometry)
{
    std::ostringstream payload;
    payload.imbue(std::locale::classic());
    payload << std::setprecision(17);
    payload << "slicesoft.scene_view_geometry.13a.1\n";
    payload << geometry.sceneid.size() << ':' << geometry.sceneid << '\n';
    payload << geometry.modelid.size() << ':' << geometry.modelid << '\n';
    payload << geometry.instanceid.size() << ':' << geometry.instanceid
            << '\n';
    payload << geometry.scenerevision << '\n';
    payload << geometry.transformrevision << '\n';
    payload << geometry.triangles.size() << '\n';
    payload << geometry.texturedtrianglecount << '\n';
    payload << geometry.materialcount << '\n';
    for (const SceneViewTriangle& triangle : geometry.triangles)
    {
        payload << triangle.a.xmm << ',' << triangle.a.ymm << ';'
                << triangle.b.xmm << ',' << triangle.b.ymm << ';'
                << triangle.c.xmm << ',' << triangle.c.ymm << '\n';
    }
    return ComputeSha256(payload.str());
}

}  // namespace

bool SceneViewGeometryResult::IsValid() const
{
    return !error.has_value();
}

std::string_view SceneViewGeometryErrorCodeName(
    const SceneViewGeometryErrorCode code)
{
    switch (code)
    {
    case SceneViewGeometryErrorCode::None:
        return "NONE";
    case SceneViewGeometryErrorCode::SceneIdEmpty:
        return "SCENE_VIEW_SCENE_ID_EMPTY";
    case SceneViewGeometryErrorCode::ModelIdEmpty:
        return "SCENE_VIEW_MODEL_ID_EMPTY";
    case SceneViewGeometryErrorCode::InstanceIdEmpty:
        return "SCENE_VIEW_INSTANCE_ID_EMPTY";
    case SceneViewGeometryErrorCode::RevisionStale:
        return "SCENE_VIEW_REVISION_STALE";
    case SceneViewGeometryErrorCode::SourceGeometryInvalid:
        return "SCENE_VIEW_SOURCE_GEOMETRY_INVALID";
    case SceneViewGeometryErrorCode::GeometryNonFinite:
        return "SCENE_VIEW_GEOMETRY_NON_FINITE";
    case SceneViewGeometryErrorCode::TransformInvalid:
        return "SCENE_VIEW_TRANSFORM_INVALID";
    }
    return "SCENE_VIEW_UNKNOWN";
}

SceneViewGeometryResult BuildSceneViewGeometry(
    const SceneModel& source,
    const SceneViewGeometryRequest& request)
{
    if (request.sceneid.empty())
    {
        return {
            {},
            MakeError(
                SceneViewGeometryErrorCode::SceneIdEmpty,
                request,
                "sceneid",
                "scene view requires a stable scene id")};
    }
    if (request.instance.modelid.empty())
    {
        return {
            {},
            MakeError(
                SceneViewGeometryErrorCode::ModelIdEmpty,
                request,
                "modelid",
                "scene view requires a stable model id")};
    }
    if (request.instance.instanceid.empty())
    {
        return {
            {},
            MakeError(
                SceneViewGeometryErrorCode::InstanceIdEmpty,
                request,
                "instanceid",
                "scene view requires a stable instance id")};
    }
    if ((request.expectedscenerevision.has_value()
         && request.expectedscenerevision.value()
             != request.scenerevision)
        || (request.expectedtransformrevision.has_value()
            && request.expectedtransformrevision.value()
                != request.instance.transformrevision))
    {
        return {
            {},
            MakeError(
                SceneViewGeometryErrorCode::RevisionStale,
                request,
                "revision",
                "scene or transform revision changed before projection")};
    }
    if (source.triangles.empty())
    {
        return {
            {},
            MakeError(
                SceneViewGeometryErrorCode::SourceGeometryInvalid,
                request,
                "source.triangles",
                "source model geometry must not be empty")};
    }

    const ModelTransformHashResult transformHash =
        ComputeModelTransformHash(
            request.instance.transform,
            request.instance.sourcetransformidentity,
            request.instance.instanceid,
            request.instance.modelid);
    if (!transformHash.IsValid())
    {
        return {
            {},
            MakeError(
                SceneViewGeometryErrorCode::TransformInvalid,
                request,
                transformHash.error->field,
                transformHash.error->message)};
    }

    TransformedModelResult transformed =
        AdaptTransformedModel(source, request.instance);
    if (!transformed.IsValid())
    {
        const SceneViewGeometryErrorCode code =
            transformed.error->code == ModelTransformErrorCode::NonFinite
            ? SceneViewGeometryErrorCode::GeometryNonFinite
            : SceneViewGeometryErrorCode::TransformInvalid;
        return {
            {},
            MakeError(
                code,
                request,
                transformed.error->field,
                transformed.error->message)};
    }

    SceneViewGeometry geometry;
    geometry.sceneid = request.sceneid;
    geometry.modelid = request.instance.modelid;
    geometry.instanceid = request.instance.instanceid;
    geometry.scenerevision = request.scenerevision;
    geometry.transformrevision = request.instance.transformrevision;
    geometry.sourcebboxmm = source.bbox_mm;
    geometry.effectivebboxmm = transformed.geometry.bboxmm;
    geometry.visible = request.instance.visible;
    geometry.locked = request.instance.locked;
    geometry.admissionstatus = request.admissionstatus;
    geometry.sourcetrianglecount = source.triangles.size();
    geometry.texturedtrianglecount = static_cast<std::size_t>(
        std::count_if(
            source.triangle_textures.begin(),
            source.triangle_textures.end(),
            [](const TriangleTextureInfo& texture)
            {
                return texture.has_uv;
            }));
    geometry.materialcount = source.material_infos.size();
    geometry.hastexturecoordinates =
        geometry.texturedtrianglecount > 0U;
    geometry.transformhash = transformHash.hash;
    geometry.worldboundsmm = {
        {transformed.geometry.bboxmm.min.x,
         transformed.geometry.bboxmm.min.y},
        {transformed.geometry.bboxmm.max.x,
         transformed.geometry.bboxmm.max.y},
    };
    if (!IsFinite(geometry.worldboundsmm.min)
        || !IsFinite(geometry.worldboundsmm.max))
    {
        return {
            {},
            MakeError(
                SceneViewGeometryErrorCode::GeometryNonFinite,
                request,
                "worldboundsmm",
                "projected model bounds must be finite")};
    }
    if (geometry.worldboundsmm.max.xmm
            <= geometry.worldboundsmm.min.xmm
        || geometry.worldboundsmm.max.ymm
            <= geometry.worldboundsmm.min.ymm)
    {
        return {
            {},
            MakeError(
                SceneViewGeometryErrorCode::SourceGeometryInvalid,
                request,
                "worldboundsmm",
                "projected model bounds must have positive width and height")};
    }

    geometry.triangles.reserve(transformed.geometry.triangles.size());
    for (const Triangle& triangle : transformed.geometry.triangles)
    {
        SceneViewTriangle projected{
            {triangle.a.x, triangle.a.y},
            {triangle.b.x, triangle.b.y},
            {triangle.c.x, triangle.c.y},
        };
        if (!IsFinite(projected.a)
            || !IsFinite(projected.b)
            || !IsFinite(projected.c))
        {
            return {
                {},
                MakeError(
                    SceneViewGeometryErrorCode::GeometryNonFinite,
                    request,
                    "triangles",
                    "projected triangle coordinates must be finite")};
        }
        geometry.triangles.push_back(projected);
    }
    geometry.geometryhash = ComputeGeometryHash(geometry);
    return {std::move(geometry), std::nullopt};
}

}  // namespace slicer_core
