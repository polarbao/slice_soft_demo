#include "slicer_core/support/InternalVoidSupport.h"

#include <algorithm>

namespace slicer_core
{

void AddInternalVoidSupportForLayer(
    const SliceConfig& config,
    const GridSpec& grid,
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>& supportMask,
    std::vector<SupportType>& supportTypeMap,
    const std::vector<std::uint32_t>* activeColumns,
    InternalVoidScratch* scratch)
{
    if (!config.support.internal_void.enabled)
    {
        return;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    // 复用暂存：表外的列 externalEmpty 恒为 1、visited 恒为 0，跨层无需重置，
    // 故每层只重置表内的列，避免每层触碰整幅面的新页。
    const bool reuseScratch = scratch != nullptr && activeColumns != nullptr;
    InternalVoidScratch localScratch;
    InternalVoidScratch& work = reuseScratch ? *scratch : localScratch;
    if (!work.initialized || work.externalEmpty.size() != pixelCount)
    {
        work.externalEmpty.assign(pixelCount, reuseScratch ? 1U : 0U);
        work.visited.assign(pixelCount, 0U);
        work.stack.clear();
        work.stack.reserve(pixelCount / 64U + 64U);
        work.initialized = true;
    }
    std::vector<std::uint8_t>& externalEmpty = work.externalEmpty;
    std::vector<int>& stack = work.stack;
    stack.clear();

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

    if (activeColumns != nullptr)
    {
        // 表外的列在任何层都是外部空白，恒标为 1。复用暂存时它们无需重写。
        if (!reuseScratch)
        {
            std::fill(externalEmpty.begin(), externalEmpty.end(), static_cast<std::uint8_t>(1));
        }
        for (const std::uint32_t column : *activeColumns)
        {
            externalEmpty.at(column) = 0;
        }
        // 种子取表内、本层为空、且紧邻表外（或紧邻幅面外）的列。
        for (const std::uint32_t column : *activeColumns)
        {
            const std::size_t index = static_cast<std::size_t>(column);
            if (modelMask.at(index) != 0)
            {
                continue;
            }
            const int x = static_cast<int>(index % static_cast<std::size_t>(grid.width_px));
            const int y = static_cast<int>(index / static_cast<std::size_t>(grid.width_px));
            const auto touchesOutside = [&](const int nx, const int ny) {
                if (nx < 0 || nx >= grid.width_px || ny < 0 || ny >= grid.height_px)
                {
                    return true;
                }
                return externalEmpty.at(mask_index(grid, nx, ny)) != 0;
            };
            if (touchesOutside(x - 1, y) || touchesOutside(x + 1, y)
                || touchesOutside(x, y - 1) || touchesOutside(x, y + 1))
            {
                pushExternalEmpty(x, y);
            }
        }
    }
    else
    {
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

    std::vector<std::uint8_t>& visited = work.visited;
    if (reuseScratch)
    {
        for (const std::uint32_t column : *activeColumns)
        {
            visited.at(column) = 0;
        }
    }
    else
    {
        std::fill(visited.begin(), visited.end(), static_cast<std::uint8_t>(0));
    }
    // 表外的列 externalEmpty 恒为 1，下方判据本就会跳过，故只遍历表内是等价的。
    const std::size_t scanCount =
        activeColumns != nullptr ? activeColumns->size() : pixelCount;
    for (std::size_t scanIndex{0}; scanIndex < scanCount; ++scanIndex)
    {
        const std::size_t start = activeColumns != nullptr
            ? static_cast<std::size_t>(activeColumns->at(scanIndex))
            : scanIndex;
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
