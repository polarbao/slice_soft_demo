#include "slicer_core/scene/SceneEffectiveConfig.h"

#include "slicer_core/system/Sha256.h"

#include <cmath>
#include <fstream>
#include <system_error>
#include <utility>

namespace slicer_core
{
namespace
{

SceneValidationError MakeEffectiveConfigError(
    const SceneValidationErrorCode code,
    const SceneEffectiveConfigRequest& request,
    const std::string_view field,
    const std::string_view message)
{
    SceneValidationError error;
    error.code = code;
    error.sceneid = request.scene.sceneid;
    error.field = field;
    error.message = message;
    return error;
}

SceneValidationError MakeReadError(
    const SceneValidationErrorCode code,
    const std::filesystem::path& path,
    const std::string_view message)
{
    SceneValidationError error;
    error.code = code;
    error.field = path.generic_string();
    error.message = message;
    return error;
}

std::filesystem::path NormalizedAbsolutePath(
    const std::filesystem::path& path)
{
    std::error_code error;
    const std::filesystem::path absolute =
        std::filesystem::absolute(path, error);
    return error
        ? path.lexically_normal()
        : absolute.lexically_normal();
}

Json BuildIdentity(const MultiModelScene& scene)
{
    Json::Array instances;
    instances.reserve(scene.instances.size());
    for (const SceneModelInstance& sceneInstance : scene.instances)
    {
        instances.push_back(
            Json::object({
                {"instanceId", sceneInstance.instance.instanceid},
                {"modelId", sceneInstance.instance.modelid},
                {"transformRevision",
                 static_cast<double>(
                     sceneInstance.instance.transformrevision)},
            }));
    }

    return Json::object({
        {"sceneId", scene.sceneid},
        {"sceneRevision", static_cast<double>(scene.scenerevision)},
        {"sceneHash", ComputeMultiModelSceneHash(scene)},
        {"instances", Json(std::move(instances))},
    });
}

std::string ComputeEffectiveDocumentHash(const Json::Object& object)
{
    return ComputeSha256(Json(object).dump(0));
}

bool PublishStagedFile(
    const std::filesystem::path& stagingPath,
    const std::filesystem::path& outputPath)
{
    const std::filesystem::path backupPath =
        outputPath.string() + ".backup";
    std::error_code error;
    std::filesystem::remove(backupPath, error);
    error.clear();

    const bool hadOutput = std::filesystem::exists(outputPath);
    if (hadOutput)
    {
        std::filesystem::rename(outputPath, backupPath, error);
        if (error)
        {
            return false;
        }
    }

    std::filesystem::rename(stagingPath, outputPath, error);
    if (error)
    {
        if (hadOutput)
        {
            std::error_code rollbackError;
            std::filesystem::rename(
                backupPath,
                outputPath,
                rollbackError);
        }
        return false;
    }

    if (hadOutput)
    {
        std::filesystem::remove(backupPath, error);
    }
    return true;
}

}  // namespace

bool SceneEffectiveConfigResult::IsValid() const
{
    return !error.has_value() && !document.is_null();
}

std::string_view SceneEffectiveConfigSchemaName()
{
    return "slicesoft.scene_effective_config.13b.1";
}

SceneEffectiveConfigResult GenerateSceneEffectiveConfig(
    const SceneEffectiveConfigRequest& request)
{
    if (request.cancelled)
    {
        return {
            {},
            {},
            MakeEffectiveConfigError(
                SceneValidationErrorCode::EffectiveConfigCancelled,
                request,
                "cancelled",
                "scene effective config generation was cancelled")};
    }

    const SceneValidationResult validation =
        ValidateMultiModelScene(
            request.scene,
            request.production
                ? SceneValidationPurpose::Production
                : SceneValidationPurpose::Draft);
    if (!validation.IsValid())
    {
        return {{}, {}, validation.errors.front()};
    }
    if (request.sourceprofileid.empty()
        || request.sourceprofileid
            != request.scene.resolvedprofileid)
    {
        return {
            {},
            {},
            MakeEffectiveConfigError(
                SceneValidationErrorCode::SceneProfileMismatch,
                request,
                "sourceprofileid",
                "effective config Profile must match the scene Profile")};
    }
    if (request.sourcescenepath.empty()
        || request.generatedatutc.empty()
        || request.dpix <= 0
        || request.dpiy <= 0
        || !std::isfinite(request.layerheightmm)
        || request.layerheightmm <= 0.0
        || (request.slicepipelinemode != "legacy"
            && request.slicepipelinemode
                != "global_surface_shell"))
    {
        return {
            {},
            {},
            MakeEffectiveConfigError(
                SceneValidationErrorCode::
                    EffectiveConfigIntegrityFailed,
                request,
                "slicecontract",
                "scene effective config audit and slice contract are invalid")};
    }

    Json::Object object{
        {"schema", std::string(SceneEffectiveConfigSchemaName())},
        {"subjectType", "scene"},
        {"sourceScenePath",
         request.sourcescenepath.generic_string()},
        {"sourceProfileId", request.sourceprofileid},
        {"generatedAtUtc", request.generatedatutc},
        {"production", request.production},
        {"sliceContract",
         Json::object({
             {"profileConfigPath",
              request.sourceprofileconfigpath.generic_string()},
             {"outputPackageDir",
              request.outputpackagedir.generic_string()},
             {"dpiX", request.dpix},
             {"dpiY", request.dpiy},
             {"layerHeightMm", request.layerheightmm},
             {"slicePipelineMode", request.slicepipelinemode},
             {"materialBindingMode", "scene_profile_only"},
             {"resolvedProfileId", request.scene.resolvedprofileid},
         })},
        {"identity", BuildIdentity(request.scene)},
        {"sceneConfig", SerializeMultiModelScene(request.scene)},
    };
    const std::string configHash =
        ComputeEffectiveDocumentHash(object);
    object.emplace("configHash", configHash);
    return {
        Json(std::move(object)),
        configHash,
        std::nullopt};
}

SceneEffectiveConfigResult WriteSceneEffectiveConfig(
    const SceneEffectiveConfigRequest& request)
{
    SceneEffectiveConfigResult result =
        GenerateSceneEffectiveConfig(request);
    if (!result.IsValid())
    {
        return result;
    }
    if (request.generatedconfigpath.empty())
    {
        result.error = MakeEffectiveConfigError(
            SceneValidationErrorCode::EffectiveConfigWriteFailed,
            request,
            "generatedconfigpath",
            "scene effective config output path must not be empty");
        return result;
    }
    if (request.generatedconfigpath.filename()
        != "scene_config.effective.json")
    {
        result.error = MakeEffectiveConfigError(
            SceneValidationErrorCode::EffectiveConfigWriteFailed,
            request,
            "generatedconfigpath",
            "scene effective config filename must be scene_config.effective.json");
        return result;
    }
    if (!request.sourcescenepath.empty()
        && NormalizedAbsolutePath(request.sourcescenepath)
            == NormalizedAbsolutePath(request.generatedconfigpath))
    {
        result.error = MakeEffectiveConfigError(
            SceneValidationErrorCode::EffectiveConfigWriteFailed,
            request,
            "generatedconfigpath",
            "scene effective config cannot overwrite the scene draft");
        return result;
    }

    std::error_code error;
    const std::filesystem::path parent =
        request.generatedconfigpath.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, error);
    }
    if (error)
    {
        result.error = MakeEffectiveConfigError(
            SceneValidationErrorCode::EffectiveConfigWriteFailed,
            request,
            "generatedconfigpath",
            "failed to create effective config output directory");
        return result;
    }

    const std::filesystem::path stagingPath =
        request.generatedconfigpath.string() + ".tmp";
    std::filesystem::remove(stagingPath, error);
    error.clear();
    {
        std::ofstream output(stagingPath, std::ios::binary);
        if (!output)
        {
            result.error = MakeEffectiveConfigError(
                SceneValidationErrorCode::EffectiveConfigWriteFailed,
                request,
                "generatedconfigpath",
                "failed to open effective config staging file");
            return result;
        }
        output << result.document.dump(2);
        output.flush();
        if (!output)
        {
            output.close();
            std::filesystem::remove(stagingPath, error);
            result.error = MakeEffectiveConfigError(
                SceneValidationErrorCode::EffectiveConfigWriteFailed,
                request,
                "generatedconfigpath",
                "failed to write effective config staging file");
            return result;
        }
    }

    if (!PublishStagedFile(
            stagingPath,
            request.generatedconfigpath))
    {
        std::filesystem::remove(stagingPath, error);
        result.error = MakeEffectiveConfigError(
            SceneValidationErrorCode::EffectiveConfigWriteFailed,
            request,
            "generatedconfigpath",
            "failed to atomically publish effective config");
    }
    return result;
}

SceneEffectiveConfigResult ReadSceneEffectiveConfig(
    const std::filesystem::path& path)
{
    try
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            return {
                {},
                {},
                MakeReadError(
                    SceneValidationErrorCode::
                        EffectiveConfigWriteFailed,
                    path,
                    "failed to open scene effective config")};
        }
        Json document = Json::parse(input);
        if (!document.is_object()
            || document.at("schema").as_string()
                != SceneEffectiveConfigSchemaName())
        {
            return {
                {},
                {},
                MakeReadError(
                    SceneValidationErrorCode::SchemaUnsupported,
                    path,
                    "scene effective config schema is unsupported")};
        }

        Json::Object hashInput = document.as_object();
        const std::string storedHash =
            hashInput.at("configHash").as_string();
        hashInput.erase("configHash");
        const std::string computedHash =
            ComputeEffectiveDocumentHash(hashInput);
        if (storedHash != computedHash)
        {
            return {
                {},
                {},
                MakeReadError(
                    SceneValidationErrorCode::
                        EffectiveConfigIntegrityFailed,
                    path,
                    "scene effective config hash mismatch")};
        }
        return {
            std::move(document),
            storedHash,
            std::nullopt};
    }
    catch (const std::exception& error)
    {
        return {
            {},
            {},
            MakeReadError(
                SceneValidationErrorCode::
                    EffectiveConfigIntegrityFailed,
                path,
                error.what())};
    }
}

bool IsSceneEffectiveConfigStale(
    const Json& document,
    const MultiModelScene& scene)
{
    try
    {
        if (!document.is_object()
            || document.at("schema").as_string()
                != SceneEffectiveConfigSchemaName())
        {
            return true;
        }
        const Json& identity = document.at("identity");
        return identity.at("sceneId").as_string() != scene.sceneid
            || static_cast<std::uint64_t>(
                   identity.at("sceneRevision").as_double())
                != scene.scenerevision
            || identity.at("sceneHash").as_string()
                != ComputeMultiModelSceneHash(scene);
    }
    catch (const std::exception&)
    {
        return true;
    }
}

}  // namespace slicer_core
