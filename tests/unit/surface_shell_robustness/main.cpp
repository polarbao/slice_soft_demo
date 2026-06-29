#include "slicer_core/geometry/MeshRobustnessDiagnostics.h"
#include "slicer_core/geometry/MeshScaleTolerance.h"
#include "slicer_core/geometry/NearestTriangleQuery.h"
#include "slicer_core/geometry/TriangleIntersectionQuery.h"
#include "slicer_core/geometry/TriangleMeshData.h"

#include <cmath>
#include <iostream>
#include <string>
#include <utility>
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

bool ExpectNear(const double actual, const double expected, const double tolerance, const std::string& message)
{
    if (std::abs(actual - expected) > tolerance)
    {
        std::cerr << "FAIL " << message << " expected=" << expected << " actual=" << actual << '\n';
        return false;
    }
    return true;
}

bool ExpectEqualInt(const int actual, const int expected, const std::string& message)
{
    if (actual != expected)
    {
        std::cerr << "FAIL " << message << " expected=" << expected << " actual=" << actual << '\n';
        return false;
    }
    return true;
}

slicer_core::MeshRobustnessOptions MakeOptions(const slicer_core::TriangleMeshData& mesh)
{
    slicer_core::MeshRobustnessOptions options;
    options.tolerance = slicer_core::MakeMeshScaleTolerance(mesh.bbox_mm, 0.05);
    return options;
}

slicer_core::TriangleMeshData MakeDuplicateFaceMesh()
{
    slicer_core::TriangleMeshData mesh;
    mesh.vertices = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
    };
    mesh.triangles = {{0, 1, 2}, {0, 1, 2}, {0, 2, 1}};
    mesh.bbox_mm.min = {0.0, 0.0, 0.0};
    mesh.bbox_mm.max = {1.0, 1.0, 0.1};
    return mesh;
}

slicer_core::TriangleMeshData MakeLocalWindingMesh()
{
    slicer_core::TriangleMeshData mesh;
    mesh.vertices = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {1.0, 1.0, 0.0},
    };
    mesh.triangles = {{0, 1, 2}, {0, 1, 3}};
    mesh.bbox_mm.min = {0.0, 0.0, 0.0};
    mesh.bbox_mm.max = {1.0, 1.0, 0.1};
    return mesh;
}

slicer_core::TriangleMeshData MakeTwoComponentMesh()
{
    slicer_core::TriangleMeshData mesh;
    mesh.vertices = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {3.0, 0.0, 0.0},
        {4.0, 0.0, 0.0},
        {3.0, 1.0, 0.0},
    };
    mesh.triangles = {{0, 1, 2}, {3, 4, 5}};
    mesh.bbox_mm.min = {0.0, 0.0, 0.0};
    mesh.bbox_mm.max = {4.0, 1.0, 0.1};
    return mesh;
}

slicer_core::TriangleMeshData MakeSelfIntersectionCandidateMesh()
{
    slicer_core::TriangleMeshData mesh;
    mesh.vertices = {
        {-1.0, -1.0, 0.0},
        {1.0, -1.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, -1.0},
        {0.2, 0.0, 1.0},
        {-0.2, 0.0, 1.0},
    };
    mesh.triangles = {{0, 1, 2}, {3, 4, 5}};
    mesh.bbox_mm.min = {-1.0, -1.0, -1.0};
    mesh.bbox_mm.max = {1.0, 1.0, 1.0};
    return mesh;
}

bool ScaleTolerance()
{
    slicer_core::BoundingBox bbox;
    bbox.min = {0.0, 0.0, 0.0};
    bbox.max = {10.0, 5.0, 1.0};
    const slicer_core::MeshScaleTolerance tolerance = slicer_core::MakeMeshScaleTolerance(bbox, 0.05);
    return ExpectTrue(tolerance.position_epsilon_mm > 0.0, "position epsilon positive")
        && ExpectTrue(tolerance.area_epsilon_mm2 > 0.0, "area epsilon positive")
        && ExpectTrue(tolerance.tie_epsilon_mm > 0.0, "tie epsilon positive");
}

bool DuplicateFaces()
{
    const slicer_core::TriangleMeshData mesh = MakeDuplicateFaceMesh();
    const slicer_core::MeshRobustnessReport report =
        slicer_core::AnalyzeMeshRobustness(mesh, MakeOptions(mesh));
    return ExpectTrue(report.duplicate_faces == 2U, "duplicate face count")
        && ExpectTrue(report.opposite_duplicate_faces == 1U, "opposite duplicate count")
        && ExpectTrue(!slicer_core::ValidateMeshRobustness(report, true).empty(), "duplicate rejected");
}

bool LocalWindingAndComponents()
{
    const slicer_core::TriangleMeshData winding = MakeLocalWindingMesh();
    const slicer_core::MeshRobustnessReport windingReport =
        slicer_core::AnalyzeMeshRobustness(winding, MakeOptions(winding));
    const slicer_core::TriangleMeshData components = MakeTwoComponentMesh();
    const slicer_core::MeshRobustnessReport componentReport =
        slicer_core::AnalyzeMeshRobustness(components, MakeOptions(components));
    return ExpectTrue(windingReport.inconsistent_oriented_edges > 0U, "local winding count")
        && ExpectTrue(componentReport.connected_components == 2U, "component count");
}

bool SelfIntersectionCandidate()
{
    const slicer_core::TriangleMeshData mesh = MakeSelfIntersectionCandidateMesh();
    const slicer_core::MeshRobustnessReport report =
        slicer_core::AnalyzeMeshRobustness(mesh, MakeOptions(mesh));
    return ExpectTrue(report.self_intersection_candidates > 0U, "self-intersection candidate count")
        && ExpectTrue(report.self_intersection_pairs > 0U, "self-intersection confirmed count")
        && ExpectTrue(report.confirmed_self_intersections > 0U, "confirmed self-intersection count")
        && ExpectTrue(!slicer_core::ValidateMeshRobustness(report, true).empty(), "self-intersection rejected");
}

slicer_core::TriangleMeshData MakeAabbFalsePositiveMesh()
{
    slicer_core::TriangleMeshData mesh;
    mesh.vertices = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.8, 0.8, -0.1},
        {1.0, 0.8, 0.1},
        {0.8, 1.0, 0.1},
    };
    mesh.triangles = {{0, 1, 2}, {3, 4, 5}};
    mesh.bbox_mm.min = {0.0, 0.0, 0.0};
    mesh.bbox_mm.max = {1.0, 1.0, 0.1};
    return mesh;
}

slicer_core::TriangleMeshData MakeCoplanarOverlapMesh()
{
    slicer_core::TriangleMeshData mesh;
    mesh.vertices = {
        {0.0, 0.0, 0.0},
        {2.0, 0.0, 0.0},
        {0.0, 2.0, 0.0},
        {0.5, 0.5, 0.0},
        {1.5, 0.5, 0.0},
        {0.5, 1.5, 0.0},
    };
    mesh.triangles = {{0, 1, 2}, {3, 4, 5}};
    mesh.bbox_mm.min = {0.0, 0.0, 0.0};
    mesh.bbox_mm.max = {2.0, 2.0, 0.1};
    return mesh;
}

slicer_core::TriangleMeshData MakeSharedEdgeMesh()
{
    slicer_core::TriangleMeshData mesh;
    mesh.vertices = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {1.0, 1.0, 0.0},
    };
    mesh.triangles = {{0, 1, 2}, {1, 3, 2}};
    mesh.bbox_mm.min = {0.0, 0.0, 0.0};
    mesh.bbox_mm.max = {1.0, 1.0, 0.1};
    return mesh;
}

bool TriangleIntersectionKinds()
{
    const slicer_core::TriangleMeshData intersect = MakeSelfIntersectionCandidateMesh();
    const slicer_core::TriangleIntersectionResult confirmed =
        slicer_core::TestTriangleIntersection(intersect, 0, 1, 1.0e-7);
    const slicer_core::TriangleMeshData falsePositive = MakeAabbFalsePositiveMesh();
    const slicer_core::TriangleIntersectionResult aabbOnly =
        slicer_core::TestTriangleIntersection(falsePositive, 0, 1, 1.0e-7);
    const slicer_core::TriangleMeshData coplanar = MakeCoplanarOverlapMesh();
    const slicer_core::TriangleIntersectionResult coplanarResult =
        slicer_core::TestTriangleIntersection(coplanar, 0, 1, 1.0e-7);
    const slicer_core::TriangleMeshData shared = MakeSharedEdgeMesh();
    const slicer_core::MeshRobustnessReport sharedReport =
        slicer_core::AnalyzeMeshRobustness(shared, MakeOptions(shared));
    return ExpectTrue(
               confirmed.kind == slicer_core::TriangleIntersectionKind::ConfirmedIntersection,
               "confirmed intersection kind")
        && ExpectEqualInt(
               static_cast<int>(aabbOnly.kind),
               static_cast<int>(slicer_core::TriangleIntersectionKind::AabbOnly),
               "aabb-only false positive")
        && ExpectTrue(
               coplanarResult.kind == slicer_core::TriangleIntersectionKind::CoplanarOverlap,
               "coplanar overlap kind")
        && ExpectTrue(sharedReport.self_intersection_candidates == 0U, "shared edge is not self-intersection");
}

bool SelfIntersectionSampling()
{
    const slicer_core::TriangleMeshData mesh = MakeCoplanarOverlapMesh();
    slicer_core::MeshRobustnessOptions options = MakeOptions(mesh);
    options.max_triangle_pair_checks = 0;
    const slicer_core::MeshRobustnessReport report = slicer_core::AnalyzeMeshRobustness(mesh, options);
    return ExpectTrue(report.self_intersection_check_sampled, "sampled marker");
}

bool StableTieBreak()
{
    slicer_core::NearestTriangleHit best;
    best.found = true;
    best.triangle_index = 7;
    best.distance_mm = 0.1;
    best.barycentric = {0.5, 0.5, 0.0};

    slicer_core::NearestTriangleHit candidate = best;
    candidate.triangle_index = 3;
    candidate.barycentric = {0.34, 0.33, 0.33};
    return ExpectTrue(slicer_core::IsBetterNearestTriangleHit(candidate, best, 1.0e-6), "interior margin tie-break");
}

bool BvhStats()
{
    const slicer_core::TriangleMeshData mesh = slicer_core::MakeGeneratedBoxMesh(3.0, 3.0, 0.5);
    slicer_core::NearestTriangleQuery query(mesh);
    slicer_core::NearestTriangleQueryStats stats = query.GetBuildStats();
    const slicer_core::NearestTriangleQueryStats buildStats = stats;
    slicer_core::NearestTriangleQueryOptions options;
    const slicer_core::NearestTriangleHit hit = query.FindNearestWithStats({1.5, 1.5, 2.0}, options, stats);
    return ExpectTrue(hit.found, "bvh hit")
        && ExpectTrue(buildStats.node_count > 0U, "bvh node count")
        && ExpectTrue(buildStats.estimated_bytes > 0U, "bvh bytes")
        && ExpectTrue(stats.query_count == 1U, "query count")
        && ExpectTrue(stats.visited_nodes > 0U, "visited nodes")
        && ExpectTrue(stats.tested_triangles > 0U, "tested triangles");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"scale_tolerance", ScaleTolerance},
        {"duplicate_faces", DuplicateFaces},
        {"local_winding_and_components", LocalWindingAndComponents},
        {"self_intersection_candidate", SelfIntersectionCandidate},
        {"triangle_intersection_kinds", TriangleIntersectionKinds},
        {"self_intersection_sampling", SelfIntersectionSampling},
        {"stable_tie_break", StableTieBreak},
        {"bvh_stats", BvhStats},
    };
    for (const auto& test : tests)
    {
        std::cout << "RUN " << test.first << std::endl;
        bool passed{false};
        try
        {
            passed = test.second();
        }
        catch (const std::exception& error)
        {
            std::cerr << "FAIL " << test.first << " exception=" << error.what() << '\n';
            return 1;
        }
        if (!passed)
        {
            return 1;
        }
        std::cout << "PASS " << test.first << '\n';
    }
    std::cout << "Surface shell robustness unit tests complete.\n";
    return 0;
}
