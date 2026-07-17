#pragma once

#include "slicer_core/geometry/GeometryKernelTypes.h"
#include "slicer_core/geometry/TriangleMeshData.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Options for converting a triangle mesh to an OpenVDB level set.
 */
struct OpenVdbLevelSetOptions
{
    double voxel_size_mm{0.05};
    double exterior_band_voxels{3.0};
    double interior_band_voxels{3.0};
    int bbox_padding_voxels{2};
    bool use_parity_interior_test{false};
};

/**
 * @brief Integer coordinate bounds used by the experimental SDF scanner.
 */
struct IndexBounds3D
{
    int min_x{0};
    int min_y{0};
    int min_z{0};
    int max_x{0};
    int max_y{0};
    int max_z{0};
};

/**
 * @brief Result of OpenVDB level set construction.
 */
struct OpenVdbLevelSetResult
{
    bool available{false};
    bool generated{false};
    OpenVdbStatus status;
    double voxel_size_mm{0.05};
    int active_voxels{0};
    std::uint64_t memory_bytes{0};
    IndexBounds3D scan_bounds;
    IndexBounds3D active_bounds;
    std::shared_ptr<void> grid;
    std::vector<std::string> warnings;
    std::string error;
};

/**
 * @brief Backend-neutral result of sampling an OpenVDB level set in world space.
 */
struct OpenVdbSignedDistanceSample
{
    bool available{false};
    bool active{false};
    double signedDistanceMm{0.0};
    std::string error;
};

/**
 * @brief Build an OpenVDB level set from a triangle mesh when USE_OPENVDB is enabled.
 * @param mesh Input triangle mesh.
 * @param options Level set options.
 * @return Build result; returns unavailable when OpenVDB is disabled.
 */
OpenVdbLevelSetResult BuildOpenVdbLevelSet(const TriangleMeshData& mesh, const OpenVdbLevelSetOptions& options);

/**
 * @brief Convert an OpenVDB index coordinate to world millimeters.
 * @param levelSet Generated level set.
 * @param x Index-space X coordinate.
 * @param y Index-space Y coordinate.
 * @param z Index-space Z coordinate.
 * @return World-space point in millimeters.
 */
Vec3 OpenVdbIndexToWorld(const OpenVdbLevelSetResult& levelSet, double x, double y, double z);

/**
 * @brief Sample a generated OpenVDB level set at one world-space point.
 * @param levelSet Generated level set.
 * @param pointMm World-space point in millimeters.
 * @return Signed-distance sample without exposing OpenVDB types.
 */
OpenVdbSignedDistanceSample SampleOpenVdbSignedDistanceWorld(
    const OpenVdbLevelSetResult& levelSet,
    const Vec3& pointMm);

}  // namespace slicer_core
