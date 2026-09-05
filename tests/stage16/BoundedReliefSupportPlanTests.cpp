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
using slicer_core::BoundedSpanLastModelLayer;
using slicer_core::BoundedSupportPlacement;
using slicer_core::BuildBoundedActiveColumns;
using slicer_core::ComputeRetainedLastModelLayers;
using slicer_core::EvaluateBoundedReliefSupportPath;
using slicer_core::GridSpec;
using slicer_core::MaterializeBottomProjectionSupportLayer;
using slicer_core::MaterializeFullVerticalProjectionSupportLayer;
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
        {"explicit both placement", "support_placement_not_bounded",
         [](SliceConfig& c)
         {
             c.support.placement_explicit = true;
             c.support.placement = "both";
         }},
        {"explicit upper placement", "support_placement_not_bounded",
         [](SliceConfig& c)
         {
             c.support.placement_explicit = true;
             c.support.placement = "upper";
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
    const auto lowerVerdict = EvaluateBoundedReliefSupportPath(explicitLower);
    passed = ExpectTrue(
        lowerVerdict.eligible, "explicit lower placement stays eligible") && passed;
    passed = ExpectTrue(
        lowerVerdict.placement == BoundedSupportPlacement::BottomProjection,
        "explicit lower placement resolves to bottom projection") && passed;

    // MF-03X2b：full vertical projection 的两条解析路径都要准入，且都要判成
    // FullVerticalProjection —— 判错档位不会报错，只会把支撑上界算成另一个。
    SliceConfig explicitFullVertical = EligibleConfig();
    explicitFullVertical.support.placement_explicit = true;
    explicitFullVertical.support.placement = "full_vertical_projection";
    const auto explicitVerdict =
        EvaluateBoundedReliefSupportPath(explicitFullVertical);
    passed = ExpectTrue(
        explicitVerdict.eligible, "explicit full vertical placement is eligible")
        && passed;
    passed = ExpectTrue(
        explicitVerdict.placement == BoundedSupportPlacement::FullVerticalProjection,
        "explicit full vertical placement resolves to full vertical") && passed;

    // legacy mode 路径。这一支原先【永不可达】：full_vertical_projection 不含
    // bottom_projection，会先被拒成 support_mode_without_bottom_projection。
    SliceConfig legacyFullVertical = EligibleConfig();
    legacyFullVertical.support.placement_explicit = false;
    legacyFullVertical.support.mode = "full_vertical_projection";
    const auto legacyVerdict = EvaluateBoundedReliefSupportPath(legacyFullVertical);
    passed = ExpectTrue(
        legacyVerdict.eligible, "legacy full vertical mode is eligible") && passed;
    passed = ExpectTrue(
        legacyVerdict.placement == BoundedSupportPlacement::FullVerticalProjection,
        "legacy full vertical mode resolves to full vertical") && passed;
    return passed;
}

/**
 * @brief 归一化守卫：`hasModel` 为真但区间无效的列，最后模型层必须是 -1。
 *
 * 这类列真实存在 —— 采样在 `startLayer > endLayer` 时 `continue`，而
 * `has_model` 已在此之前置真。直接读 `upperLayer` 会得到 -1 之外的脏值，
 * 或（更隐蔽地）让 `[0, upperLayer)` 变成一个本不该存在的区间。
 */
bool LastModelLayerMatchesRetainedReduction()
{
    std::mt19937 rng{20260906U};
    bool passed{true};
    for (int trial{0}; trial < 40; ++trial)
    {
        const int layerCount = 1 + static_cast<int>(rng() % 12U);
        const int columnCount = 1 + static_cast<int>(rng() % 24U);
        std::vector<BoundedReliefColumnSpan> spans(
            static_cast<std::size_t>(columnCount));
        std::vector<std::vector<std::uint8_t>> referenceModel(
            static_cast<std::size_t>(layerCount),
            std::vector<std::uint8_t>(static_cast<std::size_t>(columnCount), 0));
        for (std::size_t column{0};
             column < static_cast<std::size_t>(columnCount);
             ++column)
        {
            const unsigned int kind = rng() % 4U;
            if (kind == 0U)
            {
                spans.at(column) = {false, -1, -1};
                continue;
            }
            if (kind == 1U)
            {
                // 有模型标记但区间无效 —— 正是那条 continue 留下的形状。
                spans.at(column) = {true, -1, -1};
                continue;
            }
            const int lower =
                static_cast<int>(rng() % static_cast<unsigned int>(layerCount));
            const int upper = lower
                + static_cast<int>(
                    rng() % static_cast<unsigned int>(layerCount - lower));
            spans.at(column) = {true, lower, upper};
            for (int layerIndex{lower}; layerIndex <= upper; ++layerIndex)
            {
                referenceModel.at(static_cast<std::size_t>(layerIndex)).at(column) = 1;
            }
        }
        GridSpec grid;
        grid.width_px = columnCount;
        grid.height_px = 1;
        grid.layer_count = layerCount;
        const std::vector<int> reference =
            ComputeRetainedLastModelLayers(referenceModel, grid);
        for (std::size_t column{0};
             column < static_cast<std::size_t>(columnCount);
             ++column)
        {
            if (BoundedSpanLastModelLayer(spans.at(column)) != reference.at(column))
            {
                passed = ExpectTrue(
                    false,
                    "bounded last model layer matches the retained reduction at trial "
                        + std::to_string(trial) + " column "
                        + std::to_string(column)) && passed;
                break;
            }
        }
    }
    return passed;
}

/**
 * @brief full vertical projection 档与 retained 分支的逐层暴力等价。
 *
 * 参考实现照抄 `generate_support_masks` 的 full_vertical_projection 分支：
 * 先对整栈归约出 last model layer，再按半开区间 `[0, lastLayer)` 写。
 */
bool FullVerticalProjectionMatchesRetainedReference()
{
    std::mt19937 rng{20260907U};
    bool passed{true};
    for (int trial{0}; trial < 40; ++trial)
    {
        const int layerCount = 1 + static_cast<int>(rng() % 12U);
        const int columnCount = 1 + static_cast<int>(rng() % 24U);
        const auto columns = static_cast<std::size_t>(columnCount);
        std::vector<BoundedReliefColumnSpan> spans(columns);
        std::vector<std::vector<std::uint8_t>> referenceModel(
            static_cast<std::size_t>(layerCount),
            std::vector<std::uint8_t>(columns, 0));
        for (std::size_t column{0}; column < columns; ++column)
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
                + static_cast<int>(
                    rng() % static_cast<unsigned int>(layerCount - lower));
            spans.at(column) = {true, lower, upper};
            for (int layerIndex{lower}; layerIndex <= upper; ++layerIndex)
            {
                referenceModel.at(static_cast<std::size_t>(layerIndex)).at(column) = 1;
            }
        }
        GridSpec grid;
        grid.width_px = columnCount;
        grid.height_px = 1;
        grid.layer_count = layerCount;

        // 参考：retained 的 full_vertical_projection 分支。
        const std::vector<int> lastModelLayers =
            ComputeRetainedLastModelLayers(referenceModel, grid);
        std::vector<std::vector<std::uint8_t>> referenceSupport(
            static_cast<std::size_t>(layerCount),
            std::vector<std::uint8_t>(columns, 0));
        for (std::size_t column{0}; column < columns; ++column)
        {
            const int lastLayer = lastModelLayers.at(column);
            for (int layerIndex{0}; layerIndex < lastLayer; ++layerIndex)
            {
                if (referenceModel.at(static_cast<std::size_t>(layerIndex)).at(column)
                    == 0)
                {
                    referenceSupport.at(static_cast<std::size_t>(layerIndex))
                        .at(column) = 1;
                }
            }
        }

        std::vector<std::uint8_t> modelLayer(columns, 0);
        std::vector<std::uint8_t> supportLayer(columns, 0);
        std::vector<SupportType> typeLayer(columns, SupportType::None);
        for (int layerIndex{0}; layerIndex < layerCount; ++layerIndex)
        {
            MaterializeReliefModelLayer(spans, layerIndex, modelLayer);
            MaterializeFullVerticalProjectionSupportLayer(
                spans, modelLayer, true, layerIndex, supportLayer, typeLayer);
            const auto layer = static_cast<std::size_t>(layerIndex);
            if (modelLayer != referenceModel.at(layer)
                || supportLayer != referenceSupport.at(layer))
            {
                passed = ExpectTrue(
                    false,
                    "full vertical projection matches the retained reference at trial "
                        + std::to_string(trial) + " layer "
                        + std::to_string(layerIndex)) && passed;
                break;
            }
            for (std::size_t column{0}; column < columns; ++column)
            {
                const SupportType expected = supportLayer.at(column) != 0
                    ? SupportType::FullVerticalProjection
                    : SupportType::None;
                if (typeLayer.at(column) != expected)
                {
                    passed = ExpectTrue(
                        false,
                        "full vertical projection stamps its own support type at trial "
                            + std::to_string(trial)) && passed;
                    break;
                }
            }
        }
    }
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

/**
 * @brief 活动列表必须包含【面内被围住】的空列。
 *
 * MF-03X3 首版把活动列表取成「有模型的列」，结果 r01 二十层各差一字节 ——
 * 环形件孔心那类列在【所有层】都没有模型，却要写内部空腔支撑。
 * 本用例用一个 7x7 的环把该场景固化下来：孔心必须在表内，环外必须在表外。
 */
bool ActiveColumnsKeepEnclosedHoles()
{
    constexpr int width{7};
    constexpr int height{7};
    std::vector<BoundedReliefColumnSpan> spans(
        static_cast<std::size_t>(width) * height);
    // 在 (2,2)-(4,4) 画一个环：外圈有模型，正中 (3,3) 是孔。
    for (int y{2}; y <= 4; ++y)
    {
        for (int x{2}; x <= 4; ++x)
        {
            if (x == 3 && y == 3)
            {
                continue;
            }
            spans.at(static_cast<std::size_t>(y) * width + x) = {true, 0, 3};
        }
    }
    const std::vector<std::uint32_t> active =
        BuildBoundedActiveColumns(spans, width, height);
    const auto contains = [&active](const int x, const int y) {
        const auto wanted = static_cast<std::uint32_t>(y * width + x);
        for (const std::uint32_t column : active)
        {
            if (column == wanted)
            {
                return true;
            }
        }
        return false;
    };

    bool passed{true};
    passed = ExpectTrue(contains(3, 3), "active columns keep the enclosed hole") && passed;
    passed = ExpectTrue(contains(2, 2), "active columns keep model columns") && passed;
    passed = ExpectTrue(!contains(0, 0), "active columns drop the border") && passed;
    passed = ExpectTrue(!contains(6, 6), "active columns drop the far corner") && passed;
    passed = ExpectTrue(
        active.size() == 9U,
        "active columns are exactly the ring plus its hole") && passed;
    // 尺寸不符必须抛异常而非静默算错。
    bool threw{false};
    try
    {
        (void)BuildBoundedActiveColumns(spans, width, height + 1);
    }
    catch (const std::exception&)
    {
        threw = true;
    }
    return ExpectTrue(threw, "active columns reject a mismatched grid") && passed;
}

int main()
{
    bool passed{true};
    passed = EligibilityAcceptsOnlyTheBoundedConfiguration() && passed;
    passed = ModelLayerReproducesClosedInterval() && passed;
    passed = SupportLayerFollowsBottomProjectionPredicate() && passed;
    passed = BruteForceMatchesRetainedReference() && passed;
    passed = LastModelLayerMatchesRetainedReduction() && passed;
    passed = FullVerticalProjectionMatchesRetainedReference() && passed;
    passed = ActiveColumnsKeepEnclosedHoles() && passed;
    if (passed)
    {
        std::cout << "PASS MF-03X2a/X2b bounded relief support plan tests\n";
        return 0;
    }
    return 1;
}
