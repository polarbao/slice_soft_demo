#include "slicer_core/material/RetainedMaterialLayerComposer.h"

#include "slicer_core/materials/texture_application/TextureWhiteCarrierPolicy.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace slicer_core
{
namespace
{

[[nodiscard]] std::size_t CheckedPixelCount(
    const int widthPx,
    const int heightPx)
{
    if (widthPx <= 0 || heightPx <= 0)
    {
        throw std::invalid_argument(
            "retained material layer dimensions must be positive");
    }
    const auto width{static_cast<std::size_t>(widthPx)};
    const auto height{static_cast<std::size_t>(heightPx)};
    if (width > std::numeric_limits<std::size_t>::max() / height)
    {
        throw std::overflow_error("retained material layer pixel count overflow");
    }
    return width * height;
}

void ValidateMask(
    const std::span<const std::uint8_t> mask,
    const std::size_t pixelCount,
    const char* const name)
{
    if (mask.size() != pixelCount)
    {
        throw std::invalid_argument(
            std::string{"retained material layer "} + name
            + " size does not match");
    }
    if (std::any_of(mask.begin(), mask.end(), [](const std::uint8_t value)
        {
            return value > 1U;
        }))
    {
        throw std::invalid_argument(
            std::string{"retained material layer "} + name
            + " must be binary");
    }
}

void ValidateSemanticMasks(
    const MaterialClosureSemanticLayerInput& semantic,
    const std::size_t pixelCount)
{
    const auto validate = [pixelCount](
                              const std::vector<std::uint8_t>& mask,
                              const char* const name)
    {
        if (mask.size() != pixelCount)
        {
            throw std::invalid_argument(
                std::string{"retained material semantic "} + name
                + " size does not match");
        }
    };
    validate(semantic.textureSurfaceMask, "textureSurfaceMask");
    validate(semantic.modelFillMask, "modelFillMask");
    validate(semantic.modelMaterialMask, "modelMaterialMask");
    validate(semantic.supportFillMask, "supportFillMask");
    validate(semantic.internalVoidSupportMask, "internalVoidSupportMask");
    validate(semantic.surfaceVarnishMask, "surfaceVarnishMask");
    validate(semantic.outerVarnishShellMask, "outerVarnishShellMask");
    validate(semantic.modelEnvelopeMask, "modelEnvelopeMask");
    validate(semantic.supportRequiredMask, "supportRequiredMask");
    validate(semantic.expectedOccupiedDomainMask, "expectedOccupiedDomainMask");
    validate(semantic.layerEmptyMask, "layerEmptyMask");
}

[[nodiscard]] std::array<std::uint8_t, 3> NonSurfaceRgb(
    const BoundedMaterialReplayPolicy& policy) noexcept
{
    if (policy.textureNonSurfaceRgbPolicy == "empty")
    {
        return {
            policy.backgroundValue,
            policy.backgroundValue,
            policy.backgroundValue};
    }
    if (policy.textureNonSurfaceRgbPolicy == "fallback_rgb")
    {
        return policy.textureFallbackRgb;
    }
    return policy.materialRgb;
}

void WriteLegacyModelPixel(
    const BoundedMaterialReplayPolicy& policy,
    const std::size_t base,
    const std::span<std::uint8_t> pixels)
{
    if (policy.materialChannel == "V")
    {
        pixels[base + 5U] = policy.materialVarnishValue;
        return;
    }
    if (policy.materialChannel == "W")
    {
        pixels[base + 3U] = policy.materialWhiteValue;
        return;
    }
    if (policy.materialChannel == "RGB")
    {
        pixels[base + 0U] = policy.materialRgb[0U];
        pixels[base + 1U] = policy.materialRgb[1U];
        pixels[base + 2U] = policy.materialRgb[2U];
        return;
    }
    pixels[base + 0U] = policy.materialRgb[0U];
    pixels[base + 1U] = policy.materialRgb[1U];
    pixels[base + 2U] = policy.materialRgb[2U];
    pixels[base + 3U] = policy.materialWhiteValue;
    pixels[base + 4U] = policy.backgroundValue;
    pixels[base + 5U] = policy.materialVarnishValue;
}

void WriteNonSurfaceTexturePixel(
    const BoundedMaterialReplayPolicy& policy,
    const std::size_t base,
    const std::span<std::uint8_t> pixels)
{
    if (policy.textureNonSurfaceRgbPolicy == "model_material"
        || policy.textureNonSurfaceRgbPolicy == "material_policy")
    {
        WriteLegacyModelPixel(policy, base, pixels);
        return;
    }
    const auto rgb{NonSurfaceRgb(policy)};
    pixels[base + 0U] = rgb[0U];
    pixels[base + 1U] = rgb[1U];
    pixels[base + 2U] = rgb[2U];
    if (policy.materialChannel == "W")
    {
        pixels[base + 3U] = policy.materialWhiteValue;
    }
    else if (policy.materialChannel == "V")
    {
        pixels[base + 5U] = policy.materialVarnishValue;
    }
    else if (policy.materialChannel == "auto")
    {
        pixels[base + 3U] = policy.materialWhiteValue;
        pixels[base + 5U] = policy.materialVarnishValue;
    }
}

[[nodiscard]] bool ModelFillUsesExplicitPolicy(
    const BoundedMaterialReplayPolicy& policy) noexcept
{
    return policy.modelFillEnabled && !policy.modelFillLegacyRgbFallback;
}

[[nodiscard]] BoundedModelFillMaterial ResolveModelFillMaterial(
    const BoundedMaterialReplayPolicy& policy,
    const BoundedMaterialRoleColumnFact* const roleColumn) noexcept
{
    if (policy.modelFillMaterial == "white")
    {
        return BoundedModelFillMaterial::White;
    }
    if (policy.modelFillMaterial == "varnish")
    {
        return BoundedModelFillMaterial::Varnish;
    }
    if (policy.modelFillMaterial == "rgb")
    {
        return BoundedModelFillMaterial::Rgb;
    }
    if (policy.modelFillMaterial == "material_role")
    {
        if (roleColumn == nullptr || !roleColumn->hasRole)
        {
            return ResolveRetainedProfileDefaultModelFillMaterial(policy);
        }
        switch (roleColumn->role)
        {
        case BoundedMaterialRole::Rgb:
            return BoundedModelFillMaterial::Rgb;
        case BoundedMaterialRole::White:
            return BoundedModelFillMaterial::White;
        case BoundedMaterialRole::Varnish:
            return BoundedModelFillMaterial::Varnish;
        case BoundedMaterialRole::Support:
        case BoundedMaterialRole::Ignore:
        case BoundedMaterialRole::SupportCandidate:
            return BoundedModelFillMaterial::None;
        }
    }
    return ResolveRetainedProfileDefaultModelFillMaterial(policy);
}

[[nodiscard]] bool ShouldApplyModelFill(
    const BoundedMaterialReplayPolicy& policy,
    const bool textureSurfacePixel,
    const BoundedModelFillMaterial material) noexcept
{
    if (!ModelFillUsesExplicitPolicy(policy)
        || material == BoundedModelFillMaterial::None)
    {
        return false;
    }
    if (!textureSurfacePixel)
    {
        return true;
    }
    return policy.modelFillScope != "below_texture_surface"
        && material != BoundedModelFillMaterial::Rgb;
}

[[nodiscard]] bool WriteModelFillPixel(
    const BoundedMaterialReplayPolicy& policy,
    const BoundedMaterialRoleColumnFact* const roleColumn,
    const std::size_t base,
    const std::span<std::uint8_t> pixels)
{
    switch (ResolveModelFillMaterial(policy, roleColumn))
    {
    case BoundedModelFillMaterial::Rgb:
        if (roleColumn != nullptr && roleColumn->hasRole
            && roleColumn->role == BoundedMaterialRole::Rgb
            && policy.modelFillMaterial == "material_role")
        {
            pixels[base + 0U] = roleColumn->rgb[0U];
            pixels[base + 1U] = roleColumn->rgb[1U];
            pixels[base + 2U] = roleColumn->rgb[2U];
        }
        else
        {
            const auto rgb{NonSurfaceRgb(policy)};
            pixels[base + 0U] = rgb[0U];
            pixels[base + 1U] = rgb[1U];
            pixels[base + 2U] = rgb[2U];
        }
        return true;
    case BoundedModelFillMaterial::White:
        pixels[base + 3U] = policy.modelFillValue;
        return true;
    case BoundedModelFillMaterial::Varnish:
        pixels[base + 5U] = policy.modelFillValue;
        return true;
    case BoundedModelFillMaterial::None:
        return false;
    }
    return false;
}

[[nodiscard]] bool IsTopMaterialLayer(
    const std::span<const BoundedMaterialColumnRangeFact> ranges,
    const std::size_t pixelIndex,
    const int layerIndex,
    const int topLayers) noexcept
{
    if (pixelIndex >= ranges.size())
    {
        return false;
    }
    const auto& range{ranges[pixelIndex]};
    if (!range.hasModel || range.upperLayer < range.lowerLayer)
    {
        return false;
    }
    const int firstTopLayer{
        std::max(range.lowerLayer, range.upperLayer - topLayers + 1)};
    return layerIndex >= firstTopLayer && layerIndex <= range.upperLayer;
}

[[nodiscard]] BoundedTextureColumnFact ResolveTextureColor(
    const BoundedMaterialReplayPolicy& policy,
    const std::span<const BoundedTextureColumnFact> columns,
    const std::size_t pixelIndex)
{
    BoundedTextureColumnFact color;
    color.hasColor = true;
    color.rgb = policy.textureFallbackRgb;
    color.usedFallback = true;
    if (pixelIndex < columns.size() && columns[pixelIndex].hasColor)
    {
        color = columns[pixelIndex];
    }
    return color;
}

void AccumulateTextureColor(
    const BoundedTextureColumnFact& color,
    BoundedTextureComposeStats& stats) noexcept
{
    stats.sampledPixels += color.sampledTexture ? 1U : 0U;
    stats.fallbackPixels += color.usedFallback ? 1U : 0U;
    stats.uvOutOfRangePixels += color.uvOutOfRange ? 1U : 0U;
}

struct MaterialPixel
{
    std::uint8_t r{255U};
    std::uint8_t g{255U};
    std::uint8_t b{255U};
    std::uint8_t w{255U};
    std::uint8_t v{255U};
};

[[nodiscard]] MaterialPixel ComposeMaterialPolicyPixel(
    const BoundedMaterialReplayPolicy& policy,
    const std::span<const BoundedTextureColumnFact> textureColumns,
    const std::span<const BoundedMaterialColumnRangeFact> columnRanges,
    const std::size_t pixelIndex,
    const int layerIndex,
    BoundedTextureComposeStats& textureStats)
{
    MaterialPixel pixel;
    if (policy.materialPolicyRgb.enabled)
    {
        if (policy.materialPolicyRgb.source == "texture_or_fallback"
            && policy.textureEnabled
            && ShouldApplyRetainedTextureToLayer(
                policy, columnRanges, pixelIndex, layerIndex))
        {
            const auto color{
                ResolveTextureColor(policy, textureColumns, pixelIndex)};
            pixel.r = color.rgb[0U];
            pixel.g = color.rgb[1U];
            pixel.b = color.rgb[2U];
            AccumulateTextureColor(color, textureStats);
        }
        else
        {
            const bool useNonSurfaceTextureRgb{
                policy.materialPolicyRgb.source == "texture_or_fallback"
                && policy.textureEnabled};
            const auto rgb{useNonSurfaceTextureRgb
                ? NonSurfaceRgb(policy)
                : policy.materialRgb};
            pixel.r = rgb[0U];
            pixel.g = rgb[1U];
            pixel.b = rgb[2U];
        }
    }
    if (policy.materialPolicyWhite.enabled
        && (policy.materialPolicyWhite.mode == "underbase"
            || policy.materialPolicyWhite.mode == "all_model"))
    {
        pixel.w = policy.materialPolicyWhite.value;
    }
    if (policy.materialPolicyVarnish.enabled)
    {
        if (policy.materialPolicyVarnish.mode == "all_model"
            || (policy.materialPolicyVarnish.mode == "top_n_layers"
                && IsTopMaterialLayer(
                    columnRanges,
                    pixelIndex,
                    layerIndex,
                    policy.materialPolicyVarnish.top_layers)))
        {
            pixel.v = policy.materialPolicyVarnish.value;
        }
    }
    return pixel;
}

void WriteMaterialPixel(
    const MaterialPixel& pixel,
    const std::size_t base,
    const std::span<std::uint8_t> pixels,
    BoundedMaterialPolicyComposeStats& stats)
{
    pixels[base + 0U] = pixel.r;
    pixels[base + 1U] = pixel.g;
    pixels[base + 2U] = pixel.b;
    pixels[base + 3U] = pixel.w;
    pixels[base + 5U] = pixel.v;
    stats.rgbPrintPixels +=
        pixel.r < 255U || pixel.g < 255U || pixel.b < 255U ? 1U : 0U;
    stats.whitePrintPixels += pixel.w < 255U ? 1U : 0U;
    stats.varnishPrintPixels += pixel.v < 255U ? 1U : 0U;
}

[[nodiscard]] bool WriteMaterialRolePixel(
    const BoundedMaterialRoleColumnFact& roleColumn,
    const std::size_t base,
    const std::span<std::uint8_t> pixels)
{
    if (!roleColumn.hasRole || roleColumn.role == BoundedMaterialRole::Rgb)
    {
        pixels[base + 0U] = roleColumn.rgb[0U];
        pixels[base + 1U] = roleColumn.rgb[1U];
        pixels[base + 2U] = roleColumn.rgb[2U];
        return true;
    }
    switch (roleColumn.role)
    {
    case BoundedMaterialRole::White:
        pixels[base + 3U] = 0U;
        return true;
    case BoundedMaterialRole::Varnish:
        pixels[base + 5U] = 0U;
        return true;
    case BoundedMaterialRole::Support:
        pixels[base + 4U] = 0U;
        return true;
    case BoundedMaterialRole::Ignore:
    case BoundedMaterialRole::SupportCandidate:
        return false;
    case BoundedMaterialRole::Rgb:
        break;
    }
    return false;
}

template <typename ClearedPredicate>
[[nodiscard]] MaterialClosureSemanticLayerInput InitializeSemantic(
    const int layerIndex,
    const double zMm,
    const int widthPx,
    const int heightPx,
    const std::span<const std::uint8_t> modelEnvelopeMask,
    const std::span<const std::uint8_t> finalSupportMask,
    const std::span<const std::uint8_t> outerVarnishShellMask,
    ClearedPredicate&& isCleared)
{
    if (!std::isfinite(zMm))
    {
        throw std::invalid_argument("retained material layer z must be finite");
    }
    const std::size_t pixelCount{CheckedPixelCount(widthPx, heightPx)};
    ValidateMask(modelEnvelopeMask, pixelCount, "modelEnvelopeMask");
    ValidateMask(finalSupportMask, pixelCount, "finalSupportMask");
    ValidateMask(outerVarnishShellMask, pixelCount, "outerVarnishShellMask");

    MaterialClosureSemanticLayerInput input;
    input.layerIndex = layerIndex;
    input.zMm = zMm;
    input.widthPx = widthPx;
    input.heightPx = heightPx;
    input.textureSurfaceMask.assign(pixelCount, 0U);
    input.modelFillMask.assign(pixelCount, 0U);
    input.modelMaterialMask.assign(pixelCount, 0U);
    input.supportFillMask.assign(pixelCount, 0U);
    input.internalVoidSupportMask.assign(pixelCount, 0U);
    input.surfaceVarnishMask.assign(pixelCount, 0U);
    input.outerVarnishShellMask.assign(
        outerVarnishShellMask.begin(), outerVarnishShellMask.end());
    input.modelEnvelopeMask.assign(
        modelEnvelopeMask.begin(), modelEnvelopeMask.end());
    input.supportRequiredMask.assign(
        finalSupportMask.begin(), finalSupportMask.end());
    input.expectedOccupiedDomainMask.assign(pixelCount, 0U);
    input.layerEmptyMask.assign(pixelCount, 0U);
    for (std::size_t index{0U}; index < pixelCount; ++index)
    {
        if (isCleared(index))
        {
            input.supportRequiredMask[index] = 1U;
        }
        input.expectedOccupiedDomainMask[index] =
            input.modelEnvelopeMask[index] != 0U
                || input.supportRequiredMask[index] != 0U
                || input.outerVarnishShellMask[index] != 0U
            ? 1U
            : 0U;
    }
    return input;
}

}  // namespace

BoundedMaterialReplayPolicy MakeBoundedMaterialReplayPolicy(
    const SliceConfig& config)
{
    BoundedMaterialReplayPolicy policy;
    policy.backgroundValue = config.background.value;
    policy.materialChannel = config.material.material_channel;
    policy.materialRgb = config.material.rgb;
    policy.materialWhiteValue = config.material.white_value;
    policy.materialVarnishValue = config.material.varnish_value;
    policy.textureEnabled = config.texture.enabled;
    policy.textureApplyMode = config.texture.apply_mode;
    policy.textureTopSurfaceLayers = config.texture.top_surface_layers;
    policy.textureFallbackRgb = config.texture.fallback_rgb;
    policy.textureNonSurfaceRgbPolicy = config.texture.non_surface_rgb_policy;
    policy.textureUnprintableWhitePolicy =
        config.texture.unprintable_white_policy;
    policy.textureUnprintableWhiteInkThreshold =
        config.texture.unprintable_white_ink_threshold;
    policy.textureUnprintableWhiteValue =
        config.texture.unprintable_white_value;
    policy.materialPolicyEnabled = config.material_policy.enabled;
    policy.materialPolicyRgb = config.material_policy.rgb;
    policy.materialPolicyWhite = config.material_policy.white;
    policy.materialPolicyVarnish = config.material_policy.varnish;
    policy.modelFillEnabled = config.model_fill.enabled;
    policy.modelFillMaterial = config.model_fill.material;
    policy.modelFillScope = config.model_fill.scope;
    policy.modelFillValue = config.model_fill.value;
    policy.modelFillLegacyRgbFallback = config.model_fill.legacy_rgb_fallback;
    policy.processProfileEnabled = config.material_process_profile.enabled;
    policy.processProfileWhiteEnabled =
        config.material_process_profile.white.enabled;
    policy.processProfileWhiteMode = config.material_process_profile.white.mode;
    policy.processProfileVarnishEnabled =
        config.material_process_profile.varnish.enabled;
    policy.processProfileVarnishMode =
        config.material_process_profile.varnish.mode;
    policy.materialRoleMappingEnabled = config.material_role_mapping.enabled;
    policy.supportEnabled = config.support.enabled;
    policy.supportValue = config.support.value;
    policy.outerVarnishValue = config.outer_varnish.value;
    policy.surfaceVarnishSource = config.surface_varnish.source;
    policy.surfaceVarnishValue = config.surface_varnish.value;
    policy.closure = config.material_closure;
    return policy;
}

BoundedModelFillMaterial ResolveRetainedProfileDefaultModelFillMaterial(
    const BoundedMaterialReplayPolicy& policy) noexcept
{
    if (policy.processProfileEnabled)
    {
        if (policy.processProfileWhiteEnabled
            && policy.processProfileWhiteMode != "disabled")
        {
            return BoundedModelFillMaterial::White;
        }
        if (policy.processProfileVarnishEnabled
            && policy.processProfileVarnishMode != "disabled")
        {
            return BoundedModelFillMaterial::Varnish;
        }
    }
    if (policy.materialPolicyEnabled)
    {
        if (policy.materialPolicyWhite.enabled
            && policy.materialPolicyWhite.mode != "disabled")
        {
            return BoundedModelFillMaterial::White;
        }
        if (policy.materialPolicyVarnish.enabled
            && policy.materialPolicyVarnish.mode != "disabled")
        {
            return BoundedModelFillMaterial::Varnish;
        }
    }
    if (policy.materialChannel == "W")
    {
        return BoundedModelFillMaterial::White;
    }
    if (policy.materialChannel == "V")
    {
        return BoundedModelFillMaterial::Varnish;
    }
    return BoundedModelFillMaterial::Rgb;
}

std::string BoundedModelFillMaterialName(
    const BoundedModelFillMaterial material)
{
    switch (material)
    {
    case BoundedModelFillMaterial::Rgb:
        return "rgb";
    case BoundedModelFillMaterial::White:
        return "white";
    case BoundedModelFillMaterial::Varnish:
        return "varnish";
    case BoundedModelFillMaterial::None:
        return "none";
    }
    return "none";
}

MaterialClosureRepairValues ResolveRetainedMaterialClosureRepairValues(
    const BoundedMaterialReplayPolicy& policy)
{
    MaterialClosureRepairValues values;
    values.modelFillRgb = NonSurfaceRgb(policy);
    values.modelFillValue = policy.modelFillValue;
    values.supportValue = policy.supportValue;
    switch (ResolveModelFillMaterial(policy, nullptr))
    {
    case BoundedModelFillMaterial::Rgb:
        values.modelFillMaterial = MaterialClosureModelFillMaterial::Rgb;
        break;
    case BoundedModelFillMaterial::White:
        values.modelFillMaterial = MaterialClosureModelFillMaterial::White;
        break;
    case BoundedModelFillMaterial::Varnish:
        values.modelFillMaterial = MaterialClosureModelFillMaterial::Varnish;
        break;
    case BoundedModelFillMaterial::None:
        values.modelFillMaterial = MaterialClosureModelFillMaterial::None;
        break;
    }
    return values;
}

std::uint8_t ResolveRetainedSurfaceVarnishValue(
    const BoundedMaterialReplayPolicy& policy) noexcept
{
    if (policy.surfaceVarnishSource == "material_policy"
        && policy.materialPolicyEnabled
        && policy.materialPolicyVarnish.enabled)
    {
        return policy.materialPolicyVarnish.value;
    }
    return policy.surfaceVarnishValue;
}

bool ShouldApplyRetainedTextureToLayer(
    const BoundedMaterialReplayPolicy& policy,
    const std::span<const BoundedMaterialColumnRangeFact> columnRanges,
    const std::size_t pixelIndex,
    const int layerIndex) noexcept
{
    if (policy.textureApplyMode == "solid_volume_from_top_surface")
    {
        return true;
    }
    if (policy.textureApplyMode == "top_surface_only")
    {
        return IsTopMaterialLayer(columnRanges, pixelIndex, layerIndex, 1);
    }
    if (policy.textureApplyMode == "top_surface_band")
    {
        return IsTopMaterialLayer(
            columnRanges,
            pixelIndex,
            layerIndex,
            policy.textureTopSurfaceLayers);
    }
    return true;
}

MaterialClosureSemanticLayerInput InitializeRetainedMaterialClosureSemanticInput(
    const int layerIndex,
    const double zMm,
    const int widthPx,
    const int heightPx,
    const std::span<const std::uint8_t> modelEnvelopeMask,
    const std::span<const std::uint8_t> finalSupportMask,
    const std::span<const std::uint8_t> clearedOuterOverlapMask,
    const std::span<const std::uint8_t> outerVarnishShellMask)
{
    const std::size_t pixelCount{CheckedPixelCount(widthPx, heightPx)};
    ValidateMask(
        clearedOuterOverlapMask, pixelCount, "clearedOuterOverlapMask");
    return InitializeSemantic(
        layerIndex,
        zMm,
        widthPx,
        heightPx,
        modelEnvelopeMask,
        finalSupportMask,
        outerVarnishShellMask,
        [clearedOuterOverlapMask](const std::size_t index)
        {
            return clearedOuterOverlapMask[index] != 0U;
        });
}

MaterialClosureSemanticLayerInput
InitializeRetainedMaterialClosureSemanticInputFromIndices(
    const int layerIndex,
    const double zMm,
    const int widthPx,
    const int heightPx,
    const std::span<const std::uint8_t> modelEnvelopeMask,
    const std::span<const std::uint8_t> finalSupportMask,
    const std::span<const std::size_t> clearedOuterOverlapIndices,
    const std::span<const std::uint8_t> outerVarnishShellMask)
{
    const std::size_t pixelCount{CheckedPixelCount(widthPx, heightPx)};
    for (const std::size_t index : clearedOuterOverlapIndices)
    {
        if (index >= pixelCount)
        {
            throw std::invalid_argument(
                "retained material cleared overlap index is invalid");
        }
    }
    auto input{InitializeSemantic(
        layerIndex,
        zMm,
        widthPx,
        heightPx,
        modelEnvelopeMask,
        finalSupportMask,
        outerVarnishShellMask,
        [](const std::size_t)
        {
            return false;
        })};
    for (const std::size_t index : clearedOuterOverlapIndices)
    {
        input.supportRequiredMask[index] = 1U;
        input.expectedOccupiedDomainMask[index] = 1U;
    }
    return input;
}

void PopulateRetainedMaterialClosureEmptyMask(
    const std::span<const std::uint8_t> rgbwsv,
    MaterialClosureSemanticLayerInput& semantic)
{
    const std::size_t pixelCount{
        CheckedPixelCount(semantic.widthPx, semantic.heightPx)};
    if (rgbwsv.size() != pixelCount * kRetainedMaterialChannelCount)
    {
        throw std::invalid_argument(
            "retained material closure RGBWSV layer size mismatch");
    }
    ValidateSemanticMasks(semantic, pixelCount);
    for (std::size_t index{0U}; index < pixelCount; ++index)
    {
        const std::size_t base{index * kRetainedMaterialChannelCount};
        bool empty{true};
        for (std::size_t channel{0U};
             channel < kRetainedMaterialChannelCount;
             ++channel)
        {
            empty = empty && rgbwsv[base + channel] == 255U;
        }
        semantic.layerEmptyMask[index] = empty ? 1U : 0U;
    }
}

BoundedMaterialLayerComposeResult ComposeRetainedMaterialLayer(
    const BoundedMaterialLayerComposeRequest& request,
    const std::span<std::uint8_t> outputRgbwsv,
    MaterialClosureSemanticLayerInput* const semantic)
{
    if (request.policy == nullptr)
    {
        throw std::invalid_argument(
            "retained material layer policy must be provided");
    }
    const auto& policy{*request.policy};
    const std::size_t pixelCount{
        CheckedPixelCount(request.widthPx, request.heightPx)};
    ValidateMask(request.modelMask, pixelCount, "modelMask");
    ValidateMask(request.outerVarnishMask, pixelCount, "outerVarnishMask");
    ValidateMask(
        request.outerSurfaceVarnishMask,
        pixelCount,
        "outerSurfaceVarnishMask");
    ValidateMask(
        request.innerSurfaceVarnishMask,
        pixelCount,
        "innerSurfaceVarnishMask");
    ValidateMask(request.supportMask, pixelCount, "supportMask");
    if (request.supportTypeMap.size() != pixelCount
        || (!request.textureColumns.empty()
            && request.textureColumns.size() != pixelCount)
        || (!request.materialRoleColumns.empty()
            && request.materialRoleColumns.size() != pixelCount)
        || request.columnRanges.size() != pixelCount
        || outputRgbwsv.size()
            != pixelCount * kRetainedMaterialChannelCount)
    {
        throw std::invalid_argument(
            "retained material layer facts or output size does not match");
    }
    if (semantic != nullptr)
    {
        if (semantic->layerIndex != request.layerIndex
            || semantic->widthPx != request.widthPx
            || semantic->heightPx != request.heightPx)
        {
            throw std::invalid_argument(
                "retained material semantic identity does not match");
        }
        ValidateSemanticMasks(*semantic, pixelCount);
    }

    std::fill(outputRgbwsv.begin(), outputRgbwsv.end(), policy.backgroundValue);
    BoundedMaterialLayerComposeResult result;
    const bool whiteCarrierEnabled{
        policy.textureUnprintableWhitePolicy == "white_underbase"};
    for (std::size_t pixelIndex{0U}; pixelIndex < pixelCount; ++pixelIndex)
    {
        const std::size_t base{pixelIndex * kRetainedMaterialChannelCount};
        if (request.modelMask[pixelIndex] != 0U)
        {
            bool countedModelPixel{false};
            bool textureSurfacePixel{false};
            bool modelFillPixel{false};
            if (policy.materialRoleMappingEnabled
                && pixelIndex < request.materialRoleColumns.size())
            {
                const auto& roleColumn{
                    request.materialRoleColumns[pixelIndex]};
                bool wroteModel{false};
                const bool applyTexture{
                    policy.textureEnabled
                    && ShouldApplyRetainedTextureToLayer(
                        policy,
                        request.columnRanges,
                        pixelIndex,
                        request.layerIndex)};
                if (roleColumn.hasRole
                    && roleColumn.role == BoundedMaterialRole::Rgb
                    && policy.textureEnabled && !applyTexture)
                {
                    if (ModelFillUsesExplicitPolicy(policy))
                    {
                        wroteModel = WriteModelFillPixel(
                            policy, &roleColumn, base, outputRgbwsv);
                        modelFillPixel = wroteModel;
                    }
                    else
                    {
                        WriteNonSurfaceTexturePixel(policy, base, outputRgbwsv);
                        wroteModel = true;
                    }
                }
                else
                {
                    wroteModel = WriteMaterialRolePixel(
                        roleColumn, base, outputRgbwsv);
                }
                if (wroteModel)
                {
                    countedModelPixel = true;
                    if (roleColumn.hasRole
                        && roleColumn.role == BoundedMaterialRole::Rgb
                        && applyTexture)
                    {
                        const auto color{ResolveTextureColor(
                            policy, request.textureColumns, pixelIndex)};
                        AccumulateTextureColor(color, result.texture);
                        textureSurfacePixel = true;
                    }
                    else if (policy.modelFillEnabled)
                    {
                        modelFillPixel = !roleColumn.hasRole
                            || roleColumn.role == BoundedMaterialRole::Rgb
                            || (roleColumn.role == BoundedMaterialRole::White
                                && policy.modelFillMaterial == "white")
                            || (roleColumn.role == BoundedMaterialRole::Varnish
                                && policy.modelFillMaterial == "varnish");
                    }
                    if (textureSurfacePixel)
                    {
                        const auto fillMaterial{
                            ResolveModelFillMaterial(policy, &roleColumn)};
                        if (ShouldApplyModelFill(
                                policy, textureSurfacePixel, fillMaterial)
                            && WriteModelFillPixel(
                                policy, &roleColumn, base, outputRgbwsv))
                        {
                            modelFillPixel = true;
                        }
                    }
                }
            }
            else if (policy.materialPolicyEnabled)
            {
                textureSurfacePixel = policy.materialPolicyRgb.enabled
                    && policy.materialPolicyRgb.source == "texture_or_fallback"
                    && policy.textureEnabled
                    && ShouldApplyRetainedTextureToLayer(
                        policy,
                        request.columnRanges,
                        pixelIndex,
                        request.layerIndex);
                const auto fillMaterial{
                    ResolveModelFillMaterial(policy, nullptr)};
                if (ShouldApplyModelFill(
                        policy, textureSurfacePixel, fillMaterial)
                    && !textureSurfacePixel)
                {
                    countedModelPixel = WriteModelFillPixel(
                        policy, nullptr, base, outputRgbwsv);
                    modelFillPixel = countedModelPixel;
                }
                else
                {
                    const auto pixel{ComposeMaterialPolicyPixel(
                        policy,
                        request.textureColumns,
                        request.columnRanges,
                        pixelIndex,
                        request.layerIndex,
                        result.texture)};
                    WriteMaterialPixel(
                        pixel, base, outputRgbwsv, result.materialPolicy);
                    countedModelPixel = true;
                    if (ShouldApplyModelFill(
                            policy, textureSurfacePixel, fillMaterial)
                        && WriteModelFillPixel(
                            policy, nullptr, base, outputRgbwsv))
                    {
                        modelFillPixel = true;
                    }
                    else
                    {
                        modelFillPixel =
                            policy.modelFillEnabled && !textureSurfacePixel;
                    }
                }
            }
            else if (policy.textureEnabled
                     && ShouldApplyRetainedTextureToLayer(
                         policy,
                         request.columnRanges,
                         pixelIndex,
                         request.layerIndex))
            {
                const auto color{ResolveTextureColor(
                    policy, request.textureColumns, pixelIndex)};
                outputRgbwsv[base + 0U] = color.rgb[0U];
                outputRgbwsv[base + 1U] = color.rgb[1U];
                outputRgbwsv[base + 2U] = color.rgb[2U];
                AccumulateTextureColor(color, result.texture);
                if (whiteCarrierEnabled
                    && ApplyUnprintableWhiteCarrier(
                        policy.textureUnprintableWhiteInkThreshold,
                        policy.textureUnprintableWhiteValue,
                        color.rgb,
                        outputRgbwsv[base + 3U]))
                {
                    ++result.semantic.unprintable_white_carrier_pixels;
                }
                countedModelPixel = true;
                textureSurfacePixel = true;
                const auto fillMaterial{
                    ResolveModelFillMaterial(policy, nullptr)};
                if (ShouldApplyModelFill(
                        policy, textureSurfacePixel, fillMaterial)
                    && WriteModelFillPixel(
                        policy, nullptr, base, outputRgbwsv))
                {
                    modelFillPixel = true;
                }
            }
            else
            {
                if (ModelFillUsesExplicitPolicy(policy))
                {
                    countedModelPixel = WriteModelFillPixel(
                        policy, nullptr, base, outputRgbwsv);
                    modelFillPixel = countedModelPixel;
                }
                else
                {
                    WriteNonSurfaceTexturePixel(policy, base, outputRgbwsv);
                    countedModelPixel = true;
                    modelFillPixel = policy.modelFillEnabled;
                }
            }

            if (countedModelPixel)
            {
                const auto varnishValue{
                    ResolveRetainedSurfaceVarnishValue(policy)};
                if (request.outerSurfaceVarnishMask[pixelIndex] != 0U)
                {
                    outputRgbwsv[base + 5U] = varnishValue;
                    ++result.semantic.outer_surface_varnish_pixels;
                    if (semantic != nullptr)
                    {
                        semantic->surfaceVarnishMask[pixelIndex] = 1U;
                    }
                }
                if (request.innerSurfaceVarnishMask[pixelIndex] != 0U)
                {
                    outputRgbwsv[base + 5U] = varnishValue;
                    ++result.semantic.inner_surface_varnish_pixels;
                    if (semantic != nullptr)
                    {
                        semantic->surfaceVarnishMask[pixelIndex] = 1U;
                    }
                }
                ++result.modelPixels;
                if (semantic != nullptr)
                {
                    semantic->modelMaterialMask[pixelIndex] = 1U;
                }
                if (textureSurfacePixel)
                {
                    ++result.semantic.texture_surface_pixels;
                    if (semantic != nullptr)
                    {
                        semantic->textureSurfaceMask[pixelIndex] = 1U;
                    }
                }
                if (modelFillPixel)
                {
                    ++result.semantic.model_fill_pixels;
                    if (semantic != nullptr)
                    {
                        semantic->modelFillMask[pixelIndex] = 1U;
                    }
                }
            }
        }
        else if (request.outerVarnishMask[pixelIndex] != 0U)
        {
            outputRgbwsv[base + 5U] = policy.outerVarnishValue;
            ++result.semantic.outer_varnish_pixels;
        }
        else if (policy.supportEnabled && request.supportMask[pixelIndex] != 0U)
        {
            outputRgbwsv[base + 4U] = policy.supportValue;
            ++result.supportPixels;
            ++result.semantic.support_pixels;
            if (semantic != nullptr)
            {
                semantic->supportFillMask[pixelIndex] = 1U;
            }
            if (request.supportTypeMap[pixelIndex] == SupportType::InternalVoid)
            {
                ++result.semantic.internal_void_support_pixels;
                if (semantic != nullptr)
                {
                    semantic->internalVoidSupportMask[pixelIndex] = 1U;
                }
            }
        }
    }
    return result;
}

BoundedMaterialLayerChannelStats AnalyzeRetainedMaterialLayerChannels(
    const std::span<const std::uint8_t> rgbwsv)
{
    if (rgbwsv.size() % kRetainedMaterialChannelCount != 0U)
    {
        throw std::invalid_argument(
            "retained material RGBWSV channel buffer size is invalid");
    }
    BoundedMaterialLayerChannelStats result;
    const std::size_t pixelCount{
        rgbwsv.size() / kRetainedMaterialChannelCount};
    for (std::size_t index{0U}; index < pixelCount; ++index)
    {
        const std::size_t base{index * kRetainedMaterialChannelCount};
        for (std::size_t channel{0U};
             channel < kRetainedMaterialChannelCount;
             ++channel)
        {
            const int value{rgbwsv[base + channel]};
            auto& stats{result.channels[channel]};
            stats.min_value = std::min(stats.min_value, value);
            stats.max_value = std::max(stats.max_value, value);
            if (value == 255)
            {
                ++stats.empty_pixels;
            }
            else
            {
                ++stats.print_pixels;
                if (value == 0)
                {
                    ++stats.full_print_pixels;
                }
                else
                {
                    ++stats.partial_print_pixels;
                }
            }
        }
        result.rgbNonZeroPixels +=
            rgbwsv[base + 0U] < 255U || rgbwsv[base + 1U] < 255U
                || rgbwsv[base + 2U] < 255U
            ? 1
            : 0;
        result.whiteNonZeroPixels += rgbwsv[base + 3U] < 255U ? 1 : 0;
        result.supportNonZeroPixels += rgbwsv[base + 4U] < 255U ? 1 : 0;
        result.varnishNonZeroPixels += rgbwsv[base + 5U] < 255U ? 1 : 0;
    }
    return result;
}

void AccumulateRetainedMaterialChannelStats(
    std::array<BoundedMaterialChannelStats,
               kRetainedMaterialChannelCount>& totals,
    const std::array<BoundedMaterialChannelStats,
                     kRetainedMaterialChannelCount>& layer) noexcept
{
    for (std::size_t channel{0U};
         channel < kRetainedMaterialChannelCount;
         ++channel)
    {
        totals[channel].print_pixels += layer[channel].print_pixels;
        totals[channel].full_print_pixels += layer[channel].full_print_pixels;
        totals[channel].partial_print_pixels += layer[channel].partial_print_pixels;
        totals[channel].empty_pixels += layer[channel].empty_pixels;
        totals[channel].min_value =
            std::min(totals[channel].min_value, layer[channel].min_value);
        totals[channel].max_value =
            std::max(totals[channel].max_value, layer[channel].max_value);
    }
}

void AccumulateRetainedMaterialSemanticStats(
    BoundedMaterialLayerSemanticStats& totals,
    const BoundedMaterialLayerSemanticStats& layer) noexcept
{
    totals.texture_surface_pixels += layer.texture_surface_pixels;
    totals.unprintable_white_carrier_pixels +=
        layer.unprintable_white_carrier_pixels;
    totals.model_fill_pixels += layer.model_fill_pixels;
    totals.support_pixels += layer.support_pixels;
    totals.internal_void_support_pixels += layer.internal_void_support_pixels;
    totals.outer_varnish_pixels += layer.outer_varnish_pixels;
    totals.outer_surface_varnish_pixels += layer.outer_surface_varnish_pixels;
    totals.inner_surface_varnish_pixels += layer.inner_surface_varnish_pixels;
}

}  // namespace slicer_core
