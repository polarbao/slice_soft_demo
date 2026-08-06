#pragma once

bool StopIfCancellationRequested(
    const SceneLayerComposeRequest& request,
    SceneLayerComposeResult& result,
    const std::string& field,
    const int layerIndex = -1)
{
    if (request.canceltoken == nullptr
        || !request.canceltoken->IsCancelRequested())
    {
        return false;
    }
    Block(
        result,
        SceneRasterErrorCode::Cancelled,
        request,
        field,
        "scene layer composition stopped at a cooperative checkpoint",
        nullptr,
        nullptr,
        layerIndex);
    return true;
}

bool OutputPixelHasClosure(
    const std::vector<std::uint8_t>& channels,
    const std::size_t pixelIndex,
    const SceneRasterOwnership ownership,
    const RgbwsvProtocol& protocol)
{
    const std::size_t base = pixelIndex * kChannelCount;
    for (std::size_t channel{0U}; channel < kChannelCount; ++channel)
    {
        const std::uint8_t value = channels.at(base + channel);
        if (ownership == SceneRasterOwnership::Empty
            && value != protocol.empty_value)
        {
            return false;
        }
        if (ownership == SceneRasterOwnership::Support)
        {
            const std::uint8_t expected =
                channel == kSupportChannel
                    ? protocol.print_value
                    : protocol.empty_value;
            if (value != expected)
            {
                return false;
            }
        }
        if (ownership == SceneRasterOwnership::OuterVarnish)
        {
            const std::uint8_t expected =
                channel == kVarnishChannel
                    ? protocol.print_value
                    : protocol.empty_value;
            if (value != expected)
            {
                return false;
            }
        }
    }
    if (ownership != SceneRasterOwnership::Model)
    {
        return true;
    }
    if (channels.at(base + kSupportChannel) != protocol.empty_value)
    {
        return false;
    }
    for (std::size_t channel{0U}; channel < kChannelCount; ++channel)
    {
        if (channel != kSupportChannel
            && channels.at(base + channel) != protocol.empty_value)
        {
            return true;
        }
    }
    return false;
}
