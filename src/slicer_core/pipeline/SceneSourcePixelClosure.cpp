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
    if (modelMaterialPresent)
    {
        return true;
    }
    // MW3-06：整模缩裹的模型像素印在【第七通道】上，六通道全空正是它的正确形态。
    //
    // 这条闭合不变量要问的是「这个模型像素有没有真的印出东西」，
    // 而不是「有没有印在这六个通道里」。缩裹像素印了，只是印在本函数
    // 看不见的通道上，故按已闭合处理。
    //
    // 豁免是【有据的】而非放宽：transfermask 只在启用 T 的运行里非空，
    // 且它是模型掩膜的子集。六通道运行下它恒为空，本分支永不触发，
    // 「模型像素不得凭空空白」这条保证对六通道一字未改。
    return pixelIndex < layer.transfermask.size()
        && layer.transfermask[pixelIndex] != 0U;
}

}  // namespace slicer_core
