#include "slicer_core/diagnostics/MeshRepairReport.h"
#include "slicer_core/geometry/MeshRobustnessDiagnostics.h"
#include "slicer_core/geometry/MeshTopologyDiagnostics.h"
#include "slicer_core/geometry/TriangleMeshData.h"
#include "slicer_core/geometry/repair/MeshRepairEligibilityPolicy.h"
#include "slicer_core/geometry/repair/MeshRepairHash.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

struct GeneratedFixture
{
    std::string name;
    slicer_core::AdaptedTriangleMesh mesh;
    slicer_core::MeshTopologyReport topology;
    slicer_core::MeshRobustnessReport robustness;
    slicer_core::MeshRepairEligibilityEvidence evidence;
};

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
    adapted.triangle_attributes.reserve(adapted.mesh.triangles.size());
    for (std::size_t index{0U}; index < adapted.mesh.triangles.size(); ++index)
    {
        adapted.triangle_attributes.push_back(MakeAttributes(index));
    }
    return adapted;
}

slicer_core::MeshTopologyReport MakeTopology(
    const slicer_core::AdaptedTriangleMesh& mesh,
    const std::size_t degenerateTriangles = 0U,
    const std::size_t boundaryEdges = 0U,
    const std::size_t nonManifoldEdges = 0U)
{
    slicer_core::MeshTopologyReport topology;
    topology.source_triangles = mesh.mesh.triangles.size();
    topology.accepted_triangles = mesh.mesh.triangles.size();
    topology.unique_vertices = mesh.mesh.vertices.size();
    topology.degenerate_triangles = degenerateTriangles;
    topology.boundary_edges = boundaryEdges;
    topology.non_manifold_edges = nonManifoldEdges;
    return topology;
}

slicer_core::MeshRobustnessReport MakeRobustness()
{
    slicer_core::MeshRobustnessReport robustness;
    robustness.connected_components = 1U;
    return robustness;
}

GeneratedFixture MakeCleanFixture()
{
    GeneratedFixture fixture;
    fixture.name = "clean_closed";
    fixture.mesh = MakeAttributedBox();
    fixture.topology = MakeTopology(fixture.mesh);
    fixture.robustness = MakeRobustness();
    return fixture;
}

GeneratedFixture MakeDegenerateFixture()
{
    GeneratedFixture fixture = MakeCleanFixture();
    fixture.name = "degenerate_face";
    fixture.mesh.mesh.triangles.push_back({0, 0, 1});
    fixture.mesh.triangle_attributes.push_back(MakeAttributes(fixture.mesh.triangle_attributes.size()));
    fixture.topology = MakeTopology(fixture.mesh, 1U);
    return fixture;
}

GeneratedFixture MakeDuplicateFixture(const bool conflict)
{
    GeneratedFixture fixture = MakeCleanFixture();
    fixture.name = conflict ? "duplicate_uv_conflict" : "duplicate_same_attributes";
    fixture.mesh.mesh.triangles.push_back(fixture.mesh.mesh.triangles.front());
    slicer_core::SurfaceTriangleAttributes attributes = fixture.mesh.triangle_attributes.front();
    attributes.source_triangle_index = fixture.mesh.triangle_attributes.size();
    if (conflict)
    {
        attributes.uv.at(0).u = 0.25;
    }
    fixture.mesh.triangle_attributes.push_back(attributes);
    fixture.topology = MakeTopology(fixture.mesh);
    fixture.robustness = MakeRobustness();
    fixture.robustness.duplicate_faces = 1U;
    fixture.evidence.duplicateFaceAttributesEvaluated = true;
    fixture.evidence.duplicateFaceAttributeConflicts = conflict ? 1U : 0U;
    return fixture;
}

GeneratedFixture MakeOppositeDuplicateFixture()
{
    GeneratedFixture fixture = MakeCleanFixture();
    fixture.name = "opposite_duplicate";
    const std::array<int, 3> triangle = fixture.mesh.mesh.triangles.front();
    fixture.mesh.mesh.triangles.push_back({triangle.at(0), triangle.at(2), triangle.at(1)});
    fixture.mesh.triangle_attributes.push_back(fixture.mesh.triangle_attributes.front());
    fixture.topology = MakeTopology(fixture.mesh);
    fixture.robustness = MakeRobustness();
    fixture.robustness.duplicate_faces = 1U;
    fixture.robustness.opposite_duplicate_faces = 1U;
    return fixture;
}

GeneratedFixture MakeWindingFixture()
{
    GeneratedFixture fixture = MakeCleanFixture();
    fixture.name = "winding_only";
    std::swap(
        fixture.mesh.mesh.triangles.front().at(1),
        fixture.mesh.mesh.triangles.front().at(2));
    fixture.topology = MakeTopology(fixture.mesh);
    fixture.robustness = MakeRobustness();
    fixture.robustness.inconsistent_oriented_edges = 3U;
    return fixture;
}

GeneratedFixture MakeBoundaryFixture(const bool nonPlanar)
{
    GeneratedFixture fixture = MakeCleanFixture();
    fixture.name = nonPlanar ? "non_planar_boundary" : "simple_planar_boundary";
    fixture.mesh.mesh.triangles.erase(fixture.mesh.mesh.triangles.begin() + 2);
    fixture.mesh.triangle_attributes.erase(fixture.mesh.triangle_attributes.begin() + 2);
    if (nonPlanar)
    {
        fixture.mesh.mesh.vertices.at(6).z += 0.2;
    }
    fixture.topology = MakeTopology(fixture.mesh, 0U, 3U);
    fixture.robustness = MakeRobustness();
    fixture.evidence.boundaryClassification = nonPlanar
        ? slicer_core::MeshRepairBoundaryClassification::NonPlanar
        : slicer_core::MeshRepairBoundaryClassification::SimpleWithinBudget;
    return fixture;
}

GeneratedFixture MakeEdgeFanFixture(const bool ambiguous)
{
    GeneratedFixture fixture = MakeCleanFixture();
    fixture.name = ambiguous ? "ambiguous_edge_fan" : "separable_edge_fan";
    fixture.mesh.mesh.vertices.push_back({0.5, 0.0, 1.5});
    fixture.mesh.mesh.triangles.push_back({4, 5, 8});
    fixture.mesh.triangle_attributes.push_back(MakeAttributes(fixture.mesh.triangle_attributes.size()));
    if (ambiguous)
    {
        fixture.mesh.mesh.vertices.push_back({0.5, 0.0, 2.0});
        fixture.mesh.mesh.triangles.push_back({5, 4, 9});
        fixture.mesh.triangle_attributes.push_back(MakeAttributes(fixture.mesh.triangle_attributes.size()));
    }
    fixture.topology = MakeTopology(fixture.mesh, 0U, 0U, 1U);
    fixture.robustness = MakeRobustness();
    fixture.evidence.nonManifoldClassification = ambiguous
        ? slicer_core::MeshRepairNonManifoldClassification::Ambiguous
        : slicer_core::MeshRepairNonManifoldClassification::UniquelySeparable;
    return fixture;
}

GeneratedFixture MakeSelfIntersectionFixture()
{
    GeneratedFixture fixture;
    fixture.name = "self_intersection";
    fixture.mesh.mesh.source_name = fixture.name;
    fixture.mesh.mesh.vertices = {
        {-1.0, -1.0, 0.0},
        {1.0, -1.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, -0.5, -1.0},
        {0.0, -0.5, 1.0},
        {0.0, 0.5, 0.0}};
    fixture.mesh.mesh.triangles = {{0, 1, 2}, {3, 4, 5}};
    fixture.mesh.triangle_attributes = {MakeAttributes(0U), MakeAttributes(1U)};
    fixture.topology = MakeTopology(fixture.mesh, 0U, 6U);
    fixture.robustness = MakeRobustness();
    fixture.robustness.connected_components = 2U;
    fixture.robustness.self_intersection_pairs = 1U;
    fixture.robustness.confirmed_self_intersections = 1U;
    return fixture;
}

std::vector<GeneratedFixture> BuildFixtures()
{
    std::vector<GeneratedFixture> fixtures;
    fixtures.push_back(MakeCleanFixture());
    fixtures.push_back(MakeDegenerateFixture());
    fixtures.push_back(MakeDuplicateFixture(false));
    fixtures.push_back(MakeDuplicateFixture(true));
    fixtures.push_back(MakeOppositeDuplicateFixture());
    fixtures.push_back(MakeWindingFixture());
    fixtures.push_back(MakeBoundaryFixture(false));
    fixtures.push_back(MakeBoundaryFixture(true));
    fixtures.push_back(MakeEdgeFanFixture(false));
    fixtures.push_back(MakeEdgeFanFixture(true));
    fixtures.push_back(MakeSelfIntersectionFixture());
    return fixtures;
}

slicer_core::MeshRepairDiagnosticsSummary BuildDiagnostics(const GeneratedFixture& fixture)
{
    slicer_core::MeshRepairDiagnosticsSummary summary;
    summary.available = true;
    summary.strictPass = fixture.topology.degenerate_triangles == 0U
        && fixture.topology.boundary_edges == 0U
        && fixture.topology.non_manifold_edges == 0U
        && fixture.robustness.duplicate_faces == 0U
        && fixture.robustness.inconsistent_oriented_edges == 0U
        && fixture.robustness.self_intersection_pairs == 0U;
    summary.boundaryEdges = fixture.topology.boundary_edges;
    summary.nonManifoldEdges = fixture.topology.non_manifold_edges;
    summary.duplicateFaces = fixture.robustness.duplicate_faces;
    summary.oppositeDuplicateFaces = fixture.robustness.opposite_duplicate_faces;
    summary.localWindingIssues = fixture.robustness.inconsistent_oriented_edges;
    summary.degenerateTriangles = fixture.topology.degenerate_triangles;
    summary.connectedComponents = fixture.robustness.connected_components;
    summary.confirmedSelfIntersectionPairs = fixture.robustness.self_intersection_pairs;
    return summary;
}

slicer_core::Json DecisionsProjection(const slicer_core::MeshRepairEligibility& eligibility)
{
    slicer_core::Json::Array decisions;
    for (const slicer_core::MeshRepairEligibilityDecision& decision : eligibility.decisions)
    {
        decisions.push_back(slicer_core::Json::object({
            {"issueCode", decision.issueCode},
            {"classification", slicer_core::MeshRepairEligibilityClassName(decision.classification)},
            {"reasonCode", decision.reasonCode},
            {"affectedCount", decision.affectedCount},
        }));
    }
    return slicer_core::Json{std::move(decisions)};
}

slicer_core::Json BuildGoldenProjection()
{
    slicer_core::Json::Array cases;
    for (const GeneratedFixture& fixture : BuildFixtures())
    {
        slicer_core::MeshRepairOptions options;
        options.mode = "strict_closed";
        const slicer_core::MeshRepairHashes hashes =
            slicer_core::ComputeMeshRepairPreHashes(fixture.mesh, options);
        const slicer_core::MeshRepairEligibility eligibility =
            slicer_core::EvaluateMeshRepairEligibility(
                fixture.topology,
                fixture.robustness,
                fixture.evidence);

        slicer_core::MeshRepairResult result;
        result.status = eligibility.status;
        result.mode = options.mode;
        result.input.sourcePath = "generated/" + fixture.name;
        result.input.inputFormat = "generated";
        result.input.vertexCount = fixture.mesh.mesh.vertices.size();
        result.input.triangleCount = fixture.mesh.mesh.triangles.size();
        result.input.componentCount = fixture.robustness.connected_components;
        result.options = options;
        result.hashes = hashes;
        result.preRepair = BuildDiagnostics(fixture);
        result.eligibility = eligibility;
        const slicer_core::Json report = slicer_core::BuildMeshRepairReport(result);

        cases.push_back(slicer_core::Json::object({
            {"name", fixture.name},
            {"reportSchema", report.at("schema")},
            {"status", report.at("status")},
            {"automaticRepairAllowed", report.at("eligibility").at("automaticRepairAllowed")},
            {"repairAttempted", report.at("repairAttempted")},
            {"productionOutputWritten", report.at("productionOutputWritten")},
            {"geometryHash", report.at("hashes").at("preRepairGeometryHash")},
            {"attributeHash", report.at("hashes").at("preRepairAttributeHash")},
            {"decisions", DecisionsProjection(eligibility)},
        }));
    }
    return slicer_core::Json::object({
        {"schema", "slicesoft.mesh_repair_fixture_golden.12e_08c_r1.1"},
        {"cases", slicer_core::Json{std::move(cases)}},
    });
}

slicer_core::Json LoadGolden()
{
    const std::filesystem::path path = std::filesystem::path{SLICESOFT_SOURCE_DIR}
        / "tests" / "golden" / "expected" / "12e_mesh_repair_fixture_eligibility.json";
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("failed to open fixture golden: " + path.string());
    }
    return slicer_core::Json::parse(input);
}

bool DecisionsAreUnique(const std::vector<GeneratedFixture>& fixtures)
{
    for (const GeneratedFixture& fixture : fixtures)
    {
        const slicer_core::MeshRepairEligibility eligibility =
            slicer_core::EvaluateMeshRepairEligibility(
                fixture.topology,
                fixture.robustness,
                fixture.evidence);
        std::set<std::string> codes;
        for (const slicer_core::MeshRepairEligibilityDecision& decision : eligibility.decisions)
        {
            if (!codes.insert(decision.issueCode).second)
            {
                return false;
            }
        }
    }
    return true;
}

}  // namespace

int main()
{
    try
    {
        const std::vector<GeneratedFixture> fixtures = BuildFixtures();
        const slicer_core::Json first = BuildGoldenProjection();
        const slicer_core::Json second = BuildGoldenProjection();
        bool passed{true};
        passed = ExpectTrue(first.dump(2) == second.dump(2), "fixture projection should be deterministic") && passed;
        passed = ExpectTrue(DecisionsAreUnique(fixtures), "each fixture issue should have one decision") && passed;
        const slicer_core::Json expected = LoadGolden();
        if (first.dump(2) != expected.dump(2))
        {
            std::cerr << "ACTUAL_GOLDEN_BEGIN\n" << first.dump(2) << "\nACTUAL_GOLDEN_END\n";
            passed = ExpectTrue(false, "fixture eligibility projection should match golden") && passed;
        }
        if (!passed)
        {
            return 1;
        }
        std::cout << "mesh repair fixture golden unit tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL unexpected exception: " << error.what() << '\n';
        return 1;
    }
}
