#pragma once

#include "slicer_core/api/viewdata/TexturedSceneViewDataProvider.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace stage14b03a
{

class TestCancelToken final : public slicer_core::api::ICancelToken
{
public:
    explicit TestCancelToken(const bool cancelled = false)
        : m_cancelled(cancelled)
    {
    }

    [[nodiscard]] bool IsCancelRequested() const noexcept override
    {
        return m_cancelled;
    }

private:
    bool m_cancelled{false};
};

class TestModelRepository final
    : public slicer_core::api::ISceneViewModelRepository
{
public:
    explicit TestModelRepository(
        std::map<slicer_core::api::ModelId,
                 std::shared_ptr<const slicer_core::SceneModel>> models)
        : m_models(std::move(models))
    {
    }

    [[nodiscard]] slicer_core::api::ApiResult<
        std::shared_ptr<const slicer_core::SceneModel>> GetModel(
        const slicer_core::api::ModelId modelId) const noexcept override
    {
        const auto model = m_models.find(modelId);
        if (model == m_models.end())
        {
            return slicer_core::api::ApiResult<
                std::shared_ptr<const slicer_core::SceneModel>>::Failure({
                    "PM-SLICER-INPUT-0001",
                    "test model is missing",
                    std::to_string(modelId)});
        }
        return slicer_core::api::ApiResult<
            std::shared_ptr<const slicer_core::SceneModel>>::Success(
                model->second);
    }

private:
    std::map<slicer_core::api::ModelId,
             std::shared_ptr<const slicer_core::SceneModel>> m_models;
};

class TestTextureSource final
    : public slicer_core::api::ISceneViewTextureSource
{
public:
    void AddImage(
        const std::string& path,
        slicer_core::TextureImage image)
    {
        m_images.emplace(path, std::move(image));
    }

    void AddError(
        const std::string& path,
        slicer_core::api::ApiError error)
    {
        m_errors.emplace(path, std::move(error));
    }

    [[nodiscard]] slicer_core::api::ApiResult<slicer_core::TextureImage> Load(
        const std::filesystem::path& path) const noexcept override
    {
        const std::string key = path.generic_string();
        m_calls.push_back(key);
        const auto error = m_errors.find(key);
        if (error != m_errors.end())
        {
            return slicer_core::api::ApiResult<
                slicer_core::TextureImage>::Failure(error->second);
        }
        const auto image = m_images.find(key);
        if (image == m_images.end())
        {
            return slicer_core::api::ApiResult<
                slicer_core::TextureImage>::Failure({
                    "PM-SLICER-INPUT-0001",
                    "test texture is missing",
                    key});
        }
        return slicer_core::api::ApiResult<
            slicer_core::TextureImage>::Success(image->second);
    }

    [[nodiscard]] bool WasLoaded(const std::string& path) const
    {
        return std::find(m_calls.begin(), m_calls.end(), path)
            != m_calls.end();
    }

private:
    std::map<std::string, slicer_core::TextureImage> m_images;
    std::map<std::string, slicer_core::api::ApiError> m_errors;
    mutable std::vector<std::string> m_calls;
};

inline void Require(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "Stage 14B-03A: " << message << '\n';
        std::exit(1);
    }
}

inline void RequireError(
    const slicer_core::api::ApiError* error,
    const std::string& code,
    const std::string& message)
{
    Require(error != nullptr, message + " should fail");
    Require(error->code == code, message + " returned " + error->code);
}

inline slicer_core::TextureImage MakeTexture(
    const std::array<std::uint8_t, 4>& first,
    const std::array<std::uint8_t, 4>& second)
{
    slicer_core::TextureImage image;
    image.width = 2;
    image.height = 2;
    image.rgba = {
        first.at(0U), first.at(1U), first.at(2U), first.at(3U),
        second.at(0U), second.at(1U), second.at(2U), second.at(3U),
        second.at(0U), second.at(1U), second.at(2U), second.at(3U),
        first.at(0U), first.at(1U), first.at(2U), first.at(3U)};
    return image;
}

inline slicer_core::SceneModel MakeTexturedQuad(
    const std::string& modelName,
    const std::string& materialName,
    const std::string& texturePath)
{
    slicer_core::SceneModel model;
    model.model_path = modelName;
    model.format = "obj";
    model.vertex_count = 4U;
    model.face_count = 2U;
    model.triangle_count = 2U;
    model.bbox_mm = {{0.0, 0.0, 0.0}, {10.0, 10.0, 1.0}};
    model.triangles = {
        {{0.0, 0.0, 1.0}, {10.0, 0.0, 1.0}, {10.0, 10.0, 1.0}},
        {{0.0, 0.0, 1.0}, {10.0, 10.0, 1.0}, {0.0, 10.0, 1.0}}};
    slicer_core::MaterialInfo material;
    material.name = materialName;
    material.has_diffuse = true;
    material.diffuse_rgb = {255U, 255U, 255U};
    material.has_texture = true;
    material.texture_exists = true;
    material.diffuse_texture_path = texturePath;
    model.material_infos.push_back(material);

    slicer_core::TriangleTextureInfo first;
    first.has_uv = true;
    first.material_name = materialName;
    first.uv = {{{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}}};
    slicer_core::TriangleTextureInfo second;
    second.has_uv = true;
    second.material_name = materialName;
    second.uv = {{{0.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}};
    model.triangle_textures = {first, second};
    return model;
}

inline slicer_core::api::SceneSnapshot MakeSnapshot(
    const std::vector<std::pair<std::string, slicer_core::api::ModelId>>& items,
    const std::uint64_t revision = 7U)
{
    slicer_core::api::SceneSnapshot snapshot;
    snapshot.scene_id = 42U;
    snapshot.scene_revision = revision;
    snapshot.scene_hash = "scene-hash";
    double offset{0.0};
    for (const auto& [instanceId, modelId] : items)
    {
        slicer_core::api::SceneInstanceState state;
        state.instance.instance_id = instanceId;
        state.instance.model_id = modelId;
        state.instance.world_matrix.values.at(3U) = offset;
        state.effective_bounds_mm.min_mm = {offset, 0.0, 0.0};
        state.effective_bounds_mm.max_mm = {offset + 10.0, 10.0, 1.0};
        snapshot.instances.push_back(state);
        offset += 20.0;
    }
    return snapshot;
}

inline slicer_core::api::SceneViewDataRequest MakeRequest(
    const slicer_core::api::ViewMode mode,
    const std::uint64_t revision = 7U)
{
    slicer_core::api::SceneViewDataRequest request;
    request.scene_id = 42U;
    request.view_mode = mode;
    request.lod = slicer_core::api::ViewLod::Auto;
    request.mesh_transform = slicer_core::api::MeshTransform::Local;
    request.expected_scene_revision = revision;
    request.max_bytes = 64U * 1024U * 1024U;
    return request;
}

inline std::shared_ptr<const slicer_core::api::ITexturedSceneViewDataProvider>
MakeProvider(
    std::map<slicer_core::api::ModelId,
             std::shared_ptr<const slicer_core::SceneModel>> models,
    const std::shared_ptr<TestTextureSource>& textures)
{
    auto repository = std::make_shared<TestModelRepository>(std::move(models));
    auto created = slicer_core::api::CreateTexturedSceneViewDataProvider(
        repository,
        textures);
    Require(created.IsOk(), "provider dependencies should be accepted");
    return *created.Value();
}

void RunPositiveCases();
void RunSimplificationCases();
void RunFailureCases();

}  // namespace stage14b03a
