#include "slicer_core/api/viewdata/SceneViewResources.h"

#include <cstddef>
#include <exception>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

namespace slicer_core::api
{
namespace
{

template <class T>
ApiResult<T> Failure(
    const std::string& code,
    const std::string& message,
    const std::string& detail)
{
    return ApiResult<T>::Failure({code, message, detail});
}

class ImmutableSceneViewModelRepository final
    : public ISceneViewModelRepository
{
public:
    explicit ImmutableSceneViewModelRepository(
        std::map<ModelId, std::shared_ptr<const SceneModel>> models)
        : m_models(std::move(models))
    {
    }

    [[nodiscard]] ApiResult<std::shared_ptr<const SceneModel>> GetModel(
        const ModelId modelId) const noexcept override
    {
        const auto model = m_models.find(modelId);
        if (model == m_models.end() || !model->second)
        {
            return Failure<std::shared_ptr<const SceneModel>>(
                "PM-SLICER-INPUT-0001",
                "ViewData model resource is not available",
                std::to_string(modelId));
        }
        return ApiResult<std::shared_ptr<const SceneModel>>::Success(
            model->second);
    }

private:
    const std::map<ModelId, std::shared_ptr<const SceneModel>> m_models;
};

class FileSceneViewTextureSource final : public ISceneViewTextureSource
{
public:
    [[nodiscard]] ApiResult<TextureImage> Load(
        const std::filesystem::path& path) const noexcept override
    {
        try
        {
            std::error_code fileError;
            if (path.empty()
                || !std::filesystem::is_regular_file(path, fileError)
                || fileError)
            {
                return Failure<TextureImage>(
                    "PM-SLICER-INPUT-0001",
                    "declared ViewData texture is missing",
                    path.generic_string());
            }

            std::ifstream probe(path, std::ios::binary);
            if (!probe)
            {
                return Failure<TextureImage>(
                    "PM-SLICER-INPUT-0001",
                    "declared ViewData texture is unreadable",
                    path.generic_string());
            }
            probe.close();

            TextureImage image = load_texture_image(path);
            const std::size_t expectedBytes =
                static_cast<std::size_t>(image.width)
                * static_cast<std::size_t>(image.height) * 4U;
            if (image.width <= 0 || image.height <= 0
                || image.rgba.size() != expectedBytes)
            {
                return Failure<TextureImage>(
                    "PM-SLICER-INPUT-0002",
                    "declared ViewData texture decoded to invalid RGBA8",
                    path.generic_string());
            }
            return ApiResult<TextureImage>::Success(std::move(image));
        }
        catch (const std::exception& error)
        {
            return Failure<TextureImage>(
                "PM-SLICER-INPUT-0002",
                "declared ViewData texture could not be decoded",
                path.generic_string() + ": " + error.what());
        }
        catch (...)
        {
            return Failure<TextureImage>(
                "PM-SLICER-INPUT-0002",
                "declared ViewData texture could not be decoded",
                path.generic_string());
        }
    }
};

}  // namespace

ApiResult<std::shared_ptr<const ISceneViewModelRepository>>
CreateSceneViewModelRepository(
    std::map<ModelId, std::shared_ptr<const SceneModel>> models) noexcept
{
    if (models.empty())
    {
        return Failure<std::shared_ptr<const ISceneViewModelRepository>>(
            "PM-SLICER-PROFILE-0031",
            "ViewData model repository must not be empty",
            "models");
    }
    for (const auto& [modelId, model] : models)
    {
        if (modelId == 0U || !model)
        {
            return Failure<std::shared_ptr<const ISceneViewModelRepository>>(
                "PM-SLICER-PROFILE-0031",
                "ViewData model repository contains an invalid entry",
                std::to_string(modelId));
        }
    }
    return ApiResult<std::shared_ptr<const ISceneViewModelRepository>>::Success(
        std::make_shared<ImmutableSceneViewModelRepository>(
            std::move(models)));
}

std::shared_ptr<const ISceneViewTextureSource>
CreateFileSceneViewTextureSource()
{
    return std::make_shared<FileSceneViewTextureSource>();
}

}  // namespace slicer_core::api
