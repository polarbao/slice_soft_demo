#include "slicer_core/material/MaterialChannelComposer.h"

namespace slicer_core
{
namespace
{

constexpr int kMaterialChannelCount{6};
constexpr std::uint8_t kEmptyValue{255};

std::size_t PixelCount(const MaterialChannelComposerInput& input)
{
    return static_cast<std::size_t>(input.width) * static_cast<std::size_t>(input.height);
}

std::size_t ChannelIndex(const std::size_t pixelIndex, const MaterialChannelOffset offset)
{
    return pixelIndex * static_cast<std::size_t>(kMaterialChannelCount) + static_cast<std::size_t>(offset);
}

bool HasMaskValue(const std::vector<std::uint8_t>& mask, const std::size_t index)
{
    return !mask.empty() && mask.at(index) != 0;
}

bool IsMaskSizeValid(const std::vector<std::uint8_t>& mask, const std::size_t pixelCount)
{
    return mask.empty() || mask.size() == pixelCount;
}

bool ValidateInput(const MaterialChannelComposerInput& input, std::string& error)
{
    if (input.width <= 0 || input.height <= 0)
    {
        error = "material channel composer width and height must be positive";
        return false;
    }

    const std::size_t pixelCount = PixelCount(input);
    if (!IsMaskSizeValid(input.support_mask, pixelCount)
        || !IsMaskSizeValid(input.model_mask, pixelCount)
        || !IsMaskSizeValid(input.surface_shell_mask, pixelCount)
        || !IsMaskSizeValid(input.white_mask, pixelCount)
        || !IsMaskSizeValid(input.varnish_mask, pixelCount))
    {
        error = "material channel composer mask sizes must be empty or width * height";
        return false;
    }
    if (!input.surface_rgb.empty() && input.surface_rgb.size() != pixelCount)
    {
        error = "material channel composer surface_rgb size must be empty or width * height";
        return false;
    }
    return true;
}

void WriteRgb(
    std::vector<std::uint8_t>& channels,
    const std::size_t pixelIndex,
    const std::array<std::uint8_t, 3>& rgb)
{
    channels.at(ChannelIndex(pixelIndex, MaterialChannelOffset::R)) = rgb.at(0);
    channels.at(ChannelIndex(pixelIndex, MaterialChannelOffset::G)) = rgb.at(1);
    channels.at(ChannelIndex(pixelIndex, MaterialChannelOffset::B)) = rgb.at(2);
}

bool IsEmptyPixel(const std::vector<std::uint8_t>& channels, const std::size_t pixelIndex)
{
    for (int channel{0}; channel < kMaterialChannelCount; ++channel)
    {
        if (channels.at(pixelIndex * static_cast<std::size_t>(kMaterialChannelCount) + static_cast<std::size_t>(channel))
            != kEmptyValue)
        {
            return false;
        }
    }
    return true;
}

}  // namespace

int MaterialChannelCount()
{
    return kMaterialChannelCount;
}

MaterialChannelComposerResult ComposeMaterialChannels(const MaterialChannelComposerInput& input)
{
    MaterialChannelComposerResult result;
    result.width = input.width;
    result.height = input.height;
    if (!ValidateInput(input, result.error))
    {
        return result;
    }

    const std::size_t pixelCount = PixelCount(input);
    result.channels.assign(pixelCount * static_cast<std::size_t>(kMaterialChannelCount), kEmptyValue);

    for (std::size_t index{0}; index < pixelCount; ++index)
    {
        if (HasMaskValue(input.support_mask, index))
        {
            result.channels.at(ChannelIndex(index, MaterialChannelOffset::S)) = input.support_value;
            ++result.stats.support_pixels;
        }

        if (HasMaskValue(input.model_mask, index))
        {
            if (HasMaskValue(input.support_mask, index))
            {
                ++result.stats.model_support_conflict_pixels;
                result.channels.at(ChannelIndex(index, MaterialChannelOffset::S)) = kEmptyValue;
            }
            WriteRgb(result.channels, index, input.model_rgb);
            ++result.stats.model_pixels;
        }

        if (HasMaskValue(input.surface_shell_mask, index))
        {
            const std::array<std::uint8_t, 3> rgb =
                input.surface_rgb.empty() ? input.model_rgb : input.surface_rgb.at(index);
            WriteRgb(result.channels, index, rgb);
            ++result.stats.surface_rgb_pixels;
        }

        if (HasMaskValue(input.white_mask, index))
        {
            result.channels.at(ChannelIndex(index, MaterialChannelOffset::W)) = input.white_value;
            ++result.stats.white_pixels;
        }

        if (HasMaskValue(input.varnish_mask, index))
        {
            result.channels.at(ChannelIndex(index, MaterialChannelOffset::V)) = input.varnish_value;
            ++result.stats.varnish_pixels;
        }
    }

    for (std::size_t index{0}; index < pixelCount; ++index)
    {
        if (IsEmptyPixel(result.channels, index))
        {
            ++result.stats.empty_pixels;
        }
    }
    return result;
}

}  // namespace slicer_core
