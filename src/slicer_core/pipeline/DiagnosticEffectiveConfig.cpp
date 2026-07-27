#include "slicer_core/pipeline/DiagnosticEffectiveConfig.h"

#include "slicer_core/system/Sha256.h"

#include <cmath>
#include <fstream>
#include <system_error>
#include <utility>

namespace slicer_core
{
namespace
{

constexpr std::string_view kDiagnosticConfigFilename{
    "slice_config.diagnostic.effective.json"};

DiagnosticEffectiveConfigError MakeError(
    const DiagnosticEffectiveConfigErrorCode code,
    const std::string_view field,
    const std::string_view message)
{
    DiagnosticEffectiveConfigError error;
    error.code = code;
    error.field = field;
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

bool IsFiniteNonNegative(const double value)
{
    return std::isfinite(value) && value >= 0.0;
}

bool IsFiniteNonNegative(
    const std::optional<double>& value)
{
    return !value.has_value()
        || IsFiniteNonNegative(*value);
}

Json OptionalNumber(const std::optional<double>& value)
{
    return value.has_value()
        ? Json(*value)
        : Json(nullptr);
}

std::string SubjectTypeName(const DiagnosticSubjectType type)
{
    switch (type)
    {
    case DiagnosticSubjectType::SingleModel:
        return "single_model";
    case DiagnosticSubjectType::Scene:
        return "scene";
    }
    return {};
}

std::optional<DiagnosticEffectiveConfigError> ValidateRequested(
    const DiagnosticEffectiveConfigRequest& request)
{
    if (request.sessionid.empty()
        || request.sourceprofileid.empty()
        || request.generatedatutc.empty()
        || request.sourceconfigpath.empty())
    {
        return MakeError(
            DiagnosticEffectiveConfigErrorCode::RequestInvalid,
            "audit",
            "diagnostic session, Profile, source config, and timestamp are required");
    }
    if (!IsFiniteNonNegative(
            request.requested.texturesurfacewidthmm)
        || request.requested.modelfillmaterial.empty()
        || request.requested.diagnosticbackendrequest.empty())
    {
        return MakeError(
            DiagnosticEffectiveConfigErrorCode::RequestInvalid,
            "requested",
            "requested diagnostic width, fill material, or backend is invalid");
    }
    if (!IsFiniteNonNegative(
            request.effective.texturesurfacewidthmm)
        || request.effective.modelfillmaterial.empty()
        || request.effective.diagnosticbackend.empty()
        || request.effective.resolvedprofileid.empty())
    {
        return MakeError(
            DiagnosticEffectiveConfigErrorCode::RequestInvalid,
            "effective",
            "effective diagnostic settings are incomplete");
    }
    if (request.effective.resolvedprofileid
        != request.sourceprofileid)
    {
        return MakeError(
            DiagnosticEffectiveConfigErrorCode::ProfileMismatch,
            "effective.resolvedProfileId",
            "effective diagnostic Profile must match the source Profile");
    }

    const DiagnosticDerivedSettings& derived = request.derived;
    if (!IsFiniteNonNegative(derived.minimumwidthmm)
        || !IsFiniteNonNegative(derived.maximumwidthmm)
        || !IsFiniteNonNegative(derived.alltexturethresholdmm)
        || derived.backendavailability.empty()
        || derived.derivationsource.empty())
    {
        return MakeError(
            DiagnosticEffectiveConfigErrorCode::DerivationUnavailable,
            "derived",
            "derived diagnostic bounds or provenance are invalid");
    }
    if (derived.minimumwidthmm.has_value()
        && derived.maximumwidthmm.has_value()
        && *derived.minimumwidthmm > *derived.maximumwidthmm)
    {
        return MakeError(
            DiagnosticEffectiveConfigErrorCode::DerivationUnavailable,
            "derived.maximumWidthMm",
            "derived maximum width must not be below the minimum");
    }
    if (derived.minimumwidthmm.has_value()
        && request.effective.texturesurfacewidthmm
            < *derived.minimumwidthmm)
    {
        return MakeError(
            DiagnosticEffectiveConfigErrorCode::DerivationUnavailable,
            "effective.textureSurfaceWidthMm",
            "effective width is below the derived minimum");
    }
    if (derived.maximumwidthmm.has_value()
        && request.effective.texturesurfacewidthmm
            > *derived.maximumwidthmm)
    {
        return MakeError(
            DiagnosticEffectiveConfigErrorCode::DerivationUnavailable,
            "effective.textureSurfaceWidthMm",
            "effective width exceeds the derived maximum");
    }
    if (derived.alltexturethresholdmm.has_value())
    {
        if (derived.minimumwidthmm.has_value()
            && *derived.alltexturethresholdmm
                < *derived.minimumwidthmm)
        {
            return MakeError(
                DiagnosticEffectiveConfigErrorCode::
                    DerivationUnavailable,
                "derived.allTextureThresholdMm",
                "all-texture threshold is below the derived minimum");
        }
        if (derived.maximumwidthmm.has_value()
            && *derived.alltexturethresholdmm
                > *derived.maximumwidthmm)
        {
            return MakeError(
                DiagnosticEffectiveConfigErrorCode::
                    DerivationUnavailable,
                "derived.allTextureThresholdMm",
                "all-texture threshold exceeds the derived maximum");
        }
    }
    return std::nullopt;
}

std::optional<DiagnosticEffectiveConfigError>
ValidateSingleModelIdentity(
    const DiagnosticEffectiveConfigRequest& request)
{
    if (request.scene.has_value()
        || request.singlemodel.modelpath.empty()
        || request.singlemodel.modelhash.empty()
        || request.singlemodel.sourceconfighash.empty())
    {
        return MakeError(
            DiagnosticEffectiveConfigErrorCode::SubjectInvalid,
            "identity",
            "single-model diagnostics require only complete model identity");
    }
    return std::nullopt;
}

std::optional<DiagnosticEffectiveConfigError> ValidateSceneIdentity(
    const DiagnosticEffectiveConfigRequest& request)
{
    if (!request.scene.has_value())
    {
        return MakeError(
            DiagnosticEffectiveConfigErrorCode::SceneIdentityRequired,
            "identity",
            "scene diagnostics require scene and current-instance identity");
    }
    if (!request.singlemodel.modelpath.empty()
        || !request.singlemodel.modelhash.empty()
        || !request.singlemodel.sourceconfighash.empty())
    {
        return MakeError(
            DiagnosticEffectiveConfigErrorCode::SubjectInvalid,
            "identity",
            "scene diagnostics must not carry single-model identity");
    }

    const DiagnosticSceneSelection& selection = *request.scene;
    const SceneValidationResult validation =
        ValidateMultiModelScene(
            selection.scene,
            SceneValidationPurpose::Draft);
    if (!validation.IsValid())
    {
        const SceneValidationErrorCode code =
            validation.errors.front().code;
        return MakeError(
            code == SceneValidationErrorCode::SceneRevisionStale
                    || code
                        == SceneValidationErrorCode::
                            SceneRevisionInvalid
                ? DiagnosticEffectiveConfigErrorCode::RevisionStale
                : DiagnosticEffectiveConfigErrorCode::
                    SceneIdentityRequired,
            validation.errors.front().field,
            validation.errors.front().message);
    }
    if (selection.currentmodelid.empty()
        || selection.currentinstanceid.empty())
    {
        return MakeError(
            DiagnosticEffectiveConfigErrorCode::SceneIdentityRequired,
            "identity.currentInstanceId",
            "scene current model and instance are required");
    }

    const SceneModelInstance* currentInstance = nullptr;
    for (const SceneModelInstance& sceneInstance :
         selection.scene.instances)
    {
        if (sceneInstance.instance.instanceid
            == selection.currentinstanceid)
        {
            currentInstance = &sceneInstance;
            break;
        }
    }
    if (currentInstance == nullptr
        || currentInstance->instance.modelid
            != selection.currentmodelid)
    {
        return MakeError(
            DiagnosticEffectiveConfigErrorCode::
                InstanceReferenceMissing,
            "identity.currentInstanceId",
            "current scene instance or model reference is missing");
    }
    if (selection.scene.resolvedprofileid
            != request.sourceprofileid
        || currentInstance->resolvedprofileid
            != request.sourceprofileid)
    {
        return MakeError(
            DiagnosticEffectiveConfigErrorCode::ProfileMismatch,
            "sourceProfileId",
            "diagnostic Profile must match scene and current instance");
    }
    return std::nullopt;
}

std::optional<DiagnosticEffectiveConfigError> ValidateRequest(
    const DiagnosticEffectiveConfigRequest& request)
{
    if (const auto requestedError = ValidateRequested(request);
        requestedError.has_value())
    {
        return requestedError;
    }
    switch (request.subjecttype)
    {
    case DiagnosticSubjectType::SingleModel:
        return ValidateSingleModelIdentity(request);
    case DiagnosticSubjectType::Scene:
        return ValidateSceneIdentity(request);
    }
    return MakeError(
        DiagnosticEffectiveConfigErrorCode::SubjectInvalid,
        "subjectType",
        "diagnostic subject type is unsupported");
}

Json BuildIdentity(
    const DiagnosticEffectiveConfigRequest& request)
{
    if (request.subjecttype
        == DiagnosticSubjectType::SingleModel)
    {
        return Json::object({
            {"modelPath",
             request.singlemodel.modelpath.generic_string()},
            {"modelHash", request.singlemodel.modelhash},
            {"sourceConfigHash",
             request.singlemodel.sourceconfighash},
        });
    }

    const DiagnosticSceneSelection& selection =
        *request.scene;
    const SceneModelInstance* currentInstance = nullptr;
    for (const SceneModelInstance& sceneInstance :
         selection.scene.instances)
    {
        if (sceneInstance.instance.instanceid
            == selection.currentinstanceid)
        {
            currentInstance = &sceneInstance;
            break;
        }
    }
    return Json::object({
        {"sceneId", selection.scene.sceneid},
        {"sceneRevision", selection.scene.scenerevision},
        {"sceneHash",
         ComputeMultiModelSceneHash(selection.scene)},
        {"currentModelId", selection.currentmodelid},
        {"currentInstanceId", selection.currentinstanceid},
        {"transformRevision",
         currentInstance->instance.transformrevision},
    });
}

Json BuildRequested(
    const DiagnosticRequestedSettings& requested)
{
    return Json::object({
        {"textureSurfaceWidthMm",
         requested.texturesurfacewidthmm},
        {"modelFillMaterial", requested.modelfillmaterial},
        {"diagnosticBackendRequest",
         requested.diagnosticbackendrequest},
    });
}

Json BuildDerived(
    const DiagnosticDerivedSettings& derived)
{
    return Json::object({
        {"minimumWidthMm",
         OptionalNumber(derived.minimumwidthmm)},
        {"maximumWidthMm",
         OptionalNumber(derived.maximumwidthmm)},
        {"allTextureThresholdMm",
         OptionalNumber(derived.alltexturethresholdmm)},
        {"backendAvailability",
         derived.backendavailability},
        {"derivationSource", derived.derivationsource},
    });
}

Json BuildEffective(
    const DiagnosticEffectiveSettings& effective)
{
    return Json::object({
        {"textureSurfaceWidthMm",
         effective.texturesurfacewidthmm},
        {"modelFillMaterial", effective.modelfillmaterial},
        {"diagnosticBackend", effective.diagnosticbackend},
        {"resolvedProfileId", effective.resolvedprofileid},
    });
}

std::string ComputeDocumentHash(const Json::Object& object)
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

    const bool hadOutput =
        std::filesystem::exists(outputPath);
    if (hadOutput)
    {
        std::filesystem::rename(
            outputPath,
            backupPath,
            error);
        if (error)
        {
            return false;
        }
    }

    std::filesystem::rename(
        stagingPath,
        outputPath,
        error);
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

std::optional<DiagnosticEffectiveConfigError>
ValidateDocumentShape(const Json& document)
{
    try
    {
        if (!document.is_object())
        {
            return MakeError(
                DiagnosticEffectiveConfigErrorCode::
                    IntegrityFailed,
                "document",
                "diagnostic effective config must be an object");
        }
        if (document.at("schema").as_string()
            != DiagnosticEffectiveConfigSchemaName())
        {
            return MakeError(
                DiagnosticEffectiveConfigErrorCode::
                    SchemaUnsupported,
                "schema",
                "diagnostic effective config schema is unsupported");
        }
        const std::string subjectType =
            document.at("subjectType").as_string();
        if (subjectType != "single_model"
            && subjectType != "scene")
        {
            return MakeError(
                DiagnosticEffectiveConfigErrorCode::
                    SubjectInvalid,
                "subjectType",
                "diagnostic subject type is unsupported");
        }
        static_cast<void>(
            document.at("sessionId").as_string());
        static_cast<void>(
            document.at("sourceProfileId").as_string());
        static_cast<void>(
            document.at("sourceConfigPath").as_string());
        static_cast<void>(
            document.at("generatedAtUtc").as_string());
        static_cast<void>(
            document.at("identity").as_object());
        static_cast<void>(
            document.at("requested").as_object());
        static_cast<void>(
            document.at("derived").as_object());
        static_cast<void>(
            document.at("effective").as_object());
        static_cast<void>(
            document.at("configHash").as_string());
        return std::nullopt;
    }
    catch (const std::exception& error)
    {
        return MakeError(
            DiagnosticEffectiveConfigErrorCode::
                IntegrityFailed,
            "document",
            error.what());
    }
}

Json BuildStaleProjection(const Json& document)
{
    return Json::object({
        {"schema", document.at("schema")},
        {"subjectType", document.at("subjectType")},
        {"sessionId", document.at("sessionId")},
        {"sourceProfileId",
         document.at("sourceProfileId")},
        {"sourceConfigPath",
         document.at("sourceConfigPath")},
        {"identity", document.at("identity")},
        {"requested", document.at("requested")},
        {"derived", document.at("derived")},
        {"effective", document.at("effective")},
    });
}

}  // namespace

bool DiagnosticEffectiveConfigResult::IsValid() const
{
    return !error.has_value() && !document.is_null();
}

std::string_view DiagnosticEffectiveConfigSchemaName()
{
    return
        "slicesoft.diagnostic_effective_config.12e_09a.1";
}

std::string_view DiagnosticEffectiveConfigErrorCodeName(
    const DiagnosticEffectiveConfigErrorCode code)
{
    switch (code)
    {
    case DiagnosticEffectiveConfigErrorCode::None:
        return "NONE";
    case DiagnosticEffectiveConfigErrorCode::
        SchemaUnsupported:
        return "DIAGNOSTIC_CONFIG_SCHEMA_UNSUPPORTED";
    case DiagnosticEffectiveConfigErrorCode::SubjectInvalid:
        return "DIAGNOSTIC_SUBJECT_INVALID";
    case DiagnosticEffectiveConfigErrorCode::
        SceneIdentityRequired:
        return "DIAGNOSTIC_SCENE_IDENTITY_REQUIRED";
    case DiagnosticEffectiveConfigErrorCode::
        InstanceReferenceMissing:
        return "DIAGNOSTIC_INSTANCE_REFERENCE_MISSING";
    case DiagnosticEffectiveConfigErrorCode::RevisionStale:
        return "DIAGNOSTIC_REVISION_STALE";
    case DiagnosticEffectiveConfigErrorCode::ProfileMismatch:
        return "DIAGNOSTIC_PROFILE_MISMATCH";
    case DiagnosticEffectiveConfigErrorCode::RequestInvalid:
        return "DIAGNOSTIC_REQUEST_INVALID";
    case DiagnosticEffectiveConfigErrorCode::
        DerivationUnavailable:
        return "DIAGNOSTIC_DERIVATION_UNAVAILABLE";
    case DiagnosticEffectiveConfigErrorCode::Cancelled:
        return "DIAGNOSTIC_CONFIG_CANCELLED";
    case DiagnosticEffectiveConfigErrorCode::IntegrityFailed:
        return "DIAGNOSTIC_CONFIG_INTEGRITY_FAILED";
    case DiagnosticEffectiveConfigErrorCode::WriteFailed:
        return "DIAGNOSTIC_CONFIG_WRITE_FAILED";
    }
    return "DIAGNOSTIC_CONFIG_UNKNOWN";
}

DiagnosticEffectiveConfigResult
GenerateDiagnosticEffectiveConfig(
    const DiagnosticEffectiveConfigRequest& request)
{
    if (request.cancelled)
    {
        return {
            {},
            {},
            MakeError(
                DiagnosticEffectiveConfigErrorCode::Cancelled,
                "cancelled",
                "diagnostic effective config generation was cancelled")};
    }
    if (const auto error = ValidateRequest(request);
        error.has_value())
    {
        return {{}, {}, error};
    }

    Json::Object object{
        {"schema",
         std::string(
             DiagnosticEffectiveConfigSchemaName())},
        {"subjectType",
         SubjectTypeName(request.subjecttype)},
        {"sessionId", request.sessionid},
        {"sourceProfileId", request.sourceprofileid},
        {"sourceConfigPath",
         request.sourceconfigpath.generic_string()},
        {"generatedAtUtc", request.generatedatutc},
        {"identity", BuildIdentity(request)},
        {"requested", BuildRequested(request.requested)},
        {"derived", BuildDerived(request.derived)},
        {"effective", BuildEffective(request.effective)},
    };
    const std::string configHash =
        ComputeDocumentHash(object);
    object.emplace("configHash", configHash);
    return {
        Json(std::move(object)),
        configHash,
        std::nullopt};
}

DiagnosticEffectiveConfigResult
WriteDiagnosticEffectiveConfig(
    const DiagnosticEffectiveConfigRequest& request)
{
    DiagnosticEffectiveConfigResult result =
        GenerateDiagnosticEffectiveConfig(request);
    if (!result.IsValid())
    {
        return result;
    }
    if (request.generatedconfigpath.empty()
        || request.generatedconfigpath.filename()
            != kDiagnosticConfigFilename)
    {
        result.error = MakeError(
            DiagnosticEffectiveConfigErrorCode::WriteFailed,
            "generatedConfigPath",
            "diagnostic output filename must be slice_config.diagnostic.effective.json");
        return result;
    }
    const std::filesystem::path normalizedOutput =
        NormalizedAbsolutePath(
            request.generatedconfigpath);
    if (normalizedOutput
            == NormalizedAbsolutePath(request.sourceconfigpath)
        || (request.subjecttype
                == DiagnosticSubjectType::SingleModel
            && normalizedOutput
                == NormalizedAbsolutePath(
                    request.singlemodel.modelpath)))
    {
        result.error = MakeError(
            DiagnosticEffectiveConfigErrorCode::WriteFailed,
            "generatedConfigPath",
            "diagnostic config cannot overwrite source config or model");
        return result;
    }

    std::error_code fileError;
    const std::filesystem::path parent =
        request.generatedconfigpath.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(
            parent,
            fileError);
    }
    if (fileError)
    {
        result.error = MakeError(
            DiagnosticEffectiveConfigErrorCode::WriteFailed,
            "generatedConfigPath",
            "failed to create diagnostic output directory");
        return result;
    }

    const std::filesystem::path stagingPath =
        request.generatedconfigpath.string() + ".tmp";
    std::filesystem::remove(stagingPath, fileError);
    fileError.clear();
    {
        std::ofstream output(stagingPath, std::ios::binary);
        if (!output)
        {
            result.error = MakeError(
                DiagnosticEffectiveConfigErrorCode::WriteFailed,
                "generatedConfigPath",
                "failed to open diagnostic staging file");
            return result;
        }
        output << result.document.dump(2);
        output.flush();
        if (!output)
        {
            output.close();
            std::filesystem::remove(
                stagingPath,
                fileError);
            result.error = MakeError(
                DiagnosticEffectiveConfigErrorCode::WriteFailed,
                "generatedConfigPath",
                "failed to write diagnostic staging file");
            return result;
        }
    }

    const DiagnosticEffectiveConfigResult readback =
        ReadDiagnosticEffectiveConfig(stagingPath);
    if (!readback.IsValid()
        || readback.confighash != result.confighash)
    {
        std::filesystem::remove(stagingPath, fileError);
        result.error = MakeError(
            DiagnosticEffectiveConfigErrorCode::IntegrityFailed,
            "generatedConfigPath",
            "diagnostic staging readback failed");
        return result;
    }
    if (!PublishStagedFile(
            stagingPath,
            request.generatedconfigpath))
    {
        std::filesystem::remove(stagingPath, fileError);
        result.error = MakeError(
            DiagnosticEffectiveConfigErrorCode::WriteFailed,
            "generatedConfigPath",
            "failed to atomically publish diagnostic config");
    }
    return result;
}

DiagnosticEffectiveConfigResult
ReadDiagnosticEffectiveConfig(
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
                MakeError(
                    DiagnosticEffectiveConfigErrorCode::
                        WriteFailed,
                    path.generic_string(),
                    "failed to open diagnostic effective config")};
        }
        Json document = Json::parse(input);
        if (const auto shapeError =
                ValidateDocumentShape(document);
            shapeError.has_value())
        {
            return {{}, {}, shapeError};
        }

        Json::Object hashInput = document.as_object();
        const std::string storedHash =
            hashInput.at("configHash").as_string();
        hashInput.erase("configHash");
        const std::string computedHash =
            ComputeDocumentHash(hashInput);
        if (storedHash != computedHash)
        {
            return {
                {},
                {},
                MakeError(
                    DiagnosticEffectiveConfigErrorCode::
                        IntegrityFailed,
                    "configHash",
                    "diagnostic effective config hash mismatch")};
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
            MakeError(
                DiagnosticEffectiveConfigErrorCode::
                    IntegrityFailed,
                path.generic_string(),
                error.what())};
    }
}

std::optional<DiagnosticEffectiveConfigError>
ValidateDiagnosticEffectiveConfigCurrent(
    const Json& document,
    const DiagnosticEffectiveConfigRequest& request)
{
    if (const auto shapeError =
            ValidateDocumentShape(document);
        shapeError.has_value())
    {
        return MakeError(
            DiagnosticEffectiveConfigErrorCode::RevisionStale,
            shapeError->field,
            "diagnostic effective config is not reusable");
    }
    try
    {
        Json::Object hashInput = document.as_object();
        const std::string storedHash =
            hashInput.at("configHash").as_string();
        hashInput.erase("configHash");
        if (storedHash != ComputeDocumentHash(hashInput))
        {
            return MakeError(
                DiagnosticEffectiveConfigErrorCode::RevisionStale,
                "configHash",
                "diagnostic effective config integrity changed");
        }
    }
    catch (const std::exception&)
    {
        return MakeError(
            DiagnosticEffectiveConfigErrorCode::RevisionStale,
            "configHash",
            "diagnostic effective config hash is invalid");
    }
    const DiagnosticEffectiveConfigResult current =
        GenerateDiagnosticEffectiveConfig(request);
    if (!current.IsValid())
    {
        return MakeError(
            DiagnosticEffectiveConfigErrorCode::RevisionStale,
            current.error->field,
            "current diagnostic request is invalid or stale");
    }
    try
    {
        if (BuildStaleProjection(document).dump(0)
            != BuildStaleProjection(current.document).dump(0))
        {
            return MakeError(
                DiagnosticEffectiveConfigErrorCode::RevisionStale,
                "identity",
                "diagnostic identity, revision, Profile, or settings changed");
        }
    }
    catch (const std::exception&)
    {
        return MakeError(
            DiagnosticEffectiveConfigErrorCode::RevisionStale,
            "document",
            "diagnostic effective config is incomplete");
    }
    return std::nullopt;
}

bool IsDiagnosticEffectiveConfigStale(
    const Json& document,
    const DiagnosticEffectiveConfigRequest& request)
{
    return ValidateDiagnosticEffectiveConfigCurrent(
               document,
               request)
        .has_value();
}

}  // namespace slicer_core
