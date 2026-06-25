#include "slicer_core/materials/texture_application/SurfaceTextureTransfer.h"

#include "slicer_core/materials/texture_application/SurfaceAttributeMap.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <set>

namespace slicer_core
{
namespace
{

std::uint32_t EncodeRgb(const std::array<std::uint8_t, 3>& rgb)
{
    return (static_cast<std::uint32_t>(rgb.at(0)) << 16U)
        | (static_cast<std::uint32_t>(rgb.at(1)) << 8U)
        | static_cast<std::uint32_t>(rgb.at(2));
}

std::array<std::uint8_t, 3> SelectNonTextureColor(
    const MaterialInfo* material,
    const SurfaceTextureTransferOptions& options,
    SurfaceTextureTransferStats& stats,
    ShellColorSource& source)
{
    if (material != nullptr && material->has_diffuse)
    {
        ++stats.material_diffuse_voxels;
        source = ShellColorSource::MaterialDiffuse;
        return material->diffuse_rgb;
    }
    ++stats.fallback_voxels;
    source = ShellColorSource::Fallback;
    return options.fallback_rgb;
}

std::uint64_t EstimateTextureBytes(const TextureImage& image)
{
    return static_cast<std::uint64_t>(image.width) * static_cast<std::uint64_t>(image.height) * 4ULL;
}

}  // namespace

TexCoord InterpolateUv(
    const std::array<TexCoord, 3>& uv,
    const std::array<double, 3>& barycentric)
{
    TexCoord result;
    for (std::size_t index{0}; index < 3U; ++index)
    {
        result.u += uv.at(index).u * barycentric.at(index);
        result.v += uv.at(index).v * barycentric.at(index);
    }
    return result;
}

SurfaceTextureTransferResult TransferSurfaceTexture(
    const AdaptedTriangleMesh& adaptedMesh,
    const OpenVdbLevelSetResult& levelSet,
    const OpenVdbSurfaceShellResult& shell,
    const SurfaceTextureTransferOptions& options)
{
    SurfaceTextureTransferResult result;
    if (!shell.error.empty())
    {
        result.error = shell.error;
        return result;
    }
    if (adaptedMesh.mesh.triangles.size() != adaptedMesh.triangle_attributes.size())
    {
        result.error = "triangle attribute count does not match mesh triangle count";
        return result;
    }

    const double maxTransferDistance = options.max_transfer_distance_mm > 0.0
        ? options.max_transfer_distance_mm
        : shell.shell_thickness_mm + std::sqrt(3.0) * levelSet.voxel_size_mm;

    result.shell_rgb.assign(shell.shell_mask.size(), {255, 255, 255});
    result.color_sources.assign(shell.shell_mask.size(), ShellColorSource::Fallback);
    SurfaceAttributeMap attributes(adaptedMesh);
    const auto bvhStart = std::chrono::steady_clock::now();
    NearestTriangleQuery nearest(adaptedMesh.mesh);
    NearestTriangleQueryStats nearestStats = nearest.GetBuildStats();
    result.bvh_build_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - bvhStart).count();
    std::map<std::string, TextureImage> textureCache;
    std::set<std::uint32_t> uniqueColors;
    result.stats.material_count = static_cast<int>(adaptedMesh.material_infos.size());
    for (const MaterialInfo& material : adaptedMesh.material_infos)
    {
        if (material.has_texture)
        {
            ++result.stats.texture_count;
        }
    }

    const auto transferStart = std::chrono::steady_clock::now();

    for (int localZ{0}; localZ < shell.depth; ++localZ)
    {
        for (int localY{0}; localY < shell.height; ++localY)
        {
            for (int localX{0}; localX < shell.width; ++localX)
            {
                const std::size_t maskIndex = MaskIndex3D(shell.width, shell.height, localX, localY, localZ);
                if (shell.shell_mask.at(maskIndex) == 0)
                {
                    continue;
                }

                const double indexX = static_cast<double>(shell.bounds.min_x + localX);
                const double indexY = static_cast<double>(shell.bounds.min_y + localY);
                const double indexZ = static_cast<double>(shell.bounds.min_z + localZ);
                const Vec3 worldPoint = OpenVdbIndexToWorld(levelSet, indexX, indexY, indexZ);
                NearestTriangleQueryOptions queryOptions;
                queryOptions.tie_epsilon_mm = options.tie_epsilon_mm;
                const NearestTriangleHit hit = nearest.FindNearestWithStats(worldPoint, queryOptions, nearestStats);
                if (!hit.found)
                {
                    ++result.stats.query_failed_voxels;
                    ++result.stats.fallback_voxels;
                    result.shell_rgb.at(maskIndex) = options.fallback_rgb;
                    uniqueColors.insert(EncodeRgb(options.fallback_rgb));
                    continue;
                }

                result.stats.max_observed_distance_mm = std::max(result.stats.max_observed_distance_mm, hit.distance_mm);
                const SurfaceTriangleAttributes& triangleAttributes = attributes.TriangleAttributes(hit.triangle_index);
                const MaterialInfo* material = attributes.FindMaterial(triangleAttributes.material_name);
                ShellColorSource source = ShellColorSource::Fallback;
                std::array<std::uint8_t, 3> rgb{};

                const bool distanceAccepted = hit.distance_mm <= maxTransferDistance;
                if (!distanceAccepted)
                {
                    ++result.stats.transfer_distance_exceeded_voxels;
                    rgb = SelectNonTextureColor(material, options, result.stats, source);
                }
                else if (!triangleAttributes.has_uv)
                {
                    ++result.stats.missing_uv_voxels;
                    rgb = SelectNonTextureColor(material, options, result.stats, source);
                }
                else if (material == nullptr || !material->has_texture || !material->texture_exists)
                {
                    ++result.stats.missing_texture_voxels;
                    rgb = SelectNonTextureColor(material, options, result.stats, source);
                }
                else
                {
                    try
                    {
                        const std::string textureKey = material->diffuse_texture_path.lexically_normal().generic_string();
                        auto found = textureCache.find(textureKey);
                        if (found == textureCache.end())
                        {
                            found = textureCache.emplace(textureKey, load_texture_image(material->diffuse_texture_path)).first;
                            ++result.stats.texture_cache_misses;
                            result.stats.texture_cache_bytes += EstimateTextureBytes(found->second);
                        }
                        else
                        {
                            ++result.stats.texture_cache_hits;
                        }
                        const TexCoord uv = InterpolateUv(triangleAttributes.uv, hit.barycentric);
                        bool uvOutOfRange{false};
                        rgb = sample_texture_rgb(
                            found->second,
                            uv.u,
                            uv.v,
                            options.texture_sample,
                            uvOutOfRange);
                        if (uvOutOfRange)
                        {
                            ++result.stats.uv_out_of_range_voxels;
                        }
                        ++result.stats.sampled_texture_voxels;
                        ++result.stats.per_material_sampled_voxels[triangleAttributes.material_name];
                        ++result.stats.per_texture_sampled_voxels[textureKey];
                        source = ShellColorSource::Texture;
                    }
                    catch (const std::exception& error)
                    {
                        ++result.stats.missing_texture_voxels;
                        result.warnings.push_back("texture load/sample failed: " + std::string{error.what()});
                        rgb = SelectNonTextureColor(material, options, result.stats, source);
                    }
                }

                result.shell_rgb.at(maskIndex) = rgb;
                result.color_sources.at(maskIndex) = source;
                uniqueColors.insert(EncodeRgb(rgb));
            }
        }
    }

    result.stats.unique_color_count = static_cast<int>(uniqueColors.size());
    result.stats.loaded_texture_count = static_cast<int>(textureCache.size());
    result.stats.nearest_query_stats = nearestStats;
    result.outside_colored_voxels = 0;
    result.transfer_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - transferStart).count();
    return result;
}

}  // namespace slicer_core
