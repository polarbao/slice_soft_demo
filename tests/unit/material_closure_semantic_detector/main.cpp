#include "slicer_core/diagnostics/MaterialClosureSemanticDetector.h"

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
constexpr int heightPx{5};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

std::size_t PixelIndex(const int x, const int y)
{
    return static_cast<std::size_t>(y * widthPx + x);
}

slicer_core::MaterialClosureSemanticLayerInput MakeInput()
{
    slicer_core::MaterialClosureSemanticLayerInput input;
    input.layerIndex = 17;
    input.zMm = 0.17;
    input.widthPx = widthPx;
    input.heightPx = heightPx;

    const std::size_t pixelCount = static_cast<std::size_t>(widthPx * heightPx);
    input.textureSurfaceMask.assign(pixelCount, 0U);
    input.modelFillMask.assign(pixelCount, 0U);
    input.modelMaterialMask.assign(pixelCount, 0U);
    input.supportFillMask.assign(pixelCount, 0U);
    input.internalVoidSupportMask.assign(pixelCount, 0U);
    input.surfaceVarnishMask.assign(pixelCount, 0U);
    input.outerVarnishShellMask.assign(pixelCount, 0U);
    input.modelEnvelopeMask.assign(pixelCount, 0U);
    input.supportRequiredMask.assign(pixelCount, 0U);
    input.expectedOccupiedDomainMask.assign(pixelCount, 0U);
    input.layerEmptyMask.assign(pixelCount, 0U);
    return input;
}

void MarkGapPixel(slicer_core::MaterialClosureSemanticLayerInput& input)
{
    const std::size_t center = PixelIndex(2, 2);
    input.expectedOccupiedDomainMask.at(center) = 1U;
    input.layerEmptyMask.at(center) = 1U;
}

bool ExactPassHasNoGap()
{
    const slicer_core::MaterialClosureSemanticLayerResult result =
        slicer_core::DetectMaterialClosureSemanticLayer(MakeInput(), 8, 1);

    return ExpectTrue(result.layerIndex == 17, "layer index retained")
        && ExpectTrue(result.gapPixels == 0, "pass fixture has no gap")
        && ExpectTrue(result.externalBackgroundProtectedPixels == 0, "no empty background in pass fixture");
}

bool DetectsColorFillGap()
{
    slicer_core::MaterialClosureSemanticLayerInput input = MakeInput();
    MarkGapPixel(input);
    input.textureSurfaceMask.at(PixelIndex(1, 2)) = 1U;
    input.modelMaterialMask.at(PixelIndex(1, 2)) = 1U;
    input.modelFillMask.at(PixelIndex(3, 2)) = 1U;
    input.modelMaterialMask.at(PixelIndex(3, 2)) = 1U;

    const slicer_core::MaterialClosureSemanticLayerResult result =
        slicer_core::DetectMaterialClosureSemanticLayer(input, 8, 1);
    return ExpectTrue(result.colorFillGapPixels == 1, "color-fill gap detected")
        && ExpectTrue(result.gapPixels == 1, "color-fill gap in union");
}

bool DetectsModelAndColorSupportGapWithoutDoubleCounting()
{
    slicer_core::MaterialClosureSemanticLayerInput input = MakeInput();
    MarkGapPixel(input);
    input.textureSurfaceMask.at(PixelIndex(1, 2)) = 1U;
    input.modelMaterialMask.at(PixelIndex(1, 2)) = 1U;
    input.supportFillMask.at(PixelIndex(3, 2)) = 1U;

    const slicer_core::MaterialClosureSemanticLayerResult result =
        slicer_core::DetectMaterialClosureSemanticLayer(input, 4, 1);
    return ExpectTrue(result.modelSupportGapPixels == 1, "model-support gap detected")
        && ExpectTrue(result.colorSupportGapPixels == 1, "color-support gap detected")
        && ExpectTrue(result.gapPixels == 1, "overlapping classes are de-duplicated");
}

bool DetectsInternalVoidGap()
{
    slicer_core::MaterialClosureSemanticLayerInput input = MakeInput();
    MarkGapPixel(input);
    input.modelEnvelopeMask.at(PixelIndex(2, 2)) = 1U;

    const slicer_core::MaterialClosureSemanticLayerResult result =
        slicer_core::DetectMaterialClosureSemanticLayer(input, 8, 1);
    return ExpectTrue(result.internalVoidGapPixels == 1, "internal void gap detected")
        && ExpectTrue(result.gapPixels == 1, "internal void in union");
}

bool DetectsVarnishSupportGapOnlyInsideRequiredSupport()
{
    slicer_core::MaterialClosureSemanticLayerInput input = MakeInput();
    MarkGapPixel(input);
    input.outerVarnishShellMask.at(PixelIndex(1, 2)) = 1U;
    input.supportFillMask.at(PixelIndex(3, 2)) = 1U;

    const slicer_core::MaterialClosureSemanticLayerResult withoutIntent =
        slicer_core::DetectMaterialClosureSemanticLayer(input, 8, 1);
    input.supportRequiredMask.at(PixelIndex(2, 2)) = 1U;
    const slicer_core::MaterialClosureSemanticLayerResult withIntent =
        slicer_core::DetectMaterialClosureSemanticLayer(input, 8, 1);

    return ExpectTrue(withoutIntent.varnishSupportGapPixels == 0, "varnish gap requires support intent")
        && ExpectTrue(withIntent.varnishSupportGapPixels == 1, "varnish-support gap detected in required domain");
}

bool ProtectsBorderConnectedBackground()
{
    slicer_core::MaterialClosureSemanticLayerInput input = MakeInput();
    input.layerEmptyMask.assign(static_cast<std::size_t>(widthPx * heightPx), 1U);
    input.expectedOccupiedDomainMask.at(PixelIndex(2, 2)) = 1U;
    input.modelEnvelopeMask.at(PixelIndex(2, 2)) = 1U;

    const slicer_core::MaterialClosureSemanticLayerResult result =
        slicer_core::DetectMaterialClosureSemanticLayer(input, 8, 1);
    return ExpectTrue(result.externalBackgroundProtectedPixels == widthPx * heightPx, "all border-connected empty pixels protected")
        && ExpectTrue(result.gapPixels == 0, "external background is not a gap");
}

bool RejectsMaskSizeMismatch()
{
    slicer_core::MaterialClosureSemanticLayerInput input = MakeInput();
    input.modelFillMask.pop_back();
    try
    {
        (void)slicer_core::DetectMaterialClosureSemanticLayer(input, 8, 1);
    }
    catch (const std::invalid_argument&)
    {
        return true;
    }
    return ExpectTrue(false, "mask size mismatch rejected");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"exact_pass_has_no_gap", ExactPassHasNoGap},
        {"detects_color_fill_gap", DetectsColorFillGap},
        {"detects_model_and_color_support_gap_without_double_counting", DetectsModelAndColorSupportGapWithoutDoubleCounting},
        {"detects_internal_void_gap", DetectsInternalVoidGap},
        {"detects_varnish_support_gap_only_inside_required_support", DetectsVarnishSupportGapOnlyInsideRequiredSupport},
        {"protects_border_connected_background", ProtectsBorderConnectedBackground},
        {"rejects_mask_size_mismatch", RejectsMaskSizeMismatch},
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

    std::cout << "Material closure semantic detector unit tests complete.\n";
    return 0;
}
