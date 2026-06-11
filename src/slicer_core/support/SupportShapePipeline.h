#pragma once

#include "slicer_core/support/SupportShapeOptimizer.h"

namespace slicer_core
{

/**
 * @brief Apply support shape policy through the support pipeline facade.
 * @param policy Shape policy.
 * @param modelMasks Per-layer model masks.
 * @param supportMasks Per-layer support masks to modify in place.
 * @param width Mask width in pixels.
 * @param height Mask height in pixels.
 * @param connectivity Connectivity for component analysis.
 * @return Support shape optimization report data.
 */
SupportShapeOptimizationResult ApplySupportShapePolicy(
    const SupportShapePolicy& policy,
    const std::vector<std::vector<std::uint8_t>>& modelMasks,
    std::vector<std::vector<std::uint8_t>>& supportMasks,
    int width,
    int height,
    int connectivity);

/**
 * @brief Optimize one support layer through the support pipeline facade.
 * @param policy Shape policy.
 * @param modelMask Single-layer model mask.
 * @param supportMask Single-layer support mask to modify in place.
 * @param width Mask width in pixels.
 * @param height Mask height in pixels.
 * @param connectivity Connectivity for component analysis.
 * @return Support shape optimization report data for one layer.
 */
SupportShapeOptimizationResult OptimizeSupportShapeForLayer(
    const SupportShapePolicy& policy,
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>& supportMask,
    int width,
    int height,
    int connectivity);

}  // namespace slicer_core
