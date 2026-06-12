#include "slicer_core/geometry/OpenVdbLevelSetBuilder.h"

#include "slicer_core/geometry/OpenVdbAdapter.h"

#ifdef SLICER_CORE_USE_OPENVDB
#include <openvdb/math/Transform.h>
#include <openvdb/openvdb.h>
#include <openvdb/tools/MeshToVolume.h>
#endif

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace slicer_core
{
namespace
{

bool IsPositive(const double value)
{
    return value > 0.0 && std::isfinite(value);
}

#ifdef SLICER_CORE_USE_OPENVDB
IndexBounds3D MakeScanBounds(const TriangleMeshData& mesh, const openvdb::math::Transform& transform, const int padding)
{
    const openvdb::Vec3d minIndex = transform.worldToIndex(
        openvdb::Vec3d{mesh.bbox_mm.min.x, mesh.bbox_mm.min.y, mesh.bbox_mm.min.z});
    const openvdb::Vec3d maxIndex = transform.worldToIndex(
        openvdb::Vec3d{mesh.bbox_mm.max.x, mesh.bbox_mm.max.y, mesh.bbox_mm.max.z});

    IndexBounds3D bounds;
    bounds.min_x = static_cast<int>(std::floor(std::min(minIndex.x(), maxIndex.x()))) - padding;
    bounds.min_y = static_cast<int>(std::floor(std::min(minIndex.y(), maxIndex.y()))) - padding;
    bounds.min_z = static_cast<int>(std::floor(std::min(minIndex.z(), maxIndex.z()))) - padding;
    bounds.max_x = static_cast<int>(std::ceil(std::max(minIndex.x(), maxIndex.x()))) + padding;
    bounds.max_y = static_cast<int>(std::ceil(std::max(minIndex.y(), maxIndex.y()))) + padding;
    bounds.max_z = static_cast<int>(std::ceil(std::max(minIndex.z(), maxIndex.z()))) + padding;
    return bounds;
}

IndexBounds3D MakeActiveBounds(const openvdb::CoordBBox& bbox)
{
    IndexBounds3D bounds;
    bounds.min_x = bbox.min().x();
    bounds.min_y = bbox.min().y();
    bounds.min_z = bbox.min().z();
    bounds.max_x = bbox.max().x();
    bounds.max_y = bbox.max().y();
    bounds.max_z = bbox.max().z();
    return bounds;
}
#endif

}  // namespace

OpenVdbLevelSetResult BuildOpenVdbLevelSet(const TriangleMeshData& mesh, const OpenVdbLevelSetOptions& options)
{
    OpenVdbLevelSetResult result;
    result.voxel_size_mm = options.voxel_size_mm;
    result.status = GetOpenVdbStatus();

    const std::string meshError = ValidateTriangleMesh(mesh);
    if (!meshError.empty())
    {
        result.error = meshError;
        return result;
    }
    if (!IsPositive(options.voxel_size_mm))
    {
        result.error = "voxel size must be positive";
        return result;
    }
    if (!IsPositive(options.exterior_band_voxels) || !IsPositive(options.interior_band_voxels))
    {
        result.error = "OpenVDB band widths must be positive";
        return result;
    }

#ifdef SLICER_CORE_USE_OPENVDB
    openvdb::initialize();

    std::vector<openvdb::Vec3s> points;
    points.reserve(mesh.vertices.size());
    for (const Vec3& vertex : mesh.vertices)
    {
        points.emplace_back(
            static_cast<float>(vertex.x),
            static_cast<float>(vertex.y),
            static_cast<float>(vertex.z));
    }

    std::vector<openvdb::Vec3I> triangles;
    triangles.reserve(mesh.triangles.size());
    for (const std::array<int, 3>& triangle : mesh.triangles)
    {
        triangles.emplace_back(triangle.at(0), triangle.at(1), triangle.at(2));
    }

    std::vector<openvdb::Vec4I> quads;
    openvdb::math::Transform::Ptr transform =
        openvdb::math::Transform::createLinearTransform(options.voxel_size_mm);
    const float halfWidth = static_cast<float>(std::max(options.exterior_band_voxels, options.interior_band_voxels));
    openvdb::FloatGrid::Ptr grid = openvdb::tools::meshToLevelSet<openvdb::FloatGrid>(
        *transform,
        points,
        triangles,
        quads,
        halfWidth);

    if (!grid)
    {
        result.error = "OpenVDB meshToLevelSet returned null grid";
        return result;
    }

    grid->setName("surface_shell_texture_level_set");
    grid->setGridClass(openvdb::GRID_LEVEL_SET);

    result.available = true;
    result.generated = true;
    result.active_voxels = static_cast<int>(grid->activeVoxelCount());
    result.status.active_voxels = result.active_voxels;
    result.status.grid_name = grid->getName();
    result.status.grid_class = "level_set";
    result.status.voxel_size_mm = options.voxel_size_mm;
    result.scan_bounds = MakeScanBounds(mesh, *transform, std::max(0, options.bbox_padding_voxels));
    result.active_bounds = MakeActiveBounds(grid->evalActiveVoxelBoundingBox());
    result.grid = std::static_pointer_cast<void>(grid);
#else
    result.available = false;
    result.generated = false;
    result.error = "USE_OPENVDB=OFF; level set builder unavailable";
    result.warnings.push_back(result.error);
#endif
    return result;
}

Vec3 OpenVdbIndexToWorld(
    const OpenVdbLevelSetResult& levelSet,
    const double x,
    const double y,
    const double z)
{
    if (!levelSet.available || levelSet.grid == nullptr)
    {
        throw std::runtime_error("OpenVDB level set unavailable for index-to-world conversion");
    }
#ifdef SLICER_CORE_USE_OPENVDB
    const std::shared_ptr<openvdb::FloatGrid> grid = std::static_pointer_cast<openvdb::FloatGrid>(levelSet.grid);
    const openvdb::Vec3d world = grid->transform().indexToWorld(openvdb::Vec3d{x, y, z});
    return {world.x(), world.y(), world.z()};
#else
    (void)x;
    (void)y;
    (void)z;
    throw std::runtime_error("USE_OPENVDB=OFF; index-to-world conversion unavailable");
#endif
}

}  // namespace slicer_core
