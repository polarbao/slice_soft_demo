#pragma once

#include "slicer_core/TiffReadApi.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace slicer_core
{

struct RgbwsvtTiffReadResult
{
    TiffImageSpec spec;
    std::vector<std::uint8_t> pixels;
    std::array<std::uint64_t, 7> channelChecksums{};
    std::array<TiffChannelStats, 7> channelStats{};
};

/**
 * @brief Strictly reads one LibTIFF-backed RGBWSVT uint8 contiguous image.
 */
[[nodiscard]] RgbwsvtTiffReadResult ReadRgbwsvtTiff(
    const std::filesystem::path& path);

}  // namespace slicer_core
