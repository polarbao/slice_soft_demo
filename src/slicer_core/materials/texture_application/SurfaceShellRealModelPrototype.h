#pragma once

#include "slicer_core/geometry/OpenVdbLevelSetBuilder.h"
#include "slicer_core/geometry/OpenVdbSurfaceShell.h"
#include "slicer_core/geometry/MeshRobustnessDiagnostics.h"
#include "slicer_core/geometry/MeshScaleTolerance.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/materials/texture_application/SurfaceTextureTransfer.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Options for the 09B-R1 real-model shell texture prototype.
 */
struct SurfaceShellRealModelOptions
{
    double voxel_size_mm{0.05};
    double shell_thickness_mm{0.10};
    double max_transfer_distance_mm{0.0};
    MeshValidationPolicy mesh_policy{MeshValidationPolicy::StrictClosed};
    std::array<std::uint8_t, 3> fallback_rgb{0, 0, 0};
    TextureSampleOptions texture_sample;
    bool reject_self_intersection{false};
};

/**
 * @brief Performance counters for one real-model prototype run.
 */
struct SurfaceShellRealModelPerformance
{
    double import_ms{0.0};
    double adapter_ms{0.0};
    double level_set_ms{0.0};
    double bvh_build_ms{0.0};
    double transfer_ms{0.0};
    double preview_ms{0.0};
    double total_ms{0.0};
    std::uint64_t peak_estimated_bytes{0};
    std::uint64_t mesh_bytes{0};
    std::uint64_t triangle_attribute_bytes{0};
    std::uint64_t mask_bytes{0};
    std::uint64_t shell_rgb_bytes{0};
    std::uint64_t bvh_estimated_bytes{0};
    std::uint64_t texture_cache_bytes{0};
    std::uint64_t openvdb_grid_bytes{0};
    std::uint64_t preview_buffer_bytes{0};
    bool process_peak_working_set_available{false};
    std::uint64_t process_peak_working_set_bytes{0};
};

/**
 * @brief Complete real-model shell texture prototype result.
 */
struct SurfaceShellRealModelResult
{
    std::string case_name;
    std::string input_format;
    std::string model_path;
    std::string config_path;
    SurfaceShellRealModelOptions options;
    MeshScaleTolerance tolerance;
    AdaptedTriangleMesh adapted_mesh;
    MeshRobustnessReport robustness;
    OpenVdbLevelSetResult level_set;
    OpenVdbSurfaceShellResult shell;
    SurfaceTextureTransferResult transfer;
    SurfaceShellRealModelPerformance performance;
    bool non_production{false};
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

/**
 * @brief Run the real-model OpenVDB shell and UV transfer prototype.
 * @param scene Imported scene model.
 * @param config Source slice configuration used for texture sampling defaults.
 * @param options Prototype options.
 * @return Complete prototype result.
 */
SurfaceShellRealModelResult RunSurfaceShellRealModelPrototype(
    const SceneModel& scene,
    const SliceConfig& config,
    const SurfaceShellRealModelOptions& options);

/**
 * @brief Build real-model RGB preview pixels for a selected layer.
 * @param result Prototype result.
 * @param layerZ Local layer index.
 * @param mode shell, interior, or composite.
 * @return RGB preview pixels.
 */
std::vector<std::uint8_t> BuildSurfaceShellRealModelPreviewPixels(
    const SurfaceShellRealModelResult& result,
    int layerZ,
    const std::string& mode);

}  // namespace slicer_core
