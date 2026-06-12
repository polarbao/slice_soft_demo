#include "slicer_core/materials/texture_application/SurfaceShellRealModelPrototype.h"

#include <chrono>

namespace slicer_core
{
namespace
{

void AppendWarnings(std::vector<std::string>& target, const std::vector<std::string>& source)
{
    target.insert(target.end(), source.begin(), source.end());
}

std::uint64_t EstimateBytes(const SurfaceShellRealModelResult& result)
{
    return static_cast<std::uint64_t>(result.adapted_mesh.mesh.vertices.size() * sizeof(Vec3))
        + static_cast<std::uint64_t>(result.adapted_mesh.mesh.triangles.size() * sizeof(std::array<int, 3>))
        + static_cast<std::uint64_t>(result.shell.inside_mask.size() + result.shell.shell_mask.size()
                                     + result.shell.interior_mask.size())
        + static_cast<std::uint64_t>(result.transfer.shell_rgb.size() * sizeof(std::array<std::uint8_t, 3>));
}

}  // namespace

SurfaceShellRealModelResult RunSurfaceShellRealModelPrototype(
    const SceneModel& scene,
    const SliceConfig& config,
    const SurfaceShellRealModelOptions& options)
{
    SurfaceShellRealModelResult result;
    result.case_name = scene.format + "-real-texture";
    result.input_format = scene.format;
    result.model_path = scene.model_path.generic_string();
    result.options = options;

    const auto adapterStart = std::chrono::steady_clock::now();
    result.adapted_mesh = AdaptSceneModelToTriangleMesh(scene);
    result.performance.adapter_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - adapterStart).count();

    const std::string topologyError = ValidateMeshTopology(result.adapted_mesh.topology, options.mesh_policy);
    if (!topologyError.empty())
    {
        result.errors.push_back(topologyError);
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
    result.performance.peak_estimated_bytes = EstimateBytes(result);
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
