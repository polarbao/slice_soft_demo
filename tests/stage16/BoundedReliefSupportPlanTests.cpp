#include "slicer_core/support/BoundedReliefSupportPlan.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using slicer_core::BoundedReliefColumnSpan;
using slicer_core::EvaluateBoundedReliefSupportPath;
using slicer_core::MaterializeBottomProjectionSupportLayer;
using slicer_core::MaterializeReliefModelLayer;
using slicer_core::SliceConfig;
using slicer_core::SupportType;

bool ExpectTrue(const bool condition, const std::string& what)
{
    if (!condition)
    {
        std::cout << "FAIL " << what << "\n";
    }
    return condition;
}

/// 构造一个满足 X2a 全部准入条件的配置。
SliceConfig EligibleConfig()
{
    SliceConfig config;
    config.slicing_mode = "relief_heightfield";
    config.geometry_sampling.strategy = "legacy_center_sample";
    config.support.enabled = true;
    config.support.mode = "bottom_projection";
    config.support.placement_explicit = false;
    config.support.shape_enabled = false;
    config.support.base_projection.enabled = false;
    config.outer_varnish.enabled = false;
    return config;
}

bool EligibilityAcceptsOnlyTheBoundedConfiguration()
{
    bool passed{true};
    passed = ExpectTrue(
        EvaluateBoundedReliefSupportPath(EligibleConfig()).eligible,
        "eligible configuration is accepted") && passed;

    struct Case
    {
        std::string what;
        std::string expectedReason;
        void (*mutate)(SliceConfig&);
    };
    const std::vector<Case> cases{
        {"non-relief slicing mode", "slicing_mode_not_relief_heightfield",
         [](SliceConfig& c) { c.slicing_mode = "contour"; }},
        {"non-legacy geometry sampling",
         "geometry_sampling_strategy_not_legacy_center_sample",
         [](SliceConfig& c)
         { c.geometry_sampling.strategy = "layer_slab_pixel_center_candidate"; }},
        {"supersample geometry sampling",
         "geometry_sampling_strategy_not_legacy_center_sample",
         [](SliceConfig& c)
         {
             c.geometry_sampling.strategy =
                 "layer_slab_supersample_2x2_any_hit_candidate";
         }},
        {"explicit non-lower placement", "support_placement_not_lower",
         [](SliceConfig& c)
         {
             c.support.placement_explicit = true;
             c.support.placement = "both";
         }},
        {"mode without bottom projection", "support_mode_without_bottom_projection",
         [](SliceConfig& c) { c.support.mode = "unsupported_only"; }},
        {"mode including unsupported", "support_mode_includes_unsupported",
         [](SliceConfig& c)
         { c.support.mode = "bottom_projection_plus_unsupported"; }},
        {"shape optimization enabled", "support_shape_enabled",
         [](SliceConfig& c) { c.support.shape_enabled = true; }},
        {"base projection enabled", "support_base_projection_enabled",
         [](SliceConfig& c) { c.support.base_projection.enabled = true; }},
        {"outer varnish enabled", "outer_varnish_enabled",
         [](SliceConfig& c) { c.outer_varnish.enabled = true; }},
    };
    for (const Case& testCase : cases)
    {
        SliceConfig config = EligibleConfig();
        testCase.mutate(config);
        const auto verdict = EvaluateBoundedReliefSupportPath(config);
        passed = ExpectTrue(!verdict.eligible, "rejects " + testCase.what) && passed;
        passed = ExpectTrue(
            verdict.reason == testCase.expectedReason,
            "reports reason for " + testCase.what + " (got " + verdict.reason + ")")
            && passed;
    }
    // 显式 placement 为 lower 时仍应准入 —— gubao04 走的正是这一支。
    SliceConfig explicitLower = EligibleConfig();
    explicitLower.support.placement_explicit = true;
    explicitLower.support.placement = "lower";
    passed = ExpectTrue(
        EvaluateBoundedReliefSupportPath(explicitLower).eligible,
        "explicit lower placement stays eligible") && passed;
    return passed;
}

bool ModelLayerReproducesClosedInterval()
{
    const std::vector<BoundedReliefColumnSpan> spans{
        {true, 2, 4},    // 闭区间：2、3、4 命中
        {false, 1, 5},   // hasModel 为假：任何层都不命中
        {true, -1, -1},  // 采样时 startLayer 大于 endLayer 而 continue：不命中
        {true, 0, 0},    // 单层区间
    };
    bool passed{true};
    std::vector<std::uint8_t> mask(spans.size(), 7);  // 预置脏数据，验证会被清干净
    const std::vector<std::vector<std::uint8_t>> expected{
        {0, 0, 0, 1},
        {0, 0, 0, 0},
        {1, 0, 0, 0},
        {1, 0, 0, 0},
        {1, 0, 0, 0},
        {0, 0, 0, 0},
    };
    for (int layerIndex{0}; layerIndex < static_cast<int>(expected.size()); ++layerIndex)
    {
        MaterializeReliefModelLayer(spans, layerIndex, mask);
        passed = ExpectTrue(
            mask == expected.at(static_cast<std::size_t>(layerIndex)),
            "model layer " + std::to_string(layerIndex) + " matches closed interval")
            && passed;
    }
    bool threw{false};
    try
    {
        std::vector<std::uint8_t> wrongSize(spans.size() + 1, 0);
        MaterializeReliefModelLayer(spans, 0, wrongSize);
    }
    catch (const std::exception&)
    {
        threw = true;
    }
    return ExpectTrue(threw, "model layer rejects mismatched buffer size") && passed;
}

bool SupportLayerFollowsBottomProjectionPredicate()
{
    // 各列的 lower layer：col0 = 3、col1 = 0、col2 = -1（无模型）、col3 = 2
    const std::vector<int> sourceLayers{3, 0, -1, 2};
    const std::vector<std::uint8_t> modelLayer0{0, 0, 0, 0};
    bool passed{true};
    std::vector<std::uint8_t> support(4, 9);
    std::vector<SupportType> types(4, SupportType::InternalVoid);

    MaterializeBottomProjectionSupportLayer(
        sourceLayers, modelLayer0, true, 0, support, types);
    passed = ExpectTrue(
        support == std::vector<std::uint8_t>{1, 0, 0, 1},
        "layer 0 supports only columns whose lower layer is above it") && passed;
    passed = ExpectTrue(
        types.at(0) == SupportType::BottomProjection
            && types.at(1) == SupportType::None
            && types.at(2) == SupportType::None
            && types.at(3) == SupportType::BottomProjection,
        "layer 0 types are BottomProjection where supported and None elsewhere")
        && passed;

    // 本层已有模型的列不写支撑 —— 与 retained 的 model_masks 判据一致。
    const std::vector<std::uint8_t> modelWithPixel{1, 0, 0, 0};
    MaterializeBottomProjectionSupportLayer(
        sourceLayers, modelWithPixel, true, 0, support, types);
    passed = ExpectTrue(
        support == std::vector<std::uint8_t>{0, 0, 0, 1},
        "columns occupied by the model are not supported") && passed;

    // 层号达到 lower layer 之后不再写支撑（半开区间 0 到 lower）。
    MaterializeBottomProjectionSupportLayer(
        sourceLayers, modelLayer0, true, 2, support, types);
    passed = ExpectTrue(
        support == std::vector<std::uint8_t>{1, 0, 0, 0},
        "support stops at the column lower layer") && passed;

    // 关闭支撑时输出全零 —— 对应 retained 先 resize 再提前返回的行为。
    MaterializeBottomProjectionSupportLayer(
        sourceLayers, modelLayer0, false, 0, support, types);
    passed = ExpectTrue(
        support == std::vector<std::uint8_t>{0, 0, 0, 0},
        "disabled support yields an all-zero layer") && passed;
    passed = ExpectTrue(
        types == std::vector<SupportType>(4, SupportType::None),
        "disabled support resets the type map") && passed;

    bool threw{false};
    try
    {
        std::vector<std::uint8_t> shortSupport(3, 0);
        std::vector<SupportType> shortTypes(3, SupportType::None);
        MaterializeBottomProjectionSupportLayer(
            sourceLayers, modelLayer0, true, 0, shortSupport, shortTypes);
    }
    catch (const std::exception&)
    {
        threw = true;
    }
    return ExpectTrue(threw, "support layer rejects mismatched buffer sizes") && passed;
}

/**
 * @brief 对 retained 参考实现做暴力等价比对。
 *
 * 参考实现按 retained 的两步走：先由列区间物化【整栈】model mask，
 * 再跑 generate_support_masks 的 lower_enabled 双层循环。
 * 逐层物化必须与之逐字节相同。
 */
bool BruteForceMatchesRetainedReference()
{
    std::mt19937 rng{20260904U};
    bool passed{true};
    for (int trial{0}; trial < 40; ++trial)
    {
        const int layerCount = 1 + static_cast<int>(rng() % 12U);
        const std::size_t columnCount = 1U + static_cast<std::size_t>(rng() % 24U);
        std::vector<BoundedReliefColumnSpan> spans(columnCount);
        for (std::size_t column{0}; column < columnCount; ++column)
        {
            const unsigned int kind = rng() % 4U;
            if (kind == 0U)
            {
                spans.at(column) = {false, -1, -1};
                continue;
            }
            if (kind == 1U)
            {
                spans.at(column) = {true, -1, -1};
                continue;
            }
            const int lower =
                static_cast<int>(rng() % static_cast<unsigned int>(layerCount));
            const int upper = lower
                + static_cast<int>(rng() % static_cast<unsigned int>(layerCount - lower));
            spans.at(column) = {true, lower, upper};
        }

        // 参考：整栈 model mask（与采样阶段的填充循环同构）
        std::vector<std::vector<std::uint8_t>> referenceModel(
            static_cast<std::size_t>(layerCount),
            std::vector<std::uint8_t>(columnCount, 0));
        std::vector<int> sourceLayers(columnCount, -1);
        for (std::size_t column{0}; column < columnCount; ++column)
        {
            const BoundedReliefColumnSpan& span = spans.at(column);
            if (!span.hasModel || span.lowerLayer < 0)
            {
                continue;
            }
            sourceLayers.at(column) = span.lowerLayer;
            for (int layerIndex{span.lowerLayer};
                 layerIndex <= span.upperLayer;
                 ++layerIndex)
            {
                referenceModel.at(static_cast<std::size_t>(layerIndex)).at(column) = 1;
            }
        }
        // 参考：整栈 support（与 generate_support_masks 的 lower_enabled 分支同构）
        std::vector<std::vector<std::uint8_t>> referenceSupport(
            static_cast<std::size_t>(layerCount),
            std::vector<std::uint8_t>(columnCount, 0));
        for (std::size_t column{0}; column < columnCount; ++column)
        {
            const int lowerLayer = sourceLayers.at(column);
            for (int layerIndex{0}; layerIndex < lowerLayer; ++layerIndex)
            {
                if (referenceModel.at(static_cast<std::size_t>(layerIndex)).at(column)
                    == 0)
                {
                    referenceSupport.at(static_cast<std::size_t>(layerIndex)).at(column) =
                        1;
                }
            }
        }

        std::vector<std::uint8_t> modelLayer(columnCount, 0);
        std::vector<std::uint8_t> supportLayer(columnCount, 0);
        std::vector<SupportType> typeLayer(columnCount, SupportType::None);
        for (int layerIndex{0}; layerIndex < layerCount; ++layerIndex)
        {
            MaterializeReliefModelLayer(spans, layerIndex, modelLayer);
            MaterializeBottomProjectionSupportLayer(
                sourceLayers, modelLayer, true, layerIndex, supportLayer, typeLayer);
            const std::size_t index = static_cast<std::size_t>(layerIndex);
            passed = ExpectTrue(
                modelLayer == referenceModel.at(index),
                "trial " + std::to_string(trial) + " layer "
                    + std::to_string(layerIndex) + " model matches retained reference")
                && passed;
            passed = ExpectTrue(
                supportLayer == referenceSupport.at(index),
                "trial " + std::to_string(trial) + " layer "
                    + std::to_string(layerIndex) + " support matches retained reference")
                && passed;
            if (!passed)
            {
                return false;
            }
        }
    }
    return passed;
}

}  // namespace

int main()
{
    bool passed{true};
    passed = EligibilityAcceptsOnlyTheBoundedConfiguration() && passed;
    passed = ModelLayerReproducesClosedInterval() && passed;
    passed = SupportLayerFollowsBottomProjectionPredicate() && passed;
    passed = BruteForceMatchesRetainedReference() && passed;
    if (passed)
    {
        std::cout << "PASS MF-03X2a bounded relief support plan tests\n";
        return 0;
    }
    return 1;
}
