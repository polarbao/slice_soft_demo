// F-09 单测缺口的第四批（R-12 验证 Gate 第 4 条）：掩膜栅格化单元的其余四个入口。
//
// 前三批取的是纯函数，本批是八个单元里第一次碰【核心逻辑】：
// 模型采样、外光油壳、上表面支撑边界。它们要吃 GridSpec + SliceConfig + 真实三角形，
// 但这三样都是带默认值的聚合体，小尺寸即可，不需要真实模型文件。
//
// 本批最要紧的一条是「内部空腔不上光油」——那是 BuildExternalEmptyMask 存在的
// 全部理由。少了它，模型内部的封闭空腔会被当作外表面刷上光油，
// 产物看起来仍然「有光油」，只是刷在了打印不到的地方。

#include "slicer_core/geometry/SliceMaskRasterizer.h"

#include "tests/support/Expect.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {

using slicer_core::GridSpec;
using slicer_core::ModelReport;
using slicer_core::SliceConfig;
using slicer_core::Triangle;
using slicer_core::Vec3;

Vec3 P(const double x, const double y, const double z)
{
    Vec3 point{};
    point.x = x; point.y = y; point.z = z;
    return point;
}

Triangle Tri(const Vec3& a, const Vec3& b, const Vec3& c)
{
    Triangle triangle{};
    triangle.a = a; triangle.b = b; triangle.c = c;
    return triangle;
}

/// 轴对齐长方体的 12 个三角形。扫描线填充按交点奇偶判定，闭合即可。
std::vector<Triangle> MakeBox(
    const double x0, const double y0, const double z0,
    const double x1, const double y1, const double z1)
{
    const Vec3 a = P(x0, y0, z0), b = P(x1, y0, z0), c = P(x1, y1, z0), d = P(x0, y1, z0);
    const Vec3 e = P(x0, y0, z1), f = P(x1, y0, z1), g = P(x1, y1, z1), h = P(x0, y1, z1);
    return {
        Tri(a, c, b), Tri(a, d, c),   // 底
        Tri(e, f, g), Tri(e, g, h),   // 顶
        Tri(a, b, f), Tri(a, f, e),   // -Y
        Tri(b, c, g), Tri(b, g, f),   // +X
        Tri(c, d, h), Tri(c, h, g),   // +Y
        Tri(d, a, e), Tri(d, e, h),   // -X
    };
}

GridSpec MakeGrid(const int width, const int height, const int layers)
{
    GridSpec grid{};
    grid.width_px = width;
    grid.height_px = height;
    grid.layer_count = layers;
    grid.pixel_size_x_mm = 1.0;
    grid.pixel_size_y_mm = 1.0;
    return grid;
}

/// 7x7 幅面上的 5x5 实心块（含一个【封闭的内部空腔】：正中那一格）。
std::vector<std::uint8_t> MakeBlockWithVoid()
{
    std::vector<std::uint8_t> mask(49U, 0U);
    for (int y{1}; y <= 5; ++y)
    {
        for (int x{1}; x <= 5; ++x)
        {
            mask.at(static_cast<std::size_t>(y) * 7U + x) = 1U;
        }
    }
    mask.at(3U * 7U + 3U) = 0U;  // 正中挖空，四周被模型完全包住
    return mask;
}

std::size_t CountNonZero(const std::vector<std::uint8_t>& mask)
{
    std::size_t count{0};
    for (const std::uint8_t value : mask)
    {
        if (value != 0U) { ++count; }
    }
    return count;
}

}  // namespace

int main()
{
    using namespace slicer_core;
    return slicesoft_test::RunCases(
        "slicer mask rasterizer (F-09 batch 4)",
        {
            {"sampling reports the layer centre as z",
             [] {
                 // 【这条钉的是整套层号约定的根】z = (层号 + 0.5) * 层厚，
                 // 也就是 first_layer_at_or_above_z 里那个 -0.5 的来源。
                 // 改成 层号*层厚 会让每层整体下移半层，产物仍然「像模型」，只是错位。
                 ModelReport report;
                 report.triangles = MakeBox(1.0, 1.0, 0.0, 4.0, 4.0, 2.0);
                 report.bbox_mm.min = P(1.0, 1.0, 0.0);
                 report.bbox_mm.max = P(4.0, 4.0, 2.0);
                 std::vector<LayerDiagnostics> diagnostics;
                 const auto masks =
                     masks::sample_model_masks(report, MakeGrid(6, 6, 3), 1.0, diagnostics);
                 SLICESOFT_EXPECT_EQ(masks.size(), std::size_t{3}, "每层一张掩膜");
                 SLICESOFT_EXPECT_EQ(diagnostics.size(), std::size_t{3}, "每层一条诊断");
                 SLICESOFT_EXPECT_EQ(diagnostics.at(0).layer_index, 0, "层号须按序");
                 SLICESOFT_EXPECT_TRUE(
                     diagnostics.at(0).z_mm > 0.499 && diagnostics.at(0).z_mm < 0.501,
                     "第 0 层的 z 应为 0.5 而非 0");
                 SLICESOFT_EXPECT_TRUE(
                     diagnostics.at(1).z_mm > 1.499 && diagnostics.at(1).z_mm < 1.501,
                     "第 1 层的 z 应为 1.5");
             }},
            {"layers outside the bounding box stay present and empty",
             [] {
                 // 包围盒之外的层【仍然要占位】，只是全零。
                 // 写成 continue 不 push 会让掩膜下标与层号错位——
                 // 那不是少一层的问题，是此后每一层都对应错了。
                 ModelReport report;
                 report.triangles = MakeBox(1.0, 1.0, 0.0, 4.0, 4.0, 2.0);
                 report.bbox_mm.min = P(1.0, 1.0, 0.0);
                 report.bbox_mm.max = P(4.0, 4.0, 2.0);
                 std::vector<LayerDiagnostics> diagnostics;
                 const auto masks =
                     masks::sample_model_masks(report, MakeGrid(6, 6, 3), 1.0, diagnostics);
                 // 第 2 层的 z = 2.5，已越过包围盒上沿 2.0。
                 SLICESOFT_EXPECT_EQ(masks.size(), std::size_t{3}, "越界层仍须占位");
                 SLICESOFT_EXPECT_EQ(
                     CountNonZero(masks.at(2)), std::size_t{0}, "越界层须全零");
                 SLICESOFT_EXPECT_EQ(
                     diagnostics.at(2).segment_count, 0, "越界层不应有线段");
                 SLICESOFT_EXPECT_EQ(
                     masks.at(2).size(), std::size_t{36}, "越界层的掩膜仍是整幅面");
             }},
            {"a box cross-section rasterises to filled pixels",
             [] {
                 ModelReport report;
                 report.triangles = MakeBox(1.0, 1.0, 0.0, 4.0, 4.0, 2.0);
                 report.bbox_mm.min = P(1.0, 1.0, 0.0);
                 report.bbox_mm.max = P(4.0, 4.0, 2.0);
                 std::vector<LayerDiagnostics> diagnostics;
                 const auto masks =
                     masks::sample_model_masks(report, MakeGrid(6, 6, 3), 1.0, diagnostics);
                 // 不钉具体像素数：那取决于栅格化的取样点约定，钉死会让无关改动误红。
                 // 钉的是「包围盒内的层确实出了东西」，以及诊断里记到了线段。
                 SLICESOFT_EXPECT_TRUE(
                     CountNonZero(masks.at(0)) > 0U, "包围盒内的层应有填充");
                 SLICESOFT_EXPECT_TRUE(
                     diagnostics.at(0).segment_count > 0, "应切出线段");
             }},
            {"outer varnish is absent, not zero-filled, when disabled",
             [] {
                 // 关闭时返回【空向量】而不是一摞全零掩膜——调用方以此区分
                 // 「没开这个功能」与「开了但这层没有光油」。
                 SliceConfig config;
                 const std::vector<std::vector<std::uint8_t>> modelMasks{MakeBlockWithVoid()};
                 const auto varnish =
                     masks::BuildOuterVarnishMasks(config, MakeGrid(7, 7, 1), modelMasks);
                 SLICESOFT_EXPECT_TRUE(varnish.empty(), "关闭时须返回空而非全零");
             }},
            {"an enclosed void gets no varnish",
             [] {
                 // 【本批最要紧的一条】5x5 实心块正中挖掉一格，四周被模型完全包住。
                 // 那一格虽然「不是模型、且紧邻模型」，但它不与外部连通，
                 // 所以不是外表面，不该上光油。BuildExternalEmptyMask 就是为它而存在。
                 // 去掉那道判定后产物仍然有光油，只是刷在了打印不到的封闭腔里。
                 SliceConfig config;
                 config.outer_varnish.enabled = true;
                 config.outer_varnish.thickness_mm = 1.0;
                 const std::vector<std::vector<std::uint8_t>> modelMasks{MakeBlockWithVoid()};
                 const auto varnish =
                     masks::BuildOuterVarnishMasks(config, MakeGrid(7, 7, 1), modelMasks);
                 SLICESOFT_EXPECT_EQ(varnish.size(), std::size_t{1}, "开启时须逐层产出");
                 SLICESOFT_EXPECT_EQ(
                     static_cast<int>(varnish.at(0).at(3U * 7U + 3U)), 0,
                     "封闭空腔不应上光油");
                 SLICESOFT_EXPECT_TRUE(
                     CountNonZero(varnish.at(0)) > 0U, "外表面应确实上了光油");
             }},
            {"varnish never overlaps the model",
             [] {
                 // 光油是【模型之外】的一圈壳。与模型重叠意味着覆盖了本该打印的像素。
                 SliceConfig config;
                 config.outer_varnish.enabled = true;
                 config.outer_varnish.thickness_mm = 1.0;
                 const auto model = MakeBlockWithVoid();
                 const std::vector<std::vector<std::uint8_t>> modelMasks{model};
                 const auto varnish =
                     masks::BuildOuterVarnishMasks(config, MakeGrid(7, 7, 1), modelMasks);
                 for (std::size_t i{0}; i < model.size(); ++i)
                 {
                     if (model.at(i) != 0U)
                     {
                         SLICESOFT_EXPECT_EQ(
                             static_cast<int>(varnish.at(0).at(i)), 0, "模型像素上不应有光油");
                     }
                 }
             }},
            {"upper support boundary needs all three conditions",
             [] {
                 // 三个条件是【与】关系，缺一不可。改成【或】不会崩，
                 // 只会在没配光油壳时把边界源头记成另一个名字，而那个名字会进报告。
                 SliceConfig config;
                 config.support.upper.outside = "outer_varnish_shell";
                 config.outer_varnish.enabled = true;
                 config.outer_varnish.thickness_mm = 0.5;
                 const auto all = masks::ResolveUpperSupportBoundaryInfo(config);
                 SLICESOFT_EXPECT_TRUE(all.includes_outer_varnish_shell, "三条齐备时应启用");
                 SLICESOFT_EXPECT_NE(
                     all.source, std::string{"model_envelope"}, "启用时来源应改名");

                 SliceConfig noThickness = config;
                 noThickness.outer_varnish.thickness_mm = 0.0;
                 SLICESOFT_EXPECT_FALSE(
                     masks::ResolveUpperSupportBoundaryInfo(noThickness)
                         .includes_outer_varnish_shell,
                     "厚度为零时不应启用");

                 SliceConfig disabled = config;
                 disabled.outer_varnish.enabled = false;
                 SLICESOFT_EXPECT_FALSE(
                     masks::ResolveUpperSupportBoundaryInfo(disabled)
                         .includes_outer_varnish_shell,
                     "光油壳关闭时不应启用");

                 SliceConfig otherOutside = config;
                 otherOutside.support.upper.outside = "model_envelope";
                 SLICESOFT_EXPECT_FALSE(
                     masks::ResolveUpperSupportBoundaryInfo(otherOutside)
                         .includes_outer_varnish_shell,
                     "外侧判据不同时不应启用");
             }},
            {"boundary masks are the union of model and varnish",
             [] {
                 UpperSupportBoundaryInfo info;
                 info.includes_outer_varnish_shell = true;
                 const std::vector<std::vector<std::uint8_t>> model{{1U, 0U, 0U, 0U}};
                 const std::vector<std::vector<std::uint8_t>> varnish{{0U, 1U, 0U, 0U}};
                 const auto boundary = masks::BuildUpperSupportBoundaryMasks(
                     MakeGrid(2, 2, 1), model, varnish, info);
                 // 并集：两边任一非零即置位。写成交集会让边界缩到几乎为空。
                 SLICESOFT_EXPECT_EQ(
                     static_cast<int>(boundary.at(0).at(0)), 1, "模型像素应入边界");
                 SLICESOFT_EXPECT_EQ(
                     static_cast<int>(boundary.at(0).at(1)), 1, "光油像素应入边界");
                 SLICESOFT_EXPECT_EQ(
                     static_cast<int>(boundary.at(0).at(2)), 0, "两边都空的像素不应入边界");
             }},
            {"boundary masks are absent when the shell is not included",
             [] {
                 // 同样是「空而非全零」的区分。
                 const UpperSupportBoundaryInfo info;
                 const std::vector<std::vector<std::uint8_t>> model{{1U, 1U, 1U, 1U}};
                 const std::vector<std::vector<std::uint8_t>> varnish{{1U, 1U, 1U, 1U}};
                 const auto boundary = masks::BuildUpperSupportBoundaryMasks(
                     MakeGrid(2, 2, 1), model, varnish, info);
                 SLICESOFT_EXPECT_TRUE(boundary.empty(), "未启用时须返回空而非全零");
             }},
        });
}
