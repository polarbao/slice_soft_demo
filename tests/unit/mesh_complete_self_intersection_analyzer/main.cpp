#include "slicer_core/geometry/TriangleIntersectionQuery.h"
#include "slicer_core/geometry/repair/MeshCompleteSelfIntersectionAnalyzer.h"
#include "slicer_core/geometry/repair/MeshRepairHash.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{

struct BruteForceCounts
{
    std::uint64_t candidates{0U};
    std::uint64_t confirmed{0U};
    std::uint64_t coplanar{0U};
    std::uint64_t touching{0U};
    std::uint64_t aabbOnly{0U};
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

slicer_core::TriangleMeshData MakeMesh(
    const std::vector<slicer_core::Vec3>& vertices,
    const std::vector<std::array<int, 3>>& triangles)
{
    slicer_core::TriangleMeshData mesh;
    mesh.vertices = vertices;
    mesh.triangles = triangles;
    return mesh;
}

slicer_core::TriangleMeshData MakeSeparatedMesh()
{
    return MakeMesh(
        {
            {0.0, 0.0, 0.0},
            {1.0, 0.0, 0.0},
            {0.0, 1.0, 0.0},
            {3.0, 0.0, 0.0},
            {4.0, 0.0, 0.0},
            {3.0, 1.0, 0.0},
        },
        {{0, 1, 2}, {3, 4, 5}});
}

slicer_core::TriangleMeshData MakeConfirmedMesh()
{
    return MakeMesh(
        {
            {-1.0, -1.0, 0.0},
            {1.0, -1.0, 0.0},
            {0.0, 1.0, 0.0},
            {0.0, 0.0, -1.0},
            {0.2, 0.0, 1.0},
            {-0.2, 0.0, 1.0},
        },
        {{0, 1, 2}, {3, 4, 5}});
}

slicer_core::TriangleMeshData MakeCoplanarMesh()
{
    return MakeMesh(
        {
            {0.0, 0.0, 0.0},
            {2.0, 0.0, 0.0},
            {0.0, 2.0, 0.0},
            {0.5, 0.5, 0.0},
            {1.5, 0.5, 0.0},
            {0.5, 1.5, 0.0},
        },
        {{0, 1, 2}, {3, 4, 5}});
}

slicer_core::TriangleMeshData MakeTouchingMesh()
{
    return MakeMesh(
        {
            {0.0, 0.0, 0.0},
            {2.0, 0.0, 0.0},
            {0.0, 2.0, 0.0},
            {0.5, 0.5, 0.0},
            {0.5, 0.5, 1.0},
            {1.0, 0.5, 1.0},
        },
        {{0, 1, 2}, {3, 4, 5}});
}

slicer_core::TriangleMeshData MakeAdjacentMesh()
{
    return MakeMesh(
        {
            {0.0, 0.0, 0.0},
            {1.0, 0.0, 0.0},
            {0.0, 1.0, 0.0},
            {1.0, 1.0, 0.0},
        },
        {{0, 1, 2}, {1, 3, 2}});
}

BruteForceCounts RunBruteForce(
    const slicer_core::TriangleMeshData& mesh,
    const double epsilonMm)
{
    BruteForceCounts counts;
    for (std::size_t left{0U}; left < mesh.triangles.size(); ++left)
    {
        for (std::size_t right{left + 1U}; right < mesh.triangles.size(); ++right)
        {
            if (slicer_core::TrianglesShareVertexIndex(
                    mesh.triangles.at(left),
                    mesh.triangles.at(right)))
            {
                continue;
            }
            const slicer_core::TriangleIntersectionResult result =
                slicer_core::TestTriangleIntersection(mesh, left, right, epsilonMm);
            if (!result.aabb_candidate)
            {
                continue;
            }
            ++counts.candidates;
            switch (result.kind)
            {
            case slicer_core::TriangleIntersectionKind::ConfirmedIntersection:
                ++counts.confirmed;
                break;
            case slicer_core::TriangleIntersectionKind::CoplanarOverlap:
                ++counts.coplanar;
                break;
            case slicer_core::TriangleIntersectionKind::TouchingOnly:
                ++counts.touching;
                break;
            case slicer_core::TriangleIntersectionKind::AabbOnly:
            case slicer_core::TriangleIntersectionKind::None:
                ++counts.aabbOnly;
                break;
            }
        }
    }
    return counts;
}

slicer_core::MeshCompleteSelfIntersectionOptions MakeOptions()
{
    slicer_core::MeshCompleteSelfIntersectionOptions options;
    options.epsilonMm = 1.0e-7;
    options.maxCandidatePairs = 1000U;
    return options;
}

bool TestSeparatedMeshCompletesWithStableEmptyHash()
{
    const slicer_core::TriangleMeshData mesh = MakeSeparatedMesh();
    const slicer_core::MeshCompleteSelfIntersectionAnalysis first =
        slicer_core::AnalyzeCompleteMeshSelfIntersections(mesh, MakeOptions());
    const slicer_core::MeshCompleteSelfIntersectionAnalysis second =
        slicer_core::AnalyzeCompleteMeshSelfIntersections(mesh, MakeOptions());
    return ExpectTrue(first.status == "complete_no_intersection", "separated status")
        && ExpectTrue(first.complete, "separated audit complete")
        && ExpectTrue(first.candidatePairCount == 0U, "separated candidate count")
        && ExpectTrue(first.testedPairCount == 0U, "separated tested count")
        && ExpectTrue(first.candidatePairHash.has_value(), "empty pair set is hashed")
        && ExpectTrue(first.candidatePairHash->size() == 64U, "SHA-256 hash length")
        && ExpectTrue(first.candidatePairHash == second.candidatePairHash, "empty hash repeats");
}

bool TestIntersectionKindsMatchNarrowPhase()
{
    const slicer_core::MeshCompleteSelfIntersectionAnalysis confirmed =
        slicer_core::AnalyzeCompleteMeshSelfIntersections(MakeConfirmedMesh(), MakeOptions());
    const slicer_core::MeshCompleteSelfIntersectionAnalysis coplanar =
        slicer_core::AnalyzeCompleteMeshSelfIntersections(MakeCoplanarMesh(), MakeOptions());
    const slicer_core::MeshCompleteSelfIntersectionAnalysis touching =
        slicer_core::AnalyzeCompleteMeshSelfIntersections(MakeTouchingMesh(), MakeOptions());
    return ExpectTrue(confirmed.status == "confirmed_intersection", "confirmed status")
        && ExpectTrue(confirmed.confirmedIntersectionPairs == 1U, "confirmed pair")
        && ExpectTrue(coplanar.status == "coplanar_overlap", "coplanar status")
        && ExpectTrue(coplanar.coplanarOverlapPairs == 1U, "coplanar pair")
        && ExpectTrue(touching.status == "touching_only", "touching status")
        && ExpectTrue(touching.touchingOnlyPairs == 1U, "touching pair");
}

bool TestBroadPhaseMatchesBruteForce()
{
    slicer_core::TriangleMeshData mesh = MakeConfirmedMesh();
    const slicer_core::TriangleMeshData coplanar = MakeCoplanarMesh();
    const int offset = static_cast<int>(mesh.vertices.size());
    for (const slicer_core::Vec3& vertex : coplanar.vertices)
    {
        mesh.vertices.push_back({vertex.x + 5.0, vertex.y, vertex.z});
    }
    for (const std::array<int, 3>& triangle : coplanar.triangles)
    {
        mesh.triangles.push_back({
            triangle.at(0) + offset,
            triangle.at(1) + offset,
            triangle.at(2) + offset});
    }
    for (int fixtureIndex{0}; fixtureIndex < 6; ++fixtureIndex)
    {
        const int vertexOffset = static_cast<int>(mesh.vertices.size());
        const double x = 10.0 + static_cast<double>(fixtureIndex) * 3.0;
        mesh.vertices.push_back({x, 0.0, 0.0});
        mesh.vertices.push_back({x + 1.0, 0.0, 0.0});
        mesh.vertices.push_back({x, 1.0, 0.0});
        mesh.triangles.push_back({
            vertexOffset,
            vertexOffset + 1,
            vertexOffset + 2});
    }
    const slicer_core::MeshCompleteSelfIntersectionOptions options = MakeOptions();
    const BruteForceCounts bruteForce = RunBruteForce(mesh, options.epsilonMm);
    const slicer_core::MeshCompleteSelfIntersectionAnalysis analysis =
        slicer_core::AnalyzeCompleteMeshSelfIntersections(mesh, options);
    return ExpectTrue(analysis.complete, "mixed fixture complete")
        && ExpectTrue(analysis.bvhNodeCount > 1U, "mixed fixture exercises BVH")
        && ExpectTrue(analysis.candidatePairCount == bruteForce.candidates, "candidate count matches")
        && ExpectTrue(analysis.testedPairCount == bruteForce.candidates, "tested count matches")
        && ExpectTrue(analysis.confirmedIntersectionPairs == bruteForce.confirmed, "confirmed count matches")
        && ExpectTrue(analysis.coplanarOverlapPairs == bruteForce.coplanar, "coplanar count matches")
        && ExpectTrue(analysis.touchingOnlyPairs == bruteForce.touching, "touching count matches")
        && ExpectTrue(analysis.aabbOnlyPairs == bruteForce.aabbOnly, "AABB-only count matches");
}

bool TestSharedVertexAdjacentPairsAreExcluded()
{
    const slicer_core::MeshCompleteSelfIntersectionAnalysis analysis =
        slicer_core::AnalyzeCompleteMeshSelfIntersections(MakeAdjacentMesh(), MakeOptions());
    return ExpectTrue(analysis.complete, "adjacent fixture complete")
        && ExpectTrue(analysis.status == "complete_no_intersection", "adjacent pair is not self-intersection")
        && ExpectTrue(analysis.candidatePairCount == 0U, "adjacent pair excluded from candidate set")
        && ExpectTrue(analysis.testedPairCount == 0U, "adjacent pair excluded from narrow phase");
}

bool TestCandidateBudgetBlocksWithoutPartialPass()
{
    slicer_core::MeshCompleteSelfIntersectionOptions options = MakeOptions();
    options.maxCandidatePairs = 0U;
    const slicer_core::MeshCompleteSelfIntersectionAnalysis analysis =
        slicer_core::AnalyzeCompleteMeshSelfIntersections(MakeConfirmedMesh(), options);
    return ExpectTrue(analysis.status == "budget_or_resource_blocked", "budget status")
        && ExpectTrue(!analysis.complete, "budget result incomplete")
        && ExpectTrue(analysis.blockerCode == "MESH_SELF_INTERSECTION_CANDIDATE_BUDGET_EXCEEDED", "budget code")
        && ExpectTrue(analysis.candidatePairCount == 1U, "first over-budget pair recorded")
        && ExpectTrue(analysis.testedPairCount == 0U, "blocked audit does not narrow-test prefix")
        && ExpectTrue(!analysis.candidatePairHash.has_value(), "partial candidate set has no final hash");
}

bool TestInvalidIndexUsesStableRepairError()
{
    slicer_core::TriangleMeshData mesh = MakeSeparatedMesh();
    mesh.triangles.front().at(0U) = 99;
    try
    {
        static_cast<void>(slicer_core::AnalyzeCompleteMeshSelfIntersections(mesh, MakeOptions()));
    }
    catch (const slicer_core::MeshRepairError& error)
    {
        return ExpectTrue(
            error.Code() == slicer_core::MeshRepairErrorCode::InputInvalid,
            "invalid index error code");
    }
    return ExpectTrue(false, "invalid index should throw");
}

bool TestPairHashRejectsNonCanonicalOrder()
{
    try
    {
        static_cast<void>(slicer_core::ComputeMeshRepairTrianglePairHash(
            {{2U, 3U}, {0U, 1U}}));
    }
    catch (const slicer_core::MeshRepairError& error)
    {
        return ExpectTrue(
            error.Code() == slicer_core::MeshRepairErrorCode::InputInvalid,
            "non-canonical pair hash error code");
    }
    return ExpectTrue(false, "non-canonical pair order should throw");
}

}  // namespace

int main()
{
    const bool passed = TestSeparatedMeshCompletesWithStableEmptyHash()
        && TestIntersectionKindsMatchNarrowPhase()
        && TestBroadPhaseMatchesBruteForce()
        && TestSharedVertexAdjacentPairsAreExcluded()
        && TestCandidateBudgetBlocksWithoutPartialPass()
        && TestInvalidIndexUsesStableRepairError()
        && TestPairHashRejectsNonCanonicalOrder();
    if (!passed)
    {
        return 1;
    }
    std::cout << "PASS complete self-intersection analyzer unit tests\n";
    return 0;
}
