#include "slicer_core/support/InternalVoidSupport.h"

#include <algorithm>

namespace slicer_core
{

void AddInternalVoidSupportForLayer(
    const SliceConfig& config,
    const GridSpec& grid,
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>& supportMask,
    std::vector<SupportType>& supportTypeMap)
{
    if (!config.support.internal_void.enabled)
    {
        return;
    }

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

    const std::array<std::array<int, 2>, 8> neighbors8{{
        {{-1, -1}}, {{0, -1}}, {{1, -1}}, {{-1, 0}}, {{1, 0}}, {{-1, 1}}, {{0, 1}}, {{1, 1}},
    }};
    const std::array<std::array<int, 2>, 4> neighbors4{{
        {{0, -1}}, {{-1, 0}}, {{1, 0}}, {{0, 1}},
    }};

    while (!stack.empty())
    {
        const int current = stack.back();
        stack.pop_back();
        const int x = current % grid.width_px;
        const int y = current / grid.width_px;
        if (config.support.connectivity == 8)
        {
            for (const auto& neighbor : neighbors8)
            {
                pushExternalEmpty(x + neighbor.at(0), y + neighbor.at(1));
            }
        }
        else
        {
            for (const auto& neighbor : neighbors4)
            {
                pushExternalEmpty(x + neighbor.at(0), y + neighbor.at(1));
            }
        }
    }

    std::vector<std::uint8_t> visited(pixelCount, 0);
    for (std::size_t start{0}; start < pixelCount; ++start)
    {
        if (modelMask.at(start) != 0 || externalEmpty.at(start) != 0 || visited.at(start) != 0)
        {
            continue;
        }

        std::vector<int> component;
        component.reserve(64);
        stack.push_back(static_cast<int>(start));
        visited.at(start) = 1;
        while (!stack.empty())
        {
            const int current = stack.back();
            stack.pop_back();
            component.push_back(current);
            const int x = current % grid.width_px;
            const int y = current / grid.width_px;
            const auto pushComponentNeighbor = [&](const int nx, const int ny)
            {
                if (nx < 0 || nx >= grid.width_px || ny < 0 || ny >= grid.height_px)
                {
                    return;
                }
                const std::size_t next = mask_index(grid, nx, ny);
                if (modelMask.at(next) == 0 && externalEmpty.at(next) == 0 && visited.at(next) == 0)
                {
                    visited.at(next) = 1;
                    stack.push_back(static_cast<int>(next));
                }
            };
            if (config.support.connectivity == 8)
            {
                for (const auto& neighbor : neighbors8)
                {
                    pushComponentNeighbor(x + neighbor.at(0), y + neighbor.at(1));
                }
            }
            else
            {
                for (const auto& neighbor : neighbors4)
                {
                    pushComponentNeighbor(x + neighbor.at(0), y + neighbor.at(1));
                }
            }
        }

        if (static_cast<int>(component.size()) < config.support.internal_void.min_area_px)
        {
            continue;
        }
        for (const int pixel : component)
        {
            set_support_pixel(
                supportMask,
                supportTypeMap,
                static_cast<std::size_t>(pixel),
                SupportType::InternalVoid);
        }
    }
}

}  // namespace slicer_core
