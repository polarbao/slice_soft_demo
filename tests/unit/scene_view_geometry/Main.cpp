#include "slicer_core/scene/SceneViewGeometry.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

namespace
{

constexpr double kTolerance = 1.0e-9;

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

bool NearlyEqual(const double left, const double right)
{
    return std::abs(left - right) <= kTolerance;
}

slicer_core::SceneModel MakeScene()
{
    slicer_core::SceneModel scene;
    scene.model_path = "scene-view-fixture.obj";
    scene.bbox_mm.min = {0.0, 0.0, 2.0};
    scene.bbox_mm.max = {2.0, 4.0, 4.0};
    scene.triangles = {
        {
            {0.0, 0.0, 2.0},
            {2.0, 0.0, 2.0},
            {1.0, 4.0, 4.0},
        },
        {
            {0.0, 0.0, 2.0},
            {1.0, 4.0, 4.0},
            {0.0, 4.0, 2.0},
        },
    };
    scene.triangle_count = scene.triangles.size();
    scene.triangle_textures.resize(scene.triangles.size());
    scene.triangle_textures.at(0U).has_uv = true;
    scene.material_infos.resize(1U);
    return scene;
}

slicer_core::SceneViewGeometryRequest MakeRequest()
{
    slicer_core::SceneViewGeometryRequest request;
    request.sceneid = "scene-view-1";
    request.scenerevision = 3U;
    request.instance.instanceid = "instance-1";
    request.instance.modelid = "model-1";
    request.instance.sourcetransformidentity = "source-transform-1";
    request.instance.sourcebboxmm = MakeScene().bbox_mm;
    request.instance.effectivebboxmm = request.instance.sourcebboxmm;
    request.admissionstatus =
        slicer_core::SceneViewAdmissionStatus::Admitted;
    return request;
}

bool ErrorNamesAreStable()
{
    using slicer_core::SceneViewGeometryErrorCode;
    const std::array<std::pair<SceneViewGeometryErrorCode, std::string>, 8>
        expected{{
            {SceneViewGeometryErrorCode::None, "NONE"},
            {SceneViewGeometryErrorCode::SceneIdEmpty,
             "SCENE_VIEW_SCENE_ID_EMPTY"},
            {SceneViewGeometryErrorCode::ModelIdEmpty,
             "SCENE_VIEW_MODEL_ID_EMPTY"},
            {SceneViewGeometryErrorCode::InstanceIdEmpty,
             "SCENE_VIEW_INSTANCE_ID_EMPTY"},
            {SceneViewGeometryErrorCode::RevisionStale,
             "SCENE_VIEW_REVISION_STALE"},
            {SceneViewGeometryErrorCode::SourceGeometryInvalid,
             "SCENE_VIEW_SOURCE_GEOMETRY_INVALID"},
            {SceneViewGeometryErrorCode::GeometryNonFinite,
             "SCENE_VIEW_GEOMETRY_NON_FINITE"},
            {SceneViewGeometryErrorCode::TransformInvalid,
             "SCENE_VIEW_TRANSFORM_INVALID"},
        }};

    bool ok = true;
    for (const auto& [code, name] : expected)
    {
        ok = ExpectTrue(
                 slicer_core::SceneViewGeometryErrorCodeName(code) == name,
                 "scene view error name matches contract")
            && ok;
    }
    return ok;
}

bool IdentityProjectionPreservesXyAndIdentity()
{
    const slicer_core::SceneModel source = MakeScene();
    const slicer_core::SceneViewGeometryResult result =
        slicer_core::BuildSceneViewGeometry(source, MakeRequest());
    if (!ExpectTrue(result.IsValid(), "identity projection succeeds"))
    {
        return false;
    }

    const slicer_core::SceneViewGeometry& geometry = result.geometry;
    return ExpectTrue(
               geometry.sceneid == "scene-view-1"
                   && geometry.modelid == "model-1"
                   && geometry.instanceid == "instance-1",
               "scene/model/instance identity is retained")
        && ExpectTrue(
            geometry.scenerevision == 3U
                && geometry.transformrevision == 0U,
            "scene and transform revisions are retained")
        && ExpectTrue(
            geometry.triangles.size() == source.triangles.size(),
            "all source triangles are projected")
        && ExpectTrue(
            NearlyEqual(geometry.triangles.at(0U).a.xmm, 0.0)
                && NearlyEqual(geometry.triangles.at(0U).a.ymm, 0.0)
                && NearlyEqual(geometry.triangles.at(0U).b.xmm, 2.0)
                && NearlyEqual(geometry.triangles.at(0U).c.ymm, 4.0),
            "+Z projection preserves X/Y coordinates")
        && ExpectTrue(
            NearlyEqual(geometry.worldboundsmm.min.xmm, 0.0)
                && NearlyEqual(geometry.worldboundsmm.min.ymm, 0.0)
                && NearlyEqual(geometry.worldboundsmm.max.xmm, 2.0)
                && NearlyEqual(geometry.worldboundsmm.max.ymm, 4.0),
            "projected bounds match source summary")
        && ExpectTrue(
            NearlyEqual(geometry.effectivebboxmm.min.z, 2.0)
                && NearlyEqual(geometry.effectivebboxmm.max.z, 4.0),
            "effective transformed bounds are retained")
        && ExpectTrue(
            geometry.hastexturecoordinates
                && geometry.texturedtrianglecount == 1U
                && geometry.materialcount == 1U,
            "material and texture display hints are retained")
        && ExpectTrue(
            geometry.geometryhash.size() == 64U
                && geometry.transformhash.size() == 64U,
            "geometry and transform hashes use SHA-256")
        && ExpectTrue(
            geometry.admissionstatus
                == slicer_core::SceneViewAdmissionStatus::Admitted,
            "admission status is retained");
}

bool ExistingTransformIsProjectedReadOnly()
{
    const slicer_core::SceneModel source = MakeScene();
    slicer_core::SceneViewGeometryRequest request = MakeRequest();
    request.instance.transform.uniformscale = 2.0;
    request.instance.transform.rotatezdeg = 90.0;
    request.instance.transform.translatexmm = 3.0;
    request.instance.transform.translateymm = -1.0;
    request.instance.transformrevision = 4U;

    const slicer_core::SceneViewGeometryResult result =
        slicer_core::BuildSceneViewGeometry(source, request);
    return ExpectTrue(result.IsValid(), "existing transform projects")
        && ExpectTrue(
            result.geometry.transformrevision == 4U,
            "transform revision is retained")
        && ExpectTrue(
            NearlyEqual(result.geometry.worldboundsmm.min.xmm, 0.0)
                && NearlyEqual(result.geometry.worldboundsmm.max.xmm, 8.0)
                && NearlyEqual(result.geometry.worldboundsmm.min.ymm, -1.0)
                && NearlyEqual(result.geometry.worldboundsmm.max.ymm, 3.0),
            "scale, rotate and translate affect projected bounds");
}

bool StaleRevisionIsRejected()
{
    slicer_core::SceneViewGeometryRequest request = MakeRequest();
    request.expectedscenerevision = 2U;
    const auto sceneStale =
        slicer_core::BuildSceneViewGeometry(MakeScene(), request);

    request.expectedscenerevision = request.scenerevision;
    request.expectedtransformrevision = 2U;
    const auto transformStale =
        slicer_core::BuildSceneViewGeometry(MakeScene(), request);

    return ExpectTrue(
               !sceneStale.IsValid()
                   && sceneStale.error->code
                       == slicer_core::SceneViewGeometryErrorCode::RevisionStale,
               "stale scene revision is rejected")
        && ExpectTrue(
            !transformStale.IsValid()
                && transformStale.error->code
                    == slicer_core::SceneViewGeometryErrorCode::RevisionStale,
            "stale transform revision is rejected");
}

bool InvalidGeometryIsRejected()
{
    slicer_core::SceneModel empty = MakeScene();
    empty.triangles.clear();
    const auto emptyResult =
        slicer_core::BuildSceneViewGeometry(empty, MakeRequest());

    slicer_core::SceneModel nonFinite = MakeScene();
    nonFinite.triangles.at(0U).a.x =
        std::numeric_limits<double>::quiet_NaN();
    const auto nonFiniteResult =
        slicer_core::BuildSceneViewGeometry(nonFinite, MakeRequest());

    slicer_core::SceneModel zeroWidth = MakeScene();
    for (slicer_core::Triangle& triangle : zeroWidth.triangles)
    {
        triangle.a.x = 1.0;
        triangle.b.x = 1.0;
        triangle.c.x = 1.0;
    }
    zeroWidth.bbox_mm.min.x = 1.0;
    zeroWidth.bbox_mm.max.x = 1.0;
    const auto zeroWidthResult =
        slicer_core::BuildSceneViewGeometry(zeroWidth, MakeRequest());

    return ExpectTrue(
               !emptyResult.IsValid()
                   && emptyResult.error->code
                       == slicer_core::SceneViewGeometryErrorCode::
                           SourceGeometryInvalid,
               "empty source is rejected")
        && ExpectTrue(
            !nonFiniteResult.IsValid()
                && nonFiniteResult.error->code
                    == slicer_core::SceneViewGeometryErrorCode::
                        GeometryNonFinite,
            "non-finite source is rejected")
        && ExpectTrue(
            !zeroWidthResult.IsValid()
                && zeroWidthResult.error->code
                    == slicer_core::SceneViewGeometryErrorCode::
                        SourceGeometryInvalid,
            "zero-width projected geometry is rejected");
}

bool BlockedGeometryRemainsVisibleWithoutMutatingInput()
{
    const slicer_core::SceneModel source = MakeScene();
    const slicer_core::SceneModel before = source;
    slicer_core::SceneViewGeometryRequest request = MakeRequest();
    request.admissionstatus =
        slicer_core::SceneViewAdmissionStatus::Blocked;
    request.instance.locked = true;

    const auto result =
        slicer_core::BuildSceneViewGeometry(source, request);
    return ExpectTrue(result.IsValid(), "blocked model remains viewable")
        && ExpectTrue(
            result.geometry.admissionstatus
                    == slicer_core::SceneViewAdmissionStatus::Blocked
                && result.geometry.locked,
            "blocked and locked state is retained")
        && ExpectTrue(
            source.triangles.size() == before.triangles.size()
                && NearlyEqual(
                    source.triangles.at(0U).a.x,
                    before.triangles.at(0U).a.x)
                && NearlyEqual(
                    source.bbox_mm.max.y,
                    before.bbox_mm.max.y),
            "source geometry is not mutated");
}

}  // namespace

int main()
{
    const bool ok = ErrorNamesAreStable()
        && IdentityProjectionPreservesXyAndIdentity()
        && ExistingTransformIsProjectedReadOnly()
        && StaleRevisionIsRejected()
        && InvalidGeometryIsRejected()
        && BlockedGeometryRemainsVisibleWithoutMutatingInput();
    if (!ok)
    {
        return 1;
    }
    std::cout << "scene_view_geometry_unit_tests: PASS\n";
    return 0;
}
