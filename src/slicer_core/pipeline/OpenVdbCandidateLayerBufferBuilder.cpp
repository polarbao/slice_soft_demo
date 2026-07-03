#include "slicer_core/pipeline/OpenVdbCandidateLayerBufferBuilder.h"

#include <cstddef>
#include <utility>

namespace slicer_core
{
namespace
{

std::size_t VoxelCount(const OpenVdbSurfaceShellResult& shell)
{
    return static_cast<std::size_t>(shell.width)
        * static_cast<std::size_t>(shell.height)
        * static_cast<std::size_t>(shell.depth);
}

std::size_t PixelIndex2D(const int width, const int x, const int y)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
        + static_cast<std::size_t>(x);
}

bool HasMaskValue(const std::vector<std::uint8_t>& mask, const std::size_t index)
{
    return !mask.empty() && mask.at(index) != 0;
}

bool IsOptionalMaskValid(const std::vector<std::uint8_t>& mask, const std::size_t voxelCount)
{
    return mask.empty() || mask.size() == voxelCount;
}

bool ValidateInput(
    const OpenVdbSurfaceShellResult& shell,
    const SurfaceTextureTransferResult& transfer,
    const OpenVdbCandidateLayerBufferOptions& options,
    std::string& error)
{
    if (shell.width <= 0 || shell.height <= 0 || shell.depth <= 0)
    {
        error = "OpenVDB candidate layer buffer dimensions must be positive";
        return false;
    }

    const std::size_t voxelCount = VoxelCount(shell);
    if (shell.shell_mask.size() != voxelCount
        || shell.interior_mask.size() != voxelCount)
    {
        error = "OpenVDB candidate shell and interior masks must match width * height * depth";
        return false;
    }
    if (!transfer.shell_rgb.empty() && transfer.shell_rgb.size() != voxelCount)
    {
        error = "OpenVDB candidate shell RGB buffer must be empty or width * height * depth";
        return false;
    }
    if (!IsOptionalMaskValid(options.support_mask, voxelCount)
        || !IsOptionalMaskValid(options.white_mask, voxelCount)
        || !IsOptionalMaskValid(options.varnish_mask, voxelCount))
    {
        error = "OpenVDB candidate optional masks must be empty or width * height * depth";
        return false;
    }
    return true;
}

}  // namespace

OpenVdbCandidateLayerBufferBuildResult BuildOpenVdbCandidateLayerBuffers(
    const OpenVdbSurfaceShellResult& shell,
    const SurfaceTextureTransferResult& transfer,
    const OpenVdbCandidateLayerBufferOptions& options)
{
    OpenVdbCandidateLayerBufferBuildResult result;
    result.width = shell.width;
    result.height = shell.height;
    result.depth = shell.depth;
    if (!ValidateInput(shell, transfer, options, result.error))
    {
        return result;
    }

    const std::size_t pixelCount =
        static_cast<std::size_t>(shell.width) * static_cast<std::size_t>(shell.height);
    result.layers.reserve(static_cast<std::size_t>(shell.depth));

    for (int z{0}; z < shell.depth; ++z)
    {
        OpenVdbCandidateLayerBuffer layer;
        layer.layer_index = z;
        layer.stats.layer_index = z;
        layer.composer_input.width = shell.width;
        layer.composer_input.height = shell.height;
        layer.composer_input.model_rgb = options.interior_rgb;
        layer.composer_input.support_value = options.support_value;
        layer.composer_input.white_value = options.white_value;
        layer.composer_input.varnish_value = options.varnish_value;
        layer.composer_input.model_mask.assign(pixelCount, 0);
        layer.composer_input.surface_shell_mask.assign(pixelCount, 0);
        layer.composer_input.surface_rgb.assign(pixelCount, options.interior_rgb);
        if (!options.support_mask.empty())
        {
            layer.composer_input.support_mask.assign(pixelCount, 0);
        }
        if (!options.white_mask.empty())
        {
            layer.composer_input.white_mask.assign(pixelCount, 0);
        }
        if (!options.varnish_mask.empty())
        {
            layer.composer_input.varnish_mask.assign(pixelCount, 0);
        }

        for (int y{0}; y < shell.height; ++y)
        {
            for (int x{0}; x < shell.width; ++x)
            {
                const std::size_t voxelIndex = MaskIndex3D(shell.width, shell.height, x, y, z);
                const std::size_t pixelIndex = PixelIndex2D(shell.width, x, y);
                const bool shellVoxel = HasMaskValue(shell.shell_mask, voxelIndex);
                const bool interiorVoxel = HasMaskValue(shell.interior_mask, voxelIndex);
                const bool modelVoxel = shellVoxel || interiorVoxel;

                if (modelVoxel)
                {
                    layer.composer_input.model_mask.at(pixelIndex) = 1;
                    ++layer.stats.model_pixels;
                }
                if (interiorVoxel)
                {
                    ++layer.stats.interior_pixels;
                }
                if (shellVoxel)
                {
                    layer.composer_input.surface_shell_mask.at(pixelIndex) = 1;
                    if (!transfer.shell_rgb.empty())
                    {
                        layer.composer_input.surface_rgb.at(pixelIndex) =
                            transfer.shell_rgb.at(voxelIndex);
                    }
                    ++layer.stats.shell_pixels;
                }

                if (!options.support_mask.empty() && HasMaskValue(options.support_mask, voxelIndex))
                {
                    if (options.preserve_model_priority && modelVoxel)
                    {
                        ++layer.stats.cleared_support_conflict_pixels;
                    }
                    else
                    {
                        layer.composer_input.support_mask.at(pixelIndex) = 1;
                        ++layer.stats.support_pixels;
                    }
                }
                if (!options.white_mask.empty() && HasMaskValue(options.white_mask, voxelIndex))
                {
                    layer.composer_input.white_mask.at(pixelIndex) = 1;
                    ++layer.stats.white_pixels;
                }
                if (!options.varnish_mask.empty() && HasMaskValue(options.varnish_mask, voxelIndex))
                {
                    layer.composer_input.varnish_mask.at(pixelIndex) = 1;
                    ++layer.stats.varnish_pixels;
                }
            }
        }
        result.layers.push_back(std::move(layer));
    }
    return result;
}

}  // namespace slicer_core
