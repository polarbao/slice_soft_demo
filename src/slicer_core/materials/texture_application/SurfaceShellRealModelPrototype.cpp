#include "slicer_core/materials/texture_application/SurfaceShellRealModelPrototype.h"

#include "slicer_core/system/ProcessMemoryStats.h"

#include <chrono>
#include <algorithm>

namespace slicer_core
{
namespace
{

void AppendWarnings(std::vector<std::string>& target, const std::vector<std::string>& source)
{
    target.insert(target.end(), source.begin(), source.end());
}

SurfaceShellRealModelPerformance EstimateMemory(const SurfaceShellRealModelResult& result)
{
    SurfaceShellRealModelPerformance memory;
    memory.mesh_bytes =
        static_cast<std::uint64_t>(result.adapted_mesh.mesh.vertices.size() * sizeof(Vec3))
        + static_cast<std::uint64_t>(result.adapted_mesh.mesh.triangles.size() * sizeof(std::array<int, 3>));
    memory.triangle_attribute_bytes =
        static_cast<std::uint64_t>(result.adapted_mesh.triangle_attributes.size() * sizeof(SurfaceTriangleAttributes));
    memory.mask_bytes =
        static_cast<std::uint64_t>(result.shell.inside_mask.size() + result.shell.shell_mask.size()
                                   + result.shell.interior_mask.size());
    memory.shell_rgb_bytes =
        static_cast<std::uint64_t>(result.transfer.shell_rgb.size() * sizeof(std::array<std::uint8_t, 3>));
    memory.bvh_estimated_bytes = static_cast<std::uint64_t>(result.transfer.stats.nearest_query_stats.estimated_bytes);
    memory.texture_cache_bytes = result.transfer.stats.texture_cache_bytes;
    memory.openvdb_grid_bytes = result.level_set.memory_bytes;
    memory.peak_estimated_bytes =
        memory.mesh_bytes
        + memory.triangle_attribute_bytes
        + memory.mask_bytes
        + memory.shell_rgb_bytes
        + memory.bvh_estimated_bytes
        + memory.texture_cache_bytes
        + memory.openvdb_grid_bytes
        + memory.preview_buffer_bytes;
    return memory;
}

}  // namespace

SurfaceShellRealModelResult RunSurfaceShellRealModelPrototype(
    const SceneModel& scene,
    const SliceConfig& config,
    const SurfaceShellRealModelOptions& options)
{
    const auto totalStart = std::chrono::steady_clock::now();
    SurfaceShellRealModelResult result;
    result.case_name = scene.format + "-real-texture";
    result.input_format = scene.format;
    result.model_path = scene.model_path.generic_string();
    result.options = options;
    result.non_production = options.mesh_policy == MeshValidationPolicy::WarnAndAttempt;
    result.tolerance = MakeMeshScaleTolerance(scene.bbox_mm, options.voxel_size_mm);

    const auto adapterStart = std::chrono::steady_clock::now();
    SceneModelTriangleMeshAdapterOptions adapterOptions;
    adapterOptions.position_epsilon_mm = result.tolerance.position_epsilon_mm;
    adapterOptions.degenerate_area_epsilon_mm2 = result.tolerance.area_epsilon_mm2;
    result.adapted_mesh = AdaptSceneModelToTriangleMesh(scene, adapterOptions);
    result.performance.adapter_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - adapterStart).count();

    MeshRobustnessOptions robustnessOptions;
    robustnessOptions.tolerance = result.tolerance;
    result.robustness = AnalyzeMeshRobustness(result.adapted_mesh.mesh, robustnessOptions);
    AppendWarnings(result.warnings, result.robustness.warnings);

    const std::string topologyError = ValidateMeshTopology(result.adapted_mesh.topology, options.mesh_policy);
    if (!topologyError.empty())
    {
        result.errors.push_back(topologyError);
        result.performance.total_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - totalStart).count();
        return result;
    }
    const std::string robustnessError = ValidateMeshRobustness(result.robustness, options.reject_self_intersection);
    if (options.mesh_policy == MeshValidationPolicy::StrictClosed && !robustnessError.empty())
    {
        result.errors.push_back(robustnessError);
        result.performance.total_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - totalStart).count();
        return result;
    }
    if (options.mesh_policy == MeshValidationPolicy::WarnAndAttempt)
    {
        if (result.adapted_mesh.topology.boundary_edges > 0)
        {
            result.warnings.push_back("warn_and_attempt mesh has boundary edges");
        }
        if (result.adapted_mesh.topology.non_manifold_edges > 0)
        {
            result.warnings.push_back("warn_and_attempt mesh has non-manifold edges");
        }
        if (!robustnessError.empty())
        {
            result.warnings.push_back("warn_and_attempt " + robustnessError);
        }
        result.warnings.push_back("warn_and_attempt result is nonProduction");
    }

    OpenVdbLevelSetOptions levelSetOptions;
    levelSetOptions.voxel_size_mm = options.voxel_size_mm;
    levelSetOptions.exterior_band_voxels = 4.0;
    levelSetOptions.interior_band_voxels =
        std::max(4.0, options.shell_thickness_mm / options.voxel_size_mm + 4.0);
    levelSetOptions.bbox_padding_voxels = 2;

    const auto levelSetStart = std::chrono::steady_clock::now();
    result.level_set = BuildOpenVdbLevelSet(result.adapted_mesh.mesh, levelSetOptions);
    result.performance.level_set_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - levelSetStart).count();
    AppendWarnings(result.warnings, result.level_set.warnings);
    if (!result.level_set.error.empty())
    {
        result.errors.push_back(result.level_set.error);
        return result;
    }

    OpenVdbSurfaceShellOptions shellOptions;
    shellOptions.shell_thickness_mm = options.shell_thickness_mm;
    result.shell = ClassifyOpenVdbSurfaceShell(result.level_set, shellOptions);
    AppendWarnings(result.warnings, result.shell.warnings);
    if (!result.shell.error.empty())
    {
        result.errors.push_back(result.shell.error);
        return result;
    }

    SurfaceTextureTransferOptions transferOptions;
    transferOptions.max_transfer_distance_mm = options.max_transfer_distance_mm;
    transferOptions.tie_epsilon_mm = result.tolerance.tie_epsilon_mm;
    transferOptions.fallback_rgb = options.fallback_rgb;
    transferOptions.texture_sample = options.texture_sample;
    result.transfer = TransferSurfaceTexture(
        result.adapted_mesh,
        result.level_set,
        result.shell,
        transferOptions);
    AppendWarnings(result.warnings, result.transfer.warnings);
    if (!result.transfer.error.empty())
    {
        result.errors.push_back(result.transfer.error);
        return result;
    }
    result.performance.bvh_build_ms = result.transfer.bvh_build_ms;
    result.performance.transfer_ms = result.transfer.transfer_ms;
    const SurfaceShellRealModelPerformance memory = EstimateMemory(result);
    result.performance.mesh_bytes = memory.mesh_bytes;
    result.performance.triangle_attribute_bytes = memory.triangle_attribute_bytes;
    result.performance.mask_bytes = memory.mask_bytes;
    result.performance.shell_rgb_bytes = memory.shell_rgb_bytes;
    result.performance.bvh_estimated_bytes = memory.bvh_estimated_bytes;
    result.performance.texture_cache_bytes = memory.texture_cache_bytes;
    result.performance.openvdb_grid_bytes = memory.openvdb_grid_bytes;
    result.performance.peak_estimated_bytes = memory.peak_estimated_bytes;
    const ProcessMemoryStats processMemory = CaptureProcessMemoryStats();
    result.performance.process_peak_working_set_available = processMemory.available;
    result.performance.process_working_set_bytes = processMemory.working_set_bytes;
    result.performance.process_peak_working_set_bytes = processMemory.peak_working_set_bytes;
    result.performance.total_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - totalStart).count();
    (void)config;
    return result;
}

std::vector<std::uint8_t> BuildSurfaceShellRealModelPreviewPixels(
    const SurfaceShellRealModelResult& result,
    const int layerZ,
    const std::string& mode)
{
    if (layerZ < 0 || layerZ >= result.shell.depth)
    {
        throw std::runtime_error("preview layer index is out of range");
    }

    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(result.shell.width) * static_cast<std::size_t>(result.shell.height) * 3U,
        255);
    for (int y{0}; y < result.shell.height; ++y)
    {
        for (int x{0}; x < result.shell.width; ++x)
        {
            const std::size_t maskIndex = MaskIndex3D(result.shell.width, result.shell.height, x, y, layerZ);
            const std::size_t pixelIndex =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(result.shell.width)
                 + static_cast<std::size_t>(x))
                * 3U;
            if ((mode == "interior" || mode == "composite") && result.shell.interior_mask.at(maskIndex) != 0)
            {
                pixels.at(pixelIndex + 0U) = 128;
                pixels.at(pixelIndex + 1U) = 128;
                pixels.at(pixelIndex + 2U) = 128;
            }
            if ((mode == "shell" || mode == "composite") && result.shell.shell_mask.at(maskIndex) != 0)
            {
                const std::array<std::uint8_t, 3>& rgb = result.transfer.shell_rgb.at(maskIndex);
                pixels.at(pixelIndex + 0U) = rgb.at(0);
                pixels.at(pixelIndex + 1U) = rgb.at(1);
                pixels.at(pixelIndex + 2U) = rgb.at(2);
            }
        }
    }
    return pixels;
}

}  // namespace slicer_core
