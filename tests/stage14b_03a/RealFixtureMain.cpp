#include "slicer_core/api/viewdata/TexturedSceneViewDataProvider.h"
#include "slicer_core/model.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{

class ActiveCancelToken final : public slicer_core::api::ICancelToken
{
public:
    [[nodiscard]] bool IsCancelRequested() const noexcept override
    {
        return false;
    }
};

void Require(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "Stage 14B-03A real fixture: " << message << '\n';
        std::exit(1);
    }
}

slicer_core::SceneModel LoadFixture(
    const std::filesystem::path& sourceRoot,
    const std::filesystem::path& binaryRoot,
    const std::filesystem::path& relativePath,
    const std::string& format,
    const std::string& outputName)
{
    slicer_core::ModelLoadConfig config;
    config.input.model_path = sourceRoot / relativePath;
    config.input.format = format;
    config.output_package_dir = binaryRoot / "stage14b03a_real_assets"
        / outputName;
    config.auto_orient.enabled = false;
    std::filesystem::create_directories(config.output_package_dir);
    return slicer_core::load_model_report(config, sourceRoot);
}

slicer_core::api::SceneSnapshot MakeSnapshot()
{
    slicer_core::api::SceneSnapshot snapshot;
    snapshot.scene_id = 1403U;
    snapshot.scene_revision = 9U;
    snapshot.scene_hash = "stage14b03a-real-fixtures";

    slicer_core::api::SceneInstanceState checker;
    checker.instance.instance_id = "checker-3mf";
    checker.instance.model_id = 140301U;
    snapshot.instances.push_back(checker);

    slicer_core::api::SceneInstanceState shengdanjie;
    shengdanjie.instance.instance_id = "shengdanjie-obj";
    shengdanjie.instance.model_id = 140302U;
    shengdanjie.instance.world_matrix.values.at(3U) = 30.0;
    snapshot.instances.push_back(shengdanjie);
    return snapshot;
}

slicer_core::api::SceneViewDataRequest MakeRequest(
    const slicer_core::api::ViewMode viewMode)
{
    slicer_core::api::SceneViewDataRequest request;
    request.scene_id = 1403U;
    request.view_mode = viewMode;
    request.expected_scene_revision = 9U;
    request.lod = slicer_core::api::ViewLod::Auto;
    request.mesh_transform = slicer_core::api::MeshTransform::Local;
    request.max_bytes = 256U * 1024U * 1024U;
    return request;
}

std::size_t CountOpaqueColors(const std::vector<std::uint8_t>& rgba)
{
    std::vector<std::array<std::uint8_t, 3>> colors;
    for (std::size_t offset{0U}; offset + 3U < rgba.size(); offset += 4U)
    {
        if (rgba.at(offset + 3U) == 0U)
        {
            continue;
        }
        const std::array<std::uint8_t, 3> color{
            rgba.at(offset + 0U),
            rgba.at(offset + 1U),
            rgba.at(offset + 2U)};
        if (std::find(colors.begin(), colors.end(), color) == colors.end())
        {
            colors.push_back(color);
        }
        if (colors.size() >= 2U)
        {
            break;
        }
    }
    return colors.size();
}

void VerifyTopView(const slicer_core::api::SceneViewData& viewData)
{
    Require(viewData.view_mode == slicer_core::api::ViewMode::Top,
            "top request returned the wrong mode");
    Require(viewData.instances.size() == 2U,
            "top response did not retain both real models");
    Require(viewData.appearances.size() == 2U,
            "top response did not retain two real appearances");
    for (const slicer_core::api::ViewInstance& instance : viewData.instances)
    {
        Require(instance.texture_status
                    == slicer_core::api::TextureStatus::Available,
                "textured real model was reported as untextured");
        Require(instance.surface_preview.has_value(),
                "top response did not contain surfacePreview");
        Require(!instance.surface_preview->rgba8.empty(),
                "top surfacePreview had no RGBA8 payload");
        Require(instance.surface_preview->appearance_identity
                    == instance.appearance_identity,
                "top appearance reference did not close");
    }
    Require(CountOpaqueColors(
                viewData.instances.at(1U).surface_preview->rgba8) >= 2U,
            "OBJ top preview did not retain real texture variation");
}

void VerifyThreeDView(const slicer_core::api::SceneViewData& viewData)
{
    Require(viewData.view_mode == slicer_core::api::ViewMode::ThreeD,
            "three_d request returned the wrong mode");
    Require(viewData.instances.size() == 2U,
            "three_d response did not retain both real models");
    for (const slicer_core::api::ViewInstance& instance : viewData.instances)
    {
        Require(!instance.mesh.has_value(),
                "canonical three_d response must not inline instance mesh");
        const auto meshIterator = std::find_if(
            viewData.meshes.begin(),
            viewData.meshes.end(),
            [&instance](const slicer_core::api::ViewMesh& mesh)
            {
                return mesh.mesh_identity == instance.mesh_identity;
            });
        Require(meshIterator != viewData.meshes.end(),
                "three_d response did not resolve the reusable mesh");
        const slicer_core::api::ViewMesh& mesh = *meshIterator;
        Require(!mesh.positions.empty() && !mesh.indices.empty(),
                "three_d mesh buffers were empty");
        Require(mesh.texcoord0.size() == mesh.positions.size() / 3U * 2U,
                "three_d UV buffer did not close against positions");
        Require(!mesh.submeshes.empty(),
                "three_d mesh had no material submesh");
    }
    for (const slicer_core::api::ViewAppearance& appearance :
         viewData.appearances)
    {
        Require(!appearance.materials.empty(),
                "real appearance had no materials");
        Require(!appearance.textures.empty(),
                "real textured appearance had no texture resources");
    }
}

}  // namespace

int main()
{
    const std::filesystem::path sourceRoot{SLICESOFT_SOURCE_DIR};
    const std::filesystem::path binaryRoot{SLICESOFT_BINARY_DIR};
    auto checker = std::make_shared<const slicer_core::SceneModel>(
        LoadFixture(
            sourceRoot,
            binaryRoot,
            "samples/models/3mf/texture2d_checker_cube.3mf",
            "3mf",
            "checker"));
    auto shengdanjie = std::make_shared<const slicer_core::SceneModel>(
        LoadFixture(
            sourceRoot,
            binaryRoot,
            "model/obj/shengdanjie_fudiao/star/"
            "MF_shengdanjie_zhongzhi_R_fy02.obj",
            "obj",
            "shengdanjie"));

    const auto repository = slicer_core::api::CreateSceneViewModelRepository({
        {140301U, std::move(checker)},
        {140302U, std::move(shengdanjie)},
    });
    Require(repository.IsOk(), "real model repository creation failed");
    const auto provider = slicer_core::api::CreateTexturedSceneViewDataProvider(
        *repository.Value(),
        slicer_core::api::CreateFileSceneViewTextureSource());
    Require(provider.IsOk(), "real ViewData provider creation failed");

    const ActiveCancelToken cancelToken;
    const slicer_core::api::SceneSnapshot snapshot = MakeSnapshot();
    const auto top = (*provider.Value())->GetViewData(
        MakeRequest(slicer_core::api::ViewMode::Top),
        snapshot,
        cancelToken);
    Require(top.IsOk(), top.IsOk() ? "" : top.Error()->message);
    VerifyTopView(*top.Value());

    const auto threeD = (*provider.Value())->GetViewData(
        MakeRequest(slicer_core::api::ViewMode::ThreeD),
        snapshot,
        cancelToken);
    Require(threeD.IsOk(), threeD.IsOk() ? "" : threeD.Error()->message);
    VerifyThreeDView(*threeD.Value());

    std::cout << "Stage 14B-03A real OBJ/3MF ViewData: PASS\n";
    return 0;
}
