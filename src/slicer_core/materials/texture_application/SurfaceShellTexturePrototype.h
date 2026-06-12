#pragma once

#include "slicer_core/geometry/OpenVdbLevelSetBuilder.h"
#include "slicer_core/geometry/OpenVdbSurfaceShell.h"
#include "slicer_core/geometry/TriangleMeshData.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Texture source used by the 09B surface shell prototype.
 */
enum class SurfaceShellTextureSource
{
    Constant,
    Checker
};

/**
 * @brief Options for running the experimental surface shell texture prototype.
 */
struct SurfaceShellTextureOptions
{
    std::string case_name{"generated-box"};
    double voxel_size_mm{0.05};
    double shell_thickness_mm{0.10};
    SurfaceShellTextureSource texture_source{SurfaceShellTextureSource::Checker};
    std::array<std::uint8_t, 3> constant_rgb{40, 160, 240};
    std::string fill_role{"base"};
};

/**
 * @brief Result of the experimental surface shell texture prototype.
 */
struct SurfaceShellTextureResult
{
    SurfaceShellTextureOptions options;
    TriangleMeshData mesh;
    OpenVdbLevelSetResult level_set;
    OpenVdbSurfaceShellResult shell;
    int colored_shell_voxels{0};
    int outside_colored_voxels{0};
    std::vector<std::array<std::uint8_t, 3>> shell_rgb;
    std::vector<std::string> warnings;
    std::string error;
};

/**
 * @brief Parse a texture source name.
 * @param value Source name.
 * @return Texture source enum.
 */
SurfaceShellTextureSource ParseSurfaceShellTextureSource(const std::string& value);

/**
 * @brief Convert texture source enum to a stable report name.
 * @param source Texture source enum.
 * @return Source name.
 */
std::string SurfaceShellTextureSourceName(SurfaceShellTextureSource source);

/**
 * @brief Run the 09B generated fixture surface shell texture prototype.
 * @param options Prototype options.
 * @return Prototype result.
 */
SurfaceShellTextureResult RunSurfaceShellTexturePrototype(const SurfaceShellTextureOptions& options);

/**
 * @brief Build RGB preview pixels for a selected layer.
 * @param result Prototype result.
 * @param layerZ Local Z layer index.
 * @param mode Preview mode: shell, interior, or composite.
 * @return RGB pixels sized width * height * 3.
 */
std::vector<std::uint8_t> BuildSurfaceShellPreviewPixels(
    const SurfaceShellTextureResult& result,
    int layerZ,
    const std::string& mode);

}  // namespace slicer_core
