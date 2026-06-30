#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief RGBWSV channel offsets for the experimental in-memory composer.
 */
enum class MaterialChannelOffset : std::size_t
{
    R = 0,
    G = 1,
    B = 2,
    W = 3,
    S = 4,
    V = 5,
};

/**
 * @brief In-memory composition input for one layer.
 */
struct MaterialChannelComposerInput
{
    int width{0};
    int height{0};
    std::vector<std::uint8_t> support_mask;
    std::vector<std::uint8_t> model_mask;
    std::vector<std::uint8_t> surface_shell_mask;
    std::vector<std::uint8_t> white_mask;
    std::vector<std::uint8_t> varnish_mask;
    std::vector<std::array<std::uint8_t, 3>> surface_rgb;
    std::array<std::uint8_t, 3> model_rgb{0, 0, 0};
    std::uint8_t support_value{0};
    std::uint8_t white_value{0};
    std::uint8_t varnish_value{0};
};

/**
 * @brief Composition counters for one layer.
 */
struct MaterialChannelComposerStats
{
    int empty_pixels{0};
    int support_pixels{0};
    int model_pixels{0};
    int surface_rgb_pixels{0};
    int white_pixels{0};
    int varnish_pixels{0};
    int model_support_conflict_pixels{0};
};

/**
 * @brief In-memory RGBWSV composition result.
 */
struct MaterialChannelComposerResult
{
    int width{0};
    int height{0};
    std::array<std::string, 6> channel_order{"R", "G", "B", "W", "S", "V"};
    std::vector<std::uint8_t> channels;
    MaterialChannelComposerStats stats;
    std::string priority_resolver{"Empty < Support < ModelBase/Interior < SurfaceShellRGB < WhiteInk < Varnish"};
    std::string error;
};

/**
 * @brief Return the fixed RGBWSV channel count used by the composer.
 * @return Channel count.
 */
int MaterialChannelCount();

/**
 * @brief Compose one in-memory RGBWSV layer.
 * @param input Masks and color buffers for one layer.
 * @return Composed RGBWSV buffer; no TIFF or manifest is written.
 */
MaterialChannelComposerResult ComposeMaterialChannels(const MaterialChannelComposerInput& input);

}  // namespace slicer_core
