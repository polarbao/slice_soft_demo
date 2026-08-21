#include "slicer_core/materials/volume/MaterialVolumeWhiteCarrier.h"

#include "slicer_core/materials/texture_application/TextureWhiteCarrierPolicy.h"

#include <array>
#include <cstddef>
#include <stdexcept>

namespace slicer_core
{

void ApplyMaterialVolumeWhiteCarrierLayer(
    const MaterialVolumeWhiteCarrierRequest& request,
    const std::span<const std::uint8_t> rgbLayer,
    const std::span<const std::uint8_t> modelMask,
    const std::span<std::uint8_t> whiteLayer,
    MaterialVolumeWhiteCarrierStats& stats)
{
    const std::size_t columnCount = modelMask.size();
    if (rgbLayer.size() != columnCount * 3U)
    {
        throw std::invalid_argument("Material volume white carrier RGB layer size is invalid");
    }
    if (whiteLayer.size() != columnCount)
    {
        throw std::invalid_argument("Material volume white carrier white layer size is invalid");
    }
    if (!request.whiteUnderbaseEnabled)
    {
        return;
    }
    for (std::size_t column{0}; column < columnCount; ++column)
    {
        if (modelMask[column] == 0U)
        {
            continue;
        }
        ++stats.evaluatedModelPixels;
        const std::size_t base = column * 3U;
        const std::array<std::uint8_t, 3> rgb{
            rgbLayer[base], rgbLayer[base + 1U], rgbLayer[base + 2U]};
        // 复用 Stage 15 的唯一判据与写入实现，不复制近似逻辑。
        if (ApplyUnprintableWhiteCarrier(
                request.inkThreshold, request.whiteValue, rgb, whiteLayer[column]))
        {
            ++stats.unprintableWhiteCarrierPixels;
        }
    }
}

bool IsMaterialVolumeWhiteCarrierCombinationAllowed(
    const bool materialVolumeEnabled,
    const std::string& unprintableWhitePolicy,
    const bool materialPolicyEnabled,
    const bool materialRoleMappingEnabled) noexcept
{
    if (!materialVolumeEnabled)
    {
        return false;
    }
    if (unprintableWhitePolicy != "white_underbase")
    {
        return false;
    }
    return !materialPolicyEnabled && !materialRoleMappingEnabled;
}

}  // namespace slicer_core
