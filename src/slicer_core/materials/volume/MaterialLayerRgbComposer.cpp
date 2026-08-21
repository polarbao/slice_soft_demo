#include "slicer_core/materials/volume/MaterialLayerRgbComposer.h"

#include "slicer_core/materials/volume/MaterialVolumeError.h"

#include <cstddef>
#include <stdexcept>
#include <string>

namespace slicer_core
{
namespace
{

const MaterialInfo* FindMaterialInfoByName(
    const std::span<const MaterialInfo> materialInfos,
    const std::string& name)
{
    for (const MaterialInfo& info : materialInfos)
    {
        if (info.name == name)
        {
            return &info;
        }
    }
    return nullptr;
}

}  // namespace

MaterialRgbTable BuildMaterialRgbTable(const MaterialRgbTableRequest& request)
{
    if (request.plan == nullptr)
    {
        throw MaterialVolumeError(
            MaterialVolumeErrorCode::TopologyInvalid,
            "material RGB table requires a material volume plan");
    }
    const std::span<const std::string> materialNames = request.plan->MaterialNames();

    MaterialRgbTable table;
    table.unownedRgb_ = request.unownedRgb;
    table.rgbByMaterial_.reserve(materialNames.size());
    table.rgbSources_.reserve(materialNames.size());

    for (const std::string& name : materialNames)
    {
        const MaterialInfo* info = FindMaterialInfoByName(request.materialInfos, name);
        if (info != nullptr && info->has_diffuse)
        {
            table.rgbByMaterial_.push_back(info->diffuse_rgb);
            table.rgbSources_.emplace_back("mtl_kd");
            continue;
        }
        if (request.fallbackPolicy == MaterialRgbFallbackPolicy::ExplicitFallback)
        {
            table.rgbByMaterial_.push_back(request.explicitFallbackRgb);
            table.rgbSources_.emplace_back("explicit_fallback");
            continue;
        }
        throw MaterialVolumeError(
            MaterialVolumeErrorCode::MaterialMissing,
            "material '" + name + "' has no MTL diffuse colour and fallback is not permitted");
    }
    return table;
}

void ComposeMaterialLayerRgb(
    const MaterialRgbTable& table,
    const std::span<const std::uint32_t> ownerLayer,
    const std::span<const std::uint8_t> modelMask,
    const std::span<std::uint8_t> rgbOut)
{
    const std::size_t columnCount = ownerLayer.size();
    if (modelMask.size() != columnCount)
    {
        throw std::invalid_argument("Material RGB composition mask size is invalid");
    }
    if (rgbOut.size() != columnCount * 3U)
    {
        throw std::invalid_argument("Material RGB composition output size is invalid");
    }
    const std::span<const std::array<std::uint8_t, 3>> rgbByMaterial = table.RgbByMaterial();
    const std::array<std::uint8_t, 3> unowned = table.UnownedRgb();

    for (std::size_t column{0}; column < columnCount; ++column)
    {
        const std::uint32_t owner = ownerLayer[column];
        const std::size_t base = column * 3U;
        if (owner == kNoMaterialOwner)
        {
            if (modelMask[column] != 0U)
            {
                throw MaterialVolumeError(
                    MaterialVolumeErrorCode::ModelPixelUnowned,
                    "model pixel at column " + std::to_string(column)
                        + " has no material owner");
            }
            rgbOut[base] = unowned[0];
            rgbOut[base + 1U] = unowned[1];
            rgbOut[base + 2U] = unowned[2];
            continue;
        }
        if (static_cast<std::size_t>(owner) >= rgbByMaterial.size())
        {
            throw MaterialVolumeError(
                MaterialVolumeErrorCode::ReplayMismatch,
                "owner index at column " + std::to_string(column)
                    + " is outside the material RGB table");
        }
        const std::array<std::uint8_t, 3>& rgb = rgbByMaterial[static_cast<std::size_t>(owner)];
        rgbOut[base] = rgb[0];
        rgbOut[base + 1U] = rgb[1];
        rgbOut[base + 2U] = rgb[2];
    }
}

}  // namespace slicer_core
