#include "slicer_module/SceneLifecycleSupport.h"

#include "slicer_module/CapabilityJsonAdapter.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace slicesoft::module::scene_lifecycle
{
namespace
{

template <class T>
slicer_core::api::ApiResult<T> Failure(
    const std::string_view code,
    const std::string_view message,
    const std::string_view detail)
{
    return slicer_core::api::ApiResult<T>::Failure(
        {std::string(code), std::string(message), std::string(detail)});
}

std::string Lowercase(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](const unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

slicer_core::BuildVolumeOrigin ParseOrigin(const std::string& value)
{
    if (value == "lower_left")
    {
        return slicer_core::BuildVolumeOrigin::LowerLeft;
    }
    if (value == "center")
    {
        return slicer_core::BuildVolumeOrigin::Center;
    }
    throw CapabilityRequestError(
        "sceneContext buildVolume origin is invalid");
}

slicer_core::BuildVolumeAxisDirection ParseDirection(
    const std::string& value)
{
    if (value == "positive")
    {
        return slicer_core::BuildVolumeAxisDirection::Positive;
    }
    if (value == "negative")
    {
        return slicer_core::BuildVolumeAxisDirection::Negative;
    }
    throw CapabilityRequestError(
        "sceneContext buildVolume axis direction is invalid");
}

void RequireFinitePositive(const double value, const std::string& field)
{
    if (!std::isfinite(value) || value <= 0.0)
    {
        throw CapabilityRequestError(field + " must be finite and positive");
    }
}

}  // namespace

slicer_core::api::ApiResult<void>
MutableSceneViewModelRepository::Register(
    const slicer_core::api::ModelId modelId,
    std::shared_ptr<const slicer_core::SceneModel> model) noexcept
{
    if (modelId == 0U || !model)
    {
        return Failure<void>(
            "PM-SLICER-INPUT-0002",
            "ViewData model registration is invalid",
            std::to_string(modelId));
    }
    std::scoped_lock lock(m_mutex);
    const auto existing = m_models.find(modelId);
    if (existing != m_models.end())
    {
        return existing->second == model
            ? slicer_core::api::ApiResult<void>::Success()
            : Failure<void>(
                "PM-SLICER-PROFILE-0031",
                "ViewData modelId was registered with another resource",
                std::to_string(modelId));
    }
    m_models.emplace(modelId, std::move(model));
    return slicer_core::api::ApiResult<void>::Success();
}

slicer_core::api::ApiResult<std::shared_ptr<const slicer_core::SceneModel>>
MutableSceneViewModelRepository::GetModel(
    const slicer_core::api::ModelId modelId) const noexcept
{
    std::scoped_lock lock(m_mutex);
    const auto model = m_models.find(modelId);
    if (model == m_models.end() || !model->second)
    {
        return Failure<std::shared_ptr<const slicer_core::SceneModel>>(
            "PM-SLICER-INPUT-0001",
            "ViewData model resource is not available",
            std::to_string(modelId));
    }
    return slicer_core::api::ApiResult<
        std::shared_ptr<const slicer_core::SceneModel>>::Success(
        model->second);
}

slicer_core::api::ApiResult<slicer_core::api::SceneFacadeSeed>
BuildImplicitSceneSeed(
    const slicer_core::Json& sceneContext,
    const slicer_core::api::SceneId sceneId) noexcept
{
    try
    {
        if (!sceneContext.is_object())
        {
            throw CapabilityRequestError("sceneContext must be an object");
        }
        slicer_core::api::SceneFacadeSeed seed;
        seed.scene_id = sceneId;
        seed.validation_purpose = slicer_core::SceneValidationPurpose::Production;
        seed.scene.sceneid = "scene-" + std::to_string(sceneId);
        seed.scene.resolvedprofileid = RequireString(
            sceneContext,
            "resolvedProfileId");

        const slicer_core::Json& volume = RequireObject(
            sceneContext,
            "buildVolume");
        if (RequireString(volume, "source") != "device_profile")
        {
            throw CapabilityRequestError(
                "sceneContext buildVolume source must be device_profile");
        }
        if (RequireBoolean(volume, "isFixture"))
        {
            throw CapabilityRequestError(
                "sceneContext buildVolume isFixture must be false");
        }

        const double width = RequireNumber(volume, "widthMm");
        const double height = RequireNumber(volume, "heightMm");
        RequireFinitePositive(width, "sceneContext.buildVolume.widthMm");
        RequireFinitePositive(height, "sceneContext.buildVolume.heightMm");
        seed.scene.buildvolume.source =
            slicer_core::BuildVolumeSource::DeviceProfile;
        seed.scene.buildvolume.widthmm = width;
        seed.scene.buildvolume.heightmm = height;
        if (volume.contains("zLimitMm"))
        {
            const double zLimit = RequireNumber(volume, "zLimitMm");
            RequireFinitePositive(
                zLimit,
                "sceneContext.buildVolume.zLimitMm");
            seed.scene.buildvolume.zlimitmm = zLimit;
        }
        seed.scene.buildvolume.origin = ParseOrigin(
            RequireString(volume, "origin"));
        seed.scene.buildvolume.xdirection = ParseDirection(
            RequireString(volume, "xDirection"));
        seed.scene.buildvolume.ydirection = ParseDirection(
            RequireString(volume, "yDirection"));
        seed.scene.buildvolume.isfixture = false;
        return slicer_core::api::ApiResult<
            slicer_core::api::SceneFacadeSeed>::Success(std::move(seed));
    }
    catch (const std::exception& error)
    {
        return Failure<slicer_core::api::SceneFacadeSeed>(
            "PM-SLICER-INPUT-0002",
            "implicit sceneContext is invalid",
            error.what());
    }
}

slicer_core::api::ApiResult<slicer_core::api::SceneFacadeModelRegistration>
BuildModelRegistration(const ImportedModelResource& resource) noexcept
{
    try
    {
        const auto& metadata = resource.metadata;
        if (metadata.model_id == 0U || metadata.source_path.empty()
            || metadata.source_digest.empty() || !resource.scenemodel)
        {
            return Failure<slicer_core::api::SceneFacadeModelRegistration>(
                "PM-SLICER-INPUT-0002",
                "imported model resource is incomplete",
                std::to_string(metadata.model_id));
        }

        slicer_core::api::SceneFacadeModelRegistration registration;
        registration.api_model_id = metadata.model_id;
        registration.scene_model_id =
            "model-" + std::to_string(metadata.model_id);
        registration.model = resource.scenemodel;

        const std::filesystem::path sourcePath = std::filesystem::absolute(
            metadata.source_path).lexically_normal();
        const std::string format = Lowercase(metadata.format);
        registration.scope.resourcescopeid =
            "scope-" + std::to_string(metadata.model_id);
        registration.source.modelid = registration.scene_model_id;
        registration.source.sourcepath = sourcePath;
        registration.source.format = format;
        registration.source.resourcescopeid =
            registration.scope.resourcescopeid;
        registration.source.sourcehash = metadata.source_digest;
        registration.source.resourcehash = !metadata.appearance_identity.empty()
            ? metadata.appearance_identity
            : (!metadata.mesh_identity.empty()
                ? metadata.mesh_identity
                : metadata.source_digest);
        registration.source.displayname = sourcePath.stem().string();

        if (format == "obj")
        {
            registration.scope.kind = slicer_core::ResourceScopeKind::ObjDirectory;
            registration.scope.rootpath = sourcePath.parent_path();
        }
        else if (format == "3mf")
        {
            registration.scope.kind =
                slicer_core::ResourceScopeKind::ThreeMfPackage;
            registration.scope.rootpath = sourcePath;
            registration.scope.packagepath = sourcePath;
            registration.scope.partidentity = !metadata.mesh_identity.empty()
                ? metadata.mesh_identity
                : metadata.source_digest;
        }
        else if (format == "stl")
        {
            registration.scope.kind = slicer_core::ResourceScopeKind::StlFile;
            registration.scope.rootpath = sourcePath;
        }
        else
        {
            return Failure<slicer_core::api::SceneFacadeModelRegistration>(
                "PM-SLICER-INPUT-0002",
                "imported model format cannot enter a scene",
                format);
        }
        return slicer_core::api::ApiResult<
            slicer_core::api::SceneFacadeModelRegistration>::Success(
            std::move(registration));
    }
    catch (const std::exception& error)
    {
        return Failure<slicer_core::api::SceneFacadeModelRegistration>(
            "PM-SLICER-INTERNAL-0099",
            "failed to map an imported model into a scene",
            error.what());
    }
}

}  // namespace slicesoft::module::scene_lifecycle
