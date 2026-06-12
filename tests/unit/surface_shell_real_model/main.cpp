#include "slicer_core/config.h"
#include "slicer_core/geometry/NearestTriangleQuery.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/materials/texture_application/SurfaceTextureTransfer.h"
#include "slicer_core/model.h"

#include <cmath>
#include <filesystem>
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

slicer_core::TriangleMeshData MakeSingleTriangle()
{
    slicer_core::TriangleMeshData mesh;
    mesh.vertices = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}};
    mesh.triangles = {{0, 1, 2}};
    mesh.bbox_mm.min = {0.0, 0.0, 0.0};
    mesh.bbox_mm.max = {1.0, 1.0, 0.0};
    return mesh;
}

bool SceneAdapterClosedCube()
{
    const std::filesystem::path configPath{"samples/configs/openvdb/surface_shell_obj_real.json"};
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(configPath);
    const slicer_core::SceneModel scene = slicer_core::load_model_report(config, configPath.parent_path());
    const slicer_core::AdaptedTriangleMesh adapted = slicer_core::AdaptSceneModelToTriangleMesh(scene);
    return ExpectTrue(adapted.mesh.triangles.size() == 12U, "cube accepted triangles")
        && ExpectTrue(adapted.triangle_attributes.size() == adapted.mesh.triangles.size(), "attribute mapping count")
        && ExpectTrue(adapted.topology.boundary_edges == 0U, "closed cube boundary edges")
        && ExpectTrue(adapted.topology.non_manifold_edges == 0U, "closed cube non-manifold edges")
        && ExpectTrue(adapted.topology.signed_volume_mm3 > 0.0, "closed cube positive volume");
}

bool OpenMeshStrictRejected()
{
    const std::filesystem::path configPath{"samples/configs/openvdb/surface_shell_open_mesh.json"};
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(configPath);
    const slicer_core::SceneModel scene = slicer_core::load_model_report(config, configPath.parent_path());
    const slicer_core::AdaptedTriangleMesh adapted = slicer_core::AdaptSceneModelToTriangleMesh(scene);
    return ExpectTrue(adapted.topology.boundary_edges > 0U, "open mesh boundary edges")
        && ExpectTrue(
            !slicer_core::ValidateMeshTopology(
                 adapted.topology,
                 slicer_core::MeshValidationPolicy::StrictClosed)
                 .empty(),
            "strict closed rejects open mesh");
}

bool NearestTrianglePlane()
{
    const slicer_core::NearestTriangleQuery query(MakeSingleTriangle());
    const slicer_core::NearestTriangleHit hit = query.FindNearest({0.25, 0.25, 1.0});
    const double barycentricSum = hit.barycentric.at(0) + hit.barycentric.at(1) + hit.barycentric.at(2);
    return ExpectTrue(hit.found, "plane hit found")
        && ExpectNear(hit.closest_point_mm.x, 0.25, 1.0e-9, "plane closest x")
        && ExpectNear(hit.closest_point_mm.y, 0.25, 1.0e-9, "plane closest y")
        && ExpectNear(hit.closest_point_mm.z, 0.0, 1.0e-9, "plane closest z")
        && ExpectNear(hit.distance_mm, 1.0, 1.0e-9, "plane distance")
        && ExpectNear(barycentricSum, 1.0, 1.0e-9, "barycentric sum");
}

bool NearestTriangleEdgeAndVertex()
{
    const slicer_core::NearestTriangleQuery query(MakeSingleTriangle());
    const slicer_core::NearestTriangleHit edge = query.FindNearest({0.75, 0.75, 0.0});
    const slicer_core::NearestTriangleHit vertex = query.FindNearest({-1.0, -1.0, 0.0});
    return ExpectNear(edge.closest_point_mm.x, 0.5, 1.0e-9, "edge closest x")
        && ExpectNear(edge.closest_point_mm.y, 0.5, 1.0e-9, "edge closest y")
        && ExpectNear(vertex.closest_point_mm.x, 0.0, 1.0e-9, "vertex closest x")
        && ExpectNear(vertex.closest_point_mm.y, 0.0, 1.0e-9, "vertex closest y");
}

bool BvhMatchesBruteForce()
{
    const slicer_core::TriangleMeshData mesh = slicer_core::MakeGeneratedBoxMesh(3.0, 3.0, 0.5);
    const slicer_core::NearestTriangleQuery query(mesh);
    const std::vector<slicer_core::Vec3> points{{0.2, 0.3, 0.1}, {1.5, 1.5, 1.0}, {-0.2, 2.0, 0.25}};
    for (const slicer_core::Vec3& point : points)
    {
        const slicer_core::NearestTriangleHit bvh = query.FindNearest(point);
        const slicer_core::NearestTriangleHit brute = slicer_core::FindNearestTriangleBruteForce(mesh, point);
        if (!ExpectNear(bvh.distance_mm, brute.distance_mm, 1.0e-9, "BVH distance matches brute force"))
        {
            return false;
        }
    }
    return true;
}

bool UvInterpolation()
{
    const std::array<slicer_core::TexCoord, 3> uv{{{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}}};
    const slicer_core::TexCoord result = slicer_core::InterpolateUv(uv, {0.5, 0.25, 0.25});
    return ExpectNear(result.u, 0.25, 1.0e-9, "interpolated u")
        && ExpectNear(result.v, 0.25, 1.0e-9, "interpolated v");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"scene_adapter_closed_cube", SceneAdapterClosedCube},
        {"open_mesh_strict_rejected", OpenMeshStrictRejected},
        {"nearest_triangle_plane", NearestTrianglePlane},
        {"nearest_triangle_edge_and_vertex", NearestTriangleEdgeAndVertex},
        {"bvh_matches_brute_force", BvhMatchesBruteForce},
        {"uv_interpolation", UvInterpolation},
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
    std::cout << "Surface shell real-model unit tests complete.\n";
    return 0;
}
