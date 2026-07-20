#include "slicer_core/geometry/repair/MeshRepairTypes.h"

namespace slicer_core
{

MeshRepairError::MeshRepairError(
    const MeshRepairErrorCode code,
    const std::string& message)
    : std::runtime_error(message)
    , m_code(code)
{
}

MeshRepairErrorCode MeshRepairError::Code() const noexcept
{
    return m_code;
}

std::string MeshRepairStatusName(const MeshRepairStatus status)
{
    switch (status)
    {
    case MeshRepairStatus::NotEvaluated:
        return "not_evaluated";
    case MeshRepairStatus::StrictPassNoRepair:
        return "strict_pass_no_repair";
    case MeshRepairStatus::RepairCandidate:
        return "repair_candidate";
    case MeshRepairStatus::RepairedStrictPass:
        return "repaired_strict_pass";
    case MeshRepairStatus::ManualRepairRequired:
        return "manual_repair_required";
    case MeshRepairStatus::RejectedSelfIntersection:
        return "rejected_self_intersection";
    case MeshRepairStatus::RepairFailed:
        return "repair_failed";
    case MeshRepairStatus::DiagnosticOnly:
        return "diagnostic_only";
    }
    return "not_evaluated";
}

std::string MeshRepairEligibilityClassName(const MeshRepairEligibilityClass classification)
{
    switch (classification)
    {
    case MeshRepairEligibilityClass::Eligible:
        return "eligible";
    case MeshRepairEligibilityClass::Conditional:
        return "conditional";
    case MeshRepairEligibilityClass::ManualOnly:
        return "manual_only";
    case MeshRepairEligibilityClass::FailFast:
        return "fail_fast";
    }
    return "manual_only";
}

std::string MeshRepairOperationTypeName(const MeshRepairOperationType type)
{
    switch (type)
    {
    case MeshRepairOperationType::RemoveDegenerateTriangle:
        return "remove_degenerate_triangle";
    case MeshRepairOperationType::RemoveExactDuplicateFace:
        return "remove_exact_duplicate_face";
    case MeshRepairOperationType::WeldVertex:
        return "weld_vertex";
    case MeshRepairOperationType::FlipTriangleWinding:
        return "flip_triangle_winding";
    case MeshRepairOperationType::StitchBoundaryLoop:
        return "stitch_boundary_loop";
    case MeshRepairOperationType::FillBoundaryLoop:
        return "fill_boundary_loop";
    case MeshRepairOperationType::SplitEdgeFan:
        return "split_edge_fan";
    }
    return "remove_degenerate_triangle";
}

std::string MeshRepairAttributeDecisionName(const MeshRepairAttributeDecision decision)
{
    switch (decision)
    {
    case MeshRepairAttributeDecision::NotEvaluated:
        return "not_evaluated";
    case MeshRepairAttributeDecision::Preserved:
        return "preserved";
    case MeshRepairAttributeDecision::GeneratedByPolicy:
        return "generated_by_policy";
    case MeshRepairAttributeDecision::Conflict:
        return "conflict";
    }
    return "not_evaluated";
}

std::string MeshRepairErrorCodeName(const MeshRepairErrorCode code)
{
    switch (code)
    {
    case MeshRepairErrorCode::InputInvalid:
        return "E_12E_REPAIR_INPUT_INVALID";
    case MeshRepairErrorCode::RepairNotEnabled:
        return "E_12E_REPAIR_NOT_ENABLED";
    case MeshRepairErrorCode::Ineligible:
        return "E_12E_REPAIR_NOT_ELIGIBLE";
    case MeshRepairErrorCode::AmbiguousTopology:
        return "E_12E_REPAIR_AMBIGUOUS_TOPOLOGY";
    case MeshRepairErrorCode::BudgetExceeded:
        return "E_12E_REPAIR_BUDGET_EXCEEDED";
    case MeshRepairErrorCode::AttributeMismatch:
        return "E_12E_REPAIR_ATTRIBUTE_CONFLICT";
    case MeshRepairErrorCode::PostStrictFailed:
        return "E_12E_REPAIR_POST_STRICT_FAILED";
    case MeshRepairErrorCode::SelfIntersection:
        return "E_12E_REPAIR_SELF_INTERSECTION";
    case MeshRepairErrorCode::HashNondeterministic:
        return "E_12E_REPAIR_HASH_NONDETERMINISTIC";
    case MeshRepairErrorCode::ManualRequired:
        return "E_12E_REPAIR_MANUAL_REQUIRED";
    case MeshRepairErrorCode::OperationInvalid:
        return "E_12E_REPAIR_OPERATION_INVALID";
    }
    return "E_12E_REPAIR_INPUT_INVALID";
}

}  // namespace slicer_core
