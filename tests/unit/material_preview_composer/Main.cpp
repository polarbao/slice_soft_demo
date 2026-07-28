#include "slicer_core/preview/MaterialPreviewComposer.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{

constexpr std::uint32_t kWidth{3U};
constexpr std::uint32_t kHeight{2U};
constexpr std::size_t kChannelCount{6U};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

std::size_t ChannelOffset(
    const std::uint32_t x,
    const std::uint32_t y,
    const std::size_t channel)
{
    return (static_cast<std::size_t>(y) * kWidth + x)
        * kChannelCount
        + channel;
}

slicer_core::RgbwsvLayerBuffer MakeBuffer()
{
    slicer_core::RgbwsvLayerBuffer buffer;
    buffer.sourceIdentity = "source-identity";
    buffer.layerIndex = 17;
    buffer.zMm = 0.175;
    buffer.width = kWidth;
    buffer.height = kHeight;
    buffer.dpiX = 635;
    buffer.dpiY = 600;
    buffer.pixels.assign(
        static_cast<std::size_t>(kWidth * kHeight)
            * kChannelCount,
        255U);

    buffer.pixels.at(ChannelOffset(1U, 0U, 0U)) = 20U;
    buffer.pixels.at(ChannelOffset(1U, 0U, 1U)) = 40U;
    buffer.pixels.at(ChannelOffset(1U, 0U, 2U)) = 60U;
    buffer.pixels.at(ChannelOffset(2U, 0U, 3U)) = 0U;
    buffer.pixels.at(ChannelOffset(0U, 1U, 4U)) = 0U;
    buffer.pixels.at(ChannelOffset(1U, 1U, 5U)) = 0U;
    buffer.pixels.at(ChannelOffset(2U, 1U, 0U)) = 10U;
    buffer.pixels.at(ChannelOffset(2U, 1U, 1U)) = 20U;
    buffer.pixels.at(ChannelOffset(2U, 1U, 2U)) = 30U;
    buffer.pixels.at(ChannelOffset(2U, 1U, 3U)) = 0U;
    buffer.pixels.at(ChannelOffset(2U, 1U, 4U)) = 0U;
    buffer.pixels.at(ChannelOffset(2U, 1U, 5U)) = 0U;
    buffer.decodedBytes = buffer.pixels.size();
    return buffer;
}

std::array<std::uint8_t, 4> DisplayPixel(
    const slicer_core::MaterialPreviewResult& result,
    const std::uint32_t x,
    const std::uint32_t y)
{
    const std::size_t offset =
        (static_cast<std::size_t>(y) * result.width + x) * 4U;
    return {
        result.rgba.at(offset),
        result.rgba.at(offset + 1U),
        result.rgba.at(offset + 2U),
        result.rgba.at(offset + 3U)};
}

bool RgbAndSingleChannelModesAreDeterministic()
{
    const slicer_core::RgbwsvLayerBuffer buffer = MakeBuffer();
    slicer_core::MaterialPreviewRequest request;
    request.mode = slicer_core::MaterialPreviewMode::Rgb;
    const auto rgb =
        slicer_core::MaterialPreviewComposer::Compose(buffer, request);

    request.mode = slicer_core::MaterialPreviewMode::White;
    request.palette.white = {9U, 19U, 29U, 255U};
    const auto white =
        slicer_core::MaterialPreviewComposer::Compose(buffer, request);

    request.mode = slicer_core::MaterialPreviewMode::Support;
    request.palette.support = {39U, 49U, 59U, 255U};
    const auto support =
        slicer_core::MaterialPreviewComposer::Compose(buffer, request);

    request.mode = slicer_core::MaterialPreviewMode::Varnish;
    request.palette.varnish = {69U, 79U, 89U, 255U};
    const auto varnish =
        slicer_core::MaterialPreviewComposer::Compose(buffer, request);

    return ExpectTrue(
               DisplayPixel(rgb, 1U, 0U)
                   == std::array<std::uint8_t, 4>{20U, 40U, 60U, 255U},
               "RGB mode preserves production RGB values")
        && ExpectTrue(
            DisplayPixel(rgb, 0U, 0U)
                == std::array<std::uint8_t, 4>{255U, 255U, 255U, 255U},
            "RGB mode keeps true empty white")
        && ExpectTrue(
            DisplayPixel(white, 2U, 0U)
                == std::array<std::uint8_t, 4>{9U, 19U, 29U, 255U},
            "W mode uses configured pseudo color")
        && ExpectTrue(
            DisplayPixel(support, 0U, 1U)
                == std::array<std::uint8_t, 4>{39U, 49U, 59U, 255U},
            "S mode uses configured pseudo color")
        && ExpectTrue(
            DisplayPixel(varnish, 1U, 1U)
                == std::array<std::uint8_t, 4>{69U, 79U, 89U, 255U},
            "V mode uses configured pseudo color");
}

bool AllMaterialModeUsesFixedOverlayOrder()
{
    const slicer_core::RgbwsvLayerBuffer buffer = MakeBuffer();
    slicer_core::MaterialPreviewRequest request;
    request.mode = slicer_core::MaterialPreviewMode::RgbSupportWhiteVarnish;
    request.palette.white = {10U, 20U, 30U, 255U};
    request.palette.support = {40U, 50U, 60U, 255U};
    request.palette.varnish = {70U, 80U, 90U, 255U};
    const auto result =
        slicer_core::MaterialPreviewComposer::Compose(buffer, request);

    return ExpectTrue(
               DisplayPixel(result, 2U, 1U)
                   == std::array<std::uint8_t, 4>{70U, 80U, 90U, 255U},
               "V is rendered after RGB, W, and S")
        && ExpectTrue(
            result.stats.multiMaterialPixels == 1U,
            "multi-material production pixel is counted")
        && ExpectTrue(
            result.stats.rgbPixels == 2U
                && result.stats.whitePixels == 2U
                && result.stats.supportPixels == 2U
                && result.stats.varnishPixels == 2U,
            "production channel statistics are independent of display order");
}

bool RemainingChannelAndCombinationModesAreCovered()
{
    const slicer_core::RgbwsvLayerBuffer buffer = MakeBuffer();
    slicer_core::MaterialPreviewRequest request;

    request.mode = slicer_core::MaterialPreviewMode::Red;
    const auto red =
        slicer_core::MaterialPreviewComposer::Compose(buffer, request);
    request.mode = slicer_core::MaterialPreviewMode::Green;
    const auto green =
        slicer_core::MaterialPreviewComposer::Compose(buffer, request);
    request.mode = slicer_core::MaterialPreviewMode::Blue;
    const auto blue =
        slicer_core::MaterialPreviewComposer::Compose(buffer, request);

    request.palette.white = {11U, 21U, 31U, 255U};
    request.palette.support = {41U, 51U, 61U, 255U};
    request.palette.varnish = {71U, 81U, 91U, 255U};
    request.mode = slicer_core::MaterialPreviewMode::RgbWhite;
    const auto rgbWhite =
        slicer_core::MaterialPreviewComposer::Compose(buffer, request);
    request.mode = slicer_core::MaterialPreviewMode::RgbSupport;
    const auto rgbSupport =
        slicer_core::MaterialPreviewComposer::Compose(buffer, request);
    request.mode = slicer_core::MaterialPreviewMode::RgbVarnish;
    const auto rgbVarnish =
        slicer_core::MaterialPreviewComposer::Compose(buffer, request);

    request.mode = slicer_core::MaterialPreviewMode::Empty;
    const auto empty =
        slicer_core::MaterialPreviewComposer::Compose(buffer, request);

    return ExpectTrue(
               DisplayPixel(red, 1U, 0U)
                   == std::array<std::uint8_t, 4>{255U, 20U, 20U, 255U},
               "R mode maps black-is-print coverage to red")
        && ExpectTrue(
            DisplayPixel(green, 1U, 0U)
                == std::array<std::uint8_t, 4>{40U, 255U, 40U, 255U},
            "G mode maps black-is-print coverage to green")
        && ExpectTrue(
            DisplayPixel(blue, 1U, 0U)
                == std::array<std::uint8_t, 4>{60U, 60U, 255U, 255U},
            "B mode maps black-is-print coverage to blue")
        && ExpectTrue(
            DisplayPixel(rgbWhite, 2U, 1U)
                == std::array<std::uint8_t, 4>{11U, 21U, 31U, 255U},
            "RGB+W overlays W")
        && ExpectTrue(
            DisplayPixel(rgbSupport, 2U, 1U)
                == std::array<std::uint8_t, 4>{41U, 51U, 61U, 255U},
            "RGB+S overlays S")
        && ExpectTrue(
            DisplayPixel(rgbVarnish, 2U, 1U)
                == std::array<std::uint8_t, 4>{71U, 81U, 91U, 255U},
            "RGB+V overlays V")
        && ExpectTrue(
            DisplayPixel(empty, 0U, 0U)
                == std::array<std::uint8_t, 4>{255U, 255U, 255U, 255U},
            "Empty mode displays true empty with the empty color")
        && ExpectTrue(
            DisplayPixel(empty, 1U, 0U)
                == std::array<std::uint8_t, 4>{80U, 80U, 80U, 255U},
            "Empty mode distinguishes occupied pixels");
}

bool PartialCoverageBlendsAgainstEmpty()
{
    slicer_core::RgbwsvLayerBuffer buffer = MakeBuffer();
    buffer.pixels.at(ChannelOffset(2U, 0U, 3U)) = 127U;
    slicer_core::MaterialPreviewRequest request;
    request.mode = slicer_core::MaterialPreviewMode::White;
    request.palette.white = {0U, 0U, 0U, 255U};
    const auto result =
        slicer_core::MaterialPreviewComposer::Compose(buffer, request);

    return ExpectTrue(
        DisplayPixel(result, 2U, 0U)
            == std::array<std::uint8_t, 4>{127U, 127U, 127U, 255U},
        "partial black-is-print value produces deterministic coverage");
}

bool MetadataStatsAndProbeRemainAuthoritative()
{
    const slicer_core::RgbwsvLayerBuffer buffer = MakeBuffer();
    slicer_core::MaterialPreviewRequest request;
    request.mode = slicer_core::MaterialPreviewMode::Occupancy;
    const auto result =
        slicer_core::MaterialPreviewComposer::Compose(buffer, request);
    const auto probe =
        slicer_core::MaterialPreviewComposer::Probe(buffer, 2U, 1U);

    return ExpectTrue(result.sourceIdentity == buffer.sourceIdentity, "source identity is preserved")
        && ExpectTrue(result.layerIndex == 17 && result.zMm == 0.175, "layer identity is preserved")
        && ExpectTrue(result.dpiX == 635 && result.dpiY == 600, "independent DPI is preserved")
        && ExpectTrue(result.stats.pixelCount == 6U, "pixel count is reported")
        && ExpectTrue(result.stats.occupiedPixels == 5U, "occupied pixels are counted")
        && ExpectTrue(result.stats.emptyPixels == 1U, "true empty pixels are counted")
        && ExpectTrue(
            probe.values
                == std::array<std::uint8_t, 6>{10U, 20U, 30U, 0U, 0U, 0U},
            "probe exposes exact RGBWSV production values")
        && ExpectTrue(
            probe.hasRgb && probe.hasWhite && probe.hasSupport
                && probe.hasVarnish && probe.multipleMaterials
                && !probe.isEmpty,
            "probe exposes every printed material");
}

bool InvalidBufferAndProbeFailClosed()
{
    slicer_core::RgbwsvLayerBuffer invalid = MakeBuffer();
    invalid.pixels.pop_back();
    bool invalidBufferRejected{false};
    try
    {
        static_cast<void>(
            slicer_core::MaterialPreviewComposer::Compose(
                invalid,
                slicer_core::MaterialPreviewRequest{}));
    }
    catch (const slicer_core::MaterialPreviewError& error)
    {
        invalidBufferRejected =
            error.Code()
            == slicer_core::MaterialPreviewErrorCode::BufferInvalid;
    }

    bool invalidProbeRejected{false};
    try
    {
        static_cast<void>(
            slicer_core::MaterialPreviewComposer::Probe(
                MakeBuffer(),
                kWidth,
                0U));
    }
    catch (const slicer_core::MaterialPreviewError& error)
    {
        invalidProbeRejected =
            error.Code()
            == slicer_core::MaterialPreviewErrorCode::PixelOutOfRange;
    }

    bool invalidModeRejected{false};
    try
    {
        slicer_core::MaterialPreviewRequest request;
        request.mode =
            static_cast<slicer_core::MaterialPreviewMode>(999);
        static_cast<void>(
            slicer_core::MaterialPreviewComposer::Compose(
                MakeBuffer(),
                request));
    }
    catch (const slicer_core::MaterialPreviewError& error)
    {
        invalidModeRejected =
            error.Code()
            == slicer_core::MaterialPreviewErrorCode::ModeInvalid;
    }

    return ExpectTrue(invalidBufferRejected, "invalid byte count is rejected")
        && ExpectTrue(invalidProbeRejected, "out-of-range probe is rejected")
        && ExpectTrue(invalidModeRejected, "unsupported mode is rejected");
}

}  // namespace

int main()
{
    const bool ok = RgbAndSingleChannelModesAreDeterministic()
        && AllMaterialModeUsesFixedOverlayOrder()
        && RemainingChannelAndCombinationModesAreCovered()
        && PartialCoverageBlendsAgainstEmpty()
        && MetadataStatsAndProbeRemainAuthoritative()
        && InvalidBufferAndProbeFailClosed();
    if (!ok)
    {
        return 1;
    }

    std::cout << "material_preview_composer_unit_tests: PASS\n";
    return 0;
}
