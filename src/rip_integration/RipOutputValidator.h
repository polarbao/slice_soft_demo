#pragma once

#include "rip_integration/RipSettings.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace slicesoft::rip
{

/** @brief Selects whether S2 drop limits gate publication or are recorded. */
enum class RipOutputValidationMode
{
    StrictS2,
    DiagnosticUnvalidated
};

/** @brief Expected geometry and device limits for a staged RIP result. */
struct RipOutputValidationRequest
{
    std::filesystem::path package_directory;
    std::filesystem::path staging_directory;
    std::size_t expected_layer_count{0U};
    std::uint32_t expected_width_px{0U};
    std::uint32_t expected_height_px{0U};
    int gray_bits{2};
    RipOutputValidationMode validation_mode{RipOutputValidationMode::StrictS2};
    std::function<bool()> is_cancelled;
};

/** @brief First observed sample outside the selected gray-bit S2 limits. */
struct RipOutputDropViolation
{
    std::size_t layer_index{0U};
    std::string channel;
    std::uint8_t value{0U};
    std::uint8_t limit{0U};
    std::uint32_t x{0U};
    std::uint32_t y{0U};
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
    std::array<std::uint64_t, 3> samples_exceeding_drop_limit{};
};

/** @brief Full result of staged TIFF validation and name normalization. */
struct RipOutputValidationResult
{
    RipStatus status;
    std::vector<RipOutputLayer> layers;
    bool s2_drop_limits_passed{true};
    std::array<std::uint64_t, 3> samples_exceeding_drop_limit{};
    std::optional<RipOutputDropViolation> first_drop_violation;
};

/**
 * @brief Validate real staged TIFF bytes, then normalize names in place.
 *
 * Validation completes for every layer before any rename is attempted. In
 * DiagnosticUnvalidated mode, structural checks remain strict while W/S/V
 * limit violations are recorded instead of failing. Publication policy is
 * owned by the caller; this function never publishes a directory.
 */
[[nodiscard]] RipOutputValidationResult ValidateAndNormalizeRipOutput(
    const RipOutputValidationRequest& request);

}  // namespace slicesoft::rip
