#include "slicer_core/diagnostics/MaterialClosureCandidateDetector.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

constexpr int widthPx{5};
constexpr int heightPx{3};
constexpr std::size_t channelCount{6U};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

std::vector<std::uint8_t> EmptyLayer()
{
    return std::vector<std::uint8_t>(
        static_cast<std::size_t>(widthPx * heightPx) * channelCount,
        255U);
}

void SetChannel(
    std::vector<std::uint8_t>& pixels,
    const int x,
    const int y,
    const std::size_t channel,
    const std::uint8_t value = 0U)
{
    const std::size_t pixelIndex = static_cast<std::size_t>(y * widthPx + x);
    pixels.at(pixelIndex * channelCount + channel) = value;
}

bool DetectsColorFillGap()
{
    std::vector<std::uint8_t> pixels = EmptyLayer();
    SetChannel(pixels, 1, 1, 0U);
    SetChannel(pixels, 3, 1, 3U);

    const slicer_core::MaterialClosureCandidateLayer result =
        slicer_core::DetectMaterialClosureCandidateLayer(
            pixels,
            widthPx,
            heightPx,
            7,
            0.07,
            8,
            1);

    return ExpectTrue(result.layerIndex == 7, "layer index retained")
        && ExpectTrue(result.colorFillGapPixels == 1, "color-fill gap detected")
        && ExpectTrue(result.modelSupportGapPixels == 0, "no model-support gap")
        && ExpectTrue(result.colorSupportGapPixels == 0, "no color-support gap")
        && ExpectTrue(result.gapPixels == 1, "gap union is de-duplicated")
        && ExpectTrue(result.externalBackgroundProtectedPixels > 0, "external background counted");
}

bool DetectsModelSupportGap()
{
    std::vector<std::uint8_t> pixels = EmptyLayer();
    SetChannel(pixels, 1, 1, 3U);
    SetChannel(pixels, 3, 1, 4U);

    const slicer_core::MaterialClosureCandidateLayer result =
        slicer_core::DetectMaterialClosureCandidateLayer(
            pixels,
            widthPx,
            heightPx,
            8,
            0.08,
            4,
            1);

    return ExpectTrue(result.colorFillGapPixels == 0, "no color-fill gap")
        && ExpectTrue(result.modelSupportGapPixels == 1, "model-support gap detected")
        && ExpectTrue(result.colorSupportGapPixels == 0, "white is not color")
        && ExpectTrue(result.gapPixels == 1, "model-support union count");
}

bool DetectsColorSupportGapWithoutDoubleCountingUnion()
{
    std::vector<std::uint8_t> pixels = EmptyLayer();
    SetChannel(pixels, 1, 1, 1U, 127U);
    SetChannel(pixels, 3, 1, 4U);

    const slicer_core::MaterialClosureCandidateLayer result =
        slicer_core::DetectMaterialClosureCandidateLayer(
            pixels,
            widthPx,
            heightPx,
            9,
            0.09,
            8,
            1);

    return ExpectTrue(result.modelSupportGapPixels == 1, "color is also model material")
        && ExpectTrue(result.colorSupportGapPixels == 1, "color-support gap detected")
        && ExpectTrue(result.gapPixels == 1, "overlapping gap classes counted once");
}

bool HonorsConfiguredGapWidth()
{
    std::vector<std::uint8_t> pixels = EmptyLayer();
    SetChannel(pixels, 0, 1, 0U);
    SetChannel(pixels, 3, 1, 3U);

    const slicer_core::MaterialClosureCandidateLayer onePixel =
        slicer_core::DetectMaterialClosureCandidateLayer(
            pixels,
            widthPx,
            heightPx,
            10,
            0.10,
            4,
            1);
    const slicer_core::MaterialClosureCandidateLayer twoPixels =
        slicer_core::DetectMaterialClosureCandidateLayer(
            pixels,
            widthPx,
            heightPx,
            10,
            0.10,
            4,
            2);

    return ExpectTrue(onePixel.gapPixels == 0, "two-pixel run excluded by one-pixel limit")
        && ExpectTrue(twoPixels.colorFillGapPixels == 2, "two-pixel run detected when admitted")
        && ExpectTrue(twoPixels.gapPixels == 2, "two-pixel union count");
}

bool AdjacentMaterialsDoNotCreateGap()
{
    std::vector<std::uint8_t> pixels = EmptyLayer();
    SetChannel(pixels, 1, 1, 0U);
    SetChannel(pixels, 2, 1, 3U);

    const slicer_core::MaterialClosureCandidateLayer result =
        slicer_core::DetectMaterialClosureCandidateLayer(
            pixels,
            widthPx,
            heightPx,
            11,
            0.11,
            8,
            1);
    return ExpectTrue(result.gapPixels == 0, "directly adjacent materials have no empty gap");
}

bool RejectsInvalidBufferSize()
{
    try
    {
        (void)slicer_core::DetectMaterialClosureCandidateLayer(
            std::vector<std::uint8_t>{255U},
            widthPx,
            heightPx,
            0,
            0.0,
            8,
            1);
    }
    catch (const std::invalid_argument&)
    {
        return true;
    }
    return ExpectTrue(false, "invalid RGBWSV buffer size rejected");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"detects_color_fill_gap", DetectsColorFillGap},
        {"detects_model_support_gap", DetectsModelSupportGap},
        {"detects_color_support_gap_without_double_counting_union", DetectsColorSupportGapWithoutDoubleCountingUnion},
        {"honors_configured_gap_width", HonorsConfiguredGapWidth},
        {"adjacent_materials_do_not_create_gap", AdjacentMaterialsDoNotCreateGap},
        {"rejects_invalid_buffer_size", RejectsInvalidBufferSize},
    };

    for (const auto& test : tests)
    {
        std::cout << "RUN " << test.first << std::endl;
        try
        {
            if (!test.second())
            {
                return 1;
            }
        }
        catch (const std::exception& error)
        {
            std::cerr << "FAIL " << test.first << " exception=" << error.what() << '\n';
            return 1;
        }
        std::cout << "PASS " << test.first << '\n';
    }

    std::cout << "Material closure candidate detector unit tests complete.\n";
    return 0;
}
