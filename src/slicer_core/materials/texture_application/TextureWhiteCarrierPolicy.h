#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace slicer_core
{

/**
 * @brief Determine whether a sampled texture color needs an on-demand white carrier.
 * @param policy Stable texture.unprintableWhitePolicy value.
 * @param inkThreshold Maximum per-channel ink distance from pure white.
 * @param rgb Sampled RGB value in the black-is-print production domain.
 * @return True only when the enabled policy classifies the texel as unprintable white.
 */
[[nodiscard]] bool IsUnprintableWhiteTexel(
    std::string_view policy,
    std::uint8_t inkThreshold,
    const std::array<std::uint8_t, 3>& rgb) noexcept;

/**
 * @brief Apply the configured white carrier value when an RGB texel cannot print as RGB.
 * @param policy Stable unprintable-white policy identifier.
 * @param inkThreshold Maximum subtractive-ink distance accepted as unprintable white.
 * @param whiteValue Production W-channel value written for a selected texel.
 * @param rgb Source RGB texel value.
 * @param whiteChannel Mutable W-channel value; no other RGBWSV channel is accessible.
 * @return True when the W channel was written.
 */
[[nodiscard]] bool ApplyUnprintableWhiteCarrier(
    std::string_view policy,
    std::uint8_t inkThreshold,
    std::uint8_t whiteValue,
    const std::array<std::uint8_t, 3>& rgb,
    std::uint8_t& whiteChannel) noexcept;

}  // namespace slicer_core
