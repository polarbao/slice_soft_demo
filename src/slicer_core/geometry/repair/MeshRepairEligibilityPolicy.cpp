#include "slicer_core/geometry/repair/MeshRepairEligibilityPolicy.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace slicer_core
{
namespace
{

void AddDecision(
    MeshRepairEligibility& result,
    const std::string& issueCode,
    const MeshRepairEligibilityClass classification,
    const std::string& reasonCode,
    const std::uint64_t affectedCount,
    const std::string& suggestedAction)
{
    MeshRepairEligibilityDecision decision;
    decision.issueCode = issueCode;
    decision.classification = classification;
    decision.eligible = classification == MeshRepairEligibilityClass::Eligible;
    decision.reasonCode = reasonCode;
    decision.affectedCount = affectedCount;
    decision.suggestedAction = suggestedAction;
    result.decisions.push_back(std::move(decision));
}

int ClassificationPriority(const MeshRepairEligibilityClass classification)
{
    switch (classification)
    {
    case MeshRepairEligibilityClass::Eligible:
        return 0;
    case MeshRepairEligibilityClass::Conditional:
        return 1;
    case MeshRepairEligibilityClass::ManualOnly:
        return 2;
    case MeshRepairEligibilityClass::FailFast:
        return 3;
    }
    return 3;
}

MeshRepairEligibilityClass HighestClassification(const MeshRepairEligibility& result)
{
    MeshRepairEligibilityClass highest = MeshRepairEligibilityClass::Eligible;
    for (const MeshRepairEligibilityDecision& decision : result.decisions)
    {
        if (ClassificationPriority(decision.classification) > ClassificationPriority(highest))
        {
            highest = decision.classification;
        }
    }
    return highest;
}

}  // namespace

MeshRepairEligibility EvaluateMeshRepairEligibility(
    const MeshTopologyReport& topology,
    const MeshRobustnessReport& robustness)
{
    return EvaluateMeshRepairEligibility(topology, robustness, MeshRepairEligibilityEvidence{});
}

MeshRepairEligibility EvaluateMeshRepairEligibility(
    const MeshTopologyReport& topology,
    const MeshRobustnessReport& robustness,
    const MeshRepairEligibilityEvidence& evidence)
{
    MeshRepairEligibility result;

    if (topology.accepted_triangles == 0U)
    {
        AddDecision(
            result,
            "MESH_INPUT_INVALID",
            MeshRepairEligibilityClass::FailFast,
            MeshRepairErrorCodeName(MeshRepairErrorCode::InputInvalid),
            0U,
            "provide a mesh with at least one accepted triangle");
        result.status = MeshRepairStatus::RepairFailed;
        return result;
    }

    const std::size_t exactDuplicateFaces = robustness.duplicate_faces
        > robustness.opposite_duplicate_faces
        ? robustness.duplicate_faces - robustness.opposite_duplicate_faces
        : 0U;
    if ((!evidence.duplicateFaceAttributesEvaluated
         && evidence.duplicateFaceAttributeConflicts > 0U)
        || evidence.duplicateFaceAttributeConflicts > exactDuplicateFaces)
    {
        AddDecision(
            result,
            "MESH_ELIGIBILITY_EVIDENCE_INVALID",
            MeshRepairEligibilityClass::FailFast,
            MeshRepairErrorCodeName(MeshRepairErrorCode::InputInvalid),
            evidence.duplicateFaceAttributeConflicts,
            "recompute duplicate face attribute evidence for the same diagnostics");
        result.status = MeshRepairStatus::RepairFailed;
        return result;
    }

    const std::uint64_t confirmedIntersectionCount = static_cast<std::uint64_t>(
        std::max(robustness.confirmed_self_intersections, robustness.self_intersection_pairs));
    if (confirmedIntersectionCount > 0U)
    {
        AddDecision(
            result,
            "MESH_SELF_INTERSECTION_CONFIRMED",
            MeshRepairEligibilityClass::FailFast,
            MeshRepairErrorCodeName(MeshRepairErrorCode::SelfIntersection),
            confirmedIntersectionCount,
            "repair self-intersections in the source asset before retrying");
    }

    if (robustness.self_intersection_check_sampled && confirmedIntersectionCount == 0U)
    {
        AddDecision(
            result,
            "MESH_SELF_INTERSECTION_SAMPLED",
            MeshRepairEligibilityClass::ManualOnly,
            MeshRepairErrorCodeName(MeshRepairErrorCode::ManualRequired),
            static_cast<std::uint64_t>(robustness.self_intersection_candidates),
            "run complete self-intersection diagnostics before automatic repair");
    }

    if (topology.non_manifold_edges > 0U)
    {
        AddDecision(
            result,
            "MESH_NON_MANIFOLD_EDGES",
            MeshRepairEligibilityClass::ManualOnly,
            MeshRepairErrorCodeName(MeshRepairErrorCode::AmbiguousTopology),
            static_cast<std::uint64_t>(topology.non_manifold_edges),
            "classify edge-fan ownership before selecting a repair operation");
    }

    if (robustness.zero_volume_components > 0U)
    {
        AddDecision(
            result,
            "MESH_ZERO_VOLUME_COMPONENTS",
            MeshRepairEligibilityClass::ManualOnly,
            MeshRepairErrorCodeName(MeshRepairErrorCode::ManualRequired),
            static_cast<std::uint64_t>(robustness.zero_volume_components),
            "inspect zero-volume components and repair the source asset");
    }

    if (robustness.opposite_duplicate_faces > 0U)
    {
        AddDecision(
            result,
            "MESH_OPPOSITE_DUPLICATE_FACES",
            MeshRepairEligibilityClass::Conditional,
            MeshRepairErrorCodeName(MeshRepairErrorCode::AmbiguousTopology),
            static_cast<std::uint64_t>(robustness.opposite_duplicate_faces),
            "verify shell ownership and attributes before duplicate removal");
    }

    if (robustness.inconsistent_oriented_edges > 0U)
    {
        AddDecision(
            result,
            "MESH_LOCAL_WINDING_INCONSISTENCY",
            MeshRepairEligibilityClass::Conditional,
            MeshRepairErrorCodeName(MeshRepairErrorCode::Ineligible),
            static_cast<std::uint64_t>(robustness.inconsistent_oriented_edges),
            "prove a unique local winding propagation before repair");
    }

    if (topology.boundary_edges > 0U)
    {
        AddDecision(
            result,
            "MESH_BOUNDARY_EDGES",
            MeshRepairEligibilityClass::Conditional,
            MeshRepairErrorCodeName(MeshRepairErrorCode::Ineligible),
            static_cast<std::uint64_t>(topology.boundary_edges),
            "classify boundary loops and validate repair budgets");
    }

    if (topology.degenerate_triangles > 0U)
    {
        AddDecision(
            result,
            "MESH_DEGENERATE_TRIANGLES",
            MeshRepairEligibilityClass::Eligible,
            "REPAIR_ELIGIBLE_DEGENERATE_CLEANUP",
            static_cast<std::uint64_t>(topology.degenerate_triangles),
            "remove explicitly diagnosed degenerate triangles");
    }

    if (exactDuplicateFaces > 0U)
    {
        if (!evidence.duplicateFaceAttributesEvaluated)
        {
            AddDecision(
                result,
                "MESH_DUPLICATE_FACES",
                MeshRepairEligibilityClass::Conditional,
                MeshRepairErrorCodeName(MeshRepairErrorCode::AttributeMismatch),
                static_cast<std::uint64_t>(exactDuplicateFaces),
                "compare material and per-corner UV attributes before duplicate removal");
        }
        else if (evidence.duplicateFaceAttributeConflicts > 0U)
        {
            AddDecision(
                result,
                "MESH_DUPLICATE_FACE_ATTRIBUTE_CONFLICT",
                MeshRepairEligibilityClass::ManualOnly,
                MeshRepairErrorCodeName(MeshRepairErrorCode::AttributeMismatch),
                evidence.duplicateFaceAttributeConflicts,
                "resolve duplicate face material or UV conflicts in the source asset");
        }
        else
        {
            AddDecision(
                result,
                "MESH_DUPLICATE_FACES",
                MeshRepairEligibilityClass::Eligible,
                "REPAIR_ELIGIBLE_EXACT_DUPLICATE_REMOVAL",
                static_cast<std::uint64_t>(exactDuplicateFaces),
                "remove exact duplicate faces with proven equal attributes");
        }
    }

    if (result.decisions.empty())
    {
        result.status = MeshRepairStatus::StrictPassNoRepair;
        return result;
    }

    const MeshRepairEligibilityClass highest = HighestClassification(result);
    result.automaticRepairAllowed = highest == MeshRepairEligibilityClass::Eligible;
    switch (highest)
    {
    case MeshRepairEligibilityClass::Eligible:
    case MeshRepairEligibilityClass::Conditional:
        result.status = MeshRepairStatus::RepairCandidate;
        break;
    case MeshRepairEligibilityClass::ManualOnly:
        result.status = MeshRepairStatus::ManualRepairRequired;
        break;
    case MeshRepairEligibilityClass::FailFast:
        result.status = MeshRepairStatus::RejectedSelfIntersection;
        break;
    }
    return result;
}

}  // namespace slicer_core
