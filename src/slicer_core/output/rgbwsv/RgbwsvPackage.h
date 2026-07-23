#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Current RGBWSV package protocol constants.
 */
struct RgbwsvProtocol
{
    std::string schema{"p0.rgbwsv.2"};
    std::array<std::string, 6> channel_order{"R", "G", "B", "W", "S", "V"};
    int bit_depth{8};
    std::string polarity{"black_is_print"};
    std::uint8_t print_value{0};
    std::uint8_t empty_value{255};
};

/**
 * @brief One final in-memory production layer accepted by the shared RGBWSV writer.
 */
struct RgbwsvProductionLayer
{
    int layerIndex{0};
    double zMm{0.0};
    int widthPx{0};
    int heightPx{0};
    std::array<std::string, 6> channelOrder{"R", "G", "B", "W", "S", "V"};
    std::vector<std::uint8_t> channels;
};

/**
 * @brief Return the current production RGBWSV protocol constants.
 * @return RGBWSV protocol descriptor.
 */
RgbwsvProtocol CurrentRgbwsvProtocol();

}  // namespace slicer_core
