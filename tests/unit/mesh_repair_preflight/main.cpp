#include "slicer_core/diagnostics/MeshRepairReport.h"
#include "slicer_core/geometry/MeshScaleTolerance.h"
#include "slicer_core/geometry/TriangleMeshData.h"
#include "slicer_core/geometry/repair/MeshRepairHash.h"
#include "slicer_core/geometry/repair/MeshRepairPreflight.h"

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

slicer_core::AdaptedTriangleMesh MakeAttributedBox()
{
    slicer_core::AdaptedTriangleMesh mesh;
    mesh.mesh = slicer_core::MakeGeneratedBoxMesh(1.0, 2.0, 0.5);
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);
    mesh.triangle_attributes.resize(mesh.mesh.triangles.size());
    for (std::size_t index{0U}; index < mesh.triangle_attributes.size(); ++index)
    {
        slicer_core::SurfaceTriangleAttributes& attributes =
            mesh.triangle_attributes.at(index);
        attributes.source_triangle_index = index;
        attributes.has_uv = true;
        attributes.material_name = "material-a";
        attributes.uv = {
            slicer_core::TexCoord{0.0, 0.0},
            slicer_core::TexCoord{1.0, 0.0},
            slicer_core::TexCoord{0.0, 1.0}};
    }

    slicer_core::MaterialInfo material;
    material.name = "material-a";
    material.has_texture = true;
    material.texture_exists = true;
    material.diffuse_texture_path = "textures/color.png";
    mesh.material_infos.push_back(material);
    return mesh;
}

slicer_core::MeshRepairPreflightRequest MakeRequest(
    const slicer_core::AdaptedTriangleMesh& mesh)
{
    slicer_core::MeshRepairPreflightRequest request;
    request.mesh = &mesh;
    request.input.sourcePath = "model/fixture.obj";
    request.input.inputFormat = "obj";
    request.options.enabled = false;
    request.options.mode = "strict_closed";
    request.sourceHash = "0123456789abcdef";
    request.robustnessOptions.tolerance =
        slicer_core::MakeMeshScaleTolerance(mesh.mesh.bbox_mm, 0.10);
    request.robustnessOptions.max_triangle_pair_checks = 100000U;
    return request;
}

bool HasDecision(
    const slicer_core::MeshRepairEligibility& eligibility,
    const std::string& issueCode,
    const std::string& reasonCode)
{
    for (const slicer_core::MeshRepairEligibilityDecision& decision : eligibility.decisions)
    {
        if (decision.issueCode == issueCode && decision.reasonCode == reasonCode)
        {
            return true;
        }
    }
    return false;
}

bool TestClosedMeshProducesAuditableNoRepairResult()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    const slicer_core::MeshRepairResult result =
        slicer_core::EvaluateMeshRepairPreflight(MakeRequest(mesh));
    const slicer_core::Json report = slicer_core::BuildMeshRepairReport(result);

    return ExpectTrue(
               result.status == slicer_core::MeshRepairStatus::StrictPassNoRepair,
               "closed mesh should pass without repair")
        && ExpectTrue(result.preRepair.available, "pre-repair diagnostics should be available")
        && ExpectTrue(result.preRepair.strictPass, "closed mesh should pass strict diagnostics")
        && ExpectTrue(result.input.vertexCount == mesh.mesh.vertices.size(), "vertex count should be derived")
        && ExpectTrue(result.input.triangleCount == mesh.mesh.triangles.size(), "triangle count should be derived")
        && ExpectTrue(result.input.componentCount == 1U, "component count should be derived")
        && ExpectTrue(result.input.materialCount == 1U, "material count should be derived")
        && ExpectTrue(result.input.textureResourceCount == 1U, "texture count should be derived")
        && ExpectTrue(result.hashes.sourceHash == "0123456789abcdef", "source hash should be retained")
        && ExpectTrue(result.hashes.preRepairGeometryHash.has_value(), "geometry hash should be available")
        && ExpectTrue(result.hashes.preRepairAttributeHash.has_value(), "attribute hash should be available")
        && ExpectTrue(!result.repairAttempted, "preflight must not attempt repair")
        && ExpectTrue(!result.productionOutputWritten, "preflight must not write production output")
        && ExpectTrue(!result.admission.productionAllowed, "preflight must remain non-production")
        && ExpectTrue(report.at("schema").as_string() == "slicesoft.mesh_repair.12e_08c.1", "report schema should be stable");
}

bool TestDuplicateAttributeConflictIsDetectedWithoutMutation()
{
    slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    mesh.mesh.triangles.push_back(mesh.mesh.triangles.front());
    slicer_core::SurfaceTriangleAttributes conflicting = mesh.triangle_attributes.front();
    conflicting.source_triangle_index = mesh.triangle_attributes.size();
    conflicting.uv.at(0).u = 0.25;
    mesh.triangle_attributes.push_back(conflicting);
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);

    const std::string geometryBefore = slicer_core::ComputeMeshRepairGeometryHash(mesh.mesh);
    const std::string attributesBefore = slicer_core::ComputeMeshRepairAttributeHash(mesh);
    const slicer_core::MeshRepairResult result =
        slicer_core::EvaluateMeshRepairPreflight(MakeRequest(mesh));

    return ExpectTrue(
               result.status == slicer_core::MeshRepairStatus::ManualRepairRequired,
               "attribute conflict should require manual repair")
        && ExpectTrue(
            HasDecision(
                result.eligibility,
                "MESH_DUPLICATE_FACE_ATTRIBUTE_CONFLICT",
                "E_12E_REPAIR_ATTRIBUTE_CONFLICT"),
            "duplicate attribute conflict should have a stable decision")
        && ExpectTrue(
            geometryBefore == slicer_core::ComputeMeshRepairGeometryHash(mesh.mesh),
            "preflight should not mutate geometry")
        && ExpectTrue(
            attributesBefore == slicer_core::ComputeMeshRepairAttributeHash(mesh),
            "preflight should not mutate attributes")
        && ExpectTrue(result.operations.empty(), "preflight should not create repair operations")
        && ExpectTrue(!result.preRepair.strictPass, "conflicting duplicate should not pass strict diagnostics");
}

bool TestExplicitNonManifoldClassifierFeedsEligibility()
{
    slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    mesh.mesh.triangles.push_back(mesh.mesh.triangles.front());
    slicer_core::SurfaceTriangleAttributes duplicate = mesh.triangle_attributes.front();
    duplicate.source_triangle_index = mesh.triangle_attributes.size();
    mesh.triangle_attributes.push_back(duplicate);
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);

    slicer_core::MeshRepairPreflightRequest request = MakeRequest(mesh);
    request.options.classifyNonManifoldPatterns = true;
    const slicer_core::MeshRepairResult result =
        slicer_core::EvaluateMeshRepairPreflight(request);

    return ExpectTrue(
               result.nonManifoldAnalysis.status == "classified",
               "explicit classifier should produce an aggregate status")
        && ExpectTrue(
            result.nonManifoldAnalysis.nonManifoldEdgeCount == 3U,
            "duplicate box face should classify three non-manifold edges")
        && ExpectTrue(
            result.nonManifoldAnalysis.duplicateShellOrExporterDuplicateEdges == 3U,
            "duplicate box face should use duplicate exporter pattern")
        && ExpectTrue(
            HasDecision(
                result.eligibility,
                "MESH_NON_MANIFOLD_EDGES",
                "E_12E_REPAIR_AMBIGUOUS_TOPOLOGY"),
            "classifier should keep non-unique fan evidence manual");
}

bool TestExplicitCompleteIntersectionAuditReplacesSampledEvidence()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    slicer_core::MeshRepairPreflightRequest request = MakeRequest(mesh);
    request.robustnessOptions.max_triangle_pair_checks = 0U;
    request.options.analyzeCompleteSelfIntersections = true;
    request.options.maxCompleteSelfIntersectionCandidatePairs = 1000U;
    const slicer_core::MeshRepairResult result =
        slicer_core::EvaluateMeshRepairPreflight(request);
    const slicer_core::Json report = slicer_core::BuildMeshRepairReport(result);

    return ExpectTrue(
               result.completeSelfIntersectionAnalysis.status
                   == "complete_no_intersection",
               "complete audit status")
        && ExpectTrue(
            result.completeSelfIntersectionAnalysis.complete,
            "complete audit must replace sampled evidence")
        && ExpectTrue(
            !HasDecision(
                result.eligibility,
                "MESH_SELF_INTERSECTION_SAMPLED",
                "E_12E_REPAIR_MANUAL_REQUIRED"),
            "sampled decision must be removed")
        && ExpectTrue(
            result.status == slicer_core::MeshRepairStatus::StrictPassNoRepair,
            "closed mesh passes after complete audit")
        && ExpectTrue(
            report.at("completeSelfIntersectionAnalysis").at("complete").as_bool(),
            "report exposes complete audit");
}

bool TestCompleteIntersectionBudgetRemainsManual()
{
    slicer_core::AdaptedTriangleMesh mesh;
    mesh.mesh.vertices = {
        {0.0, 0.0, 0.0},
        {2.0, 0.0, 0.0},
        {0.0, 2.0, 0.0},
        {0.5, 0.5, 0.0},
        {1.5, 0.5, 0.0},
        {0.5, 1.5, 0.0},
    };
    mesh.mesh.triangles = {{0, 1, 2}, {3, 4, 5}};
    mesh.mesh.bbox_mm.min = {0.0, 0.0, 0.0};
    mesh.mesh.bbox_mm.max = {2.0, 2.0, 0.1};
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);
    mesh.triangle_attributes.resize(2U);
    for (std::size_t index{0U}; index < mesh.triangle_attributes.size(); ++index)
    {
        mesh.triangle_attributes.at(index).source_triangle_index = index;
        mesh.triangle_attributes.at(index).material_name = "material-a";
    }

    slicer_core::MeshRepairPreflightRequest request = MakeRequest(mesh);
    request.options.analyzeCompleteSelfIntersections = true;
    request.options.maxCompleteSelfIntersectionCandidatePairs = 0U;
    const slicer_core::MeshRepairResult result =
        slicer_core::EvaluateMeshRepairPreflight(request);

    return ExpectTrue(
               result.completeSelfIntersectionAnalysis.status
                   == "budget_or_resource_blocked",
               "budget audit status")
        && ExpectTrue(
            !result.completeSelfIntersectionAnalysis.complete,
            "budget audit remains incomplete")
        && ExpectTrue(
            HasDecision(
                result.eligibility,
                "MESH_SELF_INTERSECTION_BUDGET_BLOCKED",
                "E_12E_REPAIR_BUDGET_EXCEEDED"),
            "budget audit remains manual")
        && ExpectTrue(
            result.status == slicer_core::MeshRepairStatus::ManualRepairRequired,
            "budget result cannot pass strict");
}

bool TestMissingMeshUsesStableError()
{
    try
    {
        static_cast<void>(
            slicer_core::EvaluateMeshRepairPreflight(
                slicer_core::MeshRepairPreflightRequest{}));
    }
    catch (const slicer_core::MeshRepairError& error)
    {
        return ExpectTrue(
            error.Code() == slicer_core::MeshRepairErrorCode::InputInvalid,
            "missing mesh should use InputInvalid");
    }
    return ExpectTrue(false, "missing mesh should throw MeshRepairError");
}

}  // namespace

int main()
{
    const bool passed = TestClosedMeshProducesAuditableNoRepairResult()
        && TestDuplicateAttributeConflictIsDetectedWithoutMutation()
        && TestExplicitNonManifoldClassifierFeedsEligibility()
        && TestExplicitCompleteIntersectionAuditReplacesSampledEvidence()
        && TestCompleteIntersectionBudgetRemainsManual()
        && TestMissingMeshUsesStableError();
    if (!passed)
    {
        return 1;
    }
    std::cout << "mesh_repair_preflight_unit_tests: PASS\n";
    return 0;
}
