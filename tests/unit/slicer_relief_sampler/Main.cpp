// F-09 单测缺口的第三批（R-12 验证 Gate 第 4 条）：浮雕高度场采样器。
//
// 前两批落在 tests/unit/slicer_extracted_units，本批另起一个目标而不是续写那个文件：
// 那个文件已 399 行，续写会越过 G1 的 500 行阈值。G1 只判「新文件」，提交之后再长
// 就看不见了——但那是在钻我自己记录下来的门禁盲点，所以按仓库「一目录一目标」的惯例拆开。
//
// 本批覆盖 ReliefHeightfieldSampler 的四个纯函数入口。它们是八个搬出单元里
// 唯一一个此前零覆盖的单元，且四个都只吃小网格与掩膜，不需要真实模型。
//
// 每条断言都是先读实现、再写预期——前两批各有一次「按想当然写断言、撞红后才发现
// 是自己误解契约」，代价是一轮构建。

#include "slicer_core/geometry/ReliefHeightfieldSampler.h"

#include "tests/support/Expect.h"

#include <cstdint>
#include <vector>

namespace {

/// 2x2 幅面、3 层的最小网格。
slicer_core::GridSpec MakeTinyGrid()
{
    slicer_core::GridSpec grid{};
    grid.width_px = 2;
    grid.height_px = 2;
    grid.layer_count = 3;
    grid.pixel_size_x_mm = 0.1;
    grid.pixel_size_y_mm = 0.1;
    return grid;
}

/// 四个像素各有不同的命中形态，一组掩膜同时喂给两个函数：
///   像素 0：第 0 层与第 2 层命中，**第 1 层空着**（用来验证「跨度」而非「集合」）
///   像素 1：只在第 1 层
///   像素 2：只在第 2 层
///   像素 3：从不命中
std::vector<std::vector<std::uint8_t>> MakeProbeMasks()
{
    return {
        {1U, 0U, 0U, 0U},
        {0U, 1U, 0U, 0U},
        {1U, 0U, 1U, 0U},
    };
}

slicer_core::ReliefColumnInfo MakeColumn(
    const bool hasModel, const int lower, const int upper)
{
    slicer_core::ReliefColumnInfo column{};
    column.has_model = hasModel;
    column.lower_layer = lower;
    column.upper_layer = upper;
    return column;
}

}  // namespace

int main()
{
    using namespace slicer_core;
    return slicesoft_test::RunCases(
        "slicer relief heightfield sampler (F-09 batch 3)",
        {
            {"first model layer records the lowest hit only",
             [] {
                 const auto first =
                     relief::compute_first_model_layers(MakeProbeMasks(), MakeTinyGrid());
                 SLICESOFT_EXPECT_EQ(first.size(), std::size_t{4}, "长度须为 宽x高");
                 // 像素 0 在第 0 与第 2 层都命中，记录的必须是【最低】那层。
                 // 写成「最后一次命中」不会崩，只会让浮雕基准面整体偏高。
                 SLICESOFT_EXPECT_EQ(first.at(0), 0, "像素 0 应记最低层");
                 SLICESOFT_EXPECT_EQ(first.at(1), 1, "像素 1 只在第 1 层");
                 SLICESOFT_EXPECT_EQ(first.at(2), 2, "像素 2 只在第 2 层");
                 // 从不命中的像素是 -1 而不是 0——0 是合法层号，混淆会让空白处被当成实体。
                 SLICESOFT_EXPECT_EQ(first.at(3), -1, "从不命中的像素须为 -1");
             }},
            {"mask column ranges are a span, not a set",
             [] {
                 const auto ranges =
                     relief::compute_mask_column_ranges(MakeProbeMasks(), MakeTinyGrid());
                 SLICESOFT_EXPECT_EQ(ranges.size(), std::size_t{4}, "长度须为 宽x高");
                 // 【这是本函数最容易被误解的地方】像素 0 在第 1 层是空的，
                 // 但区间仍是 [0,2]——它记的是首末命中之间的【跨度】，中间的洞被吞掉。
                 // 下游据此裁剪列范围，改成「逐层集合」会改变裁剪结果与产物。
                 SLICESOFT_EXPECT_TRUE(ranges.at(0).hasModel, "像素 0 应有区间");
                 SLICESOFT_EXPECT_EQ(ranges.at(0).lowerLayer, 0, "跨度下界应为 0");
                 SLICESOFT_EXPECT_EQ(ranges.at(0).upperLayer, 2, "跨度上界应为 2，中间的空层被吞");
                 SLICESOFT_EXPECT_EQ(ranges.at(1).lowerLayer, 1, "单层命中的下界");
                 SLICESOFT_EXPECT_EQ(ranges.at(1).upperLayer, 1, "单层命中时上下界相同");
             }},
            {"a pixel with no model has no range",
             [] {
                 const auto ranges =
                     relief::compute_mask_column_ranges(MakeProbeMasks(), MakeTinyGrid());
                 // 空列必须是「没有区间」，而不是退化成 [0,0] 的空区间——
                 // 后者在下游看来是「第 0 层有东西」。
                 SLICESOFT_EXPECT_FALSE(ranges.at(3).hasModel, "空列不应标记为有模型");
                 SLICESOFT_EXPECT_EQ(ranges.at(3).lowerLayer, -1, "空列下界须为 -1");
                 SLICESOFT_EXPECT_EQ(ranges.at(3).upperLayer, -1, "空列上界须为 -1");
             }},
            {"first model layer agrees with the range lower bound",
             [] {
                 // 两个函数各自独立地扫同一批掩膜求「最低命中层」。
                 // 它们必须一致——只改其中一个是最可能发生的漂移，而产物上看不出来。
                 const auto masks = MakeProbeMasks();
                 const auto grid = MakeTinyGrid();
                 const auto first = relief::compute_first_model_layers(masks, grid);
                 const auto ranges = relief::compute_mask_column_ranges(masks, grid);
                 for (std::size_t i{0}; i < first.size(); ++i)
                 {
                     if (ranges.at(i).hasModel)
                     {
                         SLICESOFT_EXPECT_EQ(
                             first.at(i), ranges.at(i).lowerLayer, "两者的最低层应一致");
                     }
                     else
                     {
                         SLICESOFT_EXPECT_EQ(first.at(i), -1, "无区间的列首层应为 -1");
                     }
                 }
             }},
            {"relief lower layers respect has_model",
             [] {
                 // has_model 为假时，即便 lower_layer 有值也必须返回 -1。
                 // 只看 lower_layer >= 0 会让残留值被当成真实命中。
                 const std::vector<ReliefColumnInfo> columns{
                     MakeColumn(true, 2, 5),
                     MakeColumn(false, 2, 5),
                     MakeColumn(true, -1, 5),
                 };
                 const auto lower = relief::compute_relief_lower_layers(columns);
                 SLICESOFT_EXPECT_EQ(lower.at(0), 2, "有模型且层号合法时取该层");
                 SLICESOFT_EXPECT_EQ(lower.at(1), -1, "has_model 为假须返回 -1");
                 SLICESOFT_EXPECT_EQ(lower.at(2), -1, "层号为负须返回 -1");
             }},
            {"an inverted relief column is rejected",
             [] {
                 // upper < lower 的列是坏数据。宁可判成「无区间」，
                 // 也不要原样传下去——倒置区间在下游是个越界循环。
                 const std::vector<ReliefColumnInfo> columns{
                     MakeColumn(true, 1, 4),
                     MakeColumn(true, 4, 1),
                     MakeColumn(false, 1, 4),
                 };
                 const auto ranges = relief::compute_relief_column_ranges(columns);
                 SLICESOFT_EXPECT_TRUE(ranges.at(0).hasModel, "正常列应有区间");
                 SLICESOFT_EXPECT_EQ(ranges.at(0).lowerLayer, 1, "正常列下界");
                 SLICESOFT_EXPECT_EQ(ranges.at(0).upperLayer, 4, "正常列上界");
                 SLICESOFT_EXPECT_FALSE(ranges.at(1).hasModel, "倒置区间须被拒");
                 SLICESOFT_EXPECT_FALSE(ranges.at(2).hasModel, "无模型的列须被拒");
             }},
        });
}
