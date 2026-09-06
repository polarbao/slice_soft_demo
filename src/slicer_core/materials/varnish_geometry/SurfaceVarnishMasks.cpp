#include "slicer_core/materials/varnish_geometry/SurfaceVarnishMasks.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace slicer_core
{

std::vector<std::uint8_t> BuildExternalEmptyMask(
    const GridSpec& grid,
    const std::vector<std::uint8_t>& modelMask)
{
    const std::size_t pixelCount = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    std::vector<std::uint8_t> externalEmpty(pixelCount, 0);
    std::vector<int> stack;
    stack.reserve(pixelCount);

    const auto pushExternalEmpty = [&](const int x, const int y)
    {
        if (x < 0 || x >= grid.width_px || y < 0 || y >= grid.height_px)
        {
            return;
        }
        const std::size_t index = mask_index(grid, x, y);
        if (modelMask.at(index) != 0 || externalEmpty.at(index) != 0)
        {
            return;
        }
        externalEmpty.at(index) = 1;
        stack.push_back(static_cast<int>(index));
    };

    for (int x{0}; x < grid.width_px; ++x)
    {
        pushExternalEmpty(x, 0);
        pushExternalEmpty(x, grid.height_px - 1);
    }
    for (int y{0}; y < grid.height_px; ++y)
    {
        pushExternalEmpty(0, y);
        pushExternalEmpty(grid.width_px - 1, y);
    }

    constexpr std::array<std::array<int, 2>, 8> neighbors8{{
        {{-1, -1}}, {{0, -1}}, {{1, -1}}, {{-1, 0}}, {{1, 0}}, {{-1, 1}}, {{0, 1}}, {{1, 1}},
    }};

    while (!stack.empty())
    {
        const int current = stack.back();
        stack.pop_back();
        const int x = current % grid.width_px;
        const int y = current / grid.width_px;
        for (const auto& neighbor : neighbors8)
        {
            pushExternalEmpty(x + neighbor.at(0), y + neighbor.at(1));
        }
    }

    return externalEmpty;
}

[[nodiscard]] bool SurfaceVarnishMasksRequired(const SliceConfig& config)
{
    return config.surface_varnish.enabled
        && (config.surface_varnish.outer_surface
            || config.surface_varnish.inner_surface);
}

void MaterializeSurfaceVarnishLayer(
    const SliceConfig& config,
    const GridSpec& grid,
    const std::vector<std::uint8_t>& modelMask,
    const int layerIndex,
    std::vector<std::uint8_t>& outerSurfaceMask,
    std::vector<std::uint8_t>& innerSurfaceMask)
{
    const std::size_t pixelCount =
        static_cast<std::size_t>(grid.width_px) * grid.height_px;
    if (outerSurfaceMask.size() != pixelCount
        || innerSurfaceMask.size() != pixelCount
        || modelMask.size() != pixelCount)
    {
        throw std::runtime_error(
            "MaterializeSurfaceVarnishLayer received a buffer whose size does not"
            " match the grid");
    }
    std::fill(outerSurfaceMask.begin(), outerSurfaceMask.end(), 0U);
    std::fill(innerSurfaceMask.begin(), innerSurfaceMask.end(), 0U);
    if (!SurfaceVarnishMasksRequired(config))
    {
        return;
    }
    (void)layerIndex;
    const bool hasModel = std::any_of(
        modelMask.begin(), modelMask.end(), [](const std::uint8_t value) {
            return value != 0;
        });
    if (!hasModel)
    {
        return;
    }
    const int radiusPx = std::max(1, config.surface_varnish.thickness_px);
    const std::vector<std::uint8_t> externalEmpty =
        BuildExternalEmptyMask(grid, modelMask);
    for (int y{0}; y < grid.height_px; ++y)
    {
        for (int x{0}; x < grid.width_px; ++x)
        {
            const std::size_t index = mask_index(grid, x, y);
            if (modelMask.at(index) == 0)
            {
                continue;
            }
            bool touchesExternal{false};
            bool touchesInternal{false};
            for (int dy{-radiusPx}; dy <= radiusPx; ++dy)
            {
                for (int dx{-radiusPx}; dx <= radiusPx; ++dx)
                {
                    if (dx == 0 && dy == 0)
                    {
                        continue;
                    }
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (nx < 0 || nx >= grid.width_px || ny < 0
                        || ny >= grid.height_px)
                    {
                        touchesExternal = true;
                        continue;
                    }
                    const std::size_t neighborIndex = mask_index(grid, nx, ny);
                    if (modelMask.at(neighborIndex) != 0)
                    {
                        continue;
                    }
                    if (externalEmpty.at(neighborIndex) != 0)
                    {
                        touchesExternal = true;
                        continue;
                    }
                    touchesInternal = true;
                }
            }
            if (touchesExternal && config.surface_varnish.outer_surface)
            {
                outerSurfaceMask.at(index) = 1;
            }
            if (touchesInternal && config.surface_varnish.inner_surface)
            {
                innerSurfaceMask.at(index) = 1;
            }
        }
    }
}

}  // namespace slicer_core
