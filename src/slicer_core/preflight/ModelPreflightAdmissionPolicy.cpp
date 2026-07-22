#include "slicer_core/preflight/ModelPreflightAdmissionPolicy.h"

#include <algorithm>
#include <string>
#include <vector>

namespace slicer_core
{
namespace
{

bool ContainsCode(const std::vector<std::string>& codes, const std::string& code)
{
    return std::find(codes.begin(), codes.end(), code) != codes.end();
}

void AppendUnique(std::vector<std::string>& codes, const std::string& code)
{
    if (!code.empty() && !ContainsCode(codes, code))
    {
        codes.push_back(code);
    }
}

void SortUnique(std::vector<std::string>& codes)
{
    std::sort(codes.begin(), codes.end());
    codes.erase(std::unique(codes.begin(), codes.end()), codes.end());
}

bool IsTopologyCompatibilityCode(const std::string& code)
{
    return code == "MESH_SELF_INTERSECTION_CONFIRMED"
        || code == "MESH_BOUNDARY_EDGES"
        || code == "MESH_NON_MANIFOLD_EDGES"
        || code == "MESH_DEGENERATE_TRIANGLES"
        || code == "MESH_DUPLICATE_FACES"
        || code == "MESH_OPPOSITE_DUPLICATE_FACES"
        || code == "MESH_DUPLICATE_FACE_ATTRIBUTE_CONFLICT"
        || code == "MESH_LOCAL_WINDING_INCONSISTENCY";
}

void AppendSharedBlocker(ModelPreflightResult& result, const std::string& code)
{
    AppendUnique(result.legacyAdmission.blockerCodes, code);
    AppendUnique(result.globalAdmission.blockerCodes, code);
}

void AppendSharedWarning(ModelPreflightResult& result, const std::string& code)
{
    AppendUnique(result.legacyAdmission.warningCodes, code);
    AppendUnique(result.globalAdmission.warningCodes, code);
}

void ApplyLifecycleBlocker(ModelPreflightResult& result)
{
    switch (result.status)
    {
    case ModelPreflightStatus::NotRun:
    case ModelPreflightStatus::Pending:
    case ModelPreflightStatus::Running:
        AppendSharedBlocker(
            result,
            ModelPreflightErrorCodeName(ModelPreflightErrorCode::NotRun));
        return;
    case ModelPreflightStatus::Stale:
        AppendSharedBlocker(
            result,
            ModelPreflightErrorCodeName(ModelPreflightErrorCode::Stale));
        return;
    case ModelPreflightStatus::Cancelled:
        AppendSharedBlocker(
            result,
            ModelPreflightErrorCodeName(ModelPreflightErrorCode::Cancelled));
        return;
    case ModelPreflightStatus::Passed:
    case ModelPreflightStatus::Warning:
    case ModelPreflightStatus::Blocked:
        return;
    }
}

void FinalizeAdmission(ModeAdmissionResult& admission)
{
    SortUnique(admission.blockerCodes);
    SortUnique(admission.warningCodes);

    if (!admission.blockerCodes.empty())
    {
        admission.status = ModelPreflightAdmissionStatus::Blocked;
    }
    else if (!admission.warningCodes.empty())
    {
        admission.status = ModelPreflightAdmissionStatus::Warning;
    }
    else
    {
        admission.status = ModelPreflightAdmissionStatus::Passed;
    }
}

}  // namespace

ModelPreflightResult EvaluateModelPreflightAdmissions(
    const ModelPreflightResult& diagnosticResult,
    const ModelPreflightAdmissionContext& context)
{
    ModelPreflightResult result = diagnosticResult;
    result.legacyAdmission = ModeAdmissionResult{};
    result.legacyAdmission.mode = ModelPreflightPipelineMode::Legacy;
    result.globalAdmission = ModeAdmissionResult{};
    result.globalAdmission.mode = ModelPreflightPipelineMode::GlobalSurfaceShell;
    result.productionOutputWritten = false;

    ApplyLifecycleBlocker(result);

    bool hasTopologyBlocker{false};
    for (const ModelPreflightIssue& issue : result.issues)
    {
        if (IsTopologyCompatibilityCode(issue.code))
        {
            AppendUnique(result.legacyAdmission.warningCodes, issue.code);
            AppendUnique(result.globalAdmission.blockerCodes, issue.code);
            hasTopologyBlocker = true;
            continue;
        }

        if (issue.severity == ModelPreflightIssueSeverity::Error)
        {
            AppendSharedBlocker(result, issue.code);
        }
        else if (issue.severity == ModelPreflightIssueSeverity::Warning)
        {
            AppendSharedWarning(result, issue.code);
        }
    }

    if (hasTopologyBlocker)
    {
        AppendUnique(
            result.globalAdmission.blockerCodes,
            ModelPreflightErrorCodeName(
                ModelPreflightErrorCode::GlobalTopologyBlocked));
    }

    if (!context.global_backend_available)
    {
        AppendUnique(
            result.globalAdmission.blockerCodes,
            ModelPreflightErrorCodeName(
                ModelPreflightErrorCode::BackendUnavailable));
    }

    const bool inconsistentBlockedResult =
        result.status == ModelPreflightStatus::Blocked
        && result.legacyAdmission.blockerCodes.empty();
    const bool warningWithoutEvidence =
        result.status == ModelPreflightStatus::Warning
        && result.legacyAdmission.blockerCodes.empty()
        && result.legacyAdmission.warningCodes.empty();
    if (inconsistentBlockedResult || warningWithoutEvidence)
    {
        AppendSharedBlocker(
            result,
            ModelPreflightErrorCodeName(
                ModelPreflightErrorCode::AuditIncomplete));
    }

    FinalizeAdmission(result.legacyAdmission);
    FinalizeAdmission(result.globalAdmission);
    return result;
}

}  // namespace slicer_core
