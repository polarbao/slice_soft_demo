#pragma once

#include "slicer_core/config.h"

#include <cstdint>
#include <vector>

namespace slicer_core
{

/**
 * @brief Summary of one support base projection application.
 */
struct SupportBaseProjectionResult
{
    bool enabled{false};
    int configured_layer_count{0};
    int effective_layer_count{0};
    int added_layer_count{0};
    double model_lift_mm{0.0};
    std::string layer_placement{"overlay_existing"};
    int footprint_pixels{0};
    int added_support_pixels{0};
};

/**
 * @brief Physical layer reservation required before support projection.
 */
struct SupportBaseProjectionPreparation
{
    int prepended_layer_count{0};
    double model_lift_mm{0.0};
};

/**
 * @brief Resolve whether base projection adds physical layers below the model.
 * @param config Support base projection configuration.
 * @param layerThicknessMm Physical thickness of one output layer in millimeters.
 * @return Layer count and model Z lift required before raster sampling.
 * @throws std::invalid_argument When layerThicknessMm is not finite and positive.
 */
SupportBaseProjectionPreparation ResolveSupportBaseProjectionPreparation(
    const SupportBaseProjectionConfig& config,
    double layerThicknessMm);

/**
 * @brief Fill the first configured layers from the maximum ordinary support footprint.
 * @param config Support base projection configuration.
 * @param modelMasks Per-layer model masks that retain material priority.
 * @param supportMasks Per-layer ordinary support masks to update in place.
 * @return Effective layer range and pixel-count evidence.
 * @throws std::invalid_argument When mask layer counts or dimensions do not match.
 */
SupportBaseProjectionResult ApplySupportBaseProjection(
    const SupportBaseProjectionConfig& config,
    const std::vector<std::vector<std::uint8_t>>& modelMasks,
    std::vector<std::vector<std::uint8_t>>& supportMasks);

/**
 * @brief Apply support base projection to contiguous layer-major mask volumes.
 * @param config Support base projection configuration.
 * @param modelVolume Contiguous model mask volume.
 * @param supportVolume Contiguous support mask volume to update in place.
 * @param layerCount Number of layers in both volumes.
 * @param pixelCount Number of pixels in one layer.
 * @return Effective layer range and pixel-count evidence.
 * @throws std::invalid_argument When volume dimensions do not match.
 */
SupportBaseProjectionResult ApplySupportBaseProjectionVolume(
    const SupportBaseProjectionConfig& config,
    const std::vector<std::uint8_t>& modelVolume,
    std::vector<std::uint8_t>& supportVolume,
    int layerCount,
    std::size_t pixelCount);

}  // namespace slicer_core
