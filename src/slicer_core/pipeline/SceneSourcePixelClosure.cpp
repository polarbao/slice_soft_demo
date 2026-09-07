#include "slicer_core/pipeline/SceneSourcePixelClosure.h"

namespace slicer_core
{

bool IsEmptySourcePixel(
    const SceneInstanceRasterLayer& layer,
    const std::size_t pixelIndex,
    const std::uint8_t emptyValue)
{
    for (std::size_t channel{0U}; channel < kSceneChannelCount; ++channel)
    {
        if (layer.output.channels[SceneChannelIndex(pixelIndex, channel)]
            != emptyValue)
        {
            return false;
        }
    }
    return true;
}

SceneRasterOwnership ResolveOwnership(
    const SceneInstanceRasterLayer& layer,
    const std::size_t pixelIndex)
{
    if (layer.modelownership[pixelIndex] != 0U)
    {
        return SceneRasterOwnership::Model;
    }
    if (layer.outervarnishownership[pixelIndex] != 0U)
    {
        return SceneRasterOwnership::OuterVarnish;
    }
    if (layer.supportownership[pixelIndex] != 0U)
    {
        return SceneRasterOwnership::Support;
    }
    return SceneRasterOwnership::Empty;
}

bool SourcePixelHasClosure(
    const SceneInstanceRasterLayer& layer,
    const std::size_t pixelIndex,
    const SceneRasterOwnership ownership,
    const RgbwsvProtocol& protocol)
{
    const std::size_t base = pixelIndex * kSceneChannelCount;
    if (ownership == SceneRasterOwnership::Empty)
    {
        return IsEmptySourcePixel(layer, pixelIndex, protocol.empty_value);
    }
    if (ownership == SceneRasterOwnership::Support)
    {
        for (std::size_t channel{0U}; channel < kSceneChannelCount; ++channel)
        {
            const std::uint8_t expected =
                channel == kSceneSupportChannel
                    ? protocol.print_value
                    : protocol.empty_value;
            if (layer.output.channels[base + channel] != expected)
            {
                return false;
            }
        }
        return true;
    }
    if (ownership == SceneRasterOwnership::OuterVarnish)
    {
        for (std::size_t channel{0U}; channel < kSceneChannelCount; ++channel)
        {
            std::uint8_t expected = protocol.empty_value;
            if (channel == kSceneVarnishChannel
                || (channel == kSceneSupportChannel
                    && layer.supportownership[pixelIndex] != 0U))
            {
                expected = protocol.print_value;
            }
            if (layer.output.channels[base + channel] != expected)
            {
                return false;
            }
        }
        return true;
    }

    bool modelMaterialPresent{false};
    for (std::size_t channel{0U}; channel < kSceneChannelCount; ++channel)
    {
        const std::uint8_t value = layer.output.channels[base + channel];
        if (channel == kSceneSupportChannel)
        {
            const std::uint8_t expected =
                layer.supportownership[pixelIndex] != 0U
                    ? protocol.print_value
                    : protocol.empty_value;
            if (value != expected)
            {
                return false;
            }
            continue;
        }
        if (channel == kSceneVarnishChannel)
        {
            const bool modelVarnish =
                layer.modelvarnishownership[pixelIndex] != 0U;
            if (modelVarnish)
            {
                if (value == protocol.empty_value)
                {
                    return false;
                }
                modelMaterialPresent = true;
            }
            else if (
                value != protocol.empty_value
                && layer.outervarnishownership[pixelIndex] == 0U)
            {
                return false;
            }
            continue;
        }
        if (channel < kSceneSupportChannel && value != protocol.empty_value)
        {
            modelMaterialPresent = true;
        }
    }
    return modelMaterialPresent;
}

}  // namespace slicer_core
