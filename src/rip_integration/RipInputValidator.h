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
    double expected_dpi_x{0.0};
    double expected_dpi_y{0.0};
    std::function<bool()> is_cancelled;
};

/** @brief Rechecks real S1 TIFFs before launching the external process. */
[[nodiscard]] RipStatus ValidateRipInput(
    const RipInputValidationRequest& request);

}  // namespace slicesoft::rip
