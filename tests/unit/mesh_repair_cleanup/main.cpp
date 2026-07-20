#include "slicer_core/diagnostics/MeshRepairReport.h"
#include "slicer_core/geometry/MeshScaleTolerance.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/TriangleMeshData.h"
#include "slicer_core/geometry/repair/MeshRepairHash.h"
#include "slicer_core/geometry/repair/MeshRepairService.h"

#include <algorithm>
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

bool HasOperation(
    const slicer_core::MeshRepairResult& evidence,
    const slicer_core::MeshRepairOperationType type)
{
    return std::any_of(
        evidence.operations.begin(),
        evidence.operations.end(),
        [type](const slicer_core::MeshRepairOperation& operation)
        {
            return operation.type == type;
        });
}

slicer_core::AdaptedTriangleMesh MakeSplitVertexBox()
{
    slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    const std::size_t duplicateIndex = mesh.mesh.vertices.size();
    slicer_core::Vec3 duplicate = mesh.mesh.vertices.front();
    duplicate.x += 0.00025;
    mesh.mesh.vertices.push_back(duplicate);
    mesh.mesh.triangles.front().at(0U) = static_cast<int>(duplicateIndex);
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);
    return mesh;
}

slicer_core::AdaptedTriangleMesh MakeTwoNearbyBoxes()
{
    slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    const slicer_core::TriangleMeshData second =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    const std::size_t vertexOffset = mesh.mesh.vertices.size();
    for (slicer_core::Vec3 vertex : second.vertices)
    {
        vertex.x += 1.00025;
        mesh.mesh.vertices.push_back(vertex);
    }
    for (const std::array<int, 3>& triangle : second.triangles)
    {
        mesh.mesh.triangles.push_back({
            triangle.at(0U) + static_cast<int>(vertexOffset),
            triangle.at(1U) + static_cast<int>(vertexOffset),
            triangle.at(2U) + static_cast<int>(vertexOffset)});
        mesh.triangle_attributes.push_back(MakeAttributes(mesh.triangle_attributes.size()));
    }
    mesh.mesh.bbox_mm.max.x = 2.00025;
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);
    return mesh;
}

slicer_core::AdaptedTriangleMesh MakeNonOrientableStrip()
{
    slicer_core::AdaptedTriangleMesh mesh;
    mesh.mesh.source_name = "generated-non-orientable-strip";
    mesh.mesh.vertices = {
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 0.0},
        {1.0, 1.0, 0.0},
        {1.0, 0.0, 0.0},
        {2.0, 1.0, 0.0},
        {2.0, 0.0, 0.0}};
    mesh.mesh.triangles = {
        {0, 1, 2},
        {1, 3, 2},
        {2, 3, 4},
        {3, 5, 4},
        {4, 5, 1},
        {5, 0, 1}};
    mesh.mesh.bbox_mm.min = {0.0, 0.0, 0.0};
    mesh.mesh.bbox_mm.max = {2.0, 1.0, 0.001};
    for (std::size_t index{0U}; index < mesh.mesh.triangles.size(); ++index)
    {
        mesh.triangle_attributes.push_back(MakeAttributes(index));
    }
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);
    return mesh;
}

slicer_core::AdaptedTriangleMesh MakeBoxWithTopHole(const bool hasUv)
{
    slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    mesh.mesh.triangles.erase(mesh.mesh.triangles.begin() + 2, mesh.mesh.triangles.begin() + 4);
    mesh.triangle_attributes.erase(
        mesh.triangle_attributes.begin() + 2,
        mesh.triangle_attributes.begin() + 4);
    for (slicer_core::SurfaceTriangleAttributes& attributes : mesh.triangle_attributes)
    {
        attributes.has_uv = hasUv;
    }
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);
    return mesh;
}

slicer_core::AdaptedTriangleMesh MakeBranchingBoundaryBox()
{
    slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    mesh.mesh.triangles.erase(mesh.mesh.triangles.begin() + 11);
    mesh.triangle_attributes.erase(mesh.triangle_attributes.begin() + 11);
    mesh.mesh.triangles.erase(mesh.mesh.triangles.begin() + 2);
    mesh.triangle_attributes.erase(mesh.triangle_attributes.begin() + 2);
    for (slicer_core::SurfaceTriangleAttributes& attributes : mesh.triangle_attributes)
    {
        attributes.has_uv = false;
    }
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);
    return mesh;
}

void EnableBoundaryFill(slicer_core::MeshRepairCleanupRequest& request)
{
    request.options.allowVertexWeld = true;
    request.options.weldToleranceMm = 0.0001;
    request.options.allowWindingRepair = true;
    request.options.allowNewFaces = true;
    request.options.allowBoundaryFill = true;
    request.options.maxBoundaryLoopEdges = 8U;
    request.options.maxBoundaryLoopDiameterMm = 2.0;
    request.options.maxBoundaryLoopPerimeterMm = 5.0;
    request.options.maxBoundaryPlanarityErrorMm = 0.01;
    request.options.maxHoleAreaMm2 = 2.0;
    request.options.maxAffectedFaceRatio = 0.25;
    request.options.newFaceAttributePolicy = "inherit_uniform_material_no_uv";
    request.robustnessOptions.max_triangle_pair_checks = 1000000U;
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

bool TestConstrainedVertexWeldClosesSplitVertexBox()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeSplitVertexBox();
    slicer_core::MeshRepairCleanupRequest request = MakeRequest(mesh);
    request.options.allowVertexWeld = true;
    request.options.weldToleranceMm = 0.001;

    const slicer_core::MeshRepairCleanupResult cleanup =
        slicer_core::ExecuteMeshRepairCleanup(request);
    return ExpectTrue(
               cleanup.evidence.status == slicer_core::MeshRepairStatus::RepairedStrictPass,
               "safe vertex weld should pass strict")
        && ExpectTrue(
            HasOperation(cleanup.evidence, slicer_core::MeshRepairOperationType::WeldVertex),
            "safe vertex weld should be recorded")
        && ExpectTrue(cleanup.candidate.mesh.vertices.size() == 8U, "weld should merge one vertex")
        && ExpectTrue(cleanup.evidence.postRepair.connectedComponents == 1U, "component count should remain one")
        && ExpectTrue(cleanup.evidence.vertexMappings.size() == 8U, "every output vertex should be mapped")
        && ExpectTrue(
            std::any_of(
                cleanup.evidence.vertexMappings.begin(),
                cleanup.evidence.vertexMappings.end(),
                [](const slicer_core::MeshRepairVertexMapping& mapping)
                {
                    return mapping.sourceVertexIndices.size() == 2U;
                }),
            "one output vertex should map both welded sources");
}

bool TestVertexWeldDoesNotMergeConnectedComponents()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeTwoNearbyBoxes();
    slicer_core::MeshRepairCleanupRequest request = MakeRequest(mesh);
    request.options.allowVertexWeld = true;
    request.options.weldToleranceMm = 0.001;

    const slicer_core::MeshRepairCleanupResult cleanup =
        slicer_core::ExecuteMeshRepairCleanup(request);
    return ExpectTrue(
               !HasOperation(cleanup.evidence, slicer_core::MeshRepairOperationType::WeldVertex),
               "cross-component near vertices must not weld")
        && ExpectTrue(cleanup.evidence.postRepair.connectedComponents == 2U, "two components should remain")
        && ExpectTrue(cleanup.candidate.mesh.vertices.size() == 16U, "cross-component vertices should remain");
}

bool TestVertexWeldThatCreatesDegenerateFacesIsBlocked()
{
    slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    mesh.mesh.vertices.at(1U).x = 0.00025;
    mesh.mesh.bbox_mm.max.x = 0.00025;
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);
    slicer_core::MeshRepairCleanupRequest request = MakeRequest(mesh);
    request.options.allowVertexWeld = true;
    request.options.weldToleranceMm = 0.001;

    const slicer_core::MeshRepairCleanupResult cleanup =
        slicer_core::ExecuteMeshRepairCleanup(request);
    return ExpectTrue(
               cleanup.evidence.status == slicer_core::MeshRepairStatus::ManualRepairRequired,
               "unsafe weld should remain manual")
        && ExpectTrue(
            cleanup.evidence.attributePreservation.status == "blocked_vertex_weld_guard",
            "unsafe weld should expose its guard")
        && ExpectTrue(
            !HasOperation(cleanup.evidence, slicer_core::MeshRepairOperationType::WeldVertex),
            "unsafe weld must not be recorded as executed")
        && ExpectTrue(cleanup.candidate.mesh.vertices.size() == 8U, "unsafe candidate should be discarded");
}

bool TestUniqueWindingPropagationFlipsTriangleAndUv()
{
    slicer_core::AdaptedTriangleMesh mesh = MakeAttributedBox();
    std::swap(mesh.mesh.triangles.front().at(1U), mesh.mesh.triangles.front().at(2U));
    std::swap(mesh.triangle_attributes.front().uv.at(1U), mesh.triangle_attributes.front().uv.at(2U));
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);
    slicer_core::MeshRepairCleanupRequest request = MakeRequest(mesh);
    request.options.allowWindingRepair = true;

    const slicer_core::MeshRepairCleanupResult cleanup =
        slicer_core::ExecuteMeshRepairCleanup(request);
    return ExpectTrue(
               cleanup.evidence.status == slicer_core::MeshRepairStatus::RepairedStrictPass,
               "unique winding propagation should pass strict")
        && ExpectTrue(
            HasOperation(cleanup.evidence, slicer_core::MeshRepairOperationType::FlipTriangleWinding),
            "winding flip should be recorded")
        && ExpectTrue(
            cleanup.candidate.mesh.triangles.front()
                == slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0).triangles.front(),
            "triangle winding should be restored")
        && ExpectTrue(
            cleanup.candidate.triangle_attributes.front().uv.at(1U).u == 1.0,
            "per-corner UV should follow the flipped corner");
}

bool TestNonOrientableWindingConflictIsBlocked()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeNonOrientableStrip();
    slicer_core::MeshRepairCleanupRequest request = MakeRequest(mesh);
    request.options.allowWindingRepair = true;
    request.robustnessOptions.max_triangle_pair_checks = 0U;

    const slicer_core::MeshRepairCleanupResult cleanup =
        slicer_core::ExecuteMeshRepairCleanup(request);
    return ExpectTrue(
               cleanup.evidence.status == slicer_core::MeshRepairStatus::ManualRepairRequired,
               "non-orientable conflict should remain manual")
        && ExpectTrue(
            cleanup.evidence.attributePreservation.status == "blocked_winding_ambiguity",
            "winding conflict should expose its guard")
        && ExpectTrue(
            !HasOperation(cleanup.evidence, slicer_core::MeshRepairOperationType::FlipTriangleWinding),
            "ambiguous winding must not mutate triangles");
}

bool TestSimplePlanarBoundaryLoopIsFilled()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeBoxWithTopHole(false);
    slicer_core::MeshRepairCleanupRequest request = MakeRequest(mesh);
    EnableBoundaryFill(request);

    const slicer_core::MeshRepairCleanupResult cleanup =
        slicer_core::ExecuteMeshRepairCleanup(request);
    return ExpectTrue(
               cleanup.evidence.status == slicer_core::MeshRepairStatus::RepairedStrictPass,
               "simple planar boundary should pass strict after fill")
        && ExpectTrue(
            HasOperation(cleanup.evidence, slicer_core::MeshRepairOperationType::FillBoundaryLoop),
            "boundary fill should be recorded")
        && ExpectTrue(cleanup.candidate.mesh.triangles.size() == 12U, "fill should add two triangles")
        && ExpectTrue(cleanup.evidence.generatedTriangleMappings.size() == 2U, "generated faces should be mapped")
        && ExpectTrue(cleanup.evidence.attributePreservation.newTriangles == 2U, "new face count should be reported")
        && ExpectTrue(cleanup.evidence.attributePreservation.sourceMappedTriangles == 10U, "source face count should remain ten")
        && ExpectTrue(cleanup.evidence.postRepair.boundaryEdges == 0U, "filled box should have no boundary")
        && ExpectTrue(
            !cleanup.candidate.triangle_attributes.at(10U).has_uv
                && cleanup.candidate.triangle_attributes.at(10U).material_name == "fixture-material",
            "generated attributes should use the explicit uniform no-UV policy");
}

bool TestNonPlanarBoundaryLoopIsBlocked()
{
    slicer_core::AdaptedTriangleMesh mesh = MakeBoxWithTopHole(false);
    mesh.mesh.vertices.at(7U).z = 1.2;
    mesh.mesh.bbox_mm.max.z = 1.2;
    mesh.topology = slicer_core::AnalyzeMeshTopology(mesh.mesh);
    slicer_core::MeshRepairCleanupRequest request = MakeRequest(mesh);
    EnableBoundaryFill(request);

    const slicer_core::MeshRepairCleanupResult cleanup =
        slicer_core::ExecuteMeshRepairCleanup(request);
    return ExpectTrue(
               cleanup.evidence.status == slicer_core::MeshRepairStatus::ManualRepairRequired,
               "non-planar boundary should remain manual")
        && ExpectTrue(
            cleanup.evidence.attributePreservation.status == "blocked_boundary_planarity",
            "non-planar boundary should expose its guard")
        && ExpectTrue(cleanup.candidate.mesh.triangles.size() == 10U, "blocked fill must discard generated faces");
}

bool TestBoundaryBudgetExceededIsBlocked()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeBoxWithTopHole(false);
    slicer_core::MeshRepairCleanupRequest request = MakeRequest(mesh);
    EnableBoundaryFill(request);
    request.options.maxBoundaryLoopEdges = 3U;

    const slicer_core::MeshRepairCleanupResult cleanup =
        slicer_core::ExecuteMeshRepairCleanup(request);
    return ExpectTrue(
               cleanup.evidence.status == slicer_core::MeshRepairStatus::ManualRepairRequired,
               "over-budget boundary should remain manual")
        && ExpectTrue(
            cleanup.evidence.attributePreservation.status == "blocked_boundary_budget",
            "over-budget boundary should expose its guard");
}

bool TestUvBoundaryAttributePolicyIsBlocked()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeBoxWithTopHole(true);
    slicer_core::MeshRepairCleanupRequest request = MakeRequest(mesh);
    EnableBoundaryFill(request);

    const slicer_core::MeshRepairCleanupResult cleanup =
        slicer_core::ExecuteMeshRepairCleanup(request);
    return ExpectTrue(
               cleanup.evidence.status == slicer_core::MeshRepairStatus::ManualRepairRequired,
               "UV boundary should require an explicit future policy")
        && ExpectTrue(
            cleanup.evidence.attributePreservation.status == "blocked_boundary_attribute_policy",
            "UV boundary should expose its policy guard");
}

bool TestBranchingBoundaryTopologyIsBlocked()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeBranchingBoundaryBox();
    slicer_core::MeshRepairCleanupRequest request = MakeRequest(mesh);
    EnableBoundaryFill(request);

    const slicer_core::MeshRepairCleanupResult cleanup =
        slicer_core::ExecuteMeshRepairCleanup(request);
    return ExpectTrue(
               cleanup.evidence.status == slicer_core::MeshRepairStatus::ManualRepairRequired,
               "branching boundary should remain manual")
        && ExpectTrue(
            cleanup.evidence.attributePreservation.status == "blocked_boundary_topology",
            "branching boundary should expose its topology guard");
}

bool TestBoundaryFillEvidenceIsDeterministic()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeBoxWithTopHole(false);
    slicer_core::MeshRepairCleanupRequest request = MakeRequest(mesh);
    EnableBoundaryFill(request);

    const slicer_core::MeshRepairCleanupResult first =
        slicer_core::ExecuteMeshRepairCleanup(request);
    const slicer_core::MeshRepairCleanupResult second =
        slicer_core::ExecuteMeshRepairCleanup(request);
    return ExpectTrue(
               first.evidence.hashes.repairOperationHash
                   == second.evidence.hashes.repairOperationHash,
               "boundary operation hash should repeat")
        && ExpectTrue(
            first.evidence.hashes.postRepairGeometryHash
                == second.evidence.hashes.postRepairGeometryHash,
            "boundary geometry hash should repeat")
        && ExpectTrue(
            first.evidence.generatedTriangleMappings
                    .at(0U)
                    .generatingBoundaryVertexIndices
                == second.evidence.generatedTriangleMappings
                    .at(0U)
                    .generatingBoundaryVertexIndices,
            "generated face provenance should repeat");
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
        && TestInvalidTriangleIndexUsesStableError()
        && TestConstrainedVertexWeldClosesSplitVertexBox()
        && TestVertexWeldDoesNotMergeConnectedComponents()
        && TestVertexWeldThatCreatesDegenerateFacesIsBlocked()
        && TestUniqueWindingPropagationFlipsTriangleAndUv()
        && TestNonOrientableWindingConflictIsBlocked()
        && TestSimplePlanarBoundaryLoopIsFilled()
        && TestNonPlanarBoundaryLoopIsBlocked()
        && TestBoundaryBudgetExceededIsBlocked()
        && TestUvBoundaryAttributePolicyIsBlocked()
        && TestBranchingBoundaryTopologyIsBlocked()
        && TestBoundaryFillEvidenceIsDeterministic();
    if (!passed)
    {
        return 1;
    }
    std::cout << "mesh_repair_cleanup_unit_tests: PASS\n";
    return 0;
}
