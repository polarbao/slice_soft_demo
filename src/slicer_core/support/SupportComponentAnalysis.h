#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace slicer_core
{

/**
 * @brief One connected support component.
 */
struct SupportComponentInfo
{
    int area_px{0};
    int min_x{0};
    int min_y{0};
    int max_x{0};
    int max_y{0};
    std::vector<int> pixels;
};

/**
 * @brief Connected component analysis summary for one support layer.
 */
struct SupportComponentAnalysis
{
    bool enabled{false};
    int component_count{0};
    int largest_component_area{0};
    int small_component_count{0};
    int tiny_component_count{0};
    int tiny_component_area_px{8};
    int small_component_area_px{512};
    std::vector<SupportComponentInfo> components;
};

/**
 * @brief Analyze support connected components.
 * @param supportMask Support mask where non-zero means support.
 * @param width Mask width in pixels.
 * @param height Mask height in pixels.
 * @param connectivity Pixel connectivity, either 4 or 8.
 * @return Connected component analysis.
 */
SupportComponentAnalysis AnalyzeSupportComponents(
    const std::vector<std::uint8_t>& supportMask,
    int width,
    int height,
    int connectivity);

}  // namespace slicer_core
