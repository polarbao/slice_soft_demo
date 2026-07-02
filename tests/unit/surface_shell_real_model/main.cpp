#include "slicer_core/config.h"
#include "slicer_core/geometry/NearestTriangleQuery.h"
#include "slicer_core/geometry/OpenVdbAdapter.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/materials/texture_application/SurfaceShellRealModelPrototype.h"
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

bool StandardObjTemplateRetainsTextureAttributes()
{
    const std::filesystem::path configPath{"samples/configs/obj_standard/standard_obj_texture_legacy.json"};
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(configPath);
    const slicer_core::SceneModel scene = slicer_core::load_model_report(config, configPath.parent_path());
    const slicer_core::AdaptedTriangleMesh adapted = slicer_core::AdaptSceneModelToTriangleMesh(scene);

    int texturedMaterialCount{0};
    for (const slicer_core::MaterialInfo& material : scene.material_infos)
    {
        if (material.has_texture && material.texture_exists)
        {
            ++texturedMaterialCount;
        }
    }

    int uvAttributeCount{0};
    int texturedAttributeCount{0};
    for (const slicer_core::SurfaceTriangleAttributes& attributes : adapted.triangle_attributes)
    {
        if (attributes.has_uv)
        {
            ++uvAttributeCount;
        }
        if (!attributes.material_name.empty())
        {
            ++texturedAttributeCount;
        }
    }

    return ExpectTrue(scene.triangle_count > 0U, "standard OBJ has triangles")
        && ExpectTrue(scene.texcoord_count > 0U, "standard OBJ has texcoords")
        && ExpectTrue(scene.faces_with_uv > 0U, "standard OBJ has UV faces")
        && ExpectTrue(texturedMaterialCount > 0, "standard OBJ has existing texture material")
        && ExpectTrue(adapted.triangle_attributes.size() == adapted.mesh.triangles.size(), "standard OBJ attribute count")
        && ExpectTrue(uvAttributeCount > 0, "standard OBJ adapted UV attributes")
        && ExpectTrue(texturedAttributeCount > 0, "standard OBJ adapted material attributes");
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

bool SmallObjSurfaceShellTextureTransfer()
{
    const slicer_core::OpenVdbStatus status = slicer_core::GetOpenVdbStatus();
    if (!status.compiled_with_openvdb || !status.runtime_available)
    {
        std::cout << "SKIP small_obj_surface_shell_texture_transfer USE_OPENVDB=OFF\n";
        return true;
    }

    const std::filesystem::path configPath{"samples/configs/openvdb/surface_shell_obj_real.json"};
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(configPath);
    const slicer_core::SceneModel scene = slicer_core::load_model_report(config, configPath.parent_path());

    slicer_core::SurfaceShellRealModelOptions options;
    options.voxel_size_mm = 0.10;
    options.shell_thickness_mm = 0.15;
    options.fallback_rgb = config.texture.fallback_rgb;
    options.texture_sample.sampler = config.texture.sampler;
    options.texture_sample.uv_address_mode = config.texture.uv_address_mode;
    options.texture_sample.flip_v = config.texture.flip_v;

    const slicer_core::SurfaceShellRealModelResult result =
        slicer_core::RunSurfaceShellRealModelPrototype(scene, config, options);
    if (!ExpectTrue(result.errors.empty(), "small OBJ OpenVDB transfer should run"))
    {
        for (const std::string& error : result.errors)
        {
            std::cerr << "  error=" << error << '\n';
        }
        return false;
    }

    return ExpectTrue(result.shell.shell_voxels > 0, "small OBJ shell voxels")
        && ExpectTrue(result.transfer.stats.sampled_texture_voxels > 0, "small OBJ sampled texture voxels")
        && ExpectTrue(result.transfer.stats.loaded_texture_count > 0, "small OBJ loaded texture count")
        && ExpectTrue(
            result.transfer.shell_rgb.size() == result.shell.shell_mask.size(),
            "small OBJ shell RGB count matches shell mask");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"scene_adapter_closed_cube", SceneAdapterClosedCube},
        {"standard_obj_template_retains_texture_attributes", StandardObjTemplateRetainsTextureAttributes},
        {"open_mesh_strict_rejected", OpenMeshStrictRejected},
        {"nearest_triangle_plane", NearestTrianglePlane},
        {"nearest_triangle_edge_and_vertex", NearestTriangleEdgeAndVertex},
        {"bvh_matches_brute_force", BvhMatchesBruteForce},
        {"uv_interpolation", UvInterpolation},
        {"small_obj_surface_shell_texture_transfer", SmallObjSurfaceShellTextureTransfer},
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
