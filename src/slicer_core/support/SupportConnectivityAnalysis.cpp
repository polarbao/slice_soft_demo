#include "slicer_core/support/SupportConnectivityAnalysis.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace slicer_core
{

SupportConnectivityDiagnostics analyze_support_connectivity(
    const std::vector<std::uint8_t>& support_mask,
    const GridSpec& grid,
    const int connectivity) {
    constexpr int tiny_component_area_px{8};
    constexpr int small_component_area_px{512};
    const std::size_t pixel_count = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    SupportConnectivityDiagnostics diagnostics;
    diagnostics.enabled = true;
    std::vector<std::uint8_t> visited(pixel_count, 0);
    const std::array<std::array<int, 2>, 8> neighbors8{{
        {{-1, -1}}, {{0, -1}}, {{1, -1}}, {{-1, 0}}, {{1, 0}}, {{-1, 1}}, {{0, 1}}, {{1, 1}},
    }};
    const std::array<std::array<int, 2>, 4> neighbors4{{
        {{0, -1}}, {{-1, 0}}, {{1, 0}}, {{0, 1}},
    }};

    for (std::size_t start{0}; start < pixel_count; ++start) {
        if (support_mask.at(start) == 0 || visited.at(start) != 0) {
            continue;
        }

        SupportComponentSummary component;
        component.min_x = grid.width_px;
        component.min_y = grid.height_px;
        component.max_x = -1;
        component.max_y = -1;
        std::vector<int> stack{static_cast<int>(start)};
        visited.at(start) = 1;
        while (!stack.empty()) {
            const int current = stack.back();
            stack.pop_back();
            const int x = current % grid.width_px;
            const int y = current / grid.width_px;
            ++component.area_px;
            component.min_x = std::min(component.min_x, x);
            component.min_y = std::min(component.min_y, y);
            component.max_x = std::max(component.max_x, x);
            component.max_y = std::max(component.max_y, y);

            if (connectivity == 8) {
                for (const auto& neighbor : neighbors8) {
                    const int nx{x + neighbor.at(0)};
                    const int ny{y + neighbor.at(1)};
                    if (nx < 0 || nx >= grid.width_px || ny < 0 || ny >= grid.height_px) {
                        continue;
                    }
                    const std::size_t next = mask_index(grid, nx, ny);
                    if (support_mask.at(next) != 0 && visited.at(next) == 0) {
                        visited.at(next) = 1;
                        stack.push_back(static_cast<int>(next));
                    }
                }
            } else {
                for (const auto& neighbor : neighbors4) {
                    const int nx{x + neighbor.at(0)};
                    const int ny{y + neighbor.at(1)};
                    if (nx < 0 || nx >= grid.width_px || ny < 0 || ny >= grid.height_px) {
                        continue;
                    }
                    const std::size_t next = mask_index(grid, nx, ny);
                    if (support_mask.at(next) != 0 && visited.at(next) == 0) {
                        visited.at(next) = 1;
                        stack.push_back(static_cast<int>(next));
                    }
                }
            }
        }

        ++diagnostics.component_count;
        diagnostics.largest_component_pixels =
            std::max(diagnostics.largest_component_pixels, component.area_px);
        if (component.area_px <= tiny_component_area_px) {
            ++diagnostics.tiny_component_count;
        } else if (component.area_px <= small_component_area_px) {
            ++diagnostics.small_component_count;
        }
        diagnostics.components.push_back(component);
    }

    std::sort(diagnostics.components.begin(), diagnostics.components.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.area_px > rhs.area_px;
    });
    return diagnostics;
}

}  // namespace slicer_core
