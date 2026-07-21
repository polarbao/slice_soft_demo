#pragma once

#include "slicer_core/json_value.h"

#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Lifecycle state of a model preflight result.
 */
enum class ModelPreflightStatus
{
    NotRun,
    Pending,
    Running,
    Passed,
    Warning,
    Blocked,
    Stale,
};

/**
 * @brief Slice pipeline mode evaluated by model preflight admission.
 */
enum class ModelPreflightPipelineMode
{
    Legacy,
    GlobalSurfaceShell,
};

/**
 * @brief Mode-specific admission state derived from shared diagnostics.
 */
enum class ModelPreflightAdmissionStatus
{
    Passed,
    Warning,
    Blocked,
};

/**
 * @brief Severity of one model preflight issue.
 */
enum class ModelPreflightIssueSeverity
{
    Info,
    Warning,
    Error,
};

/**
 * @brief Stable machine-readable error codes for model preflight.
 */
enum class ModelPreflightErrorCode
{
    NotRun,
    Stale,
    ImportInvalid,
    ResourceMissing,
    NonFiniteGeometry,
    AuditIncomplete,
    GlobalTopologyBlocked,
    BackendUnavailable,
};

/**
 * @brief One backend-neutral and localizable preflight issue.
 */
struct ModelPreflightIssue
{
    std::string code;
    std::string category;
    ModelPreflightIssueSeverity severity{ModelPreflightIssueSeverity::Info};
    std::uint64_t count{0U};
    std::string summaryKey;
    std::string recommendationKey;
    Json context{Json::object({})};
};

/**
 * @brief Admission decision for one slice pipeline mode.
 */
struct ModeAdmissionResult
{
    ModelPreflightPipelineMode mode{ModelPreflightPipelineMode::Legacy};
    ModelPreflightAdmissionStatus status{ModelPreflightAdmissionStatus::Blocked};
    std::vector<std::string> blockerCodes;
    std::vector<std::string> warningCodes;
};

/**
 * @brief Inputs that uniquely identify a reusable model preflight result.
 */
struct ModelPreflightCacheIdentity
{
    std::string sourceHash;
    std::string resourceHash;
    std::string transformHash;
    std::string optionsHash;
    std::string algorithmVersion{"model_preflight_contract.1"};
};

/**
 * @brief Backend-neutral result serialized by the R4 model preflight report.
 */
struct ModelPreflightResult
{
    ModelPreflightStatus status{ModelPreflightStatus::NotRun};
    ModelPreflightCacheIdentity identity;
    std::string cacheKey;
    ModeAdmissionResult legacyAdmission;
    ModeAdmissionResult globalAdmission{
        ModelPreflightPipelineMode::GlobalSurfaceShell,
        ModelPreflightAdmissionStatus::Blocked,
        {},
        {}};
    std::vector<ModelPreflightIssue> issues;
    bool productionOutputWritten{false};
};

/**
 * @brief Convert a model preflight state to stable report text.
 * @param status Model preflight state.
 * @return Stable snake_case name.
 */
std::string ModelPreflightStatusName(ModelPreflightStatus status);

/**
 * @brief Convert a pipeline mode to stable report text.
 * @param mode Pipeline mode.
 * @return Stable snake_case name.
 */
std::string ModelPreflightPipelineModeName(ModelPreflightPipelineMode mode);

/**
 * @brief Convert an admission state to stable report text.
 * @param status Admission state.
 * @return Stable snake_case name.
 */
std::string ModelPreflightAdmissionStatusName(ModelPreflightAdmissionStatus status);

/**
 * @brief Convert an issue severity to stable report text.
 * @param severity Issue severity.
 * @return Stable lowercase name.
 */
std::string ModelPreflightIssueSeverityName(ModelPreflightIssueSeverity severity);

/**
 * @brief Convert a preflight error code to its stable E_12E_PREFLIGHT name.
 * @param code Error code.
 * @return Stable machine-readable name.
 */
std::string ModelPreflightErrorCodeName(ModelPreflightErrorCode code);

}  // namespace slicer_core
