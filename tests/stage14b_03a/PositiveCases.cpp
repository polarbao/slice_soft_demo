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
    Require(top.Value()->appearances.size() == 1U,
            "checker should expose one appearance");
    const auto& preview = *top.Value()->instances.front().surface_preview;
    Require(ContainsColor(preview.rgba8, {0U, 0U, 0U, 255U}),
            "checker top view should retain opaque black");
    Require(ContainsColor(preview.rgba8, {255U, 64U, 64U, 255U}),
            "checker top view should retain opaque red");

    const auto threeD = provider->GetViewData(
        MakeRequest(slicer_core::api::ViewMode::ThreeD), snapshot, active);
    Require(threeD.IsOk(), "checker three_d view should close");
    const auto& instance = threeD.Value()->instances.front();
    Require(instance.mesh.has_value(), "three_d should return a mesh");
    Require(instance.mesh->positions.size() == 18U
                && instance.mesh->normals.size() == 18U
                && instance.mesh->texcoord0.size() == 12U
                && instance.mesh->indices.size() == 6U,
            "three_d should return position/normal/UV/index buffers");
    Require(instance.mesh->submeshes.size() == 1U,
            "checker should return one material submesh");
    Require(instance.appearance_identity
                == threeD.Value()->appearances.front().appearance_identity,
            "checker instance appearance should close");
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
}

}  // namespace

void RunPositiveCases()
{
    CheckerThreeMfSemanticCase();
    ShengdanjieUsedMaterialClosureCase();
    WhiteAndNearWhiteTextureCase();
    DualAppearanceAndIdentityCase();
}

}  // namespace stage14b03a
