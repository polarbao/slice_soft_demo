#include "slicer_core/support/SupportComponentAnalysis.h"

#include <algorithm>
#include <array>

namespace slicer_core
{
namespace
{

std::size_t MaskIndex(const int width, const int x, const int y)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
}

}  // namespace

SupportComponentAnalysis AnalyzeSupportComponents(
    const std::vector<std::uint8_t>& supportMask,
    const int width,
    const int height,
    const int connectivity)
{
    SupportComponentAnalysis analysis;
    analysis.enabled = true;
    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<std::uint8_t> visited(pixelCount, 0);
    const std::array<std::array<int, 2>, 8> neighbors8{{
        {{-1, -1}},
        {{0, -1}},
        {{1, -1}},
        {{-1, 0}},
        {{1, 0}},
        {{-1, 1}},
        {{0, 1}},
        {{1, 1}},
    }};
    const std::array<std::array<int, 2>, 4> neighbors4{{
        {{0, -1}},
        {{-1, 0}},
        {{1, 0}},
        {{0, 1}},
    }};

    for (std::size_t start{0}; start < pixelCount; ++start)
    {
        if (supportMask.at(start) == 0 || visited.at(start) != 0)
        {
            continue;
        }

        SupportComponentInfo component;
        component.min_x = width;
        component.min_y = height;
        component.max_x = -1;
        component.max_y = -1;
        std::vector<int> stack{static_cast<int>(start)};
        visited.at(start) = 1;
        while (!stack.empty())
        {
            const int current = stack.back();
            stack.pop_back();
            const int x = current % width;
            const int y = current / width;
            component.pixels.push_back(current);
            ++component.area_px;
            component.min_x = std::min(component.min_x, x);
            component.min_y = std::min(component.min_y, y);
            component.max_x = std::max(component.max_x, x);
            component.max_y = std::max(component.max_y, y);

            const auto VisitNeighbor = [&](const int nx, const int ny)
            {
                if (nx < 0 || nx >= width || ny < 0 || ny >= height)
                {
                    return;
                }
                const std::size_t next = MaskIndex(width, nx, ny);
                if (supportMask.at(next) != 0 && visited.at(next) == 0)
                {
                    visited.at(next) = 1;
                    stack.push_back(static_cast<int>(next));
                }
            };

            if (connectivity == 8)
            {
                for (const auto& neighbor : neighbors8)
                {
                    VisitNeighbor(x + neighbor.at(0), y + neighbor.at(1));
                }
            }
            else
            {
                for (const auto& neighbor : neighbors4)
                {
                    VisitNeighbor(x + neighbor.at(0), y + neighbor.at(1));
                }
            }
        }

        ++analysis.component_count;
        analysis.largest_component_area = std::max(analysis.largest_component_area, component.area_px);
        if (component.area_px <= analysis.tiny_component_area_px)
        {
            ++analysis.tiny_component_count;
        }
        else if (component.area_px <= analysis.small_component_area_px)
        {
            ++analysis.small_component_count;
        }
        analysis.components.push_back(std::move(component));
    }

    return analysis;
}

}  // namespace slicer_core
