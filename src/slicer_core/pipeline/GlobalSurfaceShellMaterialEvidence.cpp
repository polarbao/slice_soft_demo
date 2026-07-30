#include "slicer_core/pipeline/GlobalSurfaceShellMaterialEvidence.h"

#include "slicer_core/materials/varnish_geometry/OuterVarnishDiscretization.h"
#include "slicer_core/support/SupportBaseProjection.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

constexpr std::size_t kChannelCount{6U};
constexpr std::size_t kWhiteChannel{3U};
constexpr std::size_t kSupportChannel{4U};
constexpr std::size_t kVarnishChannel{5U};

std::size_t PixelIndex(
    const int width,
    const int x,
    const int y)
{
    return static_cast<std::size_t>(y * width + x);
}

std::size_t VoxelIndex(
    const int width,
    const int height,
    const int x,
    const int y,
    const int z)
{
    const std::size_t layerSize =
        static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height);
    return static_cast<std::size_t>(z) * layerSize
        + PixelIndex(width, x, y);
}

bool InBounds(
    const TextureFillPartitionRasterGridSpec& grid,
    const int x,
    const int y,
    const int z)
{
    return x >= 0 && x < grid.width
        && y >= 0 && y < grid.height
        && z >= 0 && z < grid.depth;
}

bool ValidateMapping(
    const TextureFillPartitionRasterMappingResult& mapping,
    std::string& detail)
{
    if (!mapping.available
        || mapping.status != "diagnostic"
        || !mapping.stats.partitionPass
        || mapping.grid.width <= 0
        || mapping.grid.height <= 0
        || mapping.grid.depth <= 0
        || mapping.grid.pixelPitchXMm <= 0.0
        || mapping.grid.pixelPitchYMm <= 0.0
        || mapping.grid.layerThicknessMm <= 0.0
        || mapping.layers.size()
            != static_cast<std::size_t>(mapping.grid.depth))
    {
        detail = "Global material evidence requires an available production raster mapping";
        return false;
    }

    const std::size_t pixelCount =
        static_cast<std::size_t>(mapping.grid.width)
        * static_cast<std::size_t>(mapping.grid.height);
    for (const TextureFillPartitionRasterLayer& layer : mapping.layers)
    {
        if (layer.modelMask.size() != pixelCount
            || layer.textureSurfaceMask.size() != pixelCount
            || layer.modelFillMask.size() != pixelCount
            || layer.textureRgb.size() != pixelCount)
        {
            detail = "Global material evidence raster layers are not aligned";
            return false;
        }
    }
    return true;
}

std::vector<std::uint8_t> BuildModelVolume(
    const TextureFillPartitionRasterMappingResult& mapping)
{
    const std::size_t pixelCount =
        static_cast<std::size_t>(mapping.grid.width)
        * static_cast<std::size_t>(mapping.grid.height);
    std::vector<std::uint8_t> model(
        pixelCount * static_cast<std::size_t>(mapping.grid.depth),
        0U);
    for (int z{0}; z < mapping.grid.depth; ++z)
    {
        const auto& layer =
            mapping.layers.at(static_cast<std::size_t>(z));
        std::copy(
            layer.modelMask.begin(),
            layer.modelMask.end(),
            model.begin() + static_cast<std::ptrdiff_t>(
                static_cast<std::size_t>(z) * pixelCount));
    }
    return model;
}

std::vector<std::uint8_t> FindExternalEmptyVoxels(
    const TextureFillPartitionRasterGridSpec& grid,
    const std::vector<std::uint8_t>& model)
{
    const std::size_t voxelCount =
        static_cast<std::size_t>(grid.width)
        * static_cast<std::size_t>(grid.height)
        * static_cast<std::size_t>(grid.depth);
    std::vector<std::uint8_t> external(voxelCount, 0U);
    std::deque<std::array<int, 3>> queue;

    const auto seed = [&](const int x, const int y, const int z)
    {
        const std::size_t index =
            VoxelIndex(grid.width, grid.height, x, y, z);
        if (model.at(index) == 0U && external.at(index) == 0U)
        {
            external.at(index) = 1U;
            queue.push_back({x, y, z});
        }
    };

    for (int z{0}; z < grid.depth; ++z)
    {
        for (int y{0}; y < grid.height; ++y)
        {
            seed(0, y, z);
            seed(grid.width - 1, y, z);
        }
        for (int x{0}; x < grid.width; ++x)
        {
            seed(x, 0, z);
            seed(x, grid.height - 1, z);
        }
    }
    for (int y{0}; y < grid.height; ++y)
    {
        for (int x{0}; x < grid.width; ++x)
        {
            seed(x, y, 0);
            seed(x, y, grid.depth - 1);
        }
    }

    constexpr std::array<std::array<int, 3>, 6> kNeighbors{{
        {{-1, 0, 0}},
        {{1, 0, 0}},
        {{0, -1, 0}},
        {{0, 1, 0}},
        {{0, 0, -1}},
        {{0, 0, 1}},
    }};
    while (!queue.empty())
    {
        const std::array<int, 3> current = queue.front();
        queue.pop_front();
        for (const auto& delta : kNeighbors)
        {
            const int x = current.at(0U) + delta.at(0U);
            const int y = current.at(1U) + delta.at(1U);
            const int z = current.at(2U) + delta.at(2U);
            if (!InBounds(grid, x, y, z))
            {
                continue;
            }
            const std::size_t index =
                VoxelIndex(grid.width, grid.height, x, y, z);
            if (model.at(index) == 0U && external.at(index) == 0U)
            {
                external.at(index) = 1U;
                queue.push_back({x, y, z});
            }
        }
    }
    return external;
}

std::vector<std::uint8_t> BuildSurfaceVarnishVolume(
    const TextureFillPartitionRasterMappingResult& mapping,
    const SliceConfig& config,
    const std::vector<std::uint8_t>& model)
{
    std::vector<std::uint8_t> surface(model.size(), 0U);
    if (!config.surface_varnish.enabled)
    {
        return surface;
    }

    const auto& grid = mapping.grid;
    const std::vector<std::uint8_t> external =
        FindExternalEmptyVoxels(grid, model);
    constexpr std::array<std::array<int, 3>, 6> kNeighbors{{
        {{-1, 0, 0}},
        {{1, 0, 0}},
        {{0, -1, 0}},
        {{0, 1, 0}},
        {{0, 0, -1}},
        {{0, 0, 1}},
    }};

    for (int z{0}; z < grid.depth; ++z)
    {
        for (int y{0}; y < grid.height; ++y)
        {
            for (int x{0}; x < grid.width; ++x)
            {
                const std::size_t index =
                    VoxelIndex(grid.width, grid.height, x, y, z);
                if (model.at(index) == 0U)
                {
                    continue;
                }

                bool touchesOuter{false};
                bool touchesInner{false};
                for (const auto& delta : kNeighbors)
                {
                    const int nx = x + delta.at(0U);
                    const int ny = y + delta.at(1U);
                    const int nz = z + delta.at(2U);
                    if (!InBounds(grid, nx, ny, nz))
                    {
                        touchesOuter = true;
                        continue;
                    }
                    const std::size_t neighborIndex =
                        VoxelIndex(grid.width, grid.height, nx, ny, nz);
                    if (model.at(neighborIndex) != 0U)
                    {
                        continue;
                    }
                    if (external.at(neighborIndex) != 0U)
                    {
                        touchesOuter = true;
                    }
                    else
                    {
                        touchesInner = true;
                    }
                }
                if ((config.surface_varnish.outer_surface && touchesOuter)
                    || (config.surface_varnish.inner_surface && touchesInner))
                {
                    surface.at(index) = 1U;
                }
            }
        }
    }

    for (int depth{1};
         depth < config.surface_varnish.thickness_px;
         ++depth)
    {
        std::vector<std::uint8_t> expanded = surface;
        for (int z{0}; z < grid.depth; ++z)
        {
            for (int y{0}; y < grid.height; ++y)
            {
                for (int x{0}; x < grid.width; ++x)
                {
                    const std::size_t index =
                        VoxelIndex(grid.width, grid.height, x, y, z);
                    if (model.at(index) == 0U || surface.at(index) != 0U)
                    {
                        continue;
                    }
                    for (const auto& delta : kNeighbors)
                    {
                        const int nx = x + delta.at(0U);
                        const int ny = y + delta.at(1U);
                        const int nz = z + delta.at(2U);
                        if (InBounds(grid, nx, ny, nz)
                            && surface.at(VoxelIndex(
                                   grid.width,
                                   grid.height,
                                   nx,
                                   ny,
                                   nz)) != 0U)
                        {
                            expanded.at(index) = 1U;
                            break;
                        }
                    }
                }
            }
        }
        surface = std::move(expanded);
    }
    return surface;
}

std::vector<std::uint8_t> BuildOuterVarnishVolume(
    const TextureFillPartitionRasterMappingResult& mapping,
    const SliceConfig& config,
    const std::vector<std::uint8_t>& model)
{
    std::vector<std::uint8_t> outer(model.size(), 0U);
    if (!config.outer_varnish.enabled
        || config.outer_varnish.thickness_mm <= 0.0)
    {
        return outer;
    }

    const auto& grid = mapping.grid;
    const OuterVarnishDiscretization discretization =
        ComputeOuterVarnishDiscretization(
            config.outer_varnish,
            grid.pixelPitchXMm,
            grid.pixelPitchYMm);
    for (int z{0}; z < grid.depth; ++z)
    {
        for (int y{0}; y < grid.height; ++y)
        {
            for (int x{0}; x < grid.width; ++x)
            {
                const std::size_t source =
                    VoxelIndex(grid.width, grid.height, x, y, z);
                if (model.at(source) == 0U)
                {
                    continue;
                }
                for (int dy{-discretization.radius_y_px};
                     dy <= discretization.radius_y_px;
                     ++dy)
                {
                    for (int dx{-discretization.radius_x_px};
                         dx <= discretization.radius_x_px;
                         ++dx)
                    {
                        if (!IsOuterVarnishOffsetWithinThickness(
                                discretization,
                                dx,
                                dy))
                        {
                            continue;
                        }
                        const int nx = x + dx;
                        const int ny = y + dy;
                        if (!InBounds(grid, nx, ny, z))
                        {
                            continue;
                        }
                        const std::size_t destination =
                            VoxelIndex(grid.width, grid.height, nx, ny, z);
                        if (model.at(destination) == 0U)
                        {
                            outer.at(destination) = 1U;
                        }
                    }
                }
            }
        }
    }
    return outer;
}

void BuildSupportVolumes(
    const TextureFillPartitionRasterMappingResult& mapping,
    const SliceConfig& config,
    const std::vector<std::uint8_t>& model,
    const std::vector<std::uint8_t>& outerVarnish,
    std::vector<std::uint8_t>& support,
    std::vector<std::uint8_t>& internalVoid)
{
    support.assign(model.size(), 0U);
    internalVoid.assign(model.size(), 0U);
    if (!config.support.enabled)
    {
        return;
    }

    const auto& grid = mapping.grid;
    for (int y{0}; y < grid.height; ++y)
    {
        for (int x{0}; x < grid.width; ++x)
        {
            int lowestModel{std::numeric_limits<int>::max()};
            int highestModel{-1};
            int highestRequired{-1};
            for (int z{0}; z < grid.depth; ++z)
            {
                const std::size_t index =
                    VoxelIndex(grid.width, grid.height, x, y, z);
                if (model.at(index) != 0U)
                {
                    lowestModel = std::min(lowestModel, z);
                    highestModel = std::max(highestModel, z);
                }
                if (model.at(index) != 0U
                    || outerVarnish.at(index) != 0U)
                {
                    highestRequired = z;
                }
            }
            for (int z{0}; z < highestRequired; ++z)
            {
                const std::size_t index =
                    VoxelIndex(grid.width, grid.height, x, y, z);
                if (model.at(index) != 0U
                    || outerVarnish.at(index) != 0U)
                {
                    continue;
                }
                support.at(index) = 1U;
                if (config.support.internal_void.enabled
                    && z > lowestModel
                    && z < highestModel)
                {
                    internalVoid.at(index) = 1U;
                }
            }
        }
    }

    const std::size_t pixelCount =
        static_cast<std::size_t>(grid.width)
        * static_cast<std::size_t>(grid.height);
    (void)ApplySupportBaseProjectionVolume(
        config.support.base_projection,
        model,
        support,
        grid.depth,
        pixelCount);
    for (std::size_t index{0U}; index < support.size(); ++index)
    {
        if (outerVarnish.at(index) != 0U)
        {
            support.at(index) = 0U;
            internalVoid.at(index) = 0U;
        }
    }
}

void WriteModelChannels(
    const TextureFillPartitionRasterLayer& mappingLayer,
    const std::uint8_t modelFillValue,
    std::vector<std::uint8_t>& channels)
{
    for (std::size_t pixelIndex{0U};
         pixelIndex < mappingLayer.modelMask.size();
         ++pixelIndex)
    {
        const std::size_t channelOffset = pixelIndex * kChannelCount;
        if (mappingLayer.textureSurfaceMask.at(pixelIndex) != 0U)
        {
            const std::array<std::uint8_t, 3>& rgb =
                mappingLayer.textureRgb.at(pixelIndex);
            channels.at(channelOffset) = rgb.at(0U);
            channels.at(channelOffset + 1U) = rgb.at(1U);
            channels.at(channelOffset + 2U) = rgb.at(2U);
            if (rgb == std::array<std::uint8_t, 3>{255U, 255U, 255U})
            {
                channels.at(channelOffset + kWhiteChannel) =
                    modelFillValue;
            }
        }
        else if (mappingLayer.modelFillMask.at(pixelIndex) != 0U)
        {
            channels.at(channelOffset + kWhiteChannel) = modelFillValue;
        }
    }
}

}  // namespace

GlobalSurfaceShellMaterialEvidenceResult
ComposeGlobalSurfaceShellMaterialEvidence(
    const TextureFillPartitionRasterMappingResult& mapping,
    const SliceConfig& config)
{
    GlobalSurfaceShellMaterialEvidenceResult result;
    if (!ValidateMapping(mapping, result.detail))
    {
        return result;
    }

    const std::vector<std::uint8_t> model = BuildModelVolume(mapping);
    const std::vector<std::uint8_t> surfaceVarnish =
        BuildSurfaceVarnishVolume(mapping, config, model);
    const std::vector<std::uint8_t> outerVarnish =
        BuildOuterVarnishVolume(mapping, config, model);
    std::vector<std::uint8_t> support;
    std::vector<std::uint8_t> internalVoid;
    BuildSupportVolumes(
        mapping,
        config,
        model,
        outerVarnish,
        support,
        internalVoid);

    const std::size_t pixelCount =
        static_cast<std::size_t>(mapping.grid.width)
        * static_cast<std::size_t>(mapping.grid.height);
    result.layers.reserve(mapping.layers.size());
    for (int layerIndex{0}; layerIndex < mapping.grid.depth; ++layerIndex)
    {
        const std::size_t layerOffset =
            static_cast<std::size_t>(layerIndex) * pixelCount;
        const auto& mappingLayer =
            mapping.layers.at(static_cast<std::size_t>(layerIndex));
        TextureFillPartitionFullClosureLayerEvidence evidence;
        evidence.layerIndex = layerIndex;
        evidence.zMm = mappingLayer.zMm;
        evidence.widthPx = mapping.grid.width;
        evidence.heightPx = mapping.grid.height;
        evidence.supportFillMask.assign(
            support.begin() + static_cast<std::ptrdiff_t>(layerOffset),
            support.begin() + static_cast<std::ptrdiff_t>(
                layerOffset + pixelCount));
        evidence.internalVoidSupportMask.assign(
            internalVoid.begin() + static_cast<std::ptrdiff_t>(layerOffset),
            internalVoid.begin() + static_cast<std::ptrdiff_t>(
                layerOffset + pixelCount));
        evidence.surfaceVarnishMask.assign(
            surfaceVarnish.begin() + static_cast<std::ptrdiff_t>(layerOffset),
            surfaceVarnish.begin() + static_cast<std::ptrdiff_t>(
                layerOffset + pixelCount));
        evidence.outerVarnishShellMask.assign(
            outerVarnish.begin() + static_cast<std::ptrdiff_t>(layerOffset),
            outerVarnish.begin() + static_cast<std::ptrdiff_t>(
                layerOffset + pixelCount));
        evidence.modelEnvelopeMask = mappingLayer.modelMask;
        evidence.supportRequiredMask = evidence.supportFillMask;
        evidence.channels.assign(pixelCount * kChannelCount, 255U);
        WriteModelChannels(
            mappingLayer,
            config.model_fill.value,
            evidence.channels);

        for (std::size_t pixelIndex{0U};
             pixelIndex < pixelCount;
             ++pixelIndex)
        {
            const std::size_t channelOffset = pixelIndex * kChannelCount;
            if (evidence.internalVoidSupportMask.at(pixelIndex) != 0U)
            {
                evidence.modelEnvelopeMask.at(pixelIndex) = 1U;
            }
            if (evidence.supportFillMask.at(pixelIndex) != 0U)
            {
                evidence.channels.at(channelOffset + kSupportChannel) =
                    config.support.value;
                ++result.supportPixels;
            }
            if (evidence.internalVoidSupportMask.at(pixelIndex) != 0U)
            {
                ++result.internalVoidSupportPixels;
            }
            if (evidence.surfaceVarnishMask.at(pixelIndex) != 0U)
            {
                evidence.channels.at(channelOffset + kVarnishChannel) =
                    config.surface_varnish.value;
                ++result.surfaceVarnishPixels;
            }
            if (evidence.outerVarnishShellMask.at(pixelIndex) != 0U)
            {
                evidence.channels.at(channelOffset + kVarnishChannel) =
                    config.outer_varnish.value;
                ++result.outerVarnishPixels;
            }
        }
        result.layers.push_back(std::move(evidence));
    }

    result.available = true;
    result.status = "ready_for_full_closure";
    return result;
}

}  // namespace slicer_core
