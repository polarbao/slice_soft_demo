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
    scene.triangle_textures.at(0U).uv = {
        slicer_core::TexCoord{0.0, 0.0},
        slicer_core::TexCoord{1.0, 0.0},
        slicer_core::TexCoord{0.5, 1.0},
    };
    scene.triangle_textures.at(0U).material_name = "paint";
    scene.material_infos.resize(1U);
    scene.material_infos.at(0U).name = "paint";
    scene.material_infos.at(0U).diffuse_rgb = {12U, 34U, 56U};
    scene.material_infos.at(0U).has_diffuse = true;
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
            geometry.materialappearances.size() == 1U
                && geometry.materialappearances.at(0U).name == "paint"
                && geometry.materialappearances.at(0U).hasdiffuse
                && geometry.materialappearances.at(0U).diffusergb
                    == std::array<std::uint8_t, 3>{12U, 34U, 56U},
            "material appearance is retained for the top view")
        && ExpectTrue(
            geometry.triangles.at(0U).hasuv
                && geometry.triangles.at(0U).materialindex == 0
                && NearlyEqual(
                    geometry.triangles.at(0U).zmm.at(2U),
                    4.0)
                && NearlyEqual(
                    geometry.triangles.at(0U).uv.at(1U).u,
                    1.0),
            "projected triangle retains depth, UV and material binding")
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

bool StandingRotationIsProjectedAndExplicitlyGrounded()
{
    const slicer_core::SceneModel source = MakeScene();
    slicer_core::SceneViewGeometryRequest request = MakeRequest();
    request.instance.transform.rotatexdeg = 90.0;
    request.instance.transform.landonbuildplate = true;

    const slicer_core::SceneViewGeometryResult result =
        slicer_core::BuildSceneViewGeometry(source, request);
    return ExpectTrue(result.IsValid(), "standing transform projects")
        && ExpectTrue(
            NearlyEqual(result.geometry.worldboundsmm.min.xmm, 0.0)
                && NearlyEqual(result.geometry.worldboundsmm.max.xmm, 2.0)
                && NearlyEqual(result.geometry.worldboundsmm.min.ymm, 0.0)
                && NearlyEqual(result.geometry.worldboundsmm.max.ymm, 2.0),
            "X rotation changes the projected footprint")
        && ExpectTrue(
            NearlyEqual(
                result.geometry.effectivebboxmm.min.z, 0.0)
                && NearlyEqual(
                    result.geometry.effectivebboxmm.max.z,
                    4.0),
            "standing projection is explicitly landed on Z=0");
}

bool DisplayResourceLocationDoesNotChangeGeometryHash()
{
    slicer_core::SceneModel first = MakeScene();
    first.material_infos.at(0U).has_texture = true;
    first.material_infos.at(0U).texture_exists = true;
    first.material_infos.at(0U).diffuse_texture_path =
        "first-machine/paint.png";
    slicer_core::SceneModel second = first;
    second.material_infos.at(0U).diffuse_texture_path =
        "second-machine/paint.png";

    const auto firstResult =
        slicer_core::BuildSceneViewGeometry(first, MakeRequest());
    const auto secondResult =
        slicer_core::BuildSceneViewGeometry(second, MakeRequest());
    return ExpectTrue(
        firstResult.IsValid()
            && secondResult.IsValid()
            && firstResult.geometry.geometryhash
                == secondResult.geometry.geometryhash,
        "display resource location does not affect geometry identity");
}

bool TopSurfacePreviewUsesHighestMaterial()
{
    slicer_core::SceneModel scene;
    scene.model_path = "top-surface-preview.obj";
    scene.bbox_mm = {{0.0, 0.0, 0.0}, {2.0, 2.0, 1.0}};
    scene.triangles = {
        {{0.0, 0.0, 0.0}, {2.0, 0.0, 0.0}, {0.0, 2.0, 0.0}},
        {{2.0, 0.0, 0.0}, {2.0, 2.0, 0.0}, {0.0, 2.0, 0.0}},
        {{0.0, 0.0, 1.0}, {2.0, 0.0, 1.0}, {0.0, 2.0, 1.0}},
        {{2.0, 0.0, 1.0}, {2.0, 2.0, 1.0}, {0.0, 2.0, 1.0}},
    };
    scene.triangle_count = scene.triangles.size();
    scene.triangle_textures.resize(scene.triangles.size());
    scene.triangle_textures.at(0U).material_name = "lower";
    scene.triangle_textures.at(1U).material_name = "lower";
    scene.triangle_textures.at(2U).material_name = "upper";
    scene.triangle_textures.at(3U).material_name = "upper";
    slicer_core::MaterialInfo lower;
    lower.name = "lower";
    lower.has_diffuse = true;
    lower.diffuse_rgb = {220U, 30U, 30U};
    slicer_core::MaterialInfo upper;
    upper.name = "upper";
    upper.has_diffuse = true;
    upper.diffuse_rgb = {25U, 70U, 210U};
    scene.material_infos = {lower, upper};

    slicer_core::SceneViewGeometryRequest request = MakeRequest();
    request.instance.sourcebboxmm = scene.bbox_mm;
    request.instance.effectivebboxmm = scene.bbox_mm;
    const auto result =
        slicer_core::BuildSceneViewGeometry(scene, request);
    if (!ExpectTrue(result.IsValid(), "top surface preview succeeds"))
    {
        return false;
    }

    const auto& preview = result.geometry.surfacepreview;
    const std::size_t centerOffset =
        (static_cast<std::size_t>(preview.height / 2)
             * static_cast<std::size_t>(preview.width)
         + static_cast<std::size_t>(preview.width / 2))
        * 4U;
    return ExpectTrue(
        preview.IsValid()
            && preview.rgba.at(centerOffset + 0U) == 25U
            && preview.rgba.at(centerOffset + 1U) == 70U
            && preview.rgba.at(centerOffset + 2U) == 210U
            && preview.rgba.at(centerOffset + 3U) == 255U,
        "software z-buffer keeps the highest material appearance");
}

bool NonFiniteUvIsRejected()
{
    slicer_core::SceneModel scene = MakeScene();
    scene.triangle_textures.at(0U).uv.at(1U).u =
        std::numeric_limits<double>::quiet_NaN();
    const auto result =
        slicer_core::BuildSceneViewGeometry(scene, MakeRequest());
    return ExpectTrue(
        !result.IsValid()
            && result.error->code
                == slicer_core::SceneViewGeometryErrorCode::
                    GeometryNonFinite
            && result.error->field == "triangle_textures.uv",
        "non-finite UV is rejected before rendering");
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

bool ProductionProjectionCanSkipDisplayRaster()
{
    const slicer_core::SceneModel source = MakeScene();
    slicer_core::SceneViewGeometryRequest displayRequest =
        MakeRequest();
    const slicer_core::SceneViewGeometryResult display =
        slicer_core::BuildSceneViewGeometry(
            source,
            displayRequest);

    slicer_core::SceneViewGeometryRequest productionRequest =
        displayRequest;
    productionRequest.buildsurfacepreview = false;
    const slicer_core::SceneViewGeometryResult production =
        slicer_core::BuildSceneViewGeometry(
            source,
            productionRequest);
    return ExpectTrue(
               display.IsValid() && production.IsValid(),
               "display and production projections succeed")
        && ExpectTrue(
            display.geometry.surfacepreview.IsValid()
                && !production.geometry.surfacepreview.IsValid()
                && production.geometry.surfacepreview.rgba.empty(),
            "production projection skips display-only surface raster")
        && ExpectTrue(
            display.geometry.geometryhash
                == production.geometry.geometryhash,
            "display raster does not change production geometry identity");
}

}  // namespace

int main()
{
    const bool ok = ErrorNamesAreStable()
        && IdentityProjectionPreservesXyAndIdentity()
        && ExistingTransformIsProjectedReadOnly()
        && StandingRotationIsProjectedAndExplicitlyGrounded()
        && DisplayResourceLocationDoesNotChangeGeometryHash()
        && TopSurfacePreviewUsesHighestMaterial()
        && NonFiniteUvIsRejected()
        && StaleRevisionIsRejected()
        && InvalidGeometryIsRejected()
        && BlockedGeometryRemainsVisibleWithoutMutatingInput()
        && ProductionProjectionCanSkipDisplayRaster();
    if (!ok)
    {
        return 1;
    }
    std::cout << "scene_view_geometry_unit_tests: PASS\n";
    return 0;
}
