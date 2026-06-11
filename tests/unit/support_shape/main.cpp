#include "slicer_core/support/SupportComponentAnalysis.h"
#include "slicer_core/support/SupportShapePipeline.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{

std::size_t MaskIndex(const int width, const int x, const int y)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
}

void SetPixel(std::vector<std::uint8_t>& mask, const int width, const int x, const int y)
{
    mask.at(MaskIndex(width, x, y)) = 1;
}

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

bool ExpectEqual(const int actual, const int expected, const std::string& message)
{
    if (actual != expected)
    {
        std::cerr << "FAIL " << message << " expected=" << expected << " actual=" << actual << '\n';
        return false;
    }
    return true;
}

slicer_core::SupportShapePolicy EnabledPolicy()
{
    slicer_core::SupportShapePolicy policy;
    policy.enabled = true;
    policy.preserve_model_priority = true;
    policy.max_added_support_ratio = 10.0;
    return policy;
}

bool ComponentAnalysisSingleComponent()
{
    constexpr int width{5};
    constexpr int height{5};
    std::vector<std::uint8_t> supportMask(width * height, 0);
    SetPixel(supportMask, width, 1, 1);
    SetPixel(supportMask, width, 2, 1);
    SetPixel(supportMask, width, 2, 2);

    const slicer_core::SupportComponentAnalysis analysis =
        slicer_core::AnalyzeSupportComponents(supportMask, width, height, 4);
    return ExpectEqual(analysis.component_count, 1, "single component count")
        && ExpectEqual(analysis.largest_component_area, 3, "single component area");
}

bool ComponentAnalysisTwoComponents()
{
    constexpr int width{5};
    constexpr int height{5};
    std::vector<std::uint8_t> supportMask(width * height, 0);
    SetPixel(supportMask, width, 0, 0);
    SetPixel(supportMask, width, 4, 4);

    const slicer_core::SupportComponentAnalysis analysis =
        slicer_core::AnalyzeSupportComponents(supportMask, width, height, 4);
    return ExpectEqual(analysis.component_count, 2, "two component count")
        && ExpectEqual(analysis.largest_component_area, 1, "two component largest area");
}

bool FilterSmallComponent()
{
    constexpr int width{5};
    constexpr int height{5};
    std::vector<std::uint8_t> modelMask(width * height, 0);
    std::vector<std::uint8_t> supportMask(width * height, 0);
    SetPixel(supportMask, width, 0, 0);
    SetPixel(supportMask, width, 3, 3);
    SetPixel(supportMask, width, 4, 3);

    slicer_core::SupportShapePolicy policy = EnabledPolicy();
    policy.min_component_area_px = 2;
    const slicer_core::SupportShapeOptimizationResult result =
        slicer_core::OptimizeSupportShapeForLayer(policy, modelMask, supportMask, width, height, 4);

    return ExpectEqual(result.removed_support_pixels, 1, "filter removed pixels")
        && ExpectEqual(result.layers.at(0).filtered_components.size(), 1, "filter component count")
        && ExpectEqual(supportMask.at(MaskIndex(width, 0, 0)), 0, "small component cleared");
}

bool DilationPreserveModelPriority()
{
    constexpr int width{5};
    constexpr int height{5};
    std::vector<std::uint8_t> modelMask(width * height, 0);
    std::vector<std::uint8_t> supportMask(width * height, 0);
    SetPixel(supportMask, width, 2, 2);
    SetPixel(modelMask, width, 2, 1);

    slicer_core::SupportShapePolicy policy = EnabledPolicy();
    policy.xy_dilation_px = 1;
    const slicer_core::SupportShapeOptimizationResult result =
        slicer_core::OptimizeSupportShapeForLayer(policy, modelMask, supportMask, width, height, 8);

    return ExpectTrue(result.added_support_pixels > 0, "dilation added pixels")
        && ExpectEqual(supportMask.at(MaskIndex(width, 2, 1)), 0, "model priority preserved");
}

bool BridgeHorizontalGap()
{
    constexpr int width{7};
    constexpr int height{3};
    std::vector<std::uint8_t> modelMask(width * height, 0);
    std::vector<std::uint8_t> supportMask(width * height, 0);
    SetPixel(supportMask, width, 1, 1);
    SetPixel(supportMask, width, 4, 1);

    slicer_core::SupportShapePolicy policy = EnabledPolicy();
    policy.bridge_gap_px = 2;
    const slicer_core::SupportShapeOptimizationResult result =
        slicer_core::OptimizeSupportShapeForLayer(policy, modelMask, supportMask, width, height, 8);

    return ExpectEqual(result.added_support_pixels, 2, "horizontal bridge added pixels")
        && ExpectEqual(result.layers.at(0).bridged_gaps.size(), 1, "horizontal bridge count")
        && ExpectEqual(supportMask.at(MaskIndex(width, 2, 1)), 1, "horizontal bridge first pixel")
        && ExpectEqual(supportMask.at(MaskIndex(width, 3, 1)), 1, "horizontal bridge second pixel");
}

bool BridgeVerticalGap()
{
    constexpr int width{3};
    constexpr int height{7};
    std::vector<std::uint8_t> modelMask(width * height, 0);
    std::vector<std::uint8_t> supportMask(width * height, 0);
    SetPixel(supportMask, width, 1, 1);
    SetPixel(supportMask, width, 1, 4);

    slicer_core::SupportShapePolicy policy = EnabledPolicy();
    policy.bridge_gap_px = 2;
    const slicer_core::SupportShapeOptimizationResult result =
        slicer_core::OptimizeSupportShapeForLayer(policy, modelMask, supportMask, width, height, 8);

    return ExpectEqual(result.added_support_pixels, 2, "vertical bridge added pixels")
        && ExpectEqual(result.layers.at(0).bridged_gaps.size(), 1, "vertical bridge count")
        && ExpectEqual(supportMask.at(MaskIndex(width, 1, 2)), 1, "vertical bridge first pixel")
        && ExpectEqual(supportMask.at(MaskIndex(width, 1, 3)), 1, "vertical bridge second pixel");
}

bool MaxAddedRatioRollback()
{
    constexpr int width{7};
    constexpr int height{3};
    std::vector<std::uint8_t> modelMask(width * height, 0);
    std::vector<std::uint8_t> supportMask(width * height, 0);
    SetPixel(supportMask, width, 1, 1);
    SetPixel(supportMask, width, 4, 1);

    slicer_core::SupportShapePolicy policy = EnabledPolicy();
    policy.bridge_gap_px = 2;
    policy.max_added_support_ratio = 0.5;
    const slicer_core::SupportShapeOptimizationResult result =
        slicer_core::OptimizeSupportShapeForLayer(policy, modelMask, supportMask, width, height, 8);

    return ExpectEqual(result.added_support_pixels, 0, "rollback added pixels")
        && ExpectEqual(supportMask.at(MaskIndex(width, 2, 1)), 0, "rollback first bridge pixel")
        && ExpectTrue(!result.warnings.empty(), "rollback warning");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"component_analysis_single_component", ComponentAnalysisSingleComponent},
        {"component_analysis_two_components", ComponentAnalysisTwoComponents},
        {"filter_small_component", FilterSmallComponent},
        {"dilation_preserve_model_priority", DilationPreserveModelPriority},
        {"bridge_horizontal_gap", BridgeHorizontalGap},
        {"bridge_vertical_gap", BridgeVerticalGap},
        {"max_added_ratio_rollback", MaxAddedRatioRollback},
    };

    for (const auto& test : tests)
    {
        if (!test.second())
        {
            return 1;
        }
        std::cout << "PASS " << test.first << '\n';
    }

    std::cout << "Support shape unit tests complete.\n";
    return 0;
}
