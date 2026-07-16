#include "slicer_core/diagnostics/MaterialClosureSemanticDetector.h"
#include "slicer_core/material/MaterialClosureRepair.h"

#include <array>
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

bool InputsEqual(
    const slicer_core::MaterialClosureSemanticLayerInput& first,
    const slicer_core::MaterialClosureSemanticLayerInput& second)
{
    return first.layerIndex == second.layerIndex
        && first.zMm == second.zMm
        && first.widthPx == second.widthPx
        && first.heightPx == second.heightPx
        && first.textureSurfaceMask == second.textureSurfaceMask
        && first.modelFillMask == second.modelFillMask
        && first.modelMaterialMask == second.modelMaterialMask
        && first.supportFillMask == second.supportFillMask
        && first.internalVoidSupportMask == second.internalVoidSupportMask
        && first.surfaceVarnishMask == second.surfaceVarnishMask
        && first.outerVarnishShellMask == second.outerVarnishShellMask
        && first.modelEnvelopeMask == second.modelEnvelopeMask
        && first.supportRequiredMask == second.supportRequiredMask
        && first.expectedOccupiedDomainMask == second.expectedOccupiedDomainMask
        && first.layerEmptyMask == second.layerEmptyMask;
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

bool RepairDisabledDetectionPreservesSemanticEvidence()
{
    slicer_core::MaterialClosureSemanticLayerInput input = MakeInput();
    MarkGapPixel(input);
    input.textureSurfaceMask.at(PixelIndex(1, 2)) = 1U;
    input.modelFillMask.at(PixelIndex(3, 2)) = 1U;
    const slicer_core::MaterialClosureSemanticLayerInput snapshot = input;

    const slicer_core::MaterialClosureSemanticLayerResult result =
        slicer_core::DetectMaterialClosureSemanticLayer(input, 8, 1);

    return ExpectTrue(result.gapPixels == 1, "repair-disabled gap remains visible")
        && ExpectTrue(result.colorFillGapPixels == 1, "repair-disabled gap type remains visible")
        && ExpectTrue(InputsEqual(input, snapshot), "detector preserves all semantic evidence");
}

std::vector<std::uint8_t> MakeEmptyRgbwsvLayer()
{
    return std::vector<std::uint8_t>(
        static_cast<std::size_t>(widthPx * heightPx * 6),
        255U);
}

bool RepairsOnePixelColorFillGapAsWhite()
{
    slicer_core::MaterialClosureSemanticLayerInput input = MakeInput();
    MarkGapPixel(input);
    input.textureSurfaceMask.at(PixelIndex(1, 2)) = 1U;
    input.modelMaterialMask.at(PixelIndex(1, 2)) = 1U;
    input.modelFillMask.at(PixelIndex(3, 2)) = 1U;
    input.modelMaterialMask.at(PixelIndex(3, 2)) = 1U;
    input.modelEnvelopeMask.at(PixelIndex(2, 2)) = 1U;

    const slicer_core::MaterialClosureSemanticLayerAnalysis analysis =
        slicer_core::AnalyzeMaterialClosureSemanticLayer(input, 8, 1);
    const slicer_core::MaterialClosureRepairPlan plan =
        slicer_core::BuildMaterialClosureRepairPlan(input, analysis, 8);
    slicer_core::MaterialClosureRepairValues values;
    values.modelFillMaterial = slicer_core::MaterialClosureModelFillMaterial::White;
    values.modelFillValue = 0U;
    values.supportValue = 0U;
    std::vector<std::uint8_t> layer = MakeEmptyRgbwsvLayer();

    const slicer_core::MaterialClosureRepairApplicationResult result =
        slicer_core::ApplyMaterialClosureRepair(plan, values, layer, input);
    const std::size_t base = PixelIndex(2, 2) * 6U;
    return ExpectTrue(plan.modelFillRepairPixels == 1, "color-fill plan targets model fill")
        && ExpectTrue(plan.supportRepairPixels == 0, "color-fill plan does not target support")
        && ExpectTrue(result.repairedPixels == 1, "one color-fill pixel repaired")
        && ExpectTrue(result.repairedColorFillPixels == 1, "color-fill repair classified")
        && ExpectTrue(layer.at(base + 3U) == 0U, "white channel receives model fill")
        && ExpectTrue(layer.at(base + 4U) == 255U, "support channel remains empty")
        && ExpectTrue(input.modelFillMask.at(PixelIndex(2, 2)) == 1U, "model fill semantic mask updated")
        && ExpectTrue(input.layerEmptyMask.at(PixelIndex(2, 2)) == 0U, "repaired pixel is no longer empty");
}

bool RepairsModelSupportGapByEnvelopeContext()
{
    slicer_core::MaterialClosureSemanticLayerInput input = MakeInput();
    MarkGapPixel(input);
    input.modelMaterialMask.at(PixelIndex(1, 2)) = 1U;
    input.supportFillMask.at(PixelIndex(3, 2)) = 1U;
    input.modelEnvelopeMask.at(PixelIndex(2, 2)) = 1U;

    const slicer_core::MaterialClosureSemanticLayerAnalysis analysis =
        slicer_core::AnalyzeMaterialClosureSemanticLayer(input, 8, 1);
    const slicer_core::MaterialClosureRepairPlan plan =
        slicer_core::BuildMaterialClosureRepairPlan(input, analysis, 8);
    slicer_core::MaterialClosureRepairValues values;
    values.modelFillMaterial = slicer_core::MaterialClosureModelFillMaterial::Varnish;
    values.modelFillValue = 0U;
    std::vector<std::uint8_t> layer = MakeEmptyRgbwsvLayer();

    const slicer_core::MaterialClosureRepairApplicationResult result =
        slicer_core::ApplyMaterialClosureRepair(plan, values, layer, input);
    const std::size_t base = PixelIndex(2, 2) * 6U;
    return ExpectTrue(result.repairedModelSupportPixels == 1, "model-support repair classified")
        && ExpectTrue(layer.at(base + 5U) == 0U, "inside-envelope model-support gap uses model fill")
        && ExpectTrue(layer.at(base + 4U) == 255U, "inside-envelope gap does not use support");
}

bool RepairsInternalVoidGapAsSupport()
{
    slicer_core::MaterialClosureSemanticLayerInput input = MakeInput();
    MarkGapPixel(input);
    input.modelEnvelopeMask.at(PixelIndex(2, 2)) = 1U;

    const slicer_core::MaterialClosureSemanticLayerAnalysis analysis =
        slicer_core::AnalyzeMaterialClosureSemanticLayer(input, 8, 1);
    const slicer_core::MaterialClosureRepairPlan plan =
        slicer_core::BuildMaterialClosureRepairPlan(input, analysis, 8);
    slicer_core::MaterialClosureRepairValues values;
    values.supportValue = 0U;
    std::vector<std::uint8_t> layer = MakeEmptyRgbwsvLayer();

    const slicer_core::MaterialClosureRepairApplicationResult result =
        slicer_core::ApplyMaterialClosureRepair(plan, values, layer, input);
    const std::size_t base = PixelIndex(2, 2) * 6U;
    return ExpectTrue(plan.supportRepairPixels == 1, "internal void plan targets support")
        && ExpectTrue(result.repairedInternalVoidPixels == 1, "internal void repair classified")
        && ExpectTrue(layer.at(base + 4U) == 0U, "support channel receives internal void repair")
        && ExpectTrue(input.supportFillMask.at(PixelIndex(2, 2)) == 1U, "support semantic mask updated")
        && ExpectTrue(input.internalVoidSupportMask.at(PixelIndex(2, 2)) == 1U, "internal void semantic mask updated");
}

bool RejectsTwoPixelWideInternalVoid()
{
    slicer_core::MaterialClosureSemanticLayerInput input = MakeInput();
    const std::array<std::size_t, 4> gapIndices{
        PixelIndex(2, 2),
        PixelIndex(3, 2),
        PixelIndex(2, 3),
        PixelIndex(3, 3),
    };
    for (const std::size_t index : gapIndices)
    {
        input.expectedOccupiedDomainMask.at(index) = 1U;
        input.modelEnvelopeMask.at(index) = 1U;
        input.layerEmptyMask.at(index) = 1U;
    }

    const slicer_core::MaterialClosureSemanticLayerAnalysis analysis =
        slicer_core::AnalyzeMaterialClosureSemanticLayer(input, 8, 1);
    const slicer_core::MaterialClosureRepairPlan plan =
        slicer_core::BuildMaterialClosureRepairPlan(input, analysis, 8);
    slicer_core::MaterialClosureRepairValues values;
    std::vector<std::uint8_t> layer = MakeEmptyRgbwsvLayer();
    const std::vector<std::uint8_t> snapshot = layer;

    const slicer_core::MaterialClosureRepairApplicationResult result =
        slicer_core::ApplyMaterialClosureRepair(plan, values, layer, input);
    return ExpectTrue(plan.rejectedTooWidePixels == 4, "two-pixel-wide component rejected")
        && ExpectTrue(plan.supportRepairPixels == 0, "wide internal void not planned for support")
        && ExpectTrue(result.repairedPixels == 0, "wide internal void not repaired")
        && ExpectTrue(layer == snapshot, "wide gap leaves RGBWSV bytes unchanged");
}

bool RejectsTwoPixelColorFillThickness()
{
    slicer_core::MaterialClosureSemanticLayerInput input = MakeInput();
    const std::array<std::size_t, 2> gapIndices{
        PixelIndex(2, 2),
        PixelIndex(3, 2),
    };
    for (const std::size_t index : gapIndices)
    {
        input.expectedOccupiedDomainMask.at(index) = 1U;
        input.modelEnvelopeMask.at(index) = 1U;
        input.layerEmptyMask.at(index) = 1U;
    }
    input.textureSurfaceMask.at(PixelIndex(1, 2)) = 1U;
    input.modelMaterialMask.at(PixelIndex(1, 2)) = 1U;
    input.modelFillMask.at(PixelIndex(4, 2)) = 1U;
    input.modelMaterialMask.at(PixelIndex(4, 2)) = 1U;

    const slicer_core::MaterialClosureSemanticLayerAnalysis analysis =
        slicer_core::AnalyzeMaterialClosureSemanticLayer(input, 8, 1);
    const slicer_core::MaterialClosureRepairPlan plan =
        slicer_core::BuildMaterialClosureRepairPlan(input, analysis, 8);
    return ExpectTrue(plan.rejectedTooWidePixels == 2, "two-pixel color-fill thickness rejected")
        && ExpectTrue(plan.modelFillRepairPixels == 0, "two-pixel color-fill gap not repaired");
}

bool LeavesColorSupportOnlyGapUnrepaired()
{
    slicer_core::MaterialClosureSemanticLayerInput input = MakeInput();
    MarkGapPixel(input);
    input.textureSurfaceMask.at(PixelIndex(1, 2)) = 1U;
    input.modelMaterialMask.at(PixelIndex(1, 2)) = 1U;
    input.supportFillMask.at(PixelIndex(3, 2)) = 1U;
    input.modelEnvelopeMask.at(PixelIndex(2, 2)) = 1U;
    input.modelMaterialMask.at(PixelIndex(1, 2)) = 0U;

    const slicer_core::MaterialClosureSemanticLayerAnalysis analysis =
        slicer_core::AnalyzeMaterialClosureSemanticLayer(input, 8, 1);
    const slicer_core::MaterialClosureRepairPlan plan =
        slicer_core::BuildMaterialClosureRepairPlan(input, analysis, 8);
    return ExpectTrue(analysis.summary.colorSupportGapPixels == 1, "color-support-only gap detected")
        && ExpectTrue(analysis.summary.modelSupportGapPixels == 0, "fixture excludes model-support classification")
        && ExpectTrue(plan.modelFillRepairPixels == 0, "color-support-only gap not filled as model")
        && ExpectTrue(plan.supportRepairPixels == 0, "color-support-only gap not filled as support")
        && ExpectTrue(plan.rejectedTooWidePixels == 0, "unsupported color-support is not mislabeled too wide");
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
        {"repair_disabled_detection_preserves_semantic_evidence", RepairDisabledDetectionPreservesSemanticEvidence},
        {"repairs_one_pixel_color_fill_gap_as_white", RepairsOnePixelColorFillGapAsWhite},
        {"repairs_model_support_gap_by_envelope_context", RepairsModelSupportGapByEnvelopeContext},
        {"repairs_internal_void_gap_as_support", RepairsInternalVoidGapAsSupport},
        {"rejects_two_pixel_wide_internal_void", RejectsTwoPixelWideInternalVoid},
        {"rejects_two_pixel_color_fill_thickness", RejectsTwoPixelColorFillThickness},
        {"leaves_color_support_only_gap_unrepaired", LeavesColorSupportOnlyGapUnrepaired},
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
