#pragma once

#include "rip_integration/RipSettings.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <vector>

namespace slicesoft::rip
{

/** @brief Expected geometry and device limits for a staged RIP result. */
struct RipOutputValidationRequest
{
    std::filesystem::path package_directory;
    std::filesystem::path staging_directory;
    std::size_t expected_layer_count{0U};
    std::uint32_t expected_width_px{0U};
    std::uint32_t expected_height_px{0U};
    double expected_dpi_x{0.0};
    double expected_dpi_y{0.0};
    int gray_bits{2};
    std::function<bool()> is_cancelled;
};

/** @brief Validated metadata for one normalized CMYKWSV output layer. */
struct RipOutputLayer
{
    std::size_t layer_index{0U};
    std::filesystem::path path;
    std::uint8_t minimum_white{0U};
    std::uint8_t minimum_support{0U};
    std::uint8_t minimum_varnish{0U};
    std::uint8_t maximum_white{0U};
    std::uint8_t maximum_support{0U};
    std::uint8_t maximum_varnish{0U};
};

/** @brief Full result of staged TIFF validation and name normalization. */
struct RipOutputValidationResult
{
    RipStatus status;
    std::vector<RipOutputLayer> layers;
};

/**
 * @brief Validate real staged TIFF bytes, then normalize names in place.
 *
 * Validation completes for every layer before any rename is attempted. A
 * failure never publishes the staging directory.
 */
[[nodiscard]] RipOutputValidationResult ValidateAndNormalizeRipOutput(
    const RipOutputValidationRequest& request);

}  // namespace slicesoft::rip
