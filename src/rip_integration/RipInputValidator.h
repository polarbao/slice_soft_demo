#pragma once

#include "rip_integration/RipSettings.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <vector>

namespace slicesoft::rip
{

struct RipInputValidationRequest
{
    std::filesystem::path package_directory;
    std::filesystem::path input_directory;
    std::vector<std::filesystem::path> layer_paths;
    std::uint32_t expected_width_px{0U};
    std::uint32_t expected_height_px{0U};
    std::function<bool()> is_cancelled;
};

/** @brief Pixel grid observed on one real slice TIFF. */
struct RipInputGeometry
{
    std::uint32_t width_px{0U};
    std::uint32_t height_px{0U};
};

/** @brief Rechecks real S1 TIFFs before launching the external process. */
[[nodiscard]] RipStatus ValidateRipInput(
    const RipInputValidationRequest& request);

/**
 * @brief Read the grid of one slice TIFF without a Package manifest.
 *
 * The manual RIP path has no manifest to declare the grid, so the first
 * enumerated layer supplies it and every remaining layer is then checked
 * against that grid by ValidateRipInput. Structural requirements are the
 * same as ValidateRipInput; only the expected grid is unknown up front.
 */
[[nodiscard]] RipStatus ProbeRipInputGeometry(
    const std::filesystem::path& layer_path,
    RipInputGeometry* geometry);

}  // namespace slicesoft::rip
