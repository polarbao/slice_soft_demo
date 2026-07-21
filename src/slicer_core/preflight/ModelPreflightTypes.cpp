#include "slicer_core/preflight/ModelPreflightTypes.h"

namespace slicer_core
{

std::string ModelPreflightStatusName(const ModelPreflightStatus status)
{
    switch (status)
    {
    case ModelPreflightStatus::NotRun:
        return "not_run";
    case ModelPreflightStatus::Pending:
        return "pending";
    case ModelPreflightStatus::Running:
        return "running";
    case ModelPreflightStatus::Passed:
        return "passed";
    case ModelPreflightStatus::Warning:
        return "warning";
    case ModelPreflightStatus::Blocked:
        return "blocked";
    case ModelPreflightStatus::Stale:
        return "stale";
    }
    return "not_run";
}

std::string ModelPreflightPipelineModeName(const ModelPreflightPipelineMode mode)
{
    switch (mode)
    {
    case ModelPreflightPipelineMode::Legacy:
        return "legacy";
    case ModelPreflightPipelineMode::GlobalSurfaceShell:
        return "global_surface_shell";
    }
    return "legacy";
}

std::string ModelPreflightAdmissionStatusName(
    const ModelPreflightAdmissionStatus status)
{
    switch (status)
    {
    case ModelPreflightAdmissionStatus::Passed:
        return "passed";
    case ModelPreflightAdmissionStatus::Warning:
        return "warning";
    case ModelPreflightAdmissionStatus::Blocked:
        return "blocked";
    }
    return "blocked";
}

std::string ModelPreflightIssueSeverityName(const ModelPreflightIssueSeverity severity)
{
    switch (severity)
    {
    case ModelPreflightIssueSeverity::Info:
        return "info";
    case ModelPreflightIssueSeverity::Warning:
        return "warning";
    case ModelPreflightIssueSeverity::Error:
        return "error";
    }
    return "info";
}

std::string ModelPreflightErrorCodeName(const ModelPreflightErrorCode code)
{
    switch (code)
    {
    case ModelPreflightErrorCode::NotRun:
        return "E_12E_PREFLIGHT_NOT_RUN";
    case ModelPreflightErrorCode::Stale:
        return "E_12E_PREFLIGHT_STALE";
    case ModelPreflightErrorCode::ImportInvalid:
        return "E_12E_PREFLIGHT_IMPORT_INVALID";
    case ModelPreflightErrorCode::ResourceMissing:
        return "E_12E_PREFLIGHT_RESOURCE_MISSING";
    case ModelPreflightErrorCode::NonFiniteGeometry:
        return "E_12E_PREFLIGHT_NON_FINITE_GEOMETRY";
    case ModelPreflightErrorCode::AuditIncomplete:
        return "E_12E_PREFLIGHT_AUDIT_INCOMPLETE";
    case ModelPreflightErrorCode::GlobalTopologyBlocked:
        return "E_12E_PREFLIGHT_GLOBAL_TOPOLOGY_BLOCKED";
    case ModelPreflightErrorCode::BackendUnavailable:
        return "E_12E_PREFLIGHT_BACKEND_UNAVAILABLE";
    }
    return "E_12E_PREFLIGHT_NOT_RUN";
}

}  // namespace slicer_core
