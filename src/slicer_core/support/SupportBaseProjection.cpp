#include "slicer_core/support/SupportBaseProjection.h"

#include <algorithm>
#include <stdexcept>

namespace slicer_core
{
namespace
{

std::size_t ValidateMasks(
    const std::vector<std::vector<std::uint8_t>>& modelMasks,
    const std::vector<std::vector<std::uint8_t>>& supportMasks)
{
    if (modelMasks.size() != supportMasks.size())
    {
        throw std::invalid_argument(
            "support base projection model/support layer counts do not match");
    }
    if (supportMasks.empty())
    {
        return 0U;
    }

    const std::size_t pixelCount = supportMasks.front().size();
    for (std::size_t layerIndex{0U};
         layerIndex < supportMasks.size();
         ++layerIndex)
    {
        if (modelMasks.at(layerIndex).size() != pixelCount
            || supportMasks.at(layerIndex).size() != pixelCount)
        {
            throw std::invalid_argument(
                "support base projection mask dimensions do not match");
        }
    }
    return pixelCount;
}

}  // namespace

SupportBaseProjectionResult ApplySupportBaseProjection(
    const SupportBaseProjectionConfig& config,
    const std::vector<std::vector<std::uint8_t>>& modelMasks,
    std::vector<std::vector<std::uint8_t>>& supportMasks)
{
    SupportBaseProjectionResult result;
    result.enabled = config.enabled;
    result.configured_layer_count = config.layer_count;
    const std::size_t pixelCount = ValidateMasks(modelMasks, supportMasks);
    if (!config.enabled
        || config.layer_count <= 0
        || supportMasks.empty()
        || pixelCount == 0U)
    {
        return result;
    }

    std::vector<std::uint8_t> footprint(pixelCount, 0U);
    for (const std::vector<std::uint8_t>& supportMask : supportMasks)
    {
        for (std::size_t index{0U}; index < pixelCount; ++index)
        {
            if (supportMask.at(index) != 0U)
            {
                footprint.at(index) = 1U;
            }
        }
    }
    result.footprint_pixels = static_cast<int>(std::count(
        footprint.begin(),
        footprint.end(),
        static_cast<std::uint8_t>(1U)));
    result.effective_layer_count = std::min(
        config.layer_count,
        static_cast<int>(supportMasks.size()));

    for (int layerIndex{0};
         layerIndex < result.effective_layer_count;
         ++layerIndex)
    {
        const std::vector<std::uint8_t>& modelMask =
            modelMasks.at(static_cast<std::size_t>(layerIndex));
        std::vector<std::uint8_t>& supportMask =
            supportMasks.at(static_cast<std::size_t>(layerIndex));
        for (std::size_t index{0U}; index < pixelCount; ++index)
        {
            if (footprint.at(index) == 0U
                || modelMask.at(index) != 0U
                || supportMask.at(index) != 0U)
            {
                continue;
            }
            supportMask.at(index) = 1U;
            ++result.added_support_pixels;
        }
    }
    return result;
}

SupportBaseProjectionResult ApplySupportBaseProjectionVolume(
    const SupportBaseProjectionConfig& config,
    const std::vector<std::uint8_t>& modelVolume,
    std::vector<std::uint8_t>& supportVolume,
    const int layerCount,
    const std::size_t pixelCount)
{
    if (layerCount < 0
        || modelVolume.size() != supportVolume.size()
        || modelVolume.size()
            != static_cast<std::size_t>(layerCount) * pixelCount)
    {
        throw std::invalid_argument(
            "support base projection volume dimensions do not match");
    }

    SupportBaseProjectionResult result;
    result.enabled = config.enabled;
    result.configured_layer_count = config.layer_count;
    if (!config.enabled
        || config.layer_count <= 0
        || layerCount == 0
        || pixelCount == 0U)
    {
        return result;
    }

    std::vector<std::uint8_t> footprint(pixelCount, 0U);
    for (int layerIndex{0}; layerIndex < layerCount; ++layerIndex)
    {
        const std::size_t layerOffset =
            static_cast<std::size_t>(layerIndex) * pixelCount;
        for (std::size_t index{0U}; index < pixelCount; ++index)
        {
            if (supportVolume.at(layerOffset + index) != 0U)
            {
                footprint.at(index) = 1U;
            }
        }
    }
    result.footprint_pixels = static_cast<int>(std::count(
        footprint.begin(),
        footprint.end(),
        static_cast<std::uint8_t>(1U)));
    result.effective_layer_count =
        std::min(config.layer_count, layerCount);

    for (int layerIndex{0};
         layerIndex < result.effective_layer_count;
         ++layerIndex)
    {
        const std::size_t layerOffset =
            static_cast<std::size_t>(layerIndex) * pixelCount;
        for (std::size_t index{0U}; index < pixelCount; ++index)
        {
            const std::size_t volumeIndex = layerOffset + index;
            if (footprint.at(index) == 0U
                || modelVolume.at(volumeIndex) != 0U
                || supportVolume.at(volumeIndex) != 0U)
            {
                continue;
            }
            supportVolume.at(volumeIndex) = 1U;
            ++result.added_support_pixels;
        }
    }
    return result;
}

}  // namespace slicer_core
