#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/CommonDtos.h"
#include "slicer_core/scene/SceneModel.h"
#include "slicer_core/texture_image.h"

#include <filesystem>
#include <map>
#include <memory>

namespace slicer_core::api
{

/**
 * @brief Read-only model resource boundary used by textured ViewData.
 */
class ISceneViewModelRepository
{
public:
    /** @brief Destroys the repository through its interface. */
    virtual ~ISceneViewModelRepository() = default;

    /**
     * @brief Resolves one immutable imported model.
     * @param modelId Numeric API model identity from the scene snapshot.
     * @return Shared model resource or a stable PM-SLICER error.
     */
    [[nodiscard]] virtual ApiResult<std::shared_ptr<const SceneModel>> GetModel(
        ModelId modelId) const noexcept = 0;
};

/**
 * @brief Read-only texture decoder boundary used by fail-closed ViewData.
 */
class ISceneViewTextureSource
{
public:
    /** @brief Destroys the texture source through its interface. */
    virtual ~ISceneViewTextureSource() = default;

    /**
     * @brief Loads one declared texture as straight-alpha RGBA8.
     * @param path Texture path retained by the imported model.
     * @return Decoded pixels or a stable missing/decode error.
     */
    [[nodiscard]] virtual ApiResult<TextureImage> Load(
        const std::filesystem::path& path) const noexcept = 0;
};

/**
 * @brief Creates an immutable map-backed model repository.
 * @param models Models keyed by numeric API identity.
 * @return Ready repository or a stable PM-SLICER error.
 */
[[nodiscard]] ApiResult<std::shared_ptr<const ISceneViewModelRepository>>
CreateSceneViewModelRepository(
    std::map<ModelId, std::shared_ptr<const SceneModel>> models) noexcept;

/**
 * @brief Creates the filesystem/WIC texture source used by production hosts.
 * @return Ready read-only texture source.
 */
[[nodiscard]] std::shared_ptr<const ISceneViewTextureSource>
CreateFileSceneViewTextureSource();

}  // namespace slicer_core::api
