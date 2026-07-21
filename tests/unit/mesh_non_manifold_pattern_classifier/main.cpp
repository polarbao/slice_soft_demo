#include "slicer_core/geometry/repair/MeshNonManifoldPatternClassifier.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

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

slicer_core::AdaptedTriangleMesh MakeMesh(
    const std::vector<std::array<int, 3>>& triangles)
{
    slicer_core::AdaptedTriangleMesh mesh;
    mesh.mesh.vertices = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, -1.0, 0.0},
        {0.0, 0.0, 1.0},
        {0.0, 0.0, -1.0},
        {1.0, 1.0, 0.0},
        {1.0, -1.0, 0.0}};
    mesh.mesh.triangles = triangles;
    for (std::size_t index{0U}; index < triangles.size(); ++index)
    {
        slicer_core::SurfaceTriangleAttributes attributes;
        attributes.source_triangle_index = index;
        attributes.material_name = "material-a";
        mesh.triangle_attributes.push_back(attributes);
    }
    return mesh;
}

slicer_core::AdaptedTriangleMesh MakeSeparableFan()
{
    return MakeMesh({
        {0, 1, 2},
        {1, 0, 3},
        {0, 2, 3},
        {0, 1, 4},
        {1, 0, 5},
        {0, 4, 5}});
}

bool TestNoNonManifoldIsNotPresent()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeMesh({
        {0, 1, 2},
        {1, 0, 3}});
    const slicer_core::MeshNonManifoldAnalysis analysis =
        slicer_core::ClassifyMeshNonManifoldPatterns(mesh);
    return ExpectTrue(analysis.status == "not_present", "closed manifold status")
        && ExpectTrue(analysis.complete, "no non-manifold analysis is complete")
        && ExpectTrue(analysis.nonManifoldEdgeCount == 0U, "no edge records");
}

bool TestDuplicateExporterPattern()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeMesh({
        {0, 1, 2},
        {0, 1, 2},
        {1, 0, 3}});
    const slicer_core::MeshNonManifoldAnalysis analysis =
        slicer_core::ClassifyMeshNonManifoldPatterns(mesh);
    return ExpectTrue(analysis.nonManifoldEdgeCount == 1U, "duplicate triangle creates one non-manifold edge")
        && ExpectTrue(analysis.duplicateShellOrExporterDuplicateEdges == 1U, "duplicate edge classified")
        && ExpectTrue(!analysis.allUniqueFanSplitsFeasible, "duplicate fan is not repairable");
}

bool TestSeparableFanPattern()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeSeparableFan();
    const slicer_core::MeshNonManifoldAnalysis analysis =
        slicer_core::ClassifyMeshNonManifoldPatterns(mesh);
    return ExpectTrue(analysis.nonManifoldEdgeCount == 1U, "one target non-manifold edge")
        && ExpectTrue(analysis.separableLocalEdgeFanEdges == 1U, "separable pattern")
        && ExpectTrue(analysis.allUniqueFanSplitsFeasible, "unique split feasible")
        && ExpectTrue(analysis.edges.front().residualComponentIds.size() == 2U, "two residual fan owners");
}

bool TestOverlappingComponentPattern()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeMesh({
        {0, 1, 2},
        {1, 0, 3},
        {0, 1, 4},
        {1, 0, 5}});
    const slicer_core::MeshNonManifoldAnalysis analysis =
        slicer_core::ClassifyMeshNonManifoldPatterns(mesh);
    return ExpectTrue(analysis.overlappingComponentEdges == 1U, "overlapping component pattern")
        && ExpectTrue(!analysis.edges.front().uniqueFanSplitFeasible, "overlap remains ambiguous");
}

bool TestMixedWindingPattern()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeMesh({
        {0, 1, 2},
        {0, 1, 3},
        {0, 1, 4}});
    const slicer_core::MeshNonManifoldAnalysis analysis =
        slicer_core::ClassifyMeshNonManifoldPatterns(mesh);
    return ExpectTrue(analysis.mixedWindingFanEdges == 1U, "mixed winding pattern")
        && ExpectTrue(analysis.edges.front().mixedWinding, "mixed winding evidence");
}

bool TestAttributeConflictHasPriority()
{
    slicer_core::AdaptedTriangleMesh mesh = MakeSeparableFan();
    mesh.triangle_attributes.at(3U).material_name = "material-b";
    const slicer_core::MeshNonManifoldAnalysis analysis =
        slicer_core::ClassifyMeshNonManifoldPatterns(mesh);
    return ExpectTrue(analysis.attributeConflictingFanEdges == 1U, "attribute conflict pattern")
        && ExpectTrue(analysis.edges.front().attributeConflict, "attribute conflict evidence")
        && ExpectTrue(!analysis.allUniqueFanSplitsFeasible, "attribute conflict blocks split");
}

bool TestUvConflictHasPriority()
{
    slicer_core::AdaptedTriangleMesh mesh = MakeSeparableFan();
    for (std::size_t triangleIndex{0U};
         triangleIndex < mesh.mesh.triangles.size();
         ++triangleIndex)
    {
        slicer_core::SurfaceTriangleAttributes& attributes =
            mesh.triangle_attributes.at(triangleIndex);
        attributes.has_uv = true;
        for (std::size_t corner{0U}; corner < 3U; ++corner)
        {
            const double vertex = static_cast<double>(
                mesh.mesh.triangles.at(triangleIndex).at(corner));
            attributes.uv.at(corner) = {vertex, vertex * 0.5};
        }
    }
    mesh.triangle_attributes.at(3U).uv.at(0U).u += 0.25;
    const slicer_core::MeshNonManifoldAnalysis analysis =
        slicer_core::ClassifyMeshNonManifoldPatterns(mesh);
    return ExpectTrue(analysis.attributeConflictingFanEdges == 1U, "UV conflict pattern")
        && ExpectTrue(analysis.edges.front().attributeConflict, "UV conflict evidence");
}

bool TestInvalidAttributeCountUsesStableError()
{
    slicer_core::AdaptedTriangleMesh mesh = MakeSeparableFan();
    mesh.triangle_attributes.pop_back();
    try
    {
        static_cast<void>(slicer_core::ClassifyMeshNonManifoldPatterns(mesh));
    }
    catch (const slicer_core::MeshRepairError& error)
    {
        return ExpectTrue(
            error.Code() == slicer_core::MeshRepairErrorCode::AttributeMismatch,
            "invalid attributes should use AttributeMismatch");
    }
    return ExpectTrue(false, "invalid attributes should throw");
}

bool TestUnclassifiedSingleResidualPatch()
{
    const slicer_core::AdaptedTriangleMesh mesh = MakeMesh({
        {0, 1, 2},
        {1, 0, 3},
        {0, 1, 4},
        {1, 0, 5},
        {1, 2, 3},
        {1, 4, 5},
        {2, 3, 4},
        {2, 4, 5}});
    const slicer_core::MeshNonManifoldAnalysis first =
        slicer_core::ClassifyMeshNonManifoldPatterns(mesh);
    const slicer_core::MeshNonManifoldAnalysis second =
        slicer_core::ClassifyMeshNonManifoldPatterns(mesh);
    return ExpectTrue(first.unclassifiedEdges == 1U, "single residual patch is unclassified")
        && ExpectTrue(first.edges.front().edgeVertexIndices == second.edges.front().edgeVertexIndices, "edge order repeats")
        && ExpectTrue(first.edges.front().incidentSourceTriangleIndices == second.edges.front().incidentSourceTriangleIndices, "source evidence repeats");
}

}  // namespace

int main()
{
    const bool passed = TestNoNonManifoldIsNotPresent()
        && TestDuplicateExporterPattern()
        && TestSeparableFanPattern()
        && TestOverlappingComponentPattern()
        && TestMixedWindingPattern()
        && TestAttributeConflictHasPriority()
        && TestUvConflictHasPriority()
        && TestUnclassifiedSingleResidualPatch()
        && TestInvalidAttributeCountUsesStableError();
    if (!passed)
    {
        return 1;
    }
    std::cout << "PASS mesh non-manifold pattern classifier unit tests\n";
    return 0;
}
