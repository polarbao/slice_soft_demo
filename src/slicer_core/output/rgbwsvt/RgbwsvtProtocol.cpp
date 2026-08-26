#include "slicer_core/output/rgbwsvt/RgbwsvtProtocol.h"

#include "slicer_core/materials/transfer/TransferChannelError.h"

#include <algorithm>
#include <cstddef>
#include <limits>

namespace slicer_core
{
namespace
{

constexpr std::array<const char*, 6> kRgbwsvOrder{"R", "G", "B", "W", "S", "V"};

std::size_t CheckedPixelCount(const RgbwsvProductionLayer& layer)
{
    if (layer.widthPx <= 0 || layer.heightPx <= 0)
    {
        throw TransferChannelError(
            TransferChannelErrorCode::ProtocolInvalid,
            "RGBWSVT layer dimensions must be positive");
    }
    const std::size_t width = static_cast<std::size_t>(layer.widthPx);
    const std::size_t height = static_cast<std::size_t>(layer.heightPx);
    if (width > std::numeric_limits<std::size_t>::max() / height)
    {
        throw TransferChannelError(
            TransferChannelErrorCode::ProtocolInvalid,
            "RGBWSVT layer dimensions overflow");
    }
    return width * height;
}

}  // namespace

RgbwsvtProtocol CurrentRgbwsvtProtocol()
{
    return {};
}

RgbwsvtProductionLayer ComposeRgbwsvtLayer(
    const RgbwsvProductionLayer& rgbwsvLayer,
    const std::span<const std::uint8_t> modelMask,
    const std::span<const std::uint8_t> transferMask,
    const std::uint8_t transferValue)
{
    const std::size_t pixelCount = CheckedPixelCount(rgbwsvLayer);
    for (std::size_t channel{0U}; channel < kRgbwsvOrder.size(); ++channel)
    {
        if (rgbwsvLayer.channelOrder[channel] != kRgbwsvOrder[channel])
        {
            throw TransferChannelError(
                TransferChannelErrorCode::ProtocolInvalid,
                "source channel order must be exactly R G B W S V");
        }
    }
    if (rgbwsvLayer.channels.size() != pixelCount * kRgbwsvOrder.size()
        || modelMask.size() != pixelCount
        || transferMask.size() != pixelCount)
    {
        throw TransferChannelError(
            TransferChannelErrorCode::ProtocolInvalid,
            "RGBWSVT source and mask sizes do not match layer dimensions");
    }
    if (transferValue != CurrentRgbwsvtProtocol().printValue)
    {
        throw TransferChannelError(
            TransferChannelErrorCode::ConfigInvalid,
            "transfer print value must be 0 for p0.rgbwsvt.1 black_is_print");
    }

    RgbwsvtProductionLayer result;
    result.layerIndex = rgbwsvLayer.layerIndex;
    result.zMm = rgbwsvLayer.zMm;
    result.widthPx = rgbwsvLayer.widthPx;
    result.heightPx = rgbwsvLayer.heightPx;
    result.channels.assign(
        pixelCount * kRgbwsvtChannelCount, CurrentRgbwsvtProtocol().emptyValue);

    for (std::size_t pixel{0U}; pixel < pixelCount; ++pixel)
    {
        const bool isTransfer = transferMask[pixel] != 0U;
        if (isTransfer && modelMask[pixel] == 0U)
        {
            throw TransferChannelError(
                TransferChannelErrorCode::MaskOutsideModel,
                "transfer mask contains a pixel outside the model mask");
        }
        const std::size_t sourceOffset = pixel * kRgbwsvOrder.size();
        const std::size_t targetOffset = pixel * kRgbwsvtChannelCount;
        if (!isTransfer)
        {
            std::copy_n(
                rgbwsvLayer.channels.begin() + static_cast<std::ptrdiff_t>(sourceOffset),
                kRgbwsvOrder.size(),
                result.channels.begin() + static_cast<std::ptrdiff_t>(targetOffset));
            continue;
        }
        result.channels[targetOffset + kTransferChannelOffset] = transferValue;
    }
    return result;
}

}  // namespace slicer_core
