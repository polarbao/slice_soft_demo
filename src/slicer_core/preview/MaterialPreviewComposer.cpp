#include "slicer_core/preview/MaterialPreviewComposer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <utility>

namespace slicer_core
{
namespace
{

constexpr std::size_t kR{0U};
constexpr std::size_t kG{1U};
constexpr std::size_t kB{2U};
constexpr std::size_t kW{3U};
constexpr std::size_t kS{4U};
constexpr std::size_t kV{5U};
constexpr std::size_t kDisplayChannels{4U};

struct PixelState
{
    std::array<std::uint8_t, rgbwsv_channel_count> values{};
    bool hasRgb{false};
    bool hasWhite{false};
    bool hasSupport{false};
    bool hasVarnish{false};
    bool isEmpty{true};
    bool multipleMaterials{false};
};

[[noreturn]] void Fail(
    const MaterialPreviewErrorCode code,
    const std::string& message)
{
    throw MaterialPreviewError(code, message);
}

std::size_t ValidateBuffer(const RgbwsvLayerBuffer& buffer)
{
    if (buffer.width == 0U || buffer.height == 0U)
    {
        Fail(
            MaterialPreviewErrorCode::DimensionInvalid,
            "RGBWSV layer dimensions must be positive");
    }

    const std::size_t pixelCount =
        static_cast<std::size_t>(buffer.width)
        * static_cast<std::size_t>(buffer.height);
    if (pixelCount
        > std::numeric_limits<std::size_t>::max()
            / static_cast<std::size_t>(rgbwsv_channel_count))
    {
        Fail(
            MaterialPreviewErrorCode::DimensionInvalid,
            "RGBWSV layer dimensions overflow the byte count");
    }
    const std::size_t expectedBytes =
        pixelCount
        * static_cast<std::size_t>(rgbwsv_channel_count);
    if (buffer.pixels.size() != expectedBytes)
    {
        Fail(
            MaterialPreviewErrorCode::BufferInvalid,
            "RGBWSV layer byte count does not match dimensions");
    }
    return pixelCount;
}

PixelState ReadPixel(
    const RgbwsvLayerBuffer& buffer,
    const std::size_t pixelIndex)
{
    PixelState state;
    const std::size_t offset =
        pixelIndex
        * static_cast<std::size_t>(rgbwsv_channel_count);
    std::copy_n(
        buffer.pixels.begin()
            + static_cast<std::ptrdiff_t>(offset),
        rgbwsv_channel_count,
        state.values.begin());
    state.hasRgb = state.values.at(kR) < 255U
        || state.values.at(kG) < 255U
        || state.values.at(kB) < 255U;
    state.hasWhite = state.values.at(kW) < 255U;
    state.hasSupport = state.values.at(kS) < 255U;
    state.hasVarnish = state.values.at(kV) < 255U;
    state.isEmpty = !state.hasRgb
        && !state.hasWhite
        && !state.hasSupport
        && !state.hasVarnish;
    const int materialCount =
        (state.hasRgb ? 1 : 0)
        + (state.hasWhite ? 1 : 0)
        + (state.hasSupport ? 1 : 0)
        + (state.hasVarnish ? 1 : 0);
    state.multipleMaterials = materialCount > 1;
    return state;
}

std::uint8_t EffectiveAlpha(
    const PreviewColor& color,
    const std::uint8_t channelValue)
{
    const unsigned int coverage =
        255U - static_cast<unsigned int>(channelValue);
    return static_cast<std::uint8_t>(
        (static_cast<unsigned int>(color.alpha) * coverage + 127U)
        / 255U);
}

PreviewColor Blend(
    const PreviewColor& destination,
    const PreviewColor& source,
    const std::uint8_t effectiveAlpha)
{
    if (effectiveAlpha == 0U)
    {
        return destination;
    }
    if (effectiveAlpha == 255U)
    {
        return PreviewColor{
            source.red,
            source.green,
            source.blue,
            255U};
    }

    const unsigned int sourceAlpha = effectiveAlpha;
    const unsigned int destinationAlpha = destination.alpha;
    const unsigned int inverseSourceAlpha = 255U - sourceAlpha;
    const unsigned int outputAlpha =
        sourceAlpha
        + (destinationAlpha * inverseSourceAlpha + 127U) / 255U;
    if (outputAlpha == 0U)
    {
        return PreviewColor{};
    }

    const auto BlendChannel =
        [&](const std::uint8_t destinationValue,
            const std::uint8_t sourceValue)
    {
        const unsigned int sourcePremultiplied =
            static_cast<unsigned int>(sourceValue) * sourceAlpha;
        const unsigned int destinationPremultiplied =
            (static_cast<unsigned int>(destinationValue)
             * destinationAlpha * inverseSourceAlpha
             + 127U)
            / 255U;
        return static_cast<std::uint8_t>(
            (sourcePremultiplied + destinationPremultiplied
             + outputAlpha / 2U)
            / outputAlpha);
    };

    return PreviewColor{
        BlendChannel(destination.red, source.red),
        BlendChannel(destination.green, source.green),
        BlendChannel(destination.blue, source.blue),
        static_cast<std::uint8_t>(outputAlpha)};
}

void ApplyChannel(
    PreviewColor& destination,
    const PreviewColor& pseudoColor,
    const std::uint8_t channelValue)
{
    destination = Blend(
        destination,
        pseudoColor,
        EffectiveAlpha(pseudoColor, channelValue));
}

void ApplyRgb(
    PreviewColor& destination,
    const PixelState& state)
{
    if (!state.hasRgb)
    {
        return;
    }
    destination = PreviewColor{
        state.values.at(kR),
        state.values.at(kG),
        state.values.at(kB),
        255U};
}

PreviewColor ComposePixel(
    const PixelState& state,
    const MaterialPreviewRequest& request)
{
    PreviewColor display = request.palette.empty;
    switch (request.mode)
    {
    case MaterialPreviewMode::Red:
        ApplyChannel(
            display,
            request.palette.red,
            state.values.at(kR));
        break;
    case MaterialPreviewMode::Green:
        ApplyChannel(
            display,
            request.palette.green,
            state.values.at(kG));
        break;
    case MaterialPreviewMode::Blue:
        ApplyChannel(
            display,
            request.palette.blue,
            state.values.at(kB));
        break;
    case MaterialPreviewMode::White:
        ApplyChannel(
            display,
            request.palette.white,
            state.values.at(kW));
        break;
    case MaterialPreviewMode::Support:
        ApplyChannel(
            display,
            request.palette.support,
            state.values.at(kS));
        break;
    case MaterialPreviewMode::Varnish:
        ApplyChannel(
            display,
            request.palette.varnish,
            state.values.at(kV));
        break;
    case MaterialPreviewMode::Rgb:
        ApplyRgb(display, state);
        break;
    case MaterialPreviewMode::RgbWhite:
        ApplyRgb(display, state);
        ApplyChannel(
            display,
            request.palette.white,
            state.values.at(kW));
        break;
    case MaterialPreviewMode::RgbSupport:
        ApplyRgb(display, state);
        ApplyChannel(
            display,
            request.palette.support,
            state.values.at(kS));
        break;
    case MaterialPreviewMode::RgbVarnish:
        ApplyRgb(display, state);
        ApplyChannel(
            display,
            request.palette.varnish,
            state.values.at(kV));
        break;
    case MaterialPreviewMode::RgbSupportWhiteVarnish:
        ApplyRgb(display, state);
        ApplyChannel(
            display,
            request.palette.white,
            state.values.at(kW));
        ApplyChannel(
            display,
            request.palette.support,
            state.values.at(kS));
        ApplyChannel(
            display,
            request.palette.varnish,
            state.values.at(kV));
        break;
    case MaterialPreviewMode::Occupancy:
    case MaterialPreviewMode::Empty:
        display = state.isEmpty
            ? request.palette.empty
            : request.palette.occupancy;
        break;
    default:
        Fail(
            MaterialPreviewErrorCode::ModeInvalid,
            "material preview mode is unsupported");
    }
    return display;
}

void AccumulateStats(
    MaterialPreviewStats& stats,
    const PixelState& state)
{
    ++stats.pixelCount;
    if (state.hasRgb)
    {
        ++stats.rgbPixels;
    }
    if (state.hasWhite)
    {
        ++stats.whitePixels;
    }
    if (state.hasSupport)
    {
        ++stats.supportPixels;
    }
    if (state.hasVarnish)
    {
        ++stats.varnishPixels;
    }
    if (state.isEmpty)
    {
        ++stats.emptyPixels;
    }
    else
    {
        ++stats.occupiedPixels;
    }
    if (state.multipleMaterials)
    {
        ++stats.multiMaterialPixels;
    }
}

void AppendColor(
    std::vector<std::uint8_t>& rgba,
    const PreviewColor& color)
{
    rgba.push_back(color.red);
    rgba.push_back(color.green);
    rgba.push_back(color.blue);
    rgba.push_back(color.alpha);
}

}  // namespace

std::string MaterialPreviewErrorCodeString(
    const MaterialPreviewErrorCode code)
{
    switch (code)
    {
    case MaterialPreviewErrorCode::BufferInvalid:
        return "MATERIAL_PREVIEW_BUFFER_INVALID";
    case MaterialPreviewErrorCode::DimensionInvalid:
        return "MATERIAL_PREVIEW_DIMENSION_INVALID";
    case MaterialPreviewErrorCode::PixelOutOfRange:
        return "MATERIAL_PREVIEW_PIXEL_OUT_OF_RANGE";
    case MaterialPreviewErrorCode::ModeInvalid:
        return "MATERIAL_PREVIEW_MODE_INVALID";
    }
    return "MATERIAL_PREVIEW_BUFFER_INVALID";
}

MaterialPreviewError::MaterialPreviewError(
    const MaterialPreviewErrorCode code,
    std::string message)
    : std::runtime_error(
          MaterialPreviewErrorCodeString(code) + ": "
          + std::move(message)),
      m_code(code)
{
}

MaterialPreviewErrorCode MaterialPreviewError::Code() const noexcept
{
    return m_code;
}

MaterialPreviewResult MaterialPreviewComposer::Compose(
    const RgbwsvLayerBuffer& buffer,
    const MaterialPreviewRequest& request)
{
    const std::size_t pixelCount = ValidateBuffer(buffer);
    if (pixelCount
        > std::numeric_limits<std::size_t>::max()
            / kDisplayChannels)
    {
        Fail(
            MaterialPreviewErrorCode::DimensionInvalid,
            "material preview RGBA byte count overflows");
    }

    MaterialPreviewResult result;
    result.sourceIdentity = buffer.sourceIdentity;
    result.layerIndex = buffer.layerIndex;
    result.zMm = buffer.zMm;
    result.width = buffer.width;
    result.height = buffer.height;
    result.dpiX = buffer.dpiX;
    result.dpiY = buffer.dpiY;
    result.rgba.reserve(pixelCount * kDisplayChannels);
    for (std::size_t pixelIndex{0U};
         pixelIndex < pixelCount;
         ++pixelIndex)
    {
        const PixelState state = ReadPixel(buffer, pixelIndex);
        AccumulateStats(result.stats, state);
        AppendColor(result.rgba, ComposePixel(state, request));
    }
    return result;
}

MaterialPixelProbe MaterialPreviewComposer::Probe(
    const RgbwsvLayerBuffer& buffer,
    const std::uint32_t x,
    const std::uint32_t y)
{
    static_cast<void>(ValidateBuffer(buffer));
    if (x >= buffer.width || y >= buffer.height)
    {
        Fail(
            MaterialPreviewErrorCode::PixelOutOfRange,
            "material preview probe coordinate is outside the TIFF layer");
    }
    const std::size_t pixelIndex =
        static_cast<std::size_t>(y)
            * static_cast<std::size_t>(buffer.width)
        + static_cast<std::size_t>(x);
    const PixelState state = ReadPixel(buffer, pixelIndex);

    MaterialPixelProbe probe;
    probe.x = x;
    probe.y = y;
    probe.values = state.values;
    probe.hasRgb = state.hasRgb;
    probe.hasWhite = state.hasWhite;
    probe.hasSupport = state.hasSupport;
    probe.hasVarnish = state.hasVarnish;
    probe.isEmpty = state.isEmpty;
    probe.multipleMaterials = state.multipleMaterials;
    return probe;
}

}  // namespace slicer_core
