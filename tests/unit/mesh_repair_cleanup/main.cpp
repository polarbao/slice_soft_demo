#include "slicer_core/diagnostics/MeshRepairReport.h"
#include "slicer_core/geometry/MeshScaleTolerance.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/TriangleMeshData.h"
#include "slicer_core/geometry/repair/MeshRepairHash.h"
#include "slicer_core/geometry/repair/MeshRepairService.h"

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

slicer_core::SurfaceTriangleAttributes MakeAttributes(const std::size_t sourceIndex)
{
    slicer_core::SurfaceTriangleAttributes attributes;
    attributes.source_triangle_index = sourceIndex;
    attributes.has_uv = true;
    attributes.material_name = "fixture-material";
    attributes.uv = {
        slicer_core::TexCoord{0.0, 0.0},
        slicer_core::TexCoord{1.0, 0.0},
        slicer_core::TexCoord{0.0, 1.0}};
    return attributes;
}

slicer_core::AdaptedTriangleMesh MakeAttributedBox()
{
    slicer_core::AdaptedTriangleMesh adapted;
    adapted.mesh = slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    for (std::size_t index{0U}; index < adapted.mesh.triangles.size(); ++index)
    {
        adapted.triangle_attributes.push_back(MakeAttributes(index));
    }
    adapted.topology = slicer_core::AnalyzeMeshTopology(adapted.mesh);
    return adapted;
}

slicer_core::MeshRepairCleanupRequest MakeRequest(
    const slicer_core::AdaptedTriangleMesh& mesh)
{
    slicer_core::MeshRepairCleanupRequest request;
    request.mesh = &mesh;
    request.input.sourcePath = "generated/cleanup.obj";
    request.input.inputFormat = "generated";
    request.options.enabled = true;
    request.options.mode = "repair_then_strict";
    request.sourceHash = "fixture-source-hash";
    request.robustnessOptions.tolerance = slicer_core::MakeMeshScaleTolerance(
        mesh.mesh.bbox_mm,
        0.10);
    request.robustnessOptions.max_triangle_pair_checks = 100000U;
    return request;
}

const slicer_core::MeshRepairTriangleMapping* FindMapping(
    const slicer_core::MeshRepairResult& evidence,
    const std::uint64_t sourceTriangleIndex)
{
    for (const slicer_core::MeshRepairTriangleMapping& mapping : evidence.sourceMappings)
    {
        if (mapping.sourceTriangleIndex == sourceTriangleIndex)
        {
            return &mapping;
        }
    }
    return nullptr;
}

bool TestAdapterRecordsFilteredDegenerateSourceTriangle()
{
    slicer_core::SceneModel scene;
    scene.model_path = "generated/adapter.obj";
    const slicer_core::TriangleMeshData box =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    scene.bbox_mm = box.bbox_mm;
    scene.triangles.push_back({box.vertices.at(0U), box.vertices.at(0U), box.vertices.at(1U)});
    scene.triangle_textures.push_back({});
    for (const std::array<int, 3>& triangle : box.triangles)
    {
        scene.triangles.push_back({
            box.vertices.at(static_cast<std::size_t>(triangle.at(0))),
            box.vertices.at(static_cast<std::size_t>(triangle.at(1))),
            box.vertices.at(static_cast<std::size_t>(triangle.at(2)))});
        scene.triangle_textures.push_back({});
    }

    const slicer_core::AdaptedTriangleMesh adapted =
        slicer_core::AdaptSceneModelToTriangleMesh(scene);
    return ExpectTrue(adapted.topology.degenerate_triangles == 1U, "adapter should count one degenerate")
        && ExpectTrue(
            adapted.rejected_degenerate_source_triangle_indices.size() == 1U,
            "adapter should retain one rejected source id")
        && ExpectTrue(
            adapted.rejected_degenerate_source_triangle_indices.front() == 0U,
            "adapter should retain the original source triangle id")
        && ExpectTrue(
            adapted.triangle_attributes.front().source_triangle_index == 1U,
            "accepted mapping should retain its shifted source id");
}

bool TestCleanupRemovesDegenerateAndExactDuplicate()
{
    slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    mesh.rejected_degenerate_source_triangle_indices = {12U};
    mesh.mesh.triangles.push_back(mesh.mesh.triangles.front());
    mesh.triangle_attributes.push_back(MakeAttributes(13U));
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);
    mesh.topology.source_triangles = 14U;
    mesh.topology.degenerate_triangles = 1U;

    const slicer_core::MeshRepairCleanupResult cleanup =
        slicer_core::ExecuteMeshRepairCleanup(MakeRequest(mesh));
    const slicer_core::MeshRepairResult& evidence = cleanup.evidence;
    const slicer_core::MeshRepairTriangleMapping* degenerate = FindMapping(evidence, 12U);
    const slicer_core::MeshRepairTriangleMapping* duplicate = FindMapping(evidence, 13U);

    return ExpectTrue(cleanup.candidate.mesh.triangles.size() == 12U, "cleanup should retain the box faces")
        && ExpectTrue(evidence.status == slicer_core::MeshRepairStatus::RepairedStrictPass, "cleanup should pass strict")
        && ExpectTrue(evidence.repairAttempted, "cleanup should be recorded as attempted")
        && ExpectTrue(evidence.operations.size() == 2U, "cleanup should record two operations")
        && ExpectTrue(
            evidence.operations.at(0U).type == slicer_core::MeshRepairOperationType::RemoveDegenerateTriangle,
            "degenerate cleanup should execute first")
        && ExpectTrue(
            evidence.operations.at(1U).type == slicer_core::MeshRepairOperationType::RemoveExactDuplicateFace,
            "duplicate cleanup should execute second")
        && ExpectTrue(degenerate != nullptr, "degenerate source mapping should exist")
        && ExpectTrue(
            degenerate != nullptr
                && degenerate->disposition == slicer_core::MeshRepairTriangleDisposition::RemovedDegenerate,
            "degenerate mapping should explain removal")
        && ExpectTrue(duplicate != nullptr, "duplicate source mapping should exist")
        && ExpectTrue(
            duplicate != nullptr
                && duplicate->disposition == slicer_core::MeshRepairTriangleDisposition::RemovedExactDuplicate,
            "duplicate mapping should explain removal")
        && ExpectTrue(
            duplicate != nullptr && duplicate->retainedSourceTriangleIndex == 0U,
            "duplicate mapping should identify the retained source")
        && ExpectTrue(evidence.sourceMappings.size() == 14U, "every source triangle should be mapped")
        && ExpectTrue(evidence.attributePreservation.pass, "retained attributes should pass")
        && ExpectTrue(evidence.attributePreservation.sourceMappedTriangles == 12U, "all output faces should map")
        && ExpectTrue(evidence.postRepair.strictPass, "post cleanup diagnostics should pass")
        && ExpectTrue(evidence.hashes.postRepairGeometryHash.has_value(), "post geometry hash should exist")
        && ExpectTrue(evidence.hashes.postRepairAttributeHash.has_value(), "post attribute hash should exist")
        && ExpectTrue(evidence.hashes.repairOperationHash.has_value(), "operation hash should exist")
        && ExpectTrue(!evidence.productionOutputWritten, "cleanup must not write production output");
}

bool TestAttributeConflictBlocksCleanup()
{
    slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    mesh.mesh.triangles.push_back(mesh.mesh.triangles.front());
    slicer_core::SurfaceTriangleAttributes conflict = MakeAttributes(12U);
    conflict.uv.at(0U).u = 0.25;
    mesh.triangle_attributes.push_back(conflict);
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);
    mesh.topology.source_triangles = 13U;
    const std::string geometryBefore = slicer_core::ComputeMeshRepairGeometryHash(mesh.mesh);

    const slicer_core::MeshRepairCleanupResult cleanup =
        slicer_core::ExecuteMeshRepairCleanup(MakeRequest(mesh));
    return ExpectTrue(
               cleanup.evidence.status == slicer_core::MeshRepairStatus::ManualRepairRequired,
               "attribute conflict should remain manual")
        && ExpectTrue(!cleanup.evidence.repairAttempted, "blocked cleanup must not mutate")
        && ExpectTrue(cleanup.evidence.operations.empty(), "blocked cleanup must not record executed operations")
        && ExpectTrue(
            cleanup.evidence.attributePreservation.status == "blocked_attribute_conflict",
            "attribute conflict should be explicit")
        && ExpectTrue(
            slicer_core::ComputeMeshRepairGeometryHash(cleanup.candidate.mesh) == geometryBefore,
            "blocked candidate should retain original geometry");
}

bool TestOppositeDuplicateIsNotRemoved()
{
    slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    const std::array<int, 3> first = mesh.mesh.triangles.front();
    mesh.mesh.triangles.push_back({first.at(0U), first.at(2U), first.at(1U)});
    slicer_core::SurfaceTriangleAttributes attributes = MakeAttributes(12U);
    std::swap(attributes.uv.at(1U), attributes.uv.at(2U));
    mesh.triangle_attributes.push_back(attributes);
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);
    mesh.topology.source_triangles = 13U;

    const slicer_core::MeshRepairCleanupResult cleanup =
        slicer_core::ExecuteMeshRepairCleanup(MakeRequest(mesh));
    return ExpectTrue(cleanup.candidate.mesh.triangles.size() == 13U, "opposite duplicate must remain")
        && ExpectTrue(!cleanup.evidence.repairAttempted, "opposite duplicate is outside R2-01")
        && ExpectTrue(
            cleanup.evidence.status == slicer_core::MeshRepairStatus::ManualRepairRequired,
            "opposite duplicate should remain manual");
}

bool TestConfirmedSelfIntersectionTakesPriorityOverAttributeConflict()
{
    slicer_core::AdaptedTriangleMesh mesh;
    mesh.mesh.source_name = "generated-self-intersection";
    mesh.mesh.vertices = {
        {-1.0, -1.0, 0.0},
        {1.0, -1.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, -0.5, -1.0},
        {0.0, -0.5, 1.0},
        {0.0, 0.5, 0.0}};
    mesh.mesh.triangles = {{0, 1, 2}, {3, 4, 5}, {0, 1, 2}};
    mesh.mesh.bbox_mm.min = {-1.0, -1.0, -1.0};
    mesh.mesh.bbox_mm.max = {1.0, 1.0, 1.0};
    mesh.triangle_attributes = {
        MakeAttributes(0U),
        MakeAttributes(1U),
        MakeAttributes(2U)};
    mesh.triangle_attributes.back().uv.at(0U).u = 0.25;
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);

    const slicer_core::MeshRepairCleanupResult cleanup =
        slicer_core::ExecuteMeshRepairCleanup(MakeRequest(mesh));
    return ExpectTrue(
               cleanup.evidence.status == slicer_core::MeshRepairStatus::RejectedSelfIntersection,
               "confirmed self-intersection should retain fail-fast priority")
        && ExpectTrue(!cleanup.evidence.repairAttempted, "fail-fast must not execute cleanup")
        && ExpectTrue(cleanup.evidence.operations.empty(), "fail-fast must not record operations");
}

bool TestCleanupEvidenceIsDeterministic()
{
    slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    mesh.mesh.triangles.push_back(mesh.mesh.triangles.front());
    mesh.triangle_attributes.push_back(MakeAttributes(12U));
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);
    mesh.topology.source_triangles = 13U;

    const slicer_core::MeshRepairCleanupResult first =
        slicer_core::ExecuteMeshRepairCleanup(MakeRequest(mesh));
    const slicer_core::MeshRepairCleanupResult second =
        slicer_core::ExecuteMeshRepairCleanup(MakeRequest(mesh));
    const slicer_core::Json firstReport = slicer_core::BuildMeshRepairReport(first.evidence);
    const slicer_core::Json secondReport = slicer_core::BuildMeshRepairReport(second.evidence);

    return ExpectTrue(
               first.evidence.hashes.repairOperationHash == second.evidence.hashes.repairOperationHash,
               "operation hash should repeat")
        && ExpectTrue(
            first.evidence.hashes.postRepairGeometryHash == second.evidence.hashes.postRepairGeometryHash,
            "post geometry hash should repeat")
        && ExpectTrue(
            firstReport.at("sourceMappings").dump(0) == secondReport.at("sourceMappings").dump(0),
            "source mapping report should repeat");
}

bool TestDisabledCleanupUsesStableError()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    slicer_core::MeshRepairCleanupRequest request = MakeRequest(mesh);
    request.options.enabled = false;
    try
    {
        static_cast<void>(slicer_core::ExecuteMeshRepairCleanup(request));
    }
    catch (const slicer_core::MeshRepairError& error)
    {
        return ExpectTrue(
            error.Code() == slicer_core::MeshRepairErrorCode::RepairNotEnabled,
            "disabled cleanup should use RepairNotEnabled");
    }
    return ExpectTrue(false, "disabled cleanup should throw");
}

bool TestInvalidTriangleIndexUsesStableError()
{
    slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    mesh.mesh.triangles.front().at(0U) = 999;
    try
    {
        static_cast<void>(slicer_core::ExecuteMeshRepairCleanup(MakeRequest(mesh)));
    }
    catch (const slicer_core::MeshRepairError& error)
    {
        return ExpectTrue(
            error.Code() == slicer_core::MeshRepairErrorCode::InputInvalid,
            "invalid triangle index should use InputInvalid");
    }
    catch (const std::exception&)
    {
        return ExpectTrue(false, "invalid triangle index should not leak an STL exception");
    }
    return ExpectTrue(false, "invalid triangle index should throw");
}

}  // namespace

int main()
{
    const bool passed = TestAdapterRecordsFilteredDegenerateSourceTriangle()
        && TestCleanupRemovesDegenerateAndExactDuplicate()
        && TestAttributeConflictBlocksCleanup()
        && TestOppositeDuplicateIsNotRemoved()
        && TestConfirmedSelfIntersectionTakesPriorityOverAttributeConflict()
        && TestCleanupEvidenceIsDeterministic()
        && TestDisabledCleanupUsesStableError()
        && TestInvalidTriangleIndexUsesStableError();
    if (!passed)
    {
        return 1;
    }
    std::cout << "mesh_repair_cleanup_unit_tests: PASS\n";
    return 0;
}
