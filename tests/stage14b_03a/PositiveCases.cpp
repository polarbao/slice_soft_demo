// 本文件覆盖 ViewData 正向装配与纹理保真场景；与失败、量化和简化用例分离，
// 便于分别定位资源解析错误和网格预算策略漂移。
#include "TestSupport.h"

#include <algorithm>

namespace stage14b03a
{
namespace
{

bool ContainsColor(
    const std::vector<std::uint8_t>& rgba,
    const std::array<std::uint8_t, 4>& color)
{
    for (std::size_t offset{0U}; offset + 3U < rgba.size(); offset += 4U)
    {
        if (std::equal(
                color.begin(), color.end(),
                rgba.begin() + static_cast<std::ptrdiff_t>(offset)))
        {
            return true;
        }
    }
    return false;
}

slicer_core::TextureImage MakeLargeTexture(const int dimension)
{
    slicer_core::TextureImage image;
    image.width = dimension;
    image.height = dimension;
    image.rgba.resize(
        static_cast<std::size_t>(dimension)
            * static_cast<std::size_t>(dimension) * 4U);
    for (int y{0}; y < dimension; ++y)
    {
        for (int x{0}; x < dimension; ++x)
        {
            const std::size_t offset =
                (static_cast<std::size_t>(y)
                    * static_cast<std::size_t>(dimension)
                    + static_cast<std::size_t>(x))
                * 4U;
            image.rgba.at(offset) = static_cast<std::uint8_t>(x % 256);
            image.rgba.at(offset + 1U) =
                static_cast<std::uint8_t>(y % 256);
            image.rgba.at(offset + 2U) = 255U;
            image.rgba.at(offset + 3U) = 255U;
        }
    }
    return image;
}

void CheckerThreeMfSemanticCase()
{
    const auto model = std::make_shared<const slicer_core::SceneModel>(
        MakeTexturedQuad(
            "texture2d_checker_cube.3mf",
            "3mf_texture2dgroup_31",
            "3D/Textures/checker.png"));
    auto textures = std::make_shared<TestTextureSource>();
    textures->AddImage(
        "3D/Textures/checker.png",
        MakeTexture({0U, 0U, 0U, 255U}, {255U, 64U, 64U, 255U}));
    const auto provider = MakeProvider({{101U, model}}, textures);
    const TestCancelToken active;
    const auto snapshot = MakeSnapshot({{"checker", 101U}});

    const auto top = provider->GetViewData(
        MakeRequest(slicer_core::api::ViewMode::Top), snapshot, active);
    Require(top.IsOk(), "checker top view should close");
    Require(!top.Value()->truncated
                && top.Value()->truncation_reason.empty(),
            "full-budget top view must not report truncation");
    Require(top.Value()->appearances.size() == 1U,
            "checker should expose one appearance");
    const auto& preview = *top.Value()->instances.front().surface_preview;
    Require(!top.Value()->instances.front().outlines.empty(),
            "top view should return a local-space outline");
    Require(ContainsColor(preview.rgba8, {0U, 0U, 0U, 255U}),
            "checker top view should retain opaque black");
    Require(ContainsColor(preview.rgba8, {255U, 64U, 64U, 255U}),
            "checker top view should retain opaque red");

    const auto threeD = provider->GetViewData(
        MakeRequest(slicer_core::api::ViewMode::ThreeD), snapshot, active);
    Require(threeD.IsOk(), "checker three_d view should close");
    const auto& instance = threeD.Value()->instances.front();
    Require(threeD.Value()->meshes.size() == 1U,
            "three_d should return one reusable mesh");
    const auto& mesh = threeD.Value()->meshes.front();
    Require(instance.mesh_identity == mesh.mesh_identity,
            "three_d instance should reference the reusable mesh");
    Require(!instance.mesh.has_value(),
            "canonical three_d DTO must not inline the mesh per instance");
    Require(!instance.outlines.empty(),
            "three_d should return a local-space outline");
    Require(mesh.positions.size() == 12U
                && mesh.normals.size() == 12U
                && mesh.texcoord0.size() == 8U
                && mesh.indices.size() == 6U,
            "three_d should share identical position/normal/UV vertices");
    Require(mesh.submeshes.size() == 1U,
            "checker should return one material submesh");
    Require(instance.appearance_identity
                == threeD.Value()->appearances.front().appearance_identity,
            "checker instance appearance should close");
}

void ImplicitObjMaterialFallsBackToNeutralGrayCase()
{
    auto model = MakeTexturedQuad(
        "implicit-material.obj",
        "mtl0",
        "unused.png");
    model.material_infos.clear();
    model.material_libraries.clear();
    const auto provider = MakeProvider(
        {{102U, std::make_shared<const slicer_core::SceneModel>(
                    std::move(model))}},
        std::make_shared<TestTextureSource>());
    const TestCancelToken active;

    const auto top = provider->GetViewData(
        MakeRequest(slicer_core::api::ViewMode::Top),
        MakeSnapshot({{"implicit-material", 102U}}),
        active);
    Require(top.IsOk(),
            "OBJ usemtl without mtllib should use a neutral material");
    Require(top.Value()->instances.front().texture_status
                == slicer_core::api::TextureStatus::NotProvided,
            "implicit neutral material must remain untextured");
    Require(top.Value()->appearances.front().textures.empty(),
            "implicit neutral material must not invent a texture");
    Require(ContainsColor(
                top.Value()->instances.front().surface_preview->rgba8,
                {153U, 153U, 153U, 255U}),
            "implicit neutral material should render as gray");

    const auto threeD = provider->GetViewData(
        MakeRequest(slicer_core::api::ViewMode::ThreeD),
        MakeSnapshot({{"implicit-material", 102U}}),
        active);
    Require(threeD.IsOk(),
            "implicit neutral material should close in three_d");
    const auto& material = threeD.Value()->appearances.front()
        .materials.front();
    Require(material.base_color.at(0U) == 0.6F
                && material.base_color.at(1U) == 0.6F
                && material.base_color.at(2U) == 0.6F,
            "three_d implicit material should retain neutral gray");
}

void UvSeamPreservationCase()
{
    slicer_core::SceneModel model = MakeTexturedQuad(
        "uv-seam.obj",
        "uv-seam-material",
        "uv-seam.png");
    model.triangle_textures.at(1U).uv.at(0U).u = 0.25;
    auto textures = std::make_shared<TestTextureSource>();
    textures->AddImage(
        "uv-seam.png",
        MakeTexture({30U, 60U, 90U, 255U}, {180U, 150U, 120U, 255U}));
    const auto provider = MakeProvider(
        {{420U, std::make_shared<const slicer_core::SceneModel>(
                     std::move(model))}},
        textures);
    const TestCancelToken active;
    const auto result = provider->GetViewData(
        MakeRequest(slicer_core::api::ViewMode::ThreeD),
        MakeSnapshot({{"uv-seam-instance", 420U}}),
        active);
    Require(result.IsOk(), "UV seam three_d view should close");
    const auto& mesh = result.Value()->meshes.front();
    Require(mesh.positions.size() == 15U
                && mesh.texcoord0.size() == 10U
                && mesh.indices.size() == 6U,
            "a UV seam must split the shared geometric vertex");

    std::size_t seamVertexCount{0U};
    for (std::size_t vertex{0U}; vertex < mesh.positions.size() / 3U; ++vertex)
    {
        if (mesh.positions.at(vertex * 3U) == 0.0F
            && mesh.positions.at(vertex * 3U + 1U) == 0.0F)
        {
            ++seamVertexCount;
        }
    }
    Require(seamVertexCount == 2U,
            "the geometric seam point must retain both UV vertices");
}

void SmoothNormalsAndCreasePreservationCase()
{
    auto smoothModel = MakeTexturedQuad(
        "smooth-fold.obj",
        "smooth-fold-material",
        "smooth-fold.png");
    smoothModel.triangles = {
        {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 0.2}},
        {{0.0, 0.0, 0.0}, {1.0, 1.0, 0.2}, {0.0, 1.0, 0.0}}};
    smoothModel.bbox_mm = {{0.0, 0.0, 0.0}, {1.0, 1.0, 0.2}};
    auto textures = std::make_shared<TestTextureSource>();
    textures->AddImage(
        "smooth-fold.png",
        MakeTexture({20U, 40U, 60U, 255U}, {80U, 100U, 120U, 255U}));
    const auto smoothProvider = MakeProvider(
        {{421U, std::make_shared<const slicer_core::SceneModel>(
                    std::move(smoothModel))}},
        textures);
    const TestCancelToken active;
    const auto smooth = smoothProvider->GetViewData(
        MakeRequest(slicer_core::api::ViewMode::ThreeD),
        MakeSnapshot({{"smooth-fold", 421U}}),
        active);
    Require(smooth.IsOk(), "smooth fold three_d view should close");
    const auto& smoothMesh = smooth.Value()->meshes.front();
    Require(smoothMesh.positions.size() == 12U
                && smoothMesh.indices.size() == 6U,
            "a fold below the default crease angle must share vertices");

    auto hardModel = MakeTexturedQuad(
        "hard-fold.obj",
        "hard-fold-material",
        "hard-fold.png");
    hardModel.triangles = {
        {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}},
        {{0.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};
    hardModel.bbox_mm = {{0.0, 0.0, 0.0}, {1.0, 1.0, 1.0}};
    textures->AddImage(
        "hard-fold.png",
        MakeTexture({30U, 50U, 70U, 255U}, {90U, 110U, 130U, 255U}));
    const auto hardProvider = MakeProvider(
        {{422U, std::make_shared<const slicer_core::SceneModel>(
                    std::move(hardModel))}},
        textures);
    const auto hard = hardProvider->GetViewData(
        MakeRequest(slicer_core::api::ViewMode::ThreeD),
        MakeSnapshot({{"hard-fold", 422U}}),
        active);
    Require(hard.IsOk(), "hard fold three_d view should close");
    const auto& hardMesh = hard.Value()->meshes.front();
    Require(hardMesh.positions.size() == 18U
                && hardMesh.indices.size() == 6U,
            "a 90 degree fold must retain split hard-edge vertices");
}

void BudgetDegradationCase()
{
    const auto model = std::make_shared<const slicer_core::SceneModel>(
        MakeTexturedQuad(
            "budget-degradation.obj",
            "budget-material",
            "budget-large.png"));
    auto textures = std::make_shared<TestTextureSource>();
    textures->AddImage("budget-large.png", MakeLargeTexture(1024));
    const auto provider = MakeProvider({{270U, model}}, textures);
    const TestCancelToken active;
    auto request = MakeRequest(slicer_core::api::ViewMode::Top);
    request.max_bytes = 700U * 1024U;
    const auto result = provider->GetViewData(
        request,
        MakeSnapshot({{"budget-degradation", 270U}}),
        active);
    Require(result.IsOk(), "bounded top view should degrade and close");
    Require(result.Value()->truncated,
            "budget degradation must be reported as truncated");
    Require(!result.Value()->truncation_reason.empty(),
            "budget degradation must include a reason");
    const auto& texture = result.Value()->appearances.front().textures.front();
    Require(texture.width_px <= 256 && texture.height_px <= 256,
            "budget degradation should reduce texture dimensions");
    Require(!texture.rgba8.empty(),
            "budget degradation must retain textured content");

    auto threeDRequest = MakeRequest(slicer_core::api::ViewMode::ThreeD);
    threeDRequest.max_bytes = 500U * 1024U;
    const auto threeD = provider->GetViewData(
        threeDRequest,
        MakeSnapshot({{"budget-degradation", 270U}}),
        active);
    Require(threeD.IsOk(),
            "bounded three_d view should degrade and close");
    Require(threeD.Value()->truncated
                && !threeD.Value()->truncation_reason.empty(),
            "three_d budget degradation must be explicit");
    Require(
        threeD.Value()->truncation_reason
            == "texture_resolution_reduced_for_max_bytes",
        "an unchanged tiny mesh must not be reported as simplified");
    const auto& threeDTexture =
        threeD.Value()->appearances.front().textures.front();
    Require(threeDTexture.width_px <= 256
                && threeDTexture.height_px <= 256,
            "three_d budget degradation should reduce texture dimensions");
    Require(threeD.Value()->meshes.size() == 1U,
            "three_d degradation must retain one reusable mesh");
    Require(threeD.Value()->meshes.front().lod
                == slicer_core::api::ViewLod::Lod2,
            "three_d auto budget should report the selected mesh LOD");
}

void SharedLocalMeshCase()
{
    const auto model = std::make_shared<const slicer_core::SceneModel>(
        MakeTexturedQuad(
            "shared-local.obj",
            "shared-local-material",
            "shared-local.png"));
    auto textures = std::make_shared<TestTextureSource>();
    textures->AddImage(
        "shared-local.png",
        MakeTexture({40U, 80U, 120U, 255U}, {160U, 200U, 240U, 255U}));
    const auto provider = MakeProvider({{410U, model}}, textures);
    const TestCancelToken active;
    const auto snapshot = MakeSnapshot({
        {"shared-local-a", 410U},
        {"shared-local-b", 410U}});

    const auto local = provider->GetViewData(
        MakeRequest(slicer_core::api::ViewMode::ThreeD), snapshot, active);
    Require(local.IsOk(), "shared local three_d view should close");
    Require(local.Value()->instances.size() == 2U,
            "shared local view should retain both instances");
    Require(local.Value()->meshes.size() == 1U,
            "same model local instances must share one mesh");
    Require(local.Value()->instances.at(0U).mesh_identity
                == local.Value()->instances.at(1U).mesh_identity,
            "same model local instances must reference one mesh identity");
    Require(!local.Value()->instances.at(0U).mesh.has_value()
                && !local.Value()->instances.at(1U).mesh.has_value(),
            "shared local mesh must not be duplicated inline");

    auto worldRequest = MakeRequest(slicer_core::api::ViewMode::ThreeD);
    worldRequest.mesh_transform = slicer_core::api::MeshTransform::World;
    const auto world = provider->GetViewData(
        worldRequest,
        snapshot,
        active);
    Require(world.IsOk(), "shared world three_d view should close");
    Require(world.Value()->meshes.size() == 2U,
            "differently transformed world meshes must remain distinct");
    Require(world.Value()->instances.at(0U).mesh_identity
                != world.Value()->instances.at(1U).mesh_identity,
            "world mesh identity must include transformed geometry");
}

void ShengdanjieUsedMaterialClosureCase()
{
    auto mutableModel = MakeTexturedQuad(
        "shengdanjie_fudiao/star/MF_shengdanjie_zhongzhi_R_fy02.obj",
        "MI_shengdanjie_zhongzhi_R_fy2",
        "T_shengdanjie_zhongzhi_R_fy02.png");
    slicer_core::MaterialInfo unused;
    unused.name = "MI_unused_missing";
    unused.has_texture = true;
    unused.texture_exists = false;
    unused.diffuse_texture_path = "unused_missing.png";
    mutableModel.material_infos.push_back(unused);
    const auto model = std::make_shared<const slicer_core::SceneModel>(
        std::move(mutableModel));
    auto textures = std::make_shared<TestTextureSource>();
    textures->AddImage(
        "T_shengdanjie_zhongzhi_R_fy02.png",
        MakeTexture({255U, 255U, 255U, 255U}, {24U, 90U, 180U, 255U}));
    const auto provider = MakeProvider({{201U, model}}, textures);
    const TestCancelToken active;
    const auto result = provider->GetViewData(
        MakeRequest(slicer_core::api::ViewMode::Top),
        MakeSnapshot({{"shengdanjie", 201U}}),
        active);
    Require(result.IsOk(), "used shengdanjie material should close");
    Require(!textures->WasLoaded("unused_missing.png"),
            "unused MTL texture must not be loaded");
    Require(ContainsColor(
                result.Value()->instances.front().surface_preview->rgba8,
                {255U, 255U, 255U, 255U}),
            "pure white texture pixels must remain opaque white");
}

void WhiteAndNearWhiteTextureCase()
{
    const auto model = std::make_shared<const slicer_core::SceneModel>(
        MakeTexturedQuad(
            "white-near-white.obj",
            "white-material",
            "white-near-white.png"));
    auto textures = std::make_shared<TestTextureSource>();
    textures->AddImage(
        "white-near-white.png",
        MakeTexture(
            {255U, 255U, 255U, 255U},
            {252U, 250U, 248U, 255U}));
    const auto provider = MakeProvider({{250U, model}}, textures);
    const TestCancelToken active;
    const auto result = provider->GetViewData(
        MakeRequest(slicer_core::api::ViewMode::Top),
        MakeSnapshot({{"white-near-white", 250U}}),
        active);
    Require(result.IsOk(), "white texture top view should close");
    const auto& rgba = result.Value()->instances.front()
        .surface_preview->rgba8;
    Require(ContainsColor(rgba, {255U, 255U, 255U, 255U}),
            "pure white must remain opaque white");
    Require(ContainsColor(rgba, {252U, 250U, 248U, 255U}),
            "near-white must retain its original RGB values");
}

void TextureBaseColorFactorCase()
{
    slicer_core::SceneModel model = MakeTexturedQuad(
        "base-color-factor.obj",
        "tinted-material",
        "tinted.png");
    model.material_infos.front().has_diffuse = true;
    model.material_infos.front().diffuse_rgb = {128U, 255U, 64U};
    auto textures = std::make_shared<TestTextureSource>();
    textures->AddImage(
        "tinted.png",
        MakeTexture(
            {200U, 100U, 80U, 255U},
            {200U, 100U, 80U, 255U}));
    const auto provider = MakeProvider(
        {{260U, std::make_shared<const slicer_core::SceneModel>(
                     std::move(model))}},
        textures);
    const TestCancelToken active;
    const auto result = provider->GetViewData(
        MakeRequest(slicer_core::api::ViewMode::Top),
        MakeSnapshot({{"tinted", 260U}}),
        active);
    Require(result.IsOk(), "tinted texture top view should close");
    Require(ContainsColor(
                result.Value()->instances.front().surface_preview->rgba8,
                {100U, 100U, 20U, 255U}),
            "top preview must multiply texture by baseColorFactor");
}

void DualAppearanceAndIdentityCase()
{
    const auto first = std::make_shared<const slicer_core::SceneModel>(
        MakeTexturedQuad("xiao_ma.obj", "horse", "horse.png"));
    const auto second = std::make_shared<const slicer_core::SceneModel>(
        MakeTexturedQuad("yecan.obj", "leaf", "leaf.png"));
    auto textures = std::make_shared<TestTextureSource>();
    textures->AddImage(
        "horse.png",
        MakeTexture({255U, 255U, 255U, 255U}, {180U, 30U, 30U, 255U}));
    textures->AddImage(
        "leaf.png",
        MakeTexture({20U, 140U, 80U, 255U}, {10U, 50U, 20U, 255U}));
    const auto provider = MakeProvider(
        {{301U, first}, {302U, second}}, textures);
    const TestCancelToken active;
    const auto snapshot = MakeSnapshot(
        {{"horse-instance", 301U}, {"leaf-instance", 302U}});
    const auto result = provider->GetViewData(
        MakeRequest(slicer_core::api::ViewMode::Top), snapshot, active);
    Require(result.IsOk(), "dual-model top view should close");
    Require(result.Value()->appearances.size() == 2U,
            "dual-model view should expose two appearances");
    Require(result.Value()->instances.at(0U).appearance_identity
                != result.Value()->instances.at(1U).appearance_identity,
            "different model appearances must not merge");

    auto movedSnapshot = snapshot;
    movedSnapshot.scene_revision = 8U;
    movedSnapshot.instances.front().instance.world_matrix.values.at(3U) = 25.0;
    auto movedRequest = MakeRequest(slicer_core::api::ViewMode::Top, 8U);
    const auto moved = provider->GetViewData(
        movedRequest, movedSnapshot, active);
    Require(moved.IsOk(), "moved dual-model top view should close");
    Require(result.Value()->instances.front().preview_identity
                == moved.Value()->instances.front().preview_identity,
            "world movement must preserve local preview identity");
    Require(result.Value()->instances.front().appearance_identity
                == moved.Value()->instances.front().appearance_identity,
            "world movement must preserve appearance identity");
    Require(result.Value()->viewdata_identity
                != moved.Value()->viewdata_identity,
            "world movement must change the scene view identity");

    const auto localMesh = provider->GetViewData(
        MakeRequest(slicer_core::api::ViewMode::ThreeD),
        snapshot,
        active);
    auto movedMeshRequest = MakeRequest(
        slicer_core::api::ViewMode::ThreeD,
        8U);
    const auto movedLocalMesh = provider->GetViewData(
        movedMeshRequest,
        movedSnapshot,
        active);
    Require(localMesh.IsOk() && movedLocalMesh.IsOk(),
            "local three_d views should close before and after movement");
    Require(localMesh.Value()->instances.front().mesh_identity
                == movedLocalMesh.Value()->instances.front().mesh_identity,
            "world movement must preserve local mesh identity");

    auto worldMeshRequest = movedMeshRequest;
    worldMeshRequest.mesh_transform = slicer_core::api::MeshTransform::World;
    const auto movedWorldMesh = provider->GetViewData(
        worldMeshRequest,
        movedSnapshot,
        active);
    Require(movedWorldMesh.IsOk(), "world three_d view should close");
    Require(movedWorldMesh.Value()->instances.front().mesh_identity
                != movedLocalMesh.Value()->instances.front().mesh_identity,
            "world mesh content must use a distinct identity");
    const slicer_core::api::Matrix4d identity;
    Require(movedWorldMesh.Value()->instances.front().world_matrix.values
                == identity.values,
            "world mesh response must expose an identity worldMatrix");
}

}  // namespace

void RunPositiveCases()
{
    CheckerThreeMfSemanticCase();
    ImplicitObjMaterialFallsBackToNeutralGrayCase();
    ShengdanjieUsedMaterialClosureCase();
    WhiteAndNearWhiteTextureCase();
    TextureBaseColorFactorCase();
    BudgetDegradationCase();
    UvSeamPreservationCase();
    SmoothNormalsAndCreasePreservationCase();
    SharedLocalMeshCase();
    DualAppearanceAndIdentityCase();
}

}  // namespace stage14b03a
