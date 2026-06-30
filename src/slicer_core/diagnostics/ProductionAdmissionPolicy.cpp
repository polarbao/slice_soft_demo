#include "slicer_core/diagnostics/ProductionAdmissionPolicy.h"

#include <algorithm>

namespace slicer_core
{
namespace
{

constexpr const char* kSelfIntersectionConfirmed = "MESH_SELF_INTERSECTION_CONFIRMED";

bool ContainsCode(const std::vector<std::string>& codes, const std::string& code)
{
    return std::find(codes.begin(), codes.end(), code) != codes.end();
}

void AppendUnique(std::vector<std::string>& codes, const std::string& code)
{
    if (!ContainsCode(codes, code))
    {
        codes.push_back(code);
    }
}

bool IsStrictBlockerCode(const std::string& code)
{
    return code == "MESH_NON_MANIFOLD_EDGES"
        || code == "MESH_DUPLICATE_FACES"
        || code == "MESH_OPPOSITE_DUPLICATE_FACES"
        || code == "MESH_LOCAL_WINDING_INCONSISTENCY"
        || code == "OPENVDB_LEVEL_SET_FAILED"
        || code == "OPENVDB_UNAVAILABLE";
}

void CollectIssueCodes(
    const std::vector<ValidationIssue>& issues,
    ProductionAdmissionDecision& decision,
    const bool collectAllErrorsAsBlockers)
{
    for (const ValidationIssue& issue : issues)
    {
        if (collectAllErrorsAsBlockers && issue.severity == ValidationSeverity::Error)
        {
            AppendUnique(decision.blockerCodes, issue.code);
        }
        else
        {
            AppendUnique(decision.warningCodes, issue.code);
        }
    }
}

}  // namespace

std::string AdmissionModeName(const AdmissionMode mode)
{
    switch (mode)
    {
    case AdmissionMode::StrictClosed:
        return "strict_closed";
    case AdmissionMode::WarnAndAttempt:
        return "warn_and_attempt";
    case AdmissionMode::DiagnosticOnly:
        return "diagnostic_only";
    case AdmissionMode::RepairThenStrict:
        return "repair_then_strict";
    }
    return "diagnostic_only";
}

std::string AdmissionStatusName(const AdmissionStatus status)
{
    switch (status)
    {
    case AdmissionStatus::ProductionAllowed:
        return "production_allowed";
    case AdmissionStatus::NonProductionOnly:
        return "non_production_only";
    case AdmissionStatus::DiagnosticOnly:
        return "diagnostic_only";
    case AdmissionStatus::FailFast:
        return "fail_fast";
    }
    return "diagnostic_only";
}

ProductionAdmissionDecision EvaluateProductionAdmission(
    const std::vector<ValidationIssue>& issues,
    const AdmissionMode mode)
{
    ProductionAdmissionDecision decision;

    if (mode == AdmissionMode::WarnAndAttempt)
    {
        decision.status = AdmissionStatus::NonProductionOnly;
        decision.productionAllowed = false;
        decision.nonProduction = true;
        CollectIssueCodes(issues, decision, true);
        decision.suggestedActions.push_back("Keep warn_and_attempt output in nonProduction reports only.");
        return decision;
    }

    if (mode == AdmissionMode::DiagnosticOnly)
    {
        decision.status = AdmissionStatus::DiagnosticOnly;
        decision.productionAllowed = false;
        decision.nonProduction = true;
        CollectIssueCodes(issues, decision, true);
        decision.suggestedActions.push_back("Emit diagnostics without production package output.");
        return decision;
    }

    if (mode == AdmissionMode::RepairThenStrict)
    {
        decision.status = AdmissionStatus::NonProductionOnly;
        decision.productionAllowed = false;
        decision.nonProduction = true;
        CollectIssueCodes(issues, decision, true);
        decision.suggestedActions.push_back("Mesh repair is not implemented; run strict admission after a repair stage.");
        return decision;
    }

    for (const ValidationIssue& issue : issues)
    {
        if (issue.code == kSelfIntersectionConfirmed)
        {
            AppendUnique(decision.blockerCodes, issue.code);
            decision.status = AdmissionStatus::FailFast;
            decision.productionAllowed = false;
            decision.nonProduction = false;
            decision.suggestedActions.push_back("Reject confirmed self-intersection before production output.");
            return decision;
        }

        if (IsStrictBlockerCode(issue.code))
        {
            AppendUnique(decision.blockerCodes, issue.code);
        }
        else
        {
            AppendUnique(decision.warningCodes, issue.code);
        }
    }

    if (!decision.blockerCodes.empty())
    {
        decision.status = AdmissionStatus::NonProductionOnly;
        decision.productionAllowed = false;
        decision.nonProduction = true;
        decision.suggestedActions.push_back("Keep this model in experimental diagnostic output until blockers are resolved.");
        return decision;
    }

    decision.status = AdmissionStatus::ProductionAllowed;
    decision.productionAllowed = true;
    decision.nonProduction = false;
    return decision;
}

}  // namespace slicer_core
