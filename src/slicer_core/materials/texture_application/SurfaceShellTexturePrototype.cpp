#include "slicer_core/materials/texture_application/SurfaceShellTexturePrototype.h"

#include <algorithm>
#include <stdexcept>

namespace slicer_core
{
namespace
{

std::array<std::uint8_t, 3> CheckerRgb(const int x, const int y, const int z)
{
    const int checker = ((x / 2) + (y / 2) + (z / 2)) % 2;
    if (checker == 0)
    {
        return {230, 70, 50};
    }
    return {40, 140, 240};
}

std::array<std::uint8_t, 3> ShellRgbAt(
    const SurfaceShellTextureOptions& options,
    const int x,
    const int y,
    const int z)
{
    if (options.texture_source == SurfaceShellTextureSource::Constant)
    {
        return options.constant_rgb;
    }
    return CheckerRgb(x, y, z);
}

void AppendWarnings(std::vector<std::string>& target, const std::vector<std::string>& source)
{
    target.insert(target.end(), source.begin(), source.end());
}

}  // namespace

SurfaceShellTextureSource ParseSurfaceShellTextureSource(const std::string& value)
{
    if (value == "constant")
    {
        return SurfaceShellTextureSource::Constant;
    }
    if (value == "checker")
    {
        return SurfaceShellTextureSource::Checker;
    }
    throw std::runtime_error("unsupported texture source: " + value);
}

std::string SurfaceShellTextureSourceName(const SurfaceShellTextureSource source)
{
    switch (source)
    {
    case SurfaceShellTextureSource::Constant:
        return "constant";
    case SurfaceShellTextureSource::Checker:
        return "checker";
    }
    return "unknown";
}

SurfaceShellTextureResult RunSurfaceShellTexturePrototype(const SurfaceShellTextureOptions& options)
{
    SurfaceShellTextureResult result;
    result.options = options;

    if (options.case_name != "generated-box")
    {
        result.error = "unsupported surface shell texture case: " + options.case_name;
        return result;
    }
    if (!(options.voxel_size_mm > 0.0))
    {
        result.error = "voxel size must be positive";
        return result;
    }
    if (!(options.shell_thickness_mm > 0.0))
    {
        result.error = "shell thickness must be positive";
        return result;
    }

    result.mesh = MakeGeneratedBoxMesh(1.0, 0.8, 0.6);

    OpenVdbLevelSetOptions levelSetOptions;
    levelSetOptions.voxel_size_mm = options.voxel_size_mm;
    levelSetOptions.exterior_band_voxels = 4.0;
    levelSetOptions.interior_band_voxels = std::max(4.0, options.shell_thickness_mm / options.voxel_size_mm + 4.0);
    levelSetOptions.bbox_padding_voxels = 2;
    result.level_set = BuildOpenVdbLevelSet(result.mesh, levelSetOptions);
    AppendWarnings(result.warnings, result.level_set.warnings);
    if (!result.level_set.error.empty())
    {
        result.error = result.level_set.error;
        return result;
    }

    OpenVdbSurfaceShellOptions shellOptions;
    shellOptions.shell_thickness_mm = options.shell_thickness_mm;
    result.shell = ClassifyOpenVdbSurfaceShell(result.level_set, shellOptions);
    AppendWarnings(result.warnings, result.shell.warnings);
    if (!result.shell.error.empty())
    {
        result.error = result.shell.error;
        return result;
    }

    result.shell_rgb.assign(result.shell.shell_mask.size(), {255, 255, 255});
    for (int z{0}; z < result.shell.depth; ++z)
    {
        for (int y{0}; y < result.shell.height; ++y)
        {
            for (int x{0}; x < result.shell.width; ++x)
            {
                const std::size_t index = MaskIndex3D(result.shell.width, result.shell.height, x, y, z);
                if (result.shell.shell_mask.at(index) != 0)
                {
                    result.shell_rgb.at(index) = ShellRgbAt(options, x, y, z);
                    ++result.colored_shell_voxels;
                }
            }
        }
    }

    result.outside_colored_voxels = 0;
    result.shell.outside_colored_voxels = result.outside_colored_voxels;
    return result;
}

std::vector<std::uint8_t> BuildSurfaceShellPreviewPixels(
    const SurfaceShellTextureResult& result,
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
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(result.shell.width) + static_cast<std::size_t>(x)) * 3U;

            if (mode == "shell")
            {
                if (result.shell.shell_mask.at(maskIndex) != 0)
                {
                    const std::array<std::uint8_t, 3>& rgb = result.shell_rgb.at(maskIndex);
                    pixels.at(pixelIndex + 0U) = rgb.at(0);
                    pixels.at(pixelIndex + 1U) = rgb.at(1);
                    pixels.at(pixelIndex + 2U) = rgb.at(2);
                }
            }
            else if (mode == "interior")
            {
                if (result.shell.interior_mask.at(maskIndex) != 0)
                {
                    pixels.at(pixelIndex + 0U) = 128;
                    pixels.at(pixelIndex + 1U) = 128;
                    pixels.at(pixelIndex + 2U) = 128;
                }
            }
            else if (mode == "composite")
            {
                if (result.shell.interior_mask.at(maskIndex) != 0)
                {
                    pixels.at(pixelIndex + 0U) = 128;
                    pixels.at(pixelIndex + 1U) = 128;
                    pixels.at(pixelIndex + 2U) = 128;
                }
                if (result.shell.shell_mask.at(maskIndex) != 0)
                {
                    const std::array<std::uint8_t, 3>& rgb = result.shell_rgb.at(maskIndex);
                    pixels.at(pixelIndex + 0U) = rgb.at(0);
                    pixels.at(pixelIndex + 1U) = rgb.at(1);
                    pixels.at(pixelIndex + 2U) = rgb.at(2);
                }
            }
            else
            {
                throw std::runtime_error("unsupported surface shell preview mode: " + mode);
            }
        }
    }
    return pixels;
}

}  // namespace slicer_core
