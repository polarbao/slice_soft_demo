#include "slicer_core/materials/texture_application/TextureWhiteCarrierPolicy.h"

#include <algorithm>

namespace slicer_core
{

bool IsUnprintableWhiteTexel(
    const std::string_view policy,
    const std::uint8_t inkThreshold,
    const std::array<std::uint8_t, 3>& rgb) noexcept
{
    if (policy != "white_underbase")
    {
        return false;
    }

    const std::uint8_t minimumChannel = std::min({rgb.at(0), rgb.at(1), rgb.at(2)});
    const int inkDistance = 255 - static_cast<int>(minimumChannel);
    return inkDistance <= static_cast<int>(inkThreshold);
}

bool ApplyUnprintableWhiteCarrier(
    const std::string_view policy,
    const std::uint8_t inkThreshold,
    const std::uint8_t whiteValue,
    const std::array<std::uint8_t, 3>& rgb,
    std::uint8_t& whiteChannel) noexcept
{
    if (!IsUnprintableWhiteTexel(policy, inkThreshold, rgb))
    {
        return false;
    }
    whiteChannel = whiteValue;
    return true;
}

}  // namespace slicer_core
