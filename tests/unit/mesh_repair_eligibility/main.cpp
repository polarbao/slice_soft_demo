#include "slicer_core/geometry/repair/MeshRepairEligibilityPolicy.h"

#include <iostream>
#include <string>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

bool HasDecision(
    const slicer_core::MeshRepairEligibility& eligibility,
    const std::string& issueCode,
    const slicer_core::MeshRepairEligibilityClass classification,
    const std::string& reasonCode)
{
    for (const slicer_core::MeshRepairEligibilityDecision& decision : eligibility.decisions)
    {
        if (decision.issueCode == issueCode
            && decision.classification == classification
            && decision.reasonCode == reasonCode)
        {
            return true;
        }
    }
    return false;
}

slicer_core::MeshTopologyReport MakeCleanTopology()
{
    slicer_core::MeshTopologyReport report;
    report.source_triangles = 4U;
    report.accepted_triangles = 4U;
    report.unique_vertices = 4U;
    return report;
}

bool TestCleanMeshNeedsNoRepair()
{
    const slicer_core::MeshRepairEligibility result = slicer_core::EvaluateMeshRepairEligibility(
        MakeCleanTopology(),
        slicer_core::MeshRobustnessReport{});
    return ExpectTrue(
               result.status == slicer_core::MeshRepairStatus::StrictPassNoRepair,
               "clean mesh should be strict_pass_no_repair")
        && ExpectTrue(!result.automaticRepairAllowed, "clean mesh should not request repair")
        && ExpectTrue(result.decisions.empty(), "clean mesh should have no repair decisions");
}

bool TestExactCleanupIssuesAreEligible()
{
    slicer_core::MeshTopologyReport topology = MakeCleanTopology();
    topology.degenerate_triangles = 2U;
    slicer_core::MeshRobustnessReport robustness;
    robustness.duplicate_faces = 3U;
    slicer_core::MeshRepairEligibilityEvidence evidence;
    evidence.duplicateFaceAttributesEvaluated = true;

    const slicer_core::MeshRepairEligibility result =
        slicer_core::EvaluateMeshRepairEligibility(topology, robustness, evidence);
    return ExpectTrue(
               result.status == slicer_core::MeshRepairStatus::RepairCandidate,
               "exact cleanup should produce repair_candidate")
        && ExpectTrue(result.automaticRepairAllowed, "exact cleanup should allow automatic repair")
        && ExpectTrue(
            HasDecision(
                result,
                "MESH_DEGENERATE_TRIANGLES",
                slicer_core::MeshRepairEligibilityClass::Eligible,
                "REPAIR_ELIGIBLE_DEGENERATE_CLEANUP"),
            "degenerate decision should be eligible")
        && ExpectTrue(
            HasDecision(
                result,
                "MESH_DUPLICATE_FACES",
                slicer_core::MeshRepairEligibilityClass::Eligible,
                "REPAIR_ELIGIBLE_EXACT_DUPLICATE_REMOVAL"),
            "duplicate decision should be eligible");
}

bool TestDuplicateAttributeConflictRequiresManualRepair()
{
    slicer_core::MeshRobustnessReport robustness;
    robustness.duplicate_faces = 1U;
    slicer_core::MeshRepairEligibilityEvidence evidence;
    evidence.duplicateFaceAttributesEvaluated = true;
    evidence.duplicateFaceAttributeConflicts = 1U;

    const slicer_core::MeshRepairEligibility result = slicer_core::EvaluateMeshRepairEligibility(
        MakeCleanTopology(),
        robustness,
        evidence);
    return ExpectTrue(
               result.status == slicer_core::MeshRepairStatus::ManualRepairRequired,
               "duplicate attribute conflict should require manual repair")
        && ExpectTrue(
            HasDecision(
                result,
                "MESH_DUPLICATE_FACE_ATTRIBUTE_CONFLICT",
                slicer_core::MeshRepairEligibilityClass::ManualOnly,
                "E_12E_REPAIR_ATTRIBUTE_CONFLICT"),
            "attribute conflict should have a stable manual decision");
}

bool TestDuplicateWithoutAttributeEvidenceIsConditional()
{
    slicer_core::MeshRobustnessReport robustness;
    robustness.duplicate_faces = 1U;
    const slicer_core::MeshRepairEligibility result = slicer_core::EvaluateMeshRepairEligibility(
        MakeCleanTopology(),
        robustness);
    return ExpectTrue(
               result.status == slicer_core::MeshRepairStatus::RepairCandidate,
               "duplicate without attribute evidence remains a candidate")
        && ExpectTrue(!result.automaticRepairAllowed, "missing attribute evidence must block automatic repair")
        && ExpectTrue(
            HasDecision(
                result,
                "MESH_DUPLICATE_FACES",
                slicer_core::MeshRepairEligibilityClass::Conditional,
                "E_12E_REPAIR_ATTRIBUTE_CONFLICT"),
            "missing attribute evidence should be conditional");
}

bool TestInvalidAttributeEvidenceFailsFast()
{
    slicer_core::MeshRobustnessReport robustness;
    robustness.duplicate_faces = 1U;
    slicer_core::MeshRepairEligibilityEvidence evidence;
    evidence.duplicateFaceAttributesEvaluated = true;
    evidence.duplicateFaceAttributeConflicts = 2U;

    const slicer_core::MeshRepairEligibility result = slicer_core::EvaluateMeshRepairEligibility(
        MakeCleanTopology(),
        robustness,
        evidence);
    return ExpectTrue(
               result.status == slicer_core::MeshRepairStatus::RepairFailed,
               "invalid attribute evidence should fail eligibility")
        && ExpectTrue(
            HasDecision(
                result,
                "MESH_ELIGIBILITY_EVIDENCE_INVALID",
                slicer_core::MeshRepairEligibilityClass::FailFast,
                "E_12E_REPAIR_INPUT_INVALID"),
            "invalid evidence should have a stable fail-fast decision");
}

bool TestConditionalIssuesDoNotAutoRepair()
{
    slicer_core::MeshTopologyReport topology = MakeCleanTopology();
    topology.boundary_edges = 4U;
    slicer_core::MeshRobustnessReport robustness;
    robustness.inconsistent_oriented_edges = 2U;
    robustness.duplicate_faces = 1U;
    robustness.opposite_duplicate_faces = 1U;

    const slicer_core::MeshRepairEligibility result =
        slicer_core::EvaluateMeshRepairEligibility(topology, robustness);
    return ExpectTrue(
               result.status == slicer_core::MeshRepairStatus::RepairCandidate,
               "conditional issues should remain repair candidates")
        && ExpectTrue(!result.automaticRepairAllowed, "conditional issues require later evidence")
        && ExpectTrue(
            HasDecision(
                result,
                "MESH_BOUNDARY_EDGES",
                slicer_core::MeshRepairEligibilityClass::Conditional,
                "E_12E_REPAIR_NOT_ELIGIBLE"),
            "boundary decision should be conditional")
        && ExpectTrue(
            HasDecision(
                result,
                "MESH_LOCAL_WINDING_INCONSISTENCY",
                slicer_core::MeshRepairEligibilityClass::Conditional,
                "E_12E_REPAIR_NOT_ELIGIBLE"),
            "winding decision should be conditional")
        && ExpectTrue(
            HasDecision(
                result,
                "MESH_OPPOSITE_DUPLICATE_FACES",
                slicer_core::MeshRepairEligibilityClass::Conditional,
                "E_12E_REPAIR_AMBIGUOUS_TOPOLOGY"),
            "opposite duplicate decision should be conditional");
}

bool TestAmbiguousTopologyRequiresManualRepair()
{
    slicer_core::MeshTopologyReport topology = MakeCleanTopology();
    topology.non_manifold_edges = 5U;
    const slicer_core::MeshRepairEligibility result = slicer_core::EvaluateMeshRepairEligibility(
        topology,
        slicer_core::MeshRobustnessReport{});
    return ExpectTrue(
               result.status == slicer_core::MeshRepairStatus::ManualRepairRequired,
               "non-manifold topology should require manual repair")
        && ExpectTrue(!result.automaticRepairAllowed, "manual topology must not auto repair")
        && ExpectTrue(
            HasDecision(
                result,
                "MESH_NON_MANIFOLD_EDGES",
                slicer_core::MeshRepairEligibilityClass::ManualOnly,
                "E_12E_REPAIR_AMBIGUOUS_TOPOLOGY"),
            "non-manifold decision should be manual_only");
}

bool TestIncompleteIntersectionEvidenceRequiresManualRepair()
{
    slicer_core::MeshRobustnessReport robustness;
    robustness.self_intersection_check_sampled = true;
    const slicer_core::MeshRepairEligibility result = slicer_core::EvaluateMeshRepairEligibility(
        MakeCleanTopology(),
        robustness);
    return ExpectTrue(
               result.status == slicer_core::MeshRepairStatus::ManualRepairRequired,
               "sampled intersection evidence should require manual repair")
        && ExpectTrue(
            HasDecision(
                result,
                "MESH_SELF_INTERSECTION_SAMPLED",
                slicer_core::MeshRepairEligibilityClass::ManualOnly,
                "E_12E_REPAIR_MANUAL_REQUIRED"),
            "sampled intersection decision should be manual_only");
}

bool TestConfirmedSelfIntersectionWinsFailFastPriority()
{
    slicer_core::MeshTopologyReport topology = MakeCleanTopology();
    topology.degenerate_triangles = 1U;
    topology.non_manifold_edges = 2U;
    slicer_core::MeshRobustnessReport robustness;
    robustness.confirmed_self_intersections = 1U;
    robustness.self_intersection_pairs = 1U;

    const slicer_core::MeshRepairEligibility result =
        slicer_core::EvaluateMeshRepairEligibility(topology, robustness);
    return ExpectTrue(
               result.status == slicer_core::MeshRepairStatus::RejectedSelfIntersection,
               "self-intersection should win fail-fast priority")
        && ExpectTrue(!result.automaticRepairAllowed, "fail-fast must not auto repair")
        && ExpectTrue(
            HasDecision(
                result,
                "MESH_SELF_INTERSECTION_CONFIRMED",
                slicer_core::MeshRepairEligibilityClass::FailFast,
                "E_12E_REPAIR_SELF_INTERSECTION"),
            "self-intersection decision should be fail_fast");
}

bool TestInvalidDiagnosticsFailFast()
{
    slicer_core::MeshTopologyReport topology;
    const slicer_core::MeshRepairEligibility result = slicer_core::EvaluateMeshRepairEligibility(
        topology,
        slicer_core::MeshRobustnessReport{});
    return ExpectTrue(
               result.status == slicer_core::MeshRepairStatus::RepairFailed,
               "empty accepted mesh should fail eligibility")
        && ExpectTrue(
            HasDecision(
                result,
                "MESH_INPUT_INVALID",
                slicer_core::MeshRepairEligibilityClass::FailFast,
                "E_12E_REPAIR_INPUT_INVALID"),
            "invalid input should have stable fail-fast decision");
}

}  // namespace

int main()
{
    bool passed{true};
    passed = TestCleanMeshNeedsNoRepair() && passed;
    passed = TestExactCleanupIssuesAreEligible() && passed;
    passed = TestDuplicateAttributeConflictRequiresManualRepair() && passed;
    passed = TestDuplicateWithoutAttributeEvidenceIsConditional() && passed;
    passed = TestInvalidAttributeEvidenceFailsFast() && passed;
    passed = TestConditionalIssuesDoNotAutoRepair() && passed;
    passed = TestAmbiguousTopologyRequiresManualRepair() && passed;
    passed = TestIncompleteIntersectionEvidenceRequiresManualRepair() && passed;
    passed = TestConfirmedSelfIntersectionWinsFailFastPriority() && passed;
    passed = TestInvalidDiagnosticsFailFast() && passed;
    if (!passed)
    {
        return 1;
    }
    std::cout << "mesh repair eligibility unit tests passed\n";
    return 0;
}
