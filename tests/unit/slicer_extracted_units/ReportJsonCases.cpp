// F-09 单测缺口的第五批：output/reports/SliceReportJson 的其余 10 个序列化器。
//
// 这批的被测对象是【报告契约面】——产出的 JSON 由下游按键名读取。
// 所以断言的重点是**键名本身**，而不是数值：改一个键名不会崩、不会报错，
// 只会让下游读到「缺字段」并退回默认值，而报告文件看上去仍然完整。
// 数值型断言只在有判定逻辑的几个上做（条件警告、除零保护、按模式取值）。

#include "Cases.h"

#include "slicer_core/output/reports/SliceReportJson.h"

#include "tests/support/Expect.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using slicer_core::LayerDiagnostics;

bool HasKey(const std::string& text, const char* const key)
{
    return text.find(std::string{"\""} + key + "\"") != std::string::npos;
}

/// 造一条带连通性诊断的层。layer_index 刻意与其在向量中的位置不同。
LayerDiagnostics MakeConnectivityLayer(
    const int layerIndex, const bool enabled, const int componentCount)
{
    LayerDiagnostics layer;
    layer.layer_index = layerIndex;
    layer.support_connectivity.enabled = enabled;
    layer.support_connectivity.component_count = componentCount;
    return layer;
}

}  // namespace

int RunReportJsonCases()
{
    using namespace slicer_core;
    return slicesoft_test::RunCases(
        "slice report json serialisers (F-09 batch 5)",
        {
            {"semantic stats emit all eight contract keys",
             [] {
                 // 键名就是契约。改名不崩、不报错，下游只会读到缺字段并退回默认值。
                 const std::string text =
                     reports::semantic_stats_to_json(LayerSemanticStats{}).dump(0);
                 for (const char* const key : {
                          "textureSurfacePixels", "unprintableWhiteCarrierPixels",
                          "modelFillPixels", "supportPixels", "internalVoidSupportPixels",
                          "outerVarnishPixels", "outerSurfaceVarnishPixels",
                          "innerSurfaceVarnishPixels"})
                 {
                     SLICESOFT_EXPECT_TRUE(HasKey(text, key), "语义统计缺键");
                 }
             }},
            {"fill warnings appear only when scanlines are odd",
             [] {
                 // 这条警告是「模型可能不闭合」的唯一信号。恒发会让它变成噪声、
                 // 恒不发会让不闭合的模型静静切出错误产物。
                 LayerDiagnostics clean;
                 clean.odd_intersection_rows = 0;
                 const std::string cleanText = reports::layer_diagnostics_to_json(clean).dump(0);
                 SLICESOFT_EXPECT_TRUE(
                     cleanText.find("odd_scanline_intersections") == std::string::npos,
                     "无奇数交点时不应发警告");

                 LayerDiagnostics odd;
                 odd.odd_intersection_rows = 3;
                 const std::string oddText = reports::layer_diagnostics_to_json(odd).dump(0);
                 SLICESOFT_EXPECT_TRUE(
                     oddText.find("odd_scanline_intersections") != std::string::npos,
                     "有奇数交点时必须发警告");
             }},
            {"layer diagnostics carry both raster and semantic keys",
             [] {
                 const std::string text =
                     reports::layer_diagnostics_to_json(LayerDiagnostics{}).dump(0);
                 for (const char* const key : {
                          "layerIndex", "zMm", "segmentCount", "openSegmentWarnings",
                          "filledSpans", "fillWarnings", "modelNonZeroPixels",
                          "supportNonZeroPixels"})
                 {
                     SLICESOFT_EXPECT_TRUE(HasKey(text, key), "层诊断缺键");
                 }
             }},
            {"connectivity summary skips layers without components",
             [] {
                 // enabled 是跨层的【或】：只要有一层开着就为真，哪怕那层一个分量都没有。
                 // 而计数只统计「开着【且】分量数为正」的层——两条判据不同，容易被合并写错。
                 const std::vector<LayerDiagnostics> layers{
                     MakeConnectivityLayer(0, true, 0),    // 开着但无分量：不计数
                     MakeConnectivityLayer(1, false, 5),   // 有分量但没开：不计数
                     MakeConnectivityLayer(2, true, 1),    // 计数，但不算碎片化
                 };
                 const std::string text =
                     reports::support_connectivity_summary_to_json(layers).dump(0);
                 SLICESOFT_EXPECT_TRUE(HasKey(text, "enabled"), "缺 enabled 键");
                 SLICESOFT_EXPECT_TRUE(
                     text.find("\"layersWithSupportComponents\": 1") != std::string::npos,
                     "只应计入开着且有分量的那一层");
                 SLICESOFT_EXPECT_TRUE(
                     text.find("\"layersWithFragmentation\": 0") != std::string::npos,
                     "单个分量不算碎片化");
             }},
            {"max component count reports the layer index, not the position",
             [] {
                 // 【这条钉的是一处静默的错法】记录的是 layer.layer_index，
                 // 不是它在向量里的下标。用下标写也能跑、数值往往也对——
                 // 只有当诊断向量不是从第 0 层连续排起时才会分叉，而那时报告会指错层。
                 const std::vector<LayerDiagnostics> layers{
                     MakeConnectivityLayer(70, true, 2),
                     MakeConnectivityLayer(71, true, 9),   // 最大值在此，层号 71、下标 1
                     MakeConnectivityLayer(72, true, 3),
                 };
                 const std::string text =
                     reports::support_connectivity_summary_to_json(layers).dump(0);
                 SLICESOFT_EXPECT_TRUE(
                     text.find("\"maxComponentCount\": 9") != std::string::npos,
                     "最大分量数应为 9");
                 SLICESOFT_EXPECT_TRUE(
                     text.find("\"layerWithMaxComponentCount\": 71") != std::string::npos,
                     "应记层号 71 而非下标 1");
                 SLICESOFT_EXPECT_TRUE(
                     text.find("\"layersWithFragmentation\": 3") != std::string::npos,
                     "三层分量数均大于一，全部计入碎片化");
             }},
            {"relief coverage survives a zero column total",
             [] {
                 // total_columns 为 0 时不能做除法。返回 0.0 而不是 NaN——
                 // NaN 序列化进 JSON 会产出下游解析不了的字面量。
                 SliceConfig config;
                 ReliefReportData relief;
                 relief.total_columns = 0;
                 relief.hit_columns = 0;
                 const std::string text =
                     reports::relief_report_to_json(config, relief, 0, 0).dump(0);
                 SLICESOFT_EXPECT_TRUE(
                     text.find("nan") == std::string::npos
                         && text.find("NaN") == std::string::npos
                         && text.find("inf") == std::string::npos,
                     "零列时不应产出 NaN 或 inf");
             }},
            {"relief support source follows the slicing mode",
             [] {
                 // 同一份数据在两种切片模式下要给出不同的来源名与预期值。
                 // 写死任一侧都会让报告在另一种模式下说谎。
                 SliceConfig heightfield;
                 heightfield.slicing_mode = "relief_heightfield";
                 heightfield.support.enabled = true;
                 const std::string reliefText =
                     reports::relief_report_to_json(heightfield, ReliefReportData{}, 4, 2).dump(0);
                 SLICESOFT_EXPECT_TRUE(
                     reliefText.find("relief_lower_surface") != std::string::npos,
                     "浮雕模式的支撑来源");

                 SliceConfig scanline;
                 scanline.slicing_mode = "closed_mesh_scanline";
                 scanline.support.enabled = true;
                 const std::string scanlineText =
                     reports::relief_report_to_json(scanline, ReliefReportData{}, 4, 2).dump(0);
                 SLICESOFT_EXPECT_TRUE(
                     scanlineText.find("first_model_layer") != std::string::npos,
                     "扫描线模式的支撑来源");
             }},
            {"texture report emits its contract keys",
             [] {
                 const std::string text =
                     reports::texture_report_to_json(TextureReportData{}).dump(0);
                 for (const char* const key : {
                          "enabled", "applyMode", "source", "facesWithUv",
                          "facesWithoutUv", "sampledPixels", "fallbackPixels"})
                 {
                     SLICESOFT_EXPECT_TRUE(HasKey(text, key), "贴图报告缺键");
                 }
             }},
            {"material policy and role mapping serialise on defaults",
             [] {
                 // 这两个报告在功能关闭时仍会被写出。默认值下抛异常或产出空对象，
                 // 都会让「关闭」与「出错」在报告里无法区分。
                 const SliceConfig config;
                 const std::string policy =
                     reports::material_policy_report_to_json(config, MaterialPolicyReportData{})
                         .dump(0);
                 SLICESOFT_EXPECT_TRUE(HasKey(policy, "enabled"), "材料策略报告缺 enabled");
                 SLICESOFT_EXPECT_TRUE(policy.size() > 2U, "材料策略报告不应为空对象");

                 const std::string mapping =
                     reports::material_role_mapping_report_to_json(
                         MaterialRoleMappingReportData{}).dump(0);
                 SLICESOFT_EXPECT_TRUE(HasKey(mapping, "enabled"), "角色映射报告缺 enabled");
                 SLICESOFT_EXPECT_TRUE(mapping.size() > 2U, "角色映射报告不应为空对象");
             }},
            {"model format reports serialise on an empty model",
             [] {
                 // 空模型是合法输入（尚未导入时的报告）。这两个必须能产出而不是抛。
                 const ModelReport empty;
                 const std::string objMtl = reports::obj_mtl_material_report_to_json(empty).dump(0);
                 const std::string threeMf = reports::three_mf_report_to_json(empty).dump(0);
                 SLICESOFT_EXPECT_TRUE(objMtl.size() > 1U, "OBJ/MTL 报告不应为空串");
                 SLICESOFT_EXPECT_TRUE(threeMf.size() > 1U, "3MF 报告不应为空串");
             }},
            {"write_json_file creates missing parent directories",
             [] {
                 // 调用方按报告路径直接写，中间目录可能还不存在。
                 // 少了 create_directories 就会在生产收尾时报「打不开文件」而丢掉整份报告。
                 const std::filesystem::path dir =
                     std::filesystem::temp_directory_path() / "slicesoft_f09_b5" / "nested";
                 const std::filesystem::path file = dir / "report.json";
                 std::error_code ignored;
                 std::filesystem::remove_all(
                     std::filesystem::temp_directory_path() / "slicesoft_f09_b5", ignored);

                 reports::write_json_file(file, reports::bbox_to_json(BoundingBox{}));

                 SLICESOFT_EXPECT_TRUE(
                     std::filesystem::exists(file), "缺失的父目录须被创建并写入");
                 std::ifstream input{file};
                 std::ostringstream buffer;
                 buffer << input.rdbuf();
                 input.close();
                 SLICESOFT_EXPECT_TRUE(
                     buffer.str().find("\"min\"") != std::string::npos, "内容须为传入的 JSON");
                 std::filesystem::remove_all(
                     std::filesystem::temp_directory_path() / "slicesoft_f09_b5", ignored);
             }},
        });
}
