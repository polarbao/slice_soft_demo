#include "slicer_core/geometry/OpenVdbLevelSetBuilder.h"

#include "slicer_core/geometry/OpenVdbAdapter.h"
#include "slicer_core/geometry/PointInClosedMeshQuery.h"

#ifdef SLICER_CORE_USE_OPENVDB
#include <openvdb/math/Transform.h>
#include <openvdb/openvdb.h>
#include <openvdb/tools/Interpolation.h>
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

    std::vector<openvdb::Vec3s> worldPoints;
    worldPoints.reserve(mesh.vertices.size());
    for (const Vec3& vertex : mesh.vertices)
    {
        worldPoints.emplace_back(
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
    openvdb::FloatGrid::Ptr grid;
    if (options.use_parity_interior_test)
    {
        std::vector<openvdb::Vec3s> indexPoints;
        indexPoints.reserve(mesh.vertices.size());
        for (const Vec3& vertex : mesh.vertices)
        {
            const openvdb::Vec3d indexPoint = transform->worldToIndex(
                openvdb::Vec3d{vertex.x, vertex.y, vertex.z});
            indexPoints.emplace_back(
                static_cast<float>(indexPoint.x()),
                static_cast<float>(indexPoint.y()),
                static_cast<float>(indexPoint.z()));
        }
        const openvdb::tools::QuadAndTriangleDataAdapter<
            openvdb::Vec3s,
            openvdb::Vec3I> meshAdapter(indexPoints, triangles);
        PointInClosedMeshQuery parityQuery(mesh);
        PointInClosedMeshQueryOptions parityOptions;
        parityOptions.boundaryEpsilonMm = std::max(
            options.voxel_size_mm * 1.0e-6,
            1.0e-9);
        const auto interiorTest =
            [&parityQuery, &parityOptions, &transform](
                const openvdb::Coord& coordinate)
            {
                const openvdb::Vec3d worldPoint = transform->indexToWorld(
                    coordinate.asVec3d());
                PointInClosedMeshQueryStats queryStats;
                const PointInClosedMeshResult classification =
                    parityQuery.Classify(
                        {worldPoint.x(), worldPoint.y(), worldPoint.z()},
                        parityOptions,
                        queryStats);
                return classification.inside && !classification.ambiguous;
            };
        openvdb::util::NullInterrupter interrupter;
        grid = openvdb::tools::meshToVolume<openvdb::FloatGrid>(
            interrupter,
            meshAdapter,
            *transform,
            static_cast<float>(options.exterior_band_voxels),
            static_cast<float>(options.interior_band_voxels),
            0,
            nullptr,
            interiorTest,
            openvdb::tools::EVAL_EVERY_VOXEL);
    }
    else
    {
        const float halfWidth = static_cast<float>(std::max(
            options.exterior_band_voxels,
            options.interior_band_voxels));
        grid = openvdb::tools::meshToLevelSet<openvdb::FloatGrid>(
            *transform,
            worldPoints,
            triangles,
            quads,
            halfWidth);
    }

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
    result.memory_bytes = static_cast<std::uint64_t>(grid->memUsage());
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

OpenVdbSignedDistanceSample SampleOpenVdbSignedDistanceWorld(
    const OpenVdbLevelSetResult& levelSet,
    const Vec3& pointMm)
{
    OpenVdbSignedDistanceSample result;
    if (!levelSet.available || !levelSet.generated || levelSet.grid == nullptr)
    {
        result.error = levelSet.error.empty()
            ? "OpenVDB level set unavailable for world-space sampling"
            : levelSet.error;
        return result;
    }
    if (!std::isfinite(pointMm.x)
        || !std::isfinite(pointMm.y)
        || !std::isfinite(pointMm.z))
    {
        result.error = "OpenVDB world-space sample point must be finite";
        return result;
    }

#ifdef SLICER_CORE_USE_OPENVDB
    try
    {
        const std::shared_ptr<openvdb::FloatGrid> grid =
            std::static_pointer_cast<openvdb::FloatGrid>(levelSet.grid);
        const openvdb::Vec3d worldPoint{
            pointMm.x,
            pointMm.y,
            pointMm.z};
        const openvdb::Vec3d indexPoint =
            grid->transform().worldToIndex(worldPoint);
        const openvdb::Coord nearestCoord = openvdb::Coord::round(indexPoint);
        const openvdb::FloatGrid::ConstAccessor accessor =
            grid->getConstAccessor();
        const openvdb::tools::GridSampler<
            openvdb::FloatGrid,
            openvdb::tools::BoxSampler> sampler(*grid);
        const float signedDistance = sampler.wsSample(worldPoint);
        if (!std::isfinite(signedDistance))
        {
            result.error = "OpenVDB world-space sample returned a non-finite distance";
            return result;
        }
        result.available = true;
        result.active = accessor.isValueOn(nearestCoord);
        result.signedDistanceMm = static_cast<double>(signedDistance);
    }
    catch (const std::exception& error)
    {
        result.error = std::string{"OpenVDB world-space sample failed: "}
            + error.what();
    }
#else
    (void)pointMm;
    result.error = "USE_OPENVDB=OFF; world-space sampling unavailable";
#endif
    return result;
}

}  // namespace slicer_core
