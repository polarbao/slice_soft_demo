// F-09 单测缺口的第六批：support/SliceSupportGeneration 的放置策略与统计三件套。
//
// 与批次 2 配成一对。批次 2 钉住了 ResetSupportGenerationStats「只清会被重算的 8 个字段、
// 留着 island 系列」；本批钉住另一半——Accumulate 把层级计数在【入口清零】、
// 而 result 级计数是【累加】的，以及 Calculate 是「先 Reset 再逐层 Accumulate」。
// 这三条合起来才说清这组函数为什么要这样分工；单看任何一个都像是写漏了。
//
// 放置策略那条尤其要紧：非显式配置走 legacy 路径，此时 upper_enabled 被【强制关掉】。
// 重构时丢掉那一行不会崩、不会报错，只会让所有老配置静默打开上表面支撑。

#include "Cases.h"

#include "slicer_core/support/SliceSupportGeneration.h"

#include "tests/support/Expect.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {

using slicer_core::GridSpec;
using slicer_core::LayerDiagnostics;
using slicer_core::SliceConfig;
using slicer_core::SupportGenerationResult;
using slicer_core::SupportType;

GridSpec MakeGrid(const int layers)
{
    GridSpec grid{};
    grid.width_px = 2;
    grid.height_px = 2;
    grid.layer_count = layers;
    grid.pixel_size_x_mm = 1.0;
    grid.pixel_size_y_mm = 1.0;
    return grid;
}

/// 显式放置：placement_explicit 为真时走新字段。
SliceConfig ExplicitPlacement(const std::string& placement)
{
    SliceConfig config;
    config.support.placement_explicit = true;
    config.support.placement = placement;
    return config;
}

/// 老配置：placement_explicit 为假，行为由 mode 决定。
SliceConfig LegacyMode(const std::string& mode)
{
    SliceConfig config;
    config.support.placement_explicit = false;
    config.support.mode = mode;
    return config;
}

}  // namespace

int RunSupportStatsCases()
{
    using namespace slicer_core;
    return slicesoft_test::RunCases(
        "support placement and stats (F-09 batch 6)",
        {
            {"explicit placement maps each keyword to its switches",
             [] {
                 const auto both = support::ResolveSupportPlacementPolicy(
                     ExplicitPlacement("both"));
                 SLICESOFT_EXPECT_TRUE(both.lower_enabled, "both 应开下表面");
                 SLICESOFT_EXPECT_TRUE(both.upper_enabled, "both 应开上表面");

                 const auto upper = support::ResolveSupportPlacementPolicy(
                     ExplicitPlacement("upper"));
                 SLICESOFT_EXPECT_FALSE(upper.lower_enabled, "upper 不应开下表面");
                 SLICESOFT_EXPECT_TRUE(upper.upper_enabled, "upper 应开上表面");

                 const auto only = support::ResolveSupportPlacementPolicy(
                     ExplicitPlacement("unsupported_only"));
                 SLICESOFT_EXPECT_TRUE(
                     only.unsupported_only_enabled, "unsupported_only 应开该开关");
                 SLICESOFT_EXPECT_FALSE(only.lower_enabled, "unsupported_only 不应开下表面");
             }},
            {"advanced debug mirrors full vertical projection",
             [] {
                 // 这两个开关是同一件事的两个名字。分叉后诊断产物会与实际行为不符。
                 const auto full = support::ResolveSupportPlacementPolicy(
                     ExplicitPlacement("full_vertical_projection"));
                 SLICESOFT_EXPECT_TRUE(
                     full.full_vertical_projection_enabled, "应开全垂直投影");
                 SLICESOFT_EXPECT_TRUE(
                     full.advanced_debug, "advanced_debug 须随全垂直投影");

                 const auto lower = support::ResolveSupportPlacementPolicy(
                     ExplicitPlacement("lower"));
                 SLICESOFT_EXPECT_FALSE(lower.advanced_debug, "非全垂直投影时不应开");
             }},
            {"legacy configs never get upper support",
             [] {
                 // 【本批最要紧的一条】placement_explicit 为假时走 legacy 路径：
                 // requested 改记 legacy_mode、effective 取自 mode，而 upper_enabled
                 // 被【强制关掉】。丢掉那一行不会崩也不会报错，
                 // 只会让所有老配置静默打开上表面支撑——产物多出一层没人要的支撑。
                 const auto legacy = support::ResolveSupportPlacementPolicy(
                     LegacyMode("bottom_projection_plus_unsupported"));
                 SLICESOFT_EXPECT_EQ(
                     legacy.requested_placement, std::string{"legacy_mode"},
                     "老配置的 requested 应记 legacy_mode");
                 SLICESOFT_EXPECT_EQ(
                     legacy.effective_placement,
                     std::string{"bottom_projection_plus_unsupported"},
                     "老配置的 effective 应取自 mode");
                 SLICESOFT_EXPECT_FALSE(
                     legacy.upper_enabled, "老配置绝不应开上表面支撑");
                 SLICESOFT_EXPECT_FALSE(
                     legacy.placement_explicit, "老配置须标记为非显式");
                 // 该 mode 同时含底面投影与无支撑区两种成分。
                 SLICESOFT_EXPECT_TRUE(legacy.lower_enabled, "该 mode 含底面投影");
                 SLICESOFT_EXPECT_TRUE(
                     legacy.unsupported_only_enabled, "该 mode 含无支撑区");
             }},
            {"a legacy mode outside the known set enables nothing",
             [] {
                 // 未知 mode 不该退回某个默认行为——那是静默降级。两个开关都应为假。
                 const auto unknown = support::ResolveSupportPlacementPolicy(
                     LegacyMode("something_new"));
                 SLICESOFT_EXPECT_FALSE(unknown.lower_enabled, "未知 mode 不应开下表面");
                 SLICESOFT_EXPECT_FALSE(
                     unknown.unsupported_only_enabled, "未知 mode 不应开无支撑区");
             }},
            {"each support type increments its own counter",
             [] {
                 // 六种支撑类型各有独立计数，用于报告里区分「支撑从哪来」。
                 // 少一个 case 会让那种支撑的像素并入 support_pixels 却不进分型计数，
                 // 总数仍然对得上，只有分项悄悄少了。
                 SupportGenerationResult result;
                 LayerDiagnostics layer;
                 const std::vector<std::uint8_t> mask{1U, 1U, 1U, 0U};
                 const std::vector<SupportType> types{
                     SupportType::BottomProjection,
                     SupportType::UpperProjection,
                     SupportType::InternalVoid,
                     SupportType::None,
                 };
                 const SliceConfig config;
                 support::AccumulateSupportLayerStats(
                     result, layer, mask, types, MakeGrid(1), config);
                 SLICESOFT_EXPECT_EQ(result.support_pixels, 3, "三个非零像素");
                 SLICESOFT_EXPECT_EQ(
                     result.bottom_projection_support_pixels, 1, "底面投影计数");
                 SLICESOFT_EXPECT_EQ(
                     result.upper_projection_support_pixels, 1, "上表面投影计数");
                 SLICESOFT_EXPECT_EQ(
                     result.internal_void_support_pixels, 1, "内部空腔计数");
                 // 层级计数与 result 级同步写入。
                 SLICESOFT_EXPECT_EQ(
                     layer.bottom_projection_support_pixels, 1, "层级也须记底面投影");
             }},
            {"layer counters reset on entry while result counters accumulate",
             [] {
                 // 【这条钉的是分工本身】同一层连算两次：层级计数保持 1（入口清零），
                 // result 级变成 2（累加）。两者若都清零，多层汇总永远只剩最后一层；
                 // 两者若都累加，重算一层会把层级诊断翻倍。
                 SupportGenerationResult result;
                 LayerDiagnostics layer;
                 const std::vector<std::uint8_t> mask{1U, 0U, 0U, 0U};
                 const std::vector<SupportType> types(
                     4U, SupportType::BottomProjection);
                 const SliceConfig config;
                 support::AccumulateSupportLayerStats(
                     result, layer, mask, types, MakeGrid(1), config);
                 support::AccumulateSupportLayerStats(
                     result, layer, mask, types, MakeGrid(1), config);
                 SLICESOFT_EXPECT_EQ(
                     layer.bottom_projection_support_pixels, 1, "层级计数须在入口清零");
                 SLICESOFT_EXPECT_EQ(
                     result.bottom_projection_support_pixels, 2, "result 级须累加");
             }},
            {"an all-zero mask touches no counter",
             [] {
                 SupportGenerationResult result;
                 LayerDiagnostics layer;
                 const std::vector<std::uint8_t> mask(4U, 0U);
                 const std::vector<SupportType> types(4U, SupportType::BottomProjection);
                 const SliceConfig config;
                 support::AccumulateSupportLayerStats(
                     result, layer, mask, types, MakeGrid(1), config);
                 SLICESOFT_EXPECT_EQ(result.support_pixels, 0, "空层不应计入支撑像素");
                 SLICESOFT_EXPECT_EQ(result.layers_with_support, 0, "空层不应计入含支撑层数");
             }},
            {"Calculate resets then sums every layer",
             [] {
                 // Calculate = Reset + 逐层 Accumulate。这条同时验三件事：
                 // 旧的 result 计数被清掉、每层都被走到、island 计数【仍然留着】
                 // （后者正是批次 2 钉下的那条分界，这里再从调用方一侧确认一次）。
                 SupportGenerationResult result;
                 result.support_pixels = 999;        // 应被 Reset 清掉
                 result.island_count = 7;            // 不该被清
                 result.support_masks = {
                     {1U, 0U, 0U, 0U},
                     {1U, 1U, 0U, 0U},
                 };
                 result.support_type_maps = {
                     std::vector<SupportType>(4U, SupportType::BottomProjection),
                     std::vector<SupportType>(4U, SupportType::BottomProjection),
                 };
                 std::vector<LayerDiagnostics> diagnostics(2U);
                 const SliceConfig config;
                 support::CalculateSupportGenerationStats(
                     result, diagnostics, MakeGrid(2), config);
                 SLICESOFT_EXPECT_EQ(result.support_pixels, 3, "两层合计三个支撑像素");
                 SLICESOFT_EXPECT_EQ(
                     result.layers_with_support, 2, "两层都含支撑");
                 SLICESOFT_EXPECT_EQ(result.island_count, 7, "岛计数不应被 Reset 清掉");
             }},
        });
}
