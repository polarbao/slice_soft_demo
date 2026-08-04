#include "slicer_core/materials/texture_application/TextureWhiteCarrierPolicy.h"

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

    return IsUnprintableWhiteTexel(inkThreshold, rgb);
}

bool ApplyUnprintableWhiteCarrier(
    const std::string_view policy,
    const std::uint8_t inkThreshold,
    const std::uint8_t whiteValue,
    const std::array<std::uint8_t, 3>& rgb,
    std::uint8_t& whiteChannel) noexcept
{
    if (policy != "white_underbase")
    {
        return false;
    }
    return ApplyUnprintableWhiteCarrier(
        inkThreshold,
        whiteValue,
        rgb,
        whiteChannel);
}

}  // namespace slicer_core
