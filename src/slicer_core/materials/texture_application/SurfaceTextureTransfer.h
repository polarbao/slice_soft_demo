#pragma once

#include "slicer_core/config.h"
#include "slicer_core/geometry/NearestTriangleQuery.h"
#include "slicer_core/geometry/OpenVdbSurfaceShell.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/texture_image.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Color source selected for a shell voxel.
 */
enum class ShellColorSource
{
    Texture,
    MaterialDiffuse,
    Fallback
};

/**
 * @brief Real-model surface texture transfer options.
 */
struct SurfaceTextureTransferOptions
{
    double max_transfer_distance_mm{0.0};
    double tie_epsilon_mm{1.0e-7};
    std::array<std::uint8_t, 3> fallback_rgb{0, 0, 0};
    TextureSampleOptions texture_sample;
};

/**
 * @brief Texture transfer statistics for report schema v2.
 */
struct SurfaceTextureTransferStats
{
    int sampled_texture_voxels{0};
    int material_diffuse_voxels{0};
    int fallback_voxels{0};
    int missing_uv_voxels{0};
    int missing_texture_voxels{0};
    int uv_out_of_range_voxels{0};
    int repeated_sampled_voxels{0};
    int transfer_distance_exceeded_voxels{0};
    int query_failed_voxels{0};
    int unique_color_count{0};
    int loaded_texture_count{0};
    int texture_cache_hits{0};
    int texture_cache_misses{0};
    std::uint64_t texture_cache_bytes{0};
    int material_count{0};
    int texture_count{0};
    NearestTriangleQueryStats nearest_query_stats;
    std::map<std::string, int> per_material_sampled_voxels;
    std::map<std::string, int> per_texture_sampled_voxels;
    double max_observed_distance_mm{0.0};
};

/**
 * @brief RGB transfer result for all shell voxels.
 */
struct SurfaceTextureTransferResult
{
    std::vector<std::array<std::uint8_t, 3>> shell_rgb;
    std::vector<ShellColorSource> color_sources;
    SurfaceTextureTransferStats stats;
    int outside_colored_voxels{0};
    double bvh_build_ms{0.0};
    double transfer_ms{0.0};
    std::vector<std::string> warnings;
    std::string error;
};

/**
 * @brief Transfer source texture/material color onto OpenVDB shell voxels.
 * @param adaptedMesh Adapted source mesh and attributes.
 * @param levelSet OpenVDB level set.
 * @param shell Shell classification result.
 * @param options Transfer options.
 * @return Texture transfer result.
 */
SurfaceTextureTransferResult TransferSurfaceTexture(
    const AdaptedTriangleMesh& adaptedMesh,
    const OpenVdbLevelSetResult& levelSet,
    const OpenVdbSurfaceShellResult& shell,
    const SurfaceTextureTransferOptions& options);

/**
 * @brief Interpolate UV coordinates using barycentric weights.
 * @param uv Triangle UV coordinates.
 * @param barycentric Barycentric weights.
 * @return Interpolated UV.
 */
TexCoord InterpolateUv(
    const std::array<TexCoord, 3>& uv,
    const std::array<double, 3>& barycentric);

}  // namespace slicer_core
