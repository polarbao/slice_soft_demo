// F-09 单测缺口的第八批：枚举翻译、配置/生效之分、报告 schema。
//
// 三个入口互不相干，但坏掉的方式是同一种：**产出的结构看上去完整、字段一个不少，
// 只是某个值悄悄变成了另一个**。所以断言都压在「取值之间的区别」上，不在「有没有这个字段」。

#include "Cases.h"

#include "slicer_core/material/MaterialClosureRepair.h"
#include "slicer_core/materials/SliceMaterialTexture.h"
#include "slicer_core/reports/CrossSectionStackReport.h"
#include "slicer_core/support/SliceSupportGeneration.h"

#include "tests/support/Expect.h"

#include <string>

namespace {

using slicer_core::MaterialClosureModelFillMaterial;
using slicer_core::SliceConfig;
using slicer_core::SupportBaseProjectionResult;
using slicer_core::SupportGenerationResult;

SliceConfig FillMaterial(const char* const name)
{
    SliceConfig config;
    config.model_fill.material = name;
    return config;
}

/// 一份「开着、且确实做了事」的基面投影结果。
SupportBaseProjectionResult WorkingProjection()
{
    SupportBaseProjectionResult result;
    result.enabled = true;
    result.configured_layer_count = 5;
    result.effective_layer_count = 3;
    result.footprint_pixels = 40;
    result.added_layer_count = 2;
    result.added_support_pixels = 12;
    return result;
}

}  // namespace

int RunClosureAndReportCases()
{
    using namespace slicer_core;
    return slicesoft_test::RunCases(
        "closure values and report shapes (F-09 batch 8)",
        {
            // ---- materials/SliceMaterialTexture ----
            {"closure repair translates each fill material distinctly",
             [] {
                 // 【这是个枚举翻译器，而目标结构体的默认值是 White】
                 // 漏掉一个 case 不会编译失败（switch 覆盖了全部枚举值就没有警告），
                 // 也不会崩——那个材料会静默落回默认的 White，于是修补按白墨做。
                 const auto white = materials::ResolveMaterialClosureRepairValues(
                     FillMaterial("white"));
                 const auto varnish = materials::ResolveMaterialClosureRepairValues(
                     FillMaterial("varnish"));
                 const auto rgb = materials::ResolveMaterialClosureRepairValues(
                     FillMaterial("rgb"));
                 SLICESOFT_EXPECT_TRUE(
                     white.modelFillMaterial == MaterialClosureModelFillMaterial::White,
                     "white 应翻译为 White");
                 SLICESOFT_EXPECT_TRUE(
                     varnish.modelFillMaterial == MaterialClosureModelFillMaterial::Varnish,
                     "varnish 应翻译为 Varnish");
                 SLICESOFT_EXPECT_TRUE(
                     rgb.modelFillMaterial == MaterialClosureModelFillMaterial::Rgb,
                     "rgb 应翻译为 Rgb");
                 // 三者互不相同——这条才是「没有静默落回默认值」的真判据。
                 SLICESOFT_EXPECT_TRUE(
                     varnish.modelFillMaterial != white.modelFillMaterial
                         && rgb.modelFillMaterial != white.modelFillMaterial,
                     "三种材料不应翻译成同一个值");
             }},
            {"closure repair copies the fill and support values through",
             [] {
                 // 这两个值直接决定修补时写进像素的数值。搬错或漏搬不会报错，
                 // 只会让被修补的像素用了另一个灰度。
                 SliceConfig config = FillMaterial("white");
                 config.model_fill.value = 61U;
                 config.support.value = 17U;
                 const auto values =
                     materials::ResolveMaterialClosureRepairValues(config);
                 SLICESOFT_EXPECT_EQ(
                     static_cast<int>(values.modelFillValue), 61, "填充取值须原样带出");
                 SLICESOFT_EXPECT_EQ(
                     static_cast<int>(values.supportValue), 17, "支撑取值须原样带出");
             }},

            // ---- support/SliceSupportGeneration ----
            {"configured and effective are not the same thing",
             [] {
                 // 【本批最要紧的一条】configuredEnabled 只说「配置里开着」，
                 // effectiveEnabled 还要求【确实做了事】：有效层数为正【且】足迹像素为正。
                 // 把两者合并之后，报告会在什么都没做时声称已生效——
                 // 而这份报告正是下游判断「基面投影到底有没有起作用」的唯一依据。
                 SupportBaseProjectionResult noLayers = WorkingProjection();
                 noLayers.effective_layer_count = 0;
                 const std::string text = support::BuildSupportBaseProjectionReport(
                     noLayers, SupportGenerationResult{}).dump(0);
                 SLICESOFT_EXPECT_TRUE(
                     text.find("\"configuredEnabled\": true") != std::string::npos,
                     "配置上仍是开着的");
                 SLICESOFT_EXPECT_TRUE(
                     text.find("\"effectiveEnabled\": false") != std::string::npos,
                     "没有有效层时不应声称已生效");

                 SupportBaseProjectionResult noFootprint = WorkingProjection();
                 noFootprint.footprint_pixels = 0;
                 const std::string noFootprintText =
                     support::BuildSupportBaseProjectionReport(
                         noFootprint, SupportGenerationResult{}).dump(0);
                 SLICESOFT_EXPECT_TRUE(
                     noFootprintText.find("\"effectiveEnabled\": false") != std::string::npos,
                     "足迹为零时不应声称已生效");

                 const std::string working = support::BuildSupportBaseProjectionReport(
                     WorkingProjection(), SupportGenerationResult{}).dump(0);
                 SLICESOFT_EXPECT_TRUE(
                     working.find("\"effectiveEnabled\": true") != std::string::npos,
                     "三条件齐备时应声称已生效");
             }},
            {"an empty effective range is empty, not a degenerate pair",
             [] {
                 // 有效层数为 0 时区间必须是空数组，而不是 [0,-1]。
                 // 后者在下游是个会走进去的循环边界。
                 SupportBaseProjectionResult none;
                 none.enabled = true;
                 none.effective_layer_count = 0;
                 const std::string text = support::BuildSupportBaseProjectionReport(
                     none, SupportGenerationResult{}).dump(0);
                 SLICESOFT_EXPECT_TRUE(
                     text.find("\"effectiveLayerRange\": []") != std::string::npos,
                     "零层时区间须为空数组");

                 const std::string three = support::BuildSupportBaseProjectionReport(
                     WorkingProjection(), SupportGenerationResult{}).dump(0);
                 SLICESOFT_EXPECT_TRUE(
                     three.find("\"effectiveLayerRange\": [") != std::string::npos
                         && three.find("2") != std::string::npos,
                     "三层时区间上界应为 2");
             }},

            // ---- reports/CrossSectionStackReport ----
            {"consistency hint carries its schema and flips with texture",
             [] {
                 // schema 串是下游识别这份提示的依据，改了等于换了一种报告。
                 // profileKind 则随贴图开关翻转——写死任一侧会让另一种任务的报告说谎。
                 const SliceConfig plain;
                 const std::string plainText =
                     reports::BuildSingleMaterialConsistencyHint(plain).dump(0);
                 SLICESOFT_EXPECT_TRUE(
                     plainText.find("p0.single_material_consistency_hint.1")
                         != std::string::npos,
                     "schema 串须保持");
                 SLICESOFT_EXPECT_TRUE(
                     plainText.find("\"single_material\"") != std::string::npos,
                     "未开贴图时应报 single_material");

                 SliceConfig textured;
                 textured.texture.enabled = true;
                 const std::string texturedText =
                     reports::BuildSingleMaterialConsistencyHint(textured).dump(0);
                 SLICESOFT_EXPECT_TRUE(
                     texturedText.find("\"color_texture\"") != std::string::npos,
                     "开了贴图时应报 color_texture");
             }},
            {"the hint lists the fields a pair comparison may differ on",
             [] {
                 // 这两张清单是「配对比对时哪些字段必须相同、哪些允许不同」的契约。
                 // 少一项会让下游把一处真差异当成允许的差异放过去。
                 const std::string text =
                     reports::BuildSingleMaterialConsistencyHint(SliceConfig{}).dump(0);
                 for (const char* const key : {
                          "geometryComparableFields", "allowedMaterialDifferences",
                          "pairComparisonRequired", "comparisonStatus"})
                 {
                     SLICESOFT_EXPECT_TRUE(
                         text.find(key) != std::string::npos, "提示缺键");
                 }
                 // 几何可比字段里必须含层数与每层 z——这两项错了，比对就完全对不上。
                 SLICESOFT_EXPECT_TRUE(
                     text.find("grid.layerCount") != std::string::npos, "须含层数");
                 SLICESOFT_EXPECT_TRUE(
                     text.find("layers[].zMm") != std::string::npos, "须含每层 z");
             }},
        });
}
