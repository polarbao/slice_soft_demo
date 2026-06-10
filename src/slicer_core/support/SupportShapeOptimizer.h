#pragma once

#include "slicer_core/support/SupportComponentAnalysis.h"
#include "slicer_core/support/SupportShapePolicy.h"

#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Filtered support component record.
 */
struct FilteredSupportComponent
{
    int layer_index{0};
    int area_px{0};
    int min_x{0};
    int min_y{0};
    int max_x{0};
    int max_y{0};
};

/**
 * @brief Bridged support gap record.
 */
struct BridgedSupportGap
{
    int layer_index{0};
    int x0{0};
    int y0{0};
    int x1{0};
    int y1{0};
    int gap_px{0};
    std::string direction;
};

/**
 * @brief Per-layer support shape optimization report.
 */
struct SupportShapeLayerReport
{
    int layer_index{0};
    SupportComponentAnalysis pre;
    SupportComponentAnalysis post;
    int added_support_pixels{0};
    int removed_support_pixels{0};
    std::vector<FilteredSupportComponent> filtered_components;
    std::vector<BridgedSupportGap> bridged_gaps;
    std::vector<std::string> warnings;
};

/**
 * @brief Support shape optimization result.
 */
struct SupportShapeOptimizationResult
{
    bool enabled{false};
    int added_support_pixels{0};
    int removed_support_pixels{0};
    std::vector<SupportShapeLayerReport> layers;
    std::vector<std::string> warnings;
};

/**
 * @brief Optimize support masks with lightweight shape operations.
 * @param policy Shape policy.
 * @param modelMasks Per-layer model masks.
 * @param supportMasks Per-layer support masks to modify in place.
 * @param width Mask width in pixels.
 * @param height Mask height in pixels.
 * @param connectivity Connectivity for component analysis.
 * @return Support shape optimization report data.
 */
SupportShapeOptimizationResult OptimizeSupportShape(
    const SupportShapePolicy& policy,
    const std::vector<std::vector<std::uint8_t>>& modelMasks,
    std::vector<std::vector<std::uint8_t>>& supportMasks,
    int width,
    int height,
    int connectivity);

}  // namespace slicer_core
