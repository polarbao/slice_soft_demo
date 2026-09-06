#include "slicer_core/system/SlicePeakMemoryEstimate.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{

using slicer_core::EstimateSlicePeakMemory;
using slicer_core::GridSpec;
using slicer_core::SliceConfig;
using slicer_core::SlicePeakMemoryEstimate;

bool ExpectTrue(const bool condition, const std::string& what)
{
    if (!condition)
    {
        std::cout << "FAIL " << what << "\n";
    }
    return condition;
}

/// 满足有界准入的配置（与 BoundedReliefSupportPlanTests 的 EligibleConfig 同形）。
SliceConfig BoundedConfig()
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

/// 多材质配置：gubao04 那一类（六材质 + 贴图 + MATVOL 逐列求交）。
SliceConfig MultiMaterialConfig()
{
    SliceConfig config = BoundedConfig();
    config.material_volume_policy.enabled = true;
    config.texture.enabled = true;
    return config;
}

GridSpec MakeGrid(const int width, const int height, const int layers)
{
    GridSpec grid;
    grid.width_px = width;
    grid.height_px = height;
    grid.layer_count = layers;
    return grid;
}

constexpr std::uint64_t kMiB{1024ULL * 1024ULL};

/**
 * @brief 契约的核心：对真实实测点【禁止低估】。
 *
 * 这不是「误差在 X% 以内」的精度测试 —— 精度不重要，方向才重要。
 * 低估的表现形式是「承诺了预算然后 OOM」，比不做路由更糟；
 * 高估只会让一个本可跑通的作业被拒，用户立刻就会发现并调高预算。
 *
 * 三个点都是本专项跑出来的 peak working set（含分配器未归还部分）。
 */
bool NeverUnderestimatesMeasuredRuns()
{
    struct Case
    {
        std::string what;
        int width;
        int height;
        int layers;
        std::uint64_t measuredBytes;
        bool bounded;
        bool multiMaterial;
    };
    const std::vector<Case> cases{
        // 单材料点。r01：94 层，实测 90,050,560 B。
        {"r01 @0.05mm", 293, 567, 94, 90050560ULL, true, false},
        // a-2/0.2.obj @10um：1429 层，实测 0.84 GiB。本专项的主力场景。
        {"a-2 @10um", 1418, 5197, 1429, 902ULL * kMiB, true, false},
        // 同一场景在专项介入前走 retained，实测峰值 22~34 GB。
        // 取下界 22 GB 作判据：估算必须不低于它，否则就是低估。
        {"a-2 @10um retained", 1418, 5197, 1429, 22ULL * 1024ULL * kMiB, false,
         false},
        // 多材质点（六材质 + 贴图 + MATVOL 逐列求交）。
        // gubao04 原尺寸：129 层，实测 132,276,224 B。
        {"gubao04 @0.05mm", 335, 576, 129, 132276224ULL, true, true},
        // 同一模型 XY 放大 4 倍（列数 x16、层数不变）：实测 1,224,708,096 B。
        // 这个点是 2026-09-06 补的多材质大栅格基线 —— 正是它证明未建模项
        // 在多材质路径下随【列数】增长，而不是一笔常数余量兜得住的。
        {"gubao04 xy4 (308 万列)", 1340, 2303, 129, 1224708096ULL, true, true},
    };

    bool passed{true};
    for (const Case& item : cases)
    {
        const SlicePeakMemoryEstimate estimate = EstimateSlicePeakMemory(
            item.multiMaterial ? MultiMaterialConfig() : BoundedConfig(),
            MakeGrid(item.width, item.height, item.layers));
        const std::uint64_t predicted = item.bounded
            ? estimate.bounded.totalBytes
            : estimate.retained.totalBytes;
        // 把裕度打印出来：只知道「没低估」不够 —— 高估十倍的模型同样没用，
        // 而模型一旦漂移，最先变的就是这个比值。
        std::cout << "  [margin] " << item.what << ": predicted "
                  << (predicted / kMiB) << " MiB / measured "
                  << (item.measuredBytes / kMiB) << " MiB = "
                  << (static_cast<double>(predicted)
                      / static_cast<double>(item.measuredBytes))
                  << "x" << std::endl;
        // 依据强度必须如实标注：多材质那一支含拟合项，单材料那一支不含。
        // 少了这条，将来把附加项误加到单材料路径上也不会被发现。
        passed = ExpectTrue(
            estimate.empiricalOnly == item.multiMaterial,
            item.what + " declares whether it rests on empirical fitting")
            && passed;
        passed = ExpectTrue(
            predicted >= item.measuredBytes,
            "never underestimates " + item.what + " (predicted "
                + std::to_string(predicted / kMiB) + " MiB, measured "
                + std::to_string(item.measuredBytes / kMiB) + " MiB)")
            && passed;
    }
    return passed;
}

/**
 * @brief 有界路径的估算必须与层数【无关】。
 *
 * 这是 MF-03X2a/MF-05 的核心成果，也是预估器是否理解这条路径的判据：
 * 若它给有界路径算出随层数增长的项，说明建模抄错了路径。
 */
bool BoundedEstimateIsIndependentOfLayerCount()
{
    const SliceConfig config = BoundedConfig();
    const SlicePeakMemoryEstimate few =
        EstimateSlicePeakMemory(config, MakeGrid(1418, 5197, 15));
    const SlicePeakMemoryEstimate many =
        EstimateSlicePeakMemory(config, MakeGrid(1418, 5197, 1429));
    bool passed{ExpectTrue(
        few.bounded.totalBytes == many.bounded.totalBytes,
        "bounded estimate does not grow with layer count")};
    for (const auto& term : many.bounded.terms)
    {
        passed = ExpectTrue(
            !term.scalesWithLayers,
            "bounded path carries no per-layer term (" + term.name + ")")
            && passed;
    }
    // 对照：retained 必须【随层数增长】，否则模型没抓住两条路径的差别。
    passed = ExpectTrue(
        many.retained.totalBytes > few.retained.totalBytes * 10ULL,
        "retained estimate grows with layer count") && passed;
    return passed;
}

/// 准入未通过时 bounded 估算无意义，必须带出原因而不是给个数字。
bool ReportsWhyBoundedIsUnavailable()
{
    SliceConfig config = BoundedConfig();
    config.support.shape_enabled = true;
    const SlicePeakMemoryEstimate estimate =
        EstimateSlicePeakMemory(config, MakeGrid(1418, 5197, 1429));
    bool passed{ExpectTrue(
        !estimate.boundedEligible, "shape config is not bounded eligible")};
    passed = ExpectTrue(
        estimate.boundedRejectReason == "support_shape_enabled",
        "reports the admission reason verbatim") && passed;
    // shape 档在 retained 下多一份整栈深拷贝，估算必须体现出来。
    const SlicePeakMemoryEstimate without =
        EstimateSlicePeakMemory(BoundedConfig(), MakeGrid(1418, 5197, 1429));
    passed = ExpectTrue(
        estimate.retained.totalBytes > without.retained.totalBytes,
        "shape adds the retained original-stack copy") && passed;
    return passed;
}

/// 退化输入不得算出垃圾值或溢出。
bool DegenerateGridsStayHarmless()
{
    const SliceConfig config = BoundedConfig();
    bool passed{true};
    // 列数为 0 时逐列项全部消失，只剩未建模余量。
    // 余量不能省：报 0 会让预算判定误以为这次作业「不占内存」。
    for (const GridSpec& grid : {MakeGrid(0, 0, 0), MakeGrid(-1, 5, 5)})
    {
        const SlicePeakMemoryEstimate estimate =
            EstimateSlicePeakMemory(config, grid);
        passed = ExpectTrue(
            estimate.retained.totalBytes == slicer_core::kUnmodelledReserveBytes
                && estimate.bounded.totalBytes
                    == slicer_core::kUnmodelledReserveBytes,
            "zero-column grid falls back to the reserve alone") && passed;
    }
    // 层数非法而列数正常：逐列项照旧存在（它们与层数无关），
    // 但任何 O(列数 x 层数) 的项都必须消失 —— 否则就是把负层数当成了大数。
    {
        const SlicePeakMemoryEstimate estimate =
            EstimateSlicePeakMemory(config, MakeGrid(5, 5, -1));
        passed = ExpectTrue(
            estimate.retained.totalBytes > slicer_core::kUnmodelledReserveBytes,
            "negative layer count keeps the per-column terms") && passed;
        for (const auto& term : estimate.retained.terms)
        {
            passed = ExpectTrue(
                !term.scalesWithLayers,
                "negative layer count drops per-layer term (" + term.name + ")")
                && passed;
        }
    }
    return passed;
}

}  // namespace

int main()
{
    bool passed{true};
    passed = NeverUnderestimatesMeasuredRuns() && passed;
    passed = BoundedEstimateIsIndependentOfLayerCount() && passed;
    passed = ReportsWhyBoundedIsUnavailable() && passed;
    passed = DegenerateGridsStayHarmless() && passed;
    if (passed)
    {
        std::cout << "PASS MF-07c slice peak memory estimate tests\n";
        return 0;
    }
    return 1;
}
