#pragma once

#include "slicer_core/json_value.h"
#include "slicer_core/scene/MultiModelScene.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace slicer_core
{

/**
 * @brief Subject kind bound to one immutable diagnostic configuration.
 */
enum class DiagnosticSubjectType
{
    SingleModel,
    Scene,
};

/**
 * @brief Stable failure codes for diagnostic effective-config operations.
 */
enum class DiagnosticEffectiveConfigErrorCode
{
    None,
    SchemaUnsupported,
    SubjectInvalid,
    SceneIdentityRequired,
    InstanceReferenceMissing,
    RevisionStale,
    ProfileMismatch,
    RequestInvalid,
    DerivationUnavailable,
    Cancelled,
    IntegrityFailed,
    WriteFailed,
};

/**
 * @brief Stable diagnostic effective-config failure.
 */
struct DiagnosticEffectiveConfigError
{
    DiagnosticEffectiveConfigErrorCode code{
        DiagnosticEffectiveConfigErrorCode::None};
    std::string field;
    std::string message;
};

/**
 * @brief Identity used by a single-model diagnostic session.
 */
struct DiagnosticSingleModelIdentity
{
    std::filesystem::path modelpath;
    std::string modelhash;
    std::string sourceconfighash;
};

/**
 * @brief Scene and current-instance selection used by a scene diagnostic.
 */
struct DiagnosticSceneSelection
{
    MultiModelScene scene;
    std::string currentmodelid;
    std::string currentinstanceid;
};

/**
 * @brief User-requested diagnostic settings.
 */
struct DiagnosticRequestedSettings
{
    double texturesurfacewidthmm{0.0};
    std::string modelfillmaterial;
    std::string diagnosticbackendrequest;
};

/**
 * @brief Derived diagnostic bounds where missing values remain null.
 */
struct DiagnosticDerivedSettings
{
    std::optional<double> minimumwidthmm;
    std::optional<double> maximumwidthmm;
    std::optional<double> alltexturethresholdmm;
    std::string backendavailability{"unavailable"};
    std::string derivationsource;
};

/**
 * @brief Resolved settings used by one diagnostic run.
 */
struct DiagnosticEffectiveSettings
{
    double texturesurfacewidthmm{0.0};
    std::string modelfillmaterial;
    std::string diagnosticbackend;
    std::string resolvedprofileid;
};

/**
 * @brief Request used to generate one session-scoped diagnostic config.
 */
struct DiagnosticEffectiveConfigRequest
{
    DiagnosticSubjectType subjecttype{
        DiagnosticSubjectType::SingleModel};
    std::string sessionid;
    std::string sourceprofileid;
    std::string generatedatutc;
    std::filesystem::path sourceconfigpath;
    std::filesystem::path generatedconfigpath;
    DiagnosticSingleModelIdentity singlemodel;
    std::optional<DiagnosticSceneSelection> scene;
    DiagnosticRequestedSettings requested;
    DiagnosticDerivedSettings derived;
    DiagnosticEffectiveSettings effective;
    bool cancelled{false};
};

/**
 * @brief Generated diagnostic document or one stable failure.
 */
struct DiagnosticEffectiveConfigResult
{
    Json document;
    std::string confighash;
    std::optional<DiagnosticEffectiveConfigError> error;

    /**
     * @brief Report whether generation or reading succeeded.
     * @return True when a document exists without a blocking error.
     */
    bool IsValid() const;
};

/**
 * @brief Return the canonical diagnostic effective-config schema.
 * @return Stable schema string.
 */
std::string_view DiagnosticEffectiveConfigSchemaName();

/**
 * @brief Return the stable protocol name for a diagnostic config error.
 * @param code Diagnostic effective-config error code.
 * @return Stable ASCII error name.
 */
std::string_view DiagnosticEffectiveConfigErrorCodeName(
    DiagnosticEffectiveConfigErrorCode code);

/**
 * @brief Generate an immutable diagnostic effective config in memory.
 * @param request Subject identity, diagnostic settings, and audit data.
 * @return Generated document and hash or a stable validation error.
 */
DiagnosticEffectiveConfigResult GenerateDiagnosticEffectiveConfig(
    const DiagnosticEffectiveConfigRequest& request);

/**
 * @brief Atomically publish a session diagnostic effective config.
 * @param request Generation request including the fixed output filename.
 * @return Published document or a stable write/cancel error.
 */
DiagnosticEffectiveConfigResult WriteDiagnosticEffectiveConfig(
    const DiagnosticEffectiveConfigRequest& request);

/**
 * @brief Read and verify one diagnostic effective config.
 * @param path Diagnostic effective-config path.
 * @return Parsed document or a stable schema/integrity error.
 */
DiagnosticEffectiveConfigResult ReadDiagnosticEffectiveConfig(
    const std::filesystem::path& path);

/**
 * @brief Validate whether a document still matches the current request.
 * @param document Previously generated diagnostic document.
 * @param request Current subject and diagnostic settings.
 * @return Empty when current, otherwise a stable stale error.
 */
std::optional<DiagnosticEffectiveConfigError>
ValidateDiagnosticEffectiveConfigCurrent(
    const Json& document,
    const DiagnosticEffectiveConfigRequest& request);

/**
 * @brief Check whether a diagnostic document may still be reused.
 * @param document Previously generated diagnostic document.
 * @param request Current subject and diagnostic settings.
 * @return True when identity, revision, Profile, or settings changed.
 */
bool IsDiagnosticEffectiveConfigStale(
    const Json& document,
    const DiagnosticEffectiveConfigRequest& request);

}  // namespace slicer_core
