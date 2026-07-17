#include "slicer_core/geometry/PointInClosedMeshQuery.h"
#include "slicer_core/geometry/TriangleMeshData.h"
#include "slicer_core/materials/texture_application/GlobalTextureFillPartitionService.h"
#include "slicer_core/materials/texture_application/LegacyCpuGlobalDistanceBackend.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
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

bool HasIssueCode(
    const std::vector<slicer_core::ValidationIssue>& issues,
    const std::string& code)
{
    for (const slicer_core::ValidationIssue& issue : issues)
    {
        if (issue.code == code)
        {
            return true;
        }
    }
    return false;
}

slicer_core::TextureFillPartitionGridSpec MakeGrid(
    const int width,
    const int height,
    const int depth,
    const double originX,
    const double originY,
    const double originZ,
    const double spacingX,
    const double spacingY,
    const double spacingZ)
{
    slicer_core::TextureFillPartitionGridSpec grid;
    grid.width = width;
    grid.height = height;
    grid.depth = depth;
    grid.originXMm = originX;
    grid.originYMm = originY;
    grid.originZMm = originZ;
    grid.spacingXMm = spacingX;
    grid.spacingYMm = spacingY;
    grid.spacingZMm = spacingZ;
    return grid;
}

slicer_core::TextureFillPartitionGridSpec MakeUnitBoxGrid()
{
    return MakeGrid(12, 12, 12, -0.1, -0.1, -0.1, 0.1, 0.1, 0.1);
}

std::size_t GridIndex(
    const slicer_core::TextureFillPartitionGridSpec& grid,
    const int x,
    const int y,
    const int z)
{
    return
        (static_cast<std::size_t>(z) * static_cast<std::size_t>(grid.height)
         + static_cast<std::size_t>(y))
            * static_cast<std::size_t>(grid.width)
        + static_cast<std::size_t>(x);
}

void TranslateMesh(
    slicer_core::TriangleMeshData& mesh,
    const double x,
    const double y,
    const double z)
{
    for (slicer_core::Vec3& vertex : mesh.vertices)
    {
        vertex.x += x;
        vertex.y += y;
        vertex.z += z;
    }
    mesh.bbox_mm.min.x += x;
    mesh.bbox_mm.min.y += y;
    mesh.bbox_mm.min.z += z;
    mesh.bbox_mm.max.x += x;
    mesh.bbox_mm.max.y += y;
    mesh.bbox_mm.max.z += z;
}

void AppendMesh(
    slicer_core::TriangleMeshData& destination,
    const slicer_core::TriangleMeshData& source,
    const bool reverseOrientation)
{
    const int vertexOffset = static_cast<int>(destination.vertices.size());
    destination.vertices.insert(
        destination.vertices.end(),
        source.vertices.begin(),
        source.vertices.end());
    for (std::array<int, 3> triangle : source.triangles)
    {
        for (int& vertexIndex : triangle)
        {
            vertexIndex += vertexOffset;
        }
        if (reverseOrientation)
        {
            std::swap(triangle.at(1), triangle.at(2));
        }
        destination.triangles.push_back(triangle);
    }
}

slicer_core::TriangleMeshData MakeOctahedronMesh()
{
    slicer_core::TriangleMeshData mesh;
    mesh.source_name = "generated-octahedron";
    mesh.vertices = {
        {0.5, 0.5, 1.0},
        {0.5, 0.5, 0.0},
        {1.0, 0.5, 0.5},
        {0.5, 1.0, 0.5},
        {0.0, 0.5, 0.5},
        {0.5, 0.0, 0.5},
    };
    mesh.triangles = {
        {0, 2, 3},
        {0, 3, 4},
        {0, 4, 5},
        {0, 5, 2},
        {1, 3, 2},
        {1, 4, 3},
        {1, 5, 4},
        {1, 2, 5},
    };
    mesh.bbox_mm.min = {0.0, 0.0, 0.0};
    mesh.bbox_mm.max = {1.0, 1.0, 1.0};
    return mesh;
}

slicer_core::TriangleMeshData MakeGeneratedSphereMesh()
{
    constexpr int stackCount{4};
    constexpr int sliceCount{8};
    constexpr double pi{3.14159265358979323846};

    slicer_core::TriangleMeshData mesh;
    mesh.source_name = "generated-sphere";
    mesh.vertices.push_back({0.5, 0.5, 1.0});
    for (int stack{1}; stack < stackCount; ++stack)
    {
        const double theta = pi * static_cast<double>(stack)
            / static_cast<double>(stackCount);
        const double ringRadius = 0.5 * std::sin(theta);
        const double z = 0.5 + 0.5 * std::cos(theta);
        for (int slice{0}; slice < sliceCount; ++slice)
        {
            const double phi = 2.0 * pi * static_cast<double>(slice)
                / static_cast<double>(sliceCount);
            mesh.vertices.push_back({
                0.5 + ringRadius * std::cos(phi),
                0.5 + ringRadius * std::sin(phi),
                z,
            });
        }
    }
    const int southPoleIndex = static_cast<int>(mesh.vertices.size());
    mesh.vertices.push_back({0.5, 0.5, 0.0});

    const auto ringIndex = [](const int stack, const int slice)
    {
        return 1 + (stack - 1) * sliceCount + (slice % sliceCount);
    };
    for (int slice{0}; slice < sliceCount; ++slice)
    {
        const int nextSlice = (slice + 1) % sliceCount;
        mesh.triangles.push_back({0, ringIndex(1, slice), ringIndex(1, nextSlice)});
        for (int stack{1}; stack < stackCount - 1; ++stack)
        {
            const int upper = ringIndex(stack, slice);
            const int upperNext = ringIndex(stack, nextSlice);
            const int lower = ringIndex(stack + 1, slice);
            const int lowerNext = ringIndex(stack + 1, nextSlice);
            mesh.triangles.push_back({upper, lower, upperNext});
            mesh.triangles.push_back({upperNext, lower, lowerNext});
        }
        mesh.triangles.push_back({
            ringIndex(stackCount - 1, slice),
            southPoleIndex,
            ringIndex(stackCount - 1, nextSlice),
        });
    }
    mesh.bbox_mm.min = {0.0, 0.0, 0.0};
    mesh.bbox_mm.max = {1.0, 1.0, 1.0};
    return mesh;
}

slicer_core::TriangleMeshData MakeClosedCavityMesh()
{
    slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    mesh.source_name = "generated-closed-cavity";
    slicer_core::TriangleMeshData inner =
        slicer_core::MakeGeneratedBoxMesh(0.4, 0.4, 0.4);
    TranslateMesh(inner, 0.3, 0.3, 0.3);
    AppendMesh(mesh, inner, true);
    return mesh;
}

slicer_core::TriangleMeshData MakeOverlappingClosedBoxes()
{
    slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(0.7, 0.7, 0.7);
    mesh.source_name = "generated-self-intersection";
    slicer_core::TriangleMeshData second =
        slicer_core::MakeGeneratedBoxMesh(0.7, 0.7, 0.7);
    TranslateMesh(second, 0.3, 0.3, 0.3);
    AppendMesh(mesh, second, false);
    mesh.bbox_mm.min = {0.0, 0.0, 0.0};
    mesh.bbox_mm.max = {1.0, 1.0, 1.0};
    return mesh;
}

slicer_core::GlobalTextureFillPartitionResult Evaluate(
    const slicer_core::TriangleMeshData& mesh,
    const slicer_core::TextureFillPartitionGridSpec& grid,
    const double requestedWidthMm)
{
    slicer_core::LegacyCpuGlobalDistanceBackend backend;
    slicer_core::GlobalTextureFillPartitionService service(&backend);
    slicer_core::GlobalTextureFillPartitionRequest request;
    request.mesh = &mesh;
    request.grid = grid;
    request.options.requestedWidthMm = requestedWidthMm;
    return service.Evaluate(request);
}

bool PointInMeshBvhMatchesBruteForce()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    slicer_core::PointInClosedMeshQuery query(mesh);
    slicer_core::PointInClosedMeshQueryStats stats = query.GetBuildStats();
    const std::vector<slicer_core::Vec3> points{
        {0.5, 0.5, 0.5},
        {0.05, 0.25, 0.75},
        {-0.1, 0.5, 0.5},
        {1.1, 0.5, 0.5},
        {0.0, 0.5, 0.5},
    };
    for (const slicer_core::Vec3& point : points)
    {
        const slicer_core::PointInClosedMeshResult bvh = query.Classify(
            point,
            {},
            stats);
        const slicer_core::PointInClosedMeshResult brute =
            slicer_core::ClassifyPointInClosedMeshBruteForce(mesh, point);
        if (!ExpectTrue(!bvh.ambiguous, "box BVH query is not ambiguous")
            || !ExpectTrue(
                bvh.inside == brute.inside,
                "box BVH query matches brute-force oracle")
            || !ExpectTrue(
                bvh.boundary == brute.boundary,
                "box boundary classification matches oracle"))
        {
            return false;
        }
    }
    return ExpectTrue(stats.queryCount == points.size(), "query count is recorded")
        && ExpectTrue(stats.nodeCount > 0U, "BVH node count is recorded")
        && ExpectTrue(stats.testedTriangles > 0U, "triangle work is recorded");
}

bool ClosedBoxBuildsComplementaryPartition()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    const slicer_core::GlobalTextureFillPartitionResult result = Evaluate(
        mesh,
        MakeUnitBoxGrid(),
        0.20);
    std::cout
        << "METRIC closed_box"
        << " gridVoxels=" << result.performance.gridVoxelCount
        << " modelVoxels=" << result.stats.modelVoxels
        << " topologyMs=" << result.performance.topologyMs
        << " occupancyMs=" << result.performance.occupancyBuildMs
        << " distanceMs=" << result.performance.distanceQueryMs
        << " partitionMs=" << result.performance.partitionMs
        << " totalCoreMs=" << result.performance.totalCoreMs
        << " peakWorkingSetBytes="
        << result.performance.processPeakWorkingSetBytes
        << '\n';

    return ExpectTrue(result.available, "CPU backend is available")
        && ExpectTrue(result.partitionPass, "closed box partition passes")
        && ExpectTrue(result.status == "diagnostic", "closed box remains diagnostic")
        && ExpectTrue(result.stats.modelVoxels == 1000U, "closed box occupancy is exact")
        && ExpectTrue(result.stats.textureSurfaceVoxels > 0U, "closed box has texture shell")
        && ExpectTrue(result.stats.modelFillVoxels > 0U, "closed box retains model fill")
        && ExpectTrue(
               result.stats.textureSurfaceVoxels + result.stats.modelFillVoxels
                   == result.stats.modelVoxels,
               "closed box partition covers the model")
        && ExpectTrue(result.stats.overlapTextureFillVoxels == 0U, "closed box has no overlap")
        && ExpectTrue(result.stats.unassignedModelVoxels == 0U, "closed box has no gap")
        && ExpectTrue(
               result.widthMetrics.allTextureThresholdMm >= 0.45 - 1.0e-9,
               "closed box all-texture threshold is model-derived")
        && ExpectTrue(
               result.queryStats.occupancyQueryCount == 1728U,
               "occupancy queries cover the full grid")
        && ExpectTrue(
               result.queryStats.nearestQueryCount == result.stats.modelVoxels,
               "nearest queries cover model voxels")
        && ExpectTrue(
               result.performance.gridVoxelCount == 1728U,
               "grid voxel count is recorded")
        && ExpectTrue(
               result.performance.occupancyBuildMs > 0.0,
               "occupancy core timing is recorded")
        && ExpectTrue(
               result.performance.distanceQueryMs > 0.0,
               "distance core timing is recorded")
        && ExpectTrue(
               result.performance.maskBytes > 0U,
               "mask memory is recorded")
        && ExpectTrue(
               result.performance.closestReferenceBytes > 0U,
               "closest-reference memory is recorded")
        && ExpectTrue(
               !result.performance.processMemoryAvailable
                   || result.performance.processPeakWorkingSetBytes > 0U,
               "available process peak memory is recorded")
        && ExpectTrue(
               result.productionAcceptance == "not_evaluated",
               "CPU diagnostic is not production accepted");
}

bool MissingMeshAndInvalidGridAreBlocked()
{
    slicer_core::LegacyCpuGlobalDistanceBackend backend;
    slicer_core::GlobalTextureFillPartitionService service(&backend);
    slicer_core::GlobalTextureFillPartitionRequest missingMeshRequest;
    missingMeshRequest.grid = MakeUnitBoxGrid();
    missingMeshRequest.options.requestedWidthMm = 0.20;
    const slicer_core::GlobalTextureFillPartitionResult missingMesh =
        service.Evaluate(missingMeshRequest);

    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    slicer_core::GlobalTextureFillPartitionRequest invalidGridRequest;
    invalidGridRequest.mesh = &mesh;
    invalidGridRequest.grid = MakeUnitBoxGrid();
    invalidGridRequest.grid.spacingZMm = 0.0;
    invalidGridRequest.options.requestedWidthMm = 0.20;
    const slicer_core::GlobalTextureFillPartitionResult invalidGrid =
        service.Evaluate(invalidGridRequest);

    return ExpectTrue(missingMesh.status == "blocked", "missing mesh is blocked")
        && ExpectTrue(
               HasIssueCode(missingMesh.issues, "E_12E_CPU_MESH_MISSING"),
               "missing mesh uses stable issue")
        && ExpectTrue(invalidGrid.status == "blocked", "invalid CPU grid is blocked")
        && ExpectTrue(
               HasIssueCode(invalidGrid.issues, "E_12E_CPU_GRID_INVALID"),
               "invalid CPU grid uses stable issue");
}

bool SlopedBodyUsesThreeDimensionalDistance()
{
    const slicer_core::TriangleMeshData mesh = MakeOctahedronMesh();
    const slicer_core::GlobalTextureFillPartitionResult result = Evaluate(
        mesh,
        MakeUnitBoxGrid(),
        0.20);

    return ExpectTrue(result.partitionPass, "sloped octahedron partition passes")
        && ExpectTrue(result.stats.modelVoxels > 0U, "sloped body has occupancy")
        && ExpectTrue(
               result.queryStats.nearestTestedTriangles > 0U,
               "sloped body uses nearest-triangle distance")
        && ExpectTrue(
               result.widthMetrics.maxInteriorDistanceMm > 0.0,
               "sloped body reports interior distance");
}

bool SphereBuildsContinuousThreeDimensionalPartition()
{
    const slicer_core::TriangleMeshData mesh = MakeGeneratedSphereMesh();
    const slicer_core::GlobalTextureFillPartitionResult result = Evaluate(
        mesh,
        MakeUnitBoxGrid(),
        0.20);

    if (!result.partitionPass)
    {
        std::cerr << "sphere status=" << result.status << '\n';
        for (const slicer_core::ValidationIssue& issue : result.issues)
        {
            std::cerr << "sphere issue " << issue.code << ": " << issue.message << '\n';
        }
    }

    return ExpectTrue(result.partitionPass, "sphere partition passes")
        && ExpectTrue(result.stats.modelVoxels > 0U, "sphere has occupancy")
        && ExpectTrue(
               result.stats.textureSurfaceVoxels > 0U,
               "sphere has a texture shell")
        && ExpectTrue(result.stats.modelFillVoxels > 0U, "sphere retains model fill")
        && ExpectTrue(
               result.stats.textureSurfaceVoxels + result.stats.modelFillVoxels
                   == result.stats.modelVoxels,
               "sphere partition is complementary")
        && ExpectTrue(
               result.widthMetrics.maxInteriorDistanceMm > 0.20,
               "sphere reports a three-dimensional interior distance");
}

bool ThinWallNaturallyBecomesAllTexture()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 0.15);
    const slicer_core::TextureFillPartitionGridSpec grid = MakeGrid(
        12,
        12,
        5,
        -0.1,
        -0.1,
        -0.05,
        0.1,
        0.1,
        0.05);
    const slicer_core::GlobalTextureFillPartitionResult result = Evaluate(
        mesh,
        grid,
        0.20);

    return ExpectTrue(result.partitionPass, "thin-wall partition passes")
        && ExpectTrue(result.widthMetrics.allTexture, "thin wall reaches all-texture")
        && ExpectTrue(result.stats.modelVoxels > 0U, "thin wall has occupancy")
        && ExpectTrue(result.stats.modelFillVoxels == 0U, "thin wall fill naturally disappears")
        && ExpectTrue(
               result.stats.textureSurfaceVoxels == result.stats.modelVoxels,
               "thin wall is fully texture classified");
}

bool ClosedCavityParticipatesInOccupancy()
{
    const slicer_core::TriangleMeshData mesh = MakeClosedCavityMesh();
    const slicer_core::TextureFillPartitionGridSpec grid = MakeUnitBoxGrid();
    const slicer_core::GlobalTextureFillPartitionResult result = Evaluate(
        mesh,
        grid,
        0.20);
    const std::size_t cavityIndex = GridIndex(grid, 5, 5, 5);
    const std::size_t shellIndex = GridIndex(grid, 3, 5, 5);

    return ExpectTrue(result.partitionPass, "closed-cavity partition passes")
        && ExpectTrue(
               result.modelMask.values.at(cavityIndex) == 0U,
               "closed cavity remains outside model volume")
        && ExpectTrue(
               result.modelMask.values.at(shellIndex) == 1U,
               "material between outer and inner surfaces remains model")
        && ExpectTrue(
               !result.closestSurfaceReferences.at(cavityIndex).valid,
               "cavity voxel has no closest-surface assignment")
        && ExpectTrue(
               result.closestSurfaceReferences.at(shellIndex).valid,
               "model shell voxel retains closest-surface evidence");
}

bool StrictTopologyBlockersAreStable()
{
    slicer_core::TriangleMeshData open =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    open.triangles.pop_back();
    const slicer_core::GlobalTextureFillPartitionResult openResult = Evaluate(
        open,
        MakeUnitBoxGrid(),
        0.20);

    slicer_core::TriangleMeshData nonManifold =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    nonManifold.triangles.push_back(nonManifold.triangles.front());
    const slicer_core::GlobalTextureFillPartitionResult nonManifoldResult = Evaluate(
        nonManifold,
        MakeUnitBoxGrid(),
        0.20);

    const slicer_core::TriangleMeshData intersect = MakeOverlappingClosedBoxes();
    const slicer_core::GlobalTextureFillPartitionResult intersectResult = Evaluate(
        intersect,
        MakeUnitBoxGrid(),
        0.20);

    return ExpectTrue(openResult.status == "blocked", "open mesh is blocked")
        && ExpectTrue(openResult.available, "CPU backend remains available for open mesh")
        && ExpectTrue(
               HasIssueCode(openResult.issues, "E_12E_CPU_TOPOLOGY_BLOCKED"),
               "open mesh uses stable topology issue")
        && ExpectTrue(nonManifoldResult.status == "blocked", "non-manifold mesh is blocked")
        && ExpectTrue(
               HasIssueCode(
                   nonManifoldResult.issues,
                   "E_12E_CPU_TOPOLOGY_BLOCKED"),
               "non-manifold mesh uses stable topology issue")
        && ExpectTrue(intersectResult.status == "blocked", "self-intersection is blocked")
        && ExpectTrue(
               HasIssueCode(
                   intersectResult.issues,
                   "E_12E_CPU_TOPOLOGY_BLOCKED"),
               "self-intersection uses stable topology issue");
}

bool WidthMinimumAndAllTextureThresholdAreEnforced()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    const slicer_core::TextureFillPartitionGridSpec grid = MakeUnitBoxGrid();
    const slicer_core::GlobalTextureFillPartitionResult belowMinimum = Evaluate(
        mesh,
        grid,
        0.19);
    const slicer_core::GlobalTextureFillPartitionResult allTexture = Evaluate(
        mesh,
        grid,
        10.0);

    const double steppedThreshold =
        allTexture.widthMetrics.allTextureThresholdMm / 0.01;
    return ExpectTrue(belowMinimum.status == "blocked", "width below minimum is blocked")
        && ExpectTrue(
               HasIssueCode(
                   belowMinimum.issues,
                   "E_12E_SURFACE_SHELL_WIDTH_BELOW_EFFECTIVE_MINIMUM"),
               "width below minimum uses stable issue")
        && ExpectTrue(allTexture.partitionPass, "all-texture endpoint passes")
        && ExpectTrue(allTexture.widthMetrics.allTexture, "all-texture flag is true")
        && ExpectTrue(allTexture.stats.modelFillVoxels == 0U, "all-texture fill is empty")
        && ExpectTrue(
               allTexture.stats.textureSurfaceVoxels == allTexture.stats.modelVoxels,
               "all-texture endpoint covers the model")
        && ExpectTrue(
               std::abs(steppedThreshold - std::round(steppedThreshold)) < 1.0e-8,
               "all-texture threshold follows 0.01 mm step");
}

bool ClosestSurfaceReferencesRespectModelMask()
{
    const slicer_core::TriangleMeshData mesh =
        slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    const slicer_core::GlobalTextureFillPartitionResult result = Evaluate(
        mesh,
        MakeUnitBoxGrid(),
        0.20);
    std::uint64_t validReferences{0U};
    for (std::size_t index{0U};
         index < result.closestSurfaceReferences.size();
         ++index)
    {
        const bool model = result.modelMask.values.at(index) != 0U;
        const slicer_core::TextureFillClosestSurfaceReference& reference =
            result.closestSurfaceReferences.at(index);
        if (!ExpectTrue(
                reference.valid == model,
                "closest-surface validity matches model occupancy"))
        {
            return false;
        }
        if (reference.valid)
        {
            ++validReferences;
            const double barycentricSum = reference.barycentric.at(0)
                + reference.barycentric.at(1)
                + reference.barycentric.at(2);
            if (!ExpectTrue(
                    std::abs(barycentricSum - 1.0) < 1.0e-8,
                    "closest-surface barycentric coordinates are normalized")
                || !ExpectTrue(
                    reference.triangleIndex < mesh.triangles.size(),
                    "closest-surface triangle index is valid")
                || !ExpectTrue(
                    std::isfinite(reference.distanceMm),
                    "closest-surface distance is finite"))
            {
                return false;
            }
        }
    }
    return ExpectTrue(
        validReferences == result.stats.modelVoxels,
        "each model voxel has one closest-surface reference");
}

bool RepeatedCpuEvaluationIsDeterministic()
{
    const slicer_core::TriangleMeshData mesh = MakeOctahedronMesh();
    const slicer_core::TextureFillPartitionGridSpec grid = MakeUnitBoxGrid();
    const slicer_core::GlobalTextureFillPartitionResult first = Evaluate(
        mesh,
        grid,
        0.20);
    const slicer_core::GlobalTextureFillPartitionResult second = Evaluate(
        mesh,
        grid,
        0.20);

    return ExpectTrue(first.partitionPass && second.partitionPass, "repeat runs pass")
        && ExpectTrue(
               first.modelMask.values == second.modelMask.values,
               "model occupancy is deterministic")
        && ExpectTrue(
               first.textureSurfaceMask.values == second.textureSurfaceMask.values,
               "texture mask is deterministic")
        && ExpectTrue(
               first.modelFillMask.values == second.modelFillMask.values,
               "fill mask is deterministic")
        && ExpectTrue(
               first.widthMetrics.allTextureThresholdMm
                   == second.widthMetrics.allTextureThresholdMm,
               "dynamic threshold is deterministic");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"point_in_mesh_bvh_matches_brute_force", PointInMeshBvhMatchesBruteForce},
        {"closed_box_builds_complementary_partition", ClosedBoxBuildsComplementaryPartition},
        {"missing_mesh_and_invalid_grid_are_blocked", MissingMeshAndInvalidGridAreBlocked},
        {"sloped_body_uses_three_dimensional_distance", SlopedBodyUsesThreeDimensionalDistance},
        {"sphere_builds_continuous_three_dimensional_partition", SphereBuildsContinuousThreeDimensionalPartition},
        {"thin_wall_naturally_becomes_all_texture", ThinWallNaturallyBecomesAllTexture},
        {"closed_cavity_participates_in_occupancy", ClosedCavityParticipatesInOccupancy},
        {"strict_topology_blockers_are_stable", StrictTopologyBlockersAreStable},
        {"width_minimum_and_all_texture_threshold_are_enforced", WidthMinimumAndAllTextureThresholdAreEnforced},
        {"closest_surface_references_respect_model_mask", ClosestSurfaceReferencesRespectModelMask},
        {"repeated_cpu_evaluation_is_deterministic", RepeatedCpuEvaluationIsDeterministic},
    };

    for (const auto& test : tests)
    {
        std::cout << "RUN " << test.first << std::endl;
        if (!test.second())
        {
            return 1;
        }
        std::cout << "PASS " << test.first << '\n';
    }
    std::cout << "Legacy CPU global distance unit tests complete.\n";
    return 0;
}
