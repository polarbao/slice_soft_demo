// F-09 单测缺口的第一批（R-12 验证 Gate 第 4 条）。
//
// R-12 写着「搬出的每个函数至少 1 条新单测（这是搬迁的目的，不是附加项）」，
// 而八步搬运新增单测为零。R-12 自己的话：「搬出来的代码如果不加测试，
// 只是把 5888 行分成几个文件，债没减。」
//
// 本批只覆盖【无需重型夹具】的公开入口——那正是搬出来之后才第一次可单测的部分。
// 需要 GridSpec / SliceConfig / ModelReport 的入口留待后续批次，它们要先有夹具。
//
// 选用例的标准是「这条断言失败时，我能说出哪个行为坏了」，不是凑数量。

#include "Cases.h"

#include "slicer_core/geometry/SliceMaskRasterizer.h"
#include "slicer_core/materials/SliceMaterialTexture.h"
#include "slicer_core/output/preview/LayerPreviewWriter.h"
#include "slicer_core/output/reports/SliceReportJson.h"
#include "slicer_core/pipeline/SliceProgressNotifier.h"
#include "slicer_core/reports/CrossSectionStackReport.h"
#include "slicer_core/support/SliceSupportGeneration.h"

#include "tests/support/Expect.h"

#include <array>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

namespace {

slicer_core::Triangle MakeTriangle(
    const double ax, const double ay,
    const double bx, const double by,
    const double cx, const double cy)
{
    slicer_core::Triangle triangle{};
    triangle.a.x = ax; triangle.a.y = ay; triangle.a.z = 0.0;
    triangle.b.x = bx; triangle.b.y = by; triangle.b.z = 0.0;
    triangle.c.x = cx; triangle.c.y = cy; triangle.c.z = 0.0;
    return triangle;
}

slicer_core::Vec3 MakePoint(const double x, const double y)
{
    slicer_core::Vec3 point{};
    point.x = x; point.y = y; point.z = 0.0;
    return point;
}

/// 造一层 RGBWSV 缓冲：6 通道交错，255 表示空白，0 表示满印。
std::vector<std::uint8_t> MakeRgbwsvLayer(const std::size_t pixelCount)
{
    return std::vector<std::uint8_t>(pixelCount * 6U, 255U);
}

}  // namespace

int main()
{
    using namespace slicer_core;
    const int extracted = slicesoft_test::RunCases(
        "slicer extracted units, batches 1-2 (F-09)",
        {
            // ---- output/preview/LayerPreviewWriter ----
            {"layer_file_name pads to six digits",
             [] {
                 // 层文件名的位宽是包协议的一部分：改成五位会让读包端按名字排序时错位。
                 SLICESOFT_EXPECT_EQ(
                     preview::layer_file_name(0), std::string{"layers/layer_000000.tiff"},
                     "首层文件名");
                 SLICESOFT_EXPECT_EQ(
                     preview::layer_file_name(123456), std::string{"layers/layer_123456.tiff"},
                     "六位边界不应溢出成七位");
             }},
            {"canonical_preview_channel folds aliases",
             [] {
                 // 三个历史别名必须折叠到同一规范名，否则同一通道会产出两种文件名。
                 SLICESOFT_EXPECT_EQ(
                     preview::canonical_preview_channel("model_rgb"), std::string{"rgb"},
                     "model_rgb 应折叠为 rgb");
                 SLICESOFT_EXPECT_EQ(
                     preview::canonical_preview_channel("model_rgb_true_color"),
                     std::string{"texture_rgb"}, "true_color 别名");
                 SLICESOFT_EXPECT_EQ(
                     preview::canonical_preview_channel("true_rgb"),
                     std::string{"texture_rgb"}, "true_rgb 别名与上条同名");
             }},
            {"canonical_preview_channel passes unknown through",
             [] {
                 // 未知通道原样透传，而不是静默改名或丢弃——这是本仓一贯的口径。
                 SLICESOFT_EXPECT_EQ(
                     preview::canonical_preview_channel("varnish"), std::string{"varnish"},
                     "未知通道应原样返回");
             }},

            // ---- pipeline/SliceProgressNotifier ----
            {"layer progress throttles to about one percent",
             [] {
                 // 首层与末层必报，中间按 max(1,(n+99)/100) 节流。
                 SLICESOFT_EXPECT_TRUE(
                     progress::ShouldNotifyLayerProgress(1, 1000), "首层必报");
                 SLICESOFT_EXPECT_TRUE(
                     progress::ShouldNotifyLayerProgress(1000, 1000), "末层必报");
                 SLICESOFT_EXPECT_TRUE(
                     progress::ShouldNotifyLayerProgress(1010, 1000), "超出层数也按末层处理");
                 // 1000 层时间隔为 10，第 10 层报、第 11 层不报。
                 SLICESOFT_EXPECT_TRUE(
                     progress::ShouldNotifyLayerProgress(10, 1000), "间隔点应报");
                 SLICESOFT_EXPECT_FALSE(
                     progress::ShouldNotifyLayerProgress(11, 1000), "非间隔点不应报");
             }},
            {"tiny stacks never divide by zero",
             [] {
                 // 间隔取 max(1, ...)，所以层数极小时不会除零，且每层都报。
                 SLICESOFT_EXPECT_TRUE(progress::ShouldNotifyLayerProgress(1, 1), "单层");
                 SLICESOFT_EXPECT_TRUE(progress::ShouldNotifyLayerProgress(2, 2), "两层的末层");
             }},

            // ---- geometry/SliceMaskRasterizer ----
            {"layer index helpers bracket a z value",
             [] {
                 // 同一 z 上，above 与 below 必须夹住它；层厚 0.1mm 时 z=0.25 落在第 2/3 层之间。
                 const int above = masks::first_layer_at_or_above_z(0.25, 0.1);
                 const int below = masks::last_layer_at_or_below_z(0.25, 0.1);
                 SLICESOFT_EXPECT_TRUE(above >= below, "above 不应小于 below");
                 SLICESOFT_EXPECT_TRUE(above - below <= 1, "两者最多相差一层");
             }},
            {"layer index helpers agree on an exact layer centre",
             [] {
                 // 【判据是层中心，不是层边界】——两者算的是 ceil/floor(z/t - 0.5)，
                 // 即层 k 的中心在 (k+0.5)*t。只有 z 正好落在某层中心时两者才相等。
                 //
                 // 取值必须在二进制里精确：0.3/0.1 实际是 2.9999999999999996，
                 // 用它做「精确边界」的断言测的是浮点误差而不是本函数的约定。
                 // 1.25/0.5 = 2.5 两侧都精确，减 0.5 得整数 2。
                 SLICESOFT_EXPECT_EQ(
                     masks::first_layer_at_or_above_z(1.25, 0.5),
                     masks::last_layer_at_or_below_z(1.25, 0.5),
                     "层中心上两者应一致");
                 SLICESOFT_EXPECT_EQ(
                     masks::first_layer_at_or_above_z(1.25, 0.5), 2, "该中心属第 2 层");
             }},
            {"point_in_triangle_xy accepts an interior point",
             [] {
                 const auto triangle = MakeTriangle(0.0, 0.0, 10.0, 0.0, 0.0, 10.0);
                 double w0{0.0}; double w1{0.0}; double w2{0.0};
                 SLICESOFT_EXPECT_TRUE(
                     masks::point_in_triangle_xy(MakePoint(1.0, 1.0), triangle, w0, w1, w2),
                     "三角形内部的点应命中");
                 // 重心坐标必须归一：不归一会让贴图取色按错误权重插值。
                 const double sum = w0 + w1 + w2;
                 SLICESOFT_EXPECT_TRUE(
                     sum > 0.999 && sum < 1.001, "重心坐标之和应为 1");
             }},
            {"point_in_triangle_xy rejects an outside point",
             [] {
                 const auto triangle = MakeTriangle(0.0, 0.0, 10.0, 0.0, 0.0, 10.0);
                 double w0{0.0}; double w1{0.0}; double w2{0.0};
                 SLICESOFT_EXPECT_FALSE(
                     masks::point_in_triangle_xy(MakePoint(9.0, 9.0), triangle, w0, w1, w2),
                     "斜边之外的点不应命中");
             }},

            // ---- output/reports/SliceReportJson ----
            {"rgb_to_json emits three numbers in order",
             [] {
                 const std::array<std::uint8_t, 3> rgb{1U, 2U, 3U};
                 const std::string text = reports::rgb_to_json(rgb).dump(0);
                 // 顺序是报告契约的一部分：颠倒会让下游把 R 当成 B。
                 SLICESOFT_EXPECT_TRUE(
                     text.find('1') < text.find('2') && text.find('2') < text.find('3'),
                     "RGB 顺序应保持");
             }},

            // ---- materials/SliceMaterialTexture ----
            {"ModelFillMaterialToString covers every enumerator",
             [] {
                 // 四个枚举值各有其名，且互不相同——重名会让报告里两种填充无法区分。
                 const std::string rgb =
                     materials::ModelFillMaterialToString(ModelFillMaterial::Rgb);
                 const std::string white =
                     materials::ModelFillMaterialToString(ModelFillMaterial::White);
                 const std::string varnish =
                     materials::ModelFillMaterialToString(ModelFillMaterial::Varnish);
                 const std::string none =
                     materials::ModelFillMaterialToString(ModelFillMaterial::None);
                 SLICESOFT_EXPECT_FALSE(rgb.empty(), "Rgb 应有名字");
                 SLICESOFT_EXPECT_NE(rgb, white, "Rgb 与 White 不应同名");
                 SLICESOFT_EXPECT_NE(white, varnish, "White 与 Varnish 不应同名");
                 SLICESOFT_EXPECT_NE(varnish, none, "Varnish 与 None 不应同名");
             }},

            // ---- 批次 2：需手工造结构体、但不需重型夹具的入口 ----

            // ---- pipeline/SliceProgressNotifier ----
            {"ElapsedMsSince never runs backwards",
             [] {
                 const auto start = SlicerClock::now();
                 const double first = progress::ElapsedMsSince(start);
                 const double second = progress::ElapsedMsSince(start);
                 // 用 steady_clock 而非 system_clock 的理由就在这条：改成挂钟后，
                 // 对时或夏令时会让进度里的耗时变成负数。
                 SLICESOFT_EXPECT_TRUE(first >= 0.0, "耗时不应为负");
                 SLICESOFT_EXPECT_TRUE(second >= first, "同一起点的耗时不应倒退");
             }},

            // ---- support/SliceSupportGeneration ----
            {"ResetSupportGenerationStats clears only what is recomputed",
             [] {
                 // 【这是本函数最容易被「顺手修好」改坏的地方】它只清 8 个字段，
                 // 刻意留着 island 系列。理由在唯一调用方 CalculateSupportGenerationStats：
                 // 它先 Reset 再逐层 AccumulateSupportLayerStats，而 Accumulate 只重算那 8 个；
                 // island 统计是在 generate_support_masks 的连通域分析里算的，没人会补。
                 // 把 Reset 改成「全清」看起来更整齐，实际会让报告里的岛统计恒为 0——
                 // 不崩、不报错、产物照出，只是数字没了。
                 SupportGenerationResult result;
                 result.support_masks.assign(2U, std::vector<std::uint8_t>(4U, 1U));
                 result.support_pixels = 7;
                 result.layers_with_support = 2;
                 result.bottom_projection_support_pixels = 11;
                 result.island_count = 3;
                 result.island_pixels = 40;
                 result.filtered_island_count = 1;
                 result.layers_with_islands = 2;
                 result.unsupported_pixels = 9;
                 support::ResetSupportGenerationStats(result);
                 // 会被重算的：必须清。
                 SLICESOFT_EXPECT_EQ(result.support_pixels, 0, "支撑像素须清零");
                 SLICESOFT_EXPECT_EQ(result.layers_with_support, 0, "层计数须清零");
                 SLICESOFT_EXPECT_EQ(
                     result.bottom_projection_support_pixels, 0, "分型计数须清零");
                 // 不会被重算的：必须留。
                 SLICESOFT_EXPECT_EQ(result.island_count, 3, "岛计数不应被清");
                 SLICESOFT_EXPECT_EQ(result.island_pixels, 40, "岛像素不应被清");
                 SLICESOFT_EXPECT_EQ(result.filtered_island_count, 1, "过滤岛计数不应被清");
                 SLICESOFT_EXPECT_EQ(result.layers_with_islands, 2, "含岛层数不应被清");
                 SLICESOFT_EXPECT_EQ(result.unsupported_pixels, 9, "无支撑像素不应被清");
                 // 掩膜【不】清——重算统计时掩膜正是输入，清掉会让重算得到全零。
                 SLICESOFT_EXPECT_EQ(
                     result.support_masks.size(), std::size_t{2}, "掩膜不应被一并清空");
             }},
            {"outer varnish wins over support where they overlap",
             [] {
                 SupportGenerationResult result;
                 result.support_masks.assign(1U, std::vector<std::uint8_t>{1U, 1U, 0U, 1U});
                 result.support_type_maps.assign(
                     1U, std::vector<SupportType>(4U, SupportType::BottomProjection));
                 const std::vector<std::vector<std::uint8_t>> varnish{{1U, 0U, 1U, 0U}};
                 const int cleared =
                     support::ApplyOuterVarnishSupportPriority(varnish, result, nullptr);
                 // 只有【两者都非零】的像素被清：下标 0 命中，2 号位支撑本就为 0。
                 SLICESOFT_EXPECT_EQ(cleared, 1, "应恰好清掉一个像素");
                 SLICESOFT_EXPECT_EQ(
                     static_cast<int>(result.support_masks.at(0).at(0)), 0, "重叠处支撑须清零");
                 SLICESOFT_EXPECT_TRUE(
                     result.support_type_maps.at(0).at(0) == SupportType::None,
                     "清掉的像素类型须改为 None");
                 SLICESOFT_EXPECT_EQ(
                     static_cast<int>(result.support_masks.at(0).at(1)), 1, "无光油处支撑须保留");
             }},
            {"cleared-index out param is sized even with no varnish",
             [] {
                 // 这条钉的是一处容易在重构中丢掉的契约：光油掩膜为空时函数提前返回，
                 // 但出参【在提前返回之前】就已按层数铺好。调用方是按层下标直接取的，
                 // 把 assign 挪到空检查之后会让它们越界。
                 SupportGenerationResult result;
                 result.support_masks.assign(3U, std::vector<std::uint8_t>(2U, 1U));
                 result.support_type_maps.assign(
                     3U, std::vector<SupportType>(2U, SupportType::BottomProjection));
                 std::vector<std::vector<std::size_t>> clearedIndices;
                 const int cleared = support::ApplyOuterVarnishSupportPriority(
                     {}, result, &clearedIndices);
                 SLICESOFT_EXPECT_EQ(cleared, 0, "无光油则不应清任何像素");
                 SLICESOFT_EXPECT_EQ(
                     clearedIndices.size(), std::size_t{3}, "出参仍须按层数铺好");
             }},
            {"LiftModelForSupportBase moves triangles and bbox together",
             [] {
                 ModelReport report;
                 report.triangles.push_back(MakeTriangle(0.0, 0.0, 1.0, 0.0, 0.0, 1.0));
                 report.bbox_mm.min.z = 0.0;
                 report.bbox_mm.max.z = 5.0;
                 support::LiftModelForSupportBase(report, 2.0);
                 // 三角形与包围盒必须一起抬——只抬一个会让后续按 bbox 算的层数与实际几何错位。
                 SLICESOFT_EXPECT_TRUE(
                     report.triangles.at(0).a.z > 1.999
                         && report.triangles.at(0).a.z < 2.001,
                     "三角形顶点须抬升 2mm");
                 SLICESOFT_EXPECT_TRUE(
                     report.bbox_mm.min.z > 1.999 && report.bbox_mm.min.z < 2.001,
                     "包围盒下沿须同步抬升");
                 SLICESOFT_EXPECT_TRUE(
                     report.bbox_mm.max.z > 6.999 && report.bbox_mm.max.z < 7.001,
                     "包围盒上沿须同步抬升");
             }},
            {"a non-positive lift is a no-op",
             [] {
                 ModelReport report;
                 report.bbox_mm.min.z = 4.0;
                 support::LiftModelForSupportBase(report, 0.0);
                 support::LiftModelForSupportBase(report, -3.0);
                 // 负抬升不该把模型压到台面以下——直接不动，而不是照算。
                 SLICESOFT_EXPECT_TRUE(
                     report.bbox_mm.min.z > 3.999 && report.bbox_mm.min.z < 4.001,
                     "非正抬升须原样不动");
             }},

            // ---- reports/CrossSectionStackReport ----
            {"update_layer_channel_stats writes all five fields",
             [] {
                 // 本函数只做一件事：把分析结果的 5 个字段搬进 diagnostics。
                 // 少搬一个不会编译失败也不会崩，只会让报告里那一项恒为 0——故逐个钉住。
                 auto layer = MakeRgbwsvLayer(2U);
                 layer.at(0) = 0U;   // 像素 0 的 R 通道满印
                 layer.at(3) = 0U;   // 像素 0 的 W 通道满印
                 layer.at(4) = 0U;   // 像素 0 的 S 通道满印
                 layer.at(5) = 0U;   // 像素 0 的 V 通道满印
                 LayerDiagnostics diagnostics;
                 reports::update_layer_channel_stats(layer, diagnostics);
                 SLICESOFT_EXPECT_TRUE(
                     diagnostics.rgb_non_zero_pixels > 0, "rgb 字段未被写入");
                 SLICESOFT_EXPECT_TRUE(
                     diagnostics.white_non_zero_pixels > 0, "white 字段未被写入");
                 SLICESOFT_EXPECT_TRUE(
                     diagnostics.support_non_zero_pixels > 0, "support 字段未被写入");
                 SLICESOFT_EXPECT_TRUE(
                     diagnostics.varnish_non_zero_pixels > 0, "varnish 字段未被写入");
                 // 第 5 个字段是整组通道统计：R 通道见过 0 与 255 两种值。
                 SLICESOFT_EXPECT_EQ(
                     diagnostics.channel_stats.at(0).min_value, 0, "通道统计未被写入");
             }},
            {"a buffer that is not a whole number of pixels throws",
             [] {
                 // 6 通道交错，缓冲长度必须是 6 的倍数。不整除时宁可抛，
                 // 也不要按整除数默默少算一个像素——那种错误在产物里看不出来。
                 std::vector<std::uint8_t> ragged(7U, 255U);
                 LayerDiagnostics diagnostics;
                 std::string thrown;
                 try
                 {
                     reports::update_layer_channel_stats(ragged, diagnostics);
                 }
                 catch (const std::exception& error)
                 {
                     thrown = error.what();
                 }
                 SLICESOFT_EXPECT_FALSE(thrown.empty(), "长度不整除须抛出");
             }},
            {"merge_channel_stats accumulates across layers",
             [] {
                 std::array<ChannelStats, rgbwsv_channel_count> totals{};
                 LayerDiagnostics diagnostics;
                 diagnostics.channel_stats.at(0).print_pixels = 4U;
                 reports::merge_channel_stats(totals, diagnostics);
                 reports::merge_channel_stats(totals, diagnostics);
                 // 累加而非覆盖：写成覆盖时单层报告仍然正确，只有多层汇总会偏小。
                 SLICESOFT_EXPECT_EQ(
                     totals.at(0).print_pixels, std::uint64_t{8}, "两层应累加为 8");
             }},
            {"merge_semantic_stats accumulates across layers",
             [] {
                 LayerSemanticStats totals{};
                 LayerSemanticStats layer{};
                 layer.model_fill_pixels = 3;
                 layer.outer_varnish_pixels = 5;
                 reports::merge_semantic_stats(totals, layer);
                 reports::merge_semantic_stats(totals, layer);
                 SLICESOFT_EXPECT_EQ(totals.model_fill_pixels, 6, "填充像素应累加");
                 SLICESOFT_EXPECT_EQ(totals.outer_varnish_pixels, 10, "光油像素应累加");
             }},

            // ---- output/reports/SliceReportJson ----
            {"bbox_to_json keeps xyz order on both corners",
             [] {
                 BoundingBox bbox{};
                 bbox.min.x = 1.0; bbox.min.y = 2.0; bbox.min.z = 3.0;
                 bbox.max.x = 4.0; bbox.max.y = 5.0; bbox.max.z = 6.0;
                 const std::string text = reports::bbox_to_json(bbox).dump(0);
                 // 顺序颠倒会让下游把 X 当成 Z，而数值本身都「看着合理」，肉眼查不出来。
                 SLICESOFT_EXPECT_TRUE(
                     text.find("1") < text.find("2") && text.find("2") < text.find("3"),
                     "min 角须按 xyz 排列");
                 SLICESOFT_EXPECT_TRUE(
                     text.find("4") < text.find("5") && text.find("5") < text.find("6"),
                     "max 角须按 xyz 排列");
             }},
            {"channel_stats_array_to_json names all six channels",
             [] {
                 const std::array<ChannelStats, rgbwsv_channel_count> stats{};
                 const std::string text = reports::channel_stats_array_to_json(stats).dump(0);
                 // 六个通道名是报告契约的一部分：少一个或改名，下游按键取值就取空。
                 for (const char* name : {"\"R\"", "\"G\"", "\"B\"",
                                          "\"W\"", "\"S\"", "\"V\""})
                 {
                     SLICESOFT_EXPECT_TRUE(
                         text.find(name) != std::string::npos, "通道名缺失");
                 }
             }},
        });

    // 各批自成一套、各自打印。用 | 而非 || ：即便前一批已经红了，
    // 后一批也要跑完并报出全部失败，否则一次改动只能看到最前面那条。
    const int reportJson = RunReportJsonCases();
    return extracted | reportJson;
}
