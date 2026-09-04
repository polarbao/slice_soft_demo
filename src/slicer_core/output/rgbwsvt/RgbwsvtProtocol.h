#pragma once

#include "slicer_core/output/rgbwsv/RgbwsvPackage.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace slicer_core
{

inline constexpr std::size_t kRgbwsvtChannelCount{7U};
inline constexpr std::size_t kTransferChannelOffset{6U};

struct RgbwsvtProtocol
{
    std::string schema{"p0.rgbwsvt.1"};
    std::array<std::string, kRgbwsvtChannelCount> channelOrder{
        "R", "G", "B", "W", "S", "V", "T"};
    int bitDepth{8};
    std::string polarity{"black_is_print"};
    std::uint8_t printValue{0U};
    std::uint8_t emptyValue{255U};
};

struct RgbwsvtProductionLayer
{
    int layerIndex{0};
    double zMm{0.0};
    int widthPx{0};
    int heightPx{0};
    std::array<std::string, kRgbwsvtChannelCount> channelOrder{
        "R", "G", "B", "W", "S", "V", "T"};
    std::vector<std::uint8_t> channels;
};

[[nodiscard]] RgbwsvtProtocol CurrentRgbwsvtProtocol();

/**
 * @brief Upgrade one final RGBWSV layer with an exclusive transfer channel.
 *
 * Non-transfer pixels preserve the first six channel values byte-for-byte.
 * Transfer pixels clear RGBWSV and write only T. A transfer pixel outside the
 * model mask is rejected instead of being silently clipped.
 */
[[nodiscard]] RgbwsvtProductionLayer ComposeRgbwsvtLayer(
    const RgbwsvProductionLayer& rgbwsvLayer,
    std::span<const std::uint8_t> modelMask,
    std::span<const std::uint8_t> transferMask,
    std::uint8_t transferValue);

}  // namespace slicer_core
