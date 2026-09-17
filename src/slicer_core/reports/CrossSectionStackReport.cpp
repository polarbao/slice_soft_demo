// 横截面材料栈报告的实现（F-09 第 8 步从 slicer.cpp 搬出）。
// 只移位、不改行为：函数体逐字节搬过来，判据是字节级基线逐产物全等。

#include "slicer_core/reports/CrossSectionStackReport.h"

#include "slicer_core/material/MaterialClosureRepair.h"
#include "slicer_core/material/RetainedMaterialLayerComposer.h"
#include "slicer_core/materials/SliceMaterialTexture.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace slicer_core::reports {

namespace {

Json BuildCrossSectionStackEntry(
    const int order,
    const std::string& id,
    const std::string& label,
    const std::string& channel,
    const std::string& semantic,
    const std::string& reportField,
    const std::uint64_t printPixels,
    const bool enabled,
    const std::string& sourceMask,
    const std::string& note)
{
    return Json::object({
        {"order", order},
        {"id", id},
        {"label", label},
        {"channel", channel},
        {"semantic", semantic},
        {"reportField", reportField},
        {"printPixels", printPixels},
        {"present", printPixels > 0U},
        {"enabled", enabled},
        {"sourceMask", sourceMask},
        {"note", note},
    });
}

}  // namespace

void update_layer_channel_stats(
    const std::vector<std::uint8_t>& layer,
    LayerDiagnostics& diagnostics,
    const std::vector<std::uint32_t>* active_columns,
    const std::uint8_t background_value) {
    const auto stats = AnalyzeRetainedMaterialLayerChannels(
        layer, active_columns, background_value);
    diagnostics.channel_stats = stats.channels;
    diagnostics.rgb_non_zero_pixels = stats.rgbNonZeroPixels;
    diagnostics.white_non_zero_pixels = stats.whiteNonZeroPixels;
    diagnostics.support_non_zero_pixels = stats.supportNonZeroPixels;
    diagnostics.varnish_non_zero_pixels = stats.varnishNonZeroPixels;
}

void merge_channel_stats(std::array<ChannelStats, rgbwsv_channel_count>& totals, const LayerDiagnostics& diagnostics) {
    AccumulateRetainedMaterialChannelStats(totals, diagnostics.channel_stats);
}

void merge_semantic_stats(LayerSemanticStats& totals, const LayerSemanticStats& layer) {
    AccumulateRetainedMaterialSemanticStats(totals, layer);
}

Json BuildCrossSectionMaterialStackReport(
    const SliceConfig& config,
    const LayerSemanticStats& semanticStats,
    const SupportGenerationResult& supportGeneration,
    const SupportPlacementPolicy& supportPlacementPolicy)
{
    const std::uint64_t textureSurfacePixels =
        static_cast<std::uint64_t>(std::max(0, semanticStats.texture_surface_pixels));
    const std::uint64_t modelFillPixels =
        static_cast<std::uint64_t>(std::max(0, semanticStats.model_fill_pixels));
    const std::uint64_t outerSurfaceVarnishPixels =
        static_cast<std::uint64_t>(std::max(0, semanticStats.outer_surface_varnish_pixels));
    const std::uint64_t innerSurfaceVarnishPixels =
        static_cast<std::uint64_t>(std::max(0, semanticStats.inner_surface_varnish_pixels));
    const std::uint64_t upperSurfaceSupportPixels =
        static_cast<std::uint64_t>(std::max(0, supportGeneration.upper_projection_support_pixels));
    const std::uint64_t lowerSurfaceSupportPixels =
        static_cast<std::uint64_t>(std::max(0, supportGeneration.bottom_projection_support_pixels));
    const bool surfaceColorEnabled =
        config.texture.enabled
        || (config.material_process_profile.enabled && config.material_process_profile.rgb.enabled)
        || (config.material_policy.enabled && config.material_policy.rgb.enabled);

    Json::Array stack;
    stack.push_back(BuildCrossSectionStackEntry(
        1,
        "upper_surface_support",
        "上表面支撑",
        "S",
        "UpperSurfaceSupportMask",
        "upperSurfaceSupportPixels",
        upperSurfaceSupportPixels,
        supportPlacementPolicy.upper_enabled,
        "support_type.upper_projection",
        "模型外部可剥离支撑；启用 outerVarnish 时应位于外侧光油边界之外"));
    stack.push_back(BuildCrossSectionStackEntry(
        2,
        "outer_surface_varnish",
        "表面层光油",
        "V",
        "OuterSurfaceVarnishMask",
        "outerSurfaceVarnishPixels",
        outerSurfaceVarnishPixels,
        config.surface_varnish.enabled && config.surface_varnish.outer_surface,
        "surfaceVarnish.outerSurface",
        "写在模型外表面像素上的 V 通道；不同于扩张模型 XY 的 outerVarnish shell"));
    stack.push_back(BuildCrossSectionStackEntry(
        3,
        "outer_surface_color",
        "模型表层色彩层",
        "RGB",
        "OuterSurfaceColorMask",
        "textureSurfacePixels",
        textureSurfacePixels,
        surfaceColorEnabled,
        "textureSurfacePixels",
        "当前 legacy 统计未拆分外/内表面 RGB，使用 textureSurfacePixels 解释表面色彩层"));
    stack.push_back(BuildCrossSectionStackEntry(
        4,
        "model_fill",
        "模型内部填充层",
        config.model_fill.material == "varnish" ? "V" : (config.model_fill.material == "rgb" ? "RGB" : "W"),
        "ModelFillMask",
        "modelFillPixels",
        modelFillPixels,
        config.model_fill.enabled,
        "modelFill",
        "生产 Profile 不允许内部填充为空；默认材料为 white"));
    stack.push_back(BuildCrossSectionStackEntry(
        5,
        "inner_surface_color",
        "模型内表层色彩层",
        "RGB",
        "InnerSurfaceColorMask",
        "textureSurfacePixels",
        textureSurfacePixels,
        surfaceColorEnabled,
        "textureSurfacePixels",
        "当前 legacy 统计未拆分外/内表面 RGB，使用同一 textureSurfacePixels 解释内表层色彩"));
    stack.push_back(BuildCrossSectionStackEntry(
        6,
        "inner_surface_varnish",
        "模型内表面光油层",
        "V",
        "InnerSurfaceVarnishMask",
        "innerSurfaceVarnishPixels",
        innerSurfaceVarnishPixels,
        config.surface_varnish.enabled && config.surface_varnish.inner_surface,
        "surfaceVarnish.innerSurface",
        "写在模型内表面像素上的 V 通道，主要用于解释真实 RIP 横截面内侧清漆带"));
    stack.push_back(BuildCrossSectionStackEntry(
        7,
        "lower_surface_support",
        "模型下表面支撑层",
        "S",
        "LowerSurfaceSupportMask",
        "supportTypeStats.bottom_projection",
        lowerSurfaceSupportPixels,
        supportPlacementPolicy.lower_enabled,
        "support_type.bottom_projection",
        "模型外部可剥离下表面支撑"));

    Json::Array missing;
    if (upperSurfaceSupportPixels == 0U)
    {
        missing.push_back("upper_surface_support");
    }
    if (outerSurfaceVarnishPixels == 0U)
    {
        missing.push_back("outer_surface_varnish");
    }
    if (textureSurfacePixels == 0U)
    {
        missing.push_back("surface_color");
    }
    if (modelFillPixels == 0U)
    {
        missing.push_back("model_fill");
    }
    if (innerSurfaceVarnishPixels == 0U)
    {
        missing.push_back("inner_surface_varnish");
    }
    if (lowerSurfaceSupportPixels == 0U)
    {
        missing.push_back("lower_surface_support");
    }

    const bool hasRequiredStack = missing.empty();
    return Json::object({
        {"schema", "p0.cross_section_material_stack.1"},
        {"reference",
         Json::object({
             {"diagram", "docs/slice/DOC/DIAGRAM_12A_指甲模型横截面材料示意图.png"},
             {"realRipLayer", "slice.446.png"},
             {"review", "docs/slice/DOC/DOC_REVIEW_12A_真实RIP横截面示意图对齐审查.md"},
             {"oldConceptDiagramGeometryAcceptance", "deprecated"},
         })},
        {"stackOrder",
         "UpperSurfaceSupport>SurfaceVarnish>SurfaceColor>ModelFill>SurfaceColor>InnerSurfaceVarnish>LowerSupport"},
        {"note", "该材料栈描述模型横截面材料关系，不表示画布坐标上下方向，也不实现 RIP 半色调。"},
        {"outerInnerColorSplit",
         Json::object({
             {"available", false},
             {"source", "textureSurfacePixels"},
             {"reason", "当前 legacy pipeline 只统计总 texture surface RGB，尚未拆分 outer/inner surface RGB mask"},
         })},
        {"outerVarnishShell",
         Json::object({
             {"enabled", config.outer_varnish.enabled},
             {"printPixels", static_cast<std::uint64_t>(std::max(0, semanticStats.outer_varnish_pixels))},
             {"note", "outerVarnishShell 是模型外侧扩张光油壳层，不等同于真实横截面中的表面/内表面光油带"},
         })},
        {"summary",
         Json::object({
             {"canExplainRealRipCrossSection", hasRequiredStack},
             {"missingElements", Json{missing}},
             {"modelFillMaterial", config.model_fill.material},
             {"supportPlacement", supportPlacementPolicy.effective_placement},
             {"semanticPriority", "Model>OuterVarnishShell>Support>Empty"},
         })},
        {"stack", Json{stack}},
    });
}

Json BuildSingleMaterialConsistencyHint(const SliceConfig& config)
{
    const std::string profileKind = config.texture.enabled ? "color_texture" : "single_material";
    return Json::object({
        {"schema", "p0.single_material_consistency_hint.1"},
        {"profileKind", profileKind},
        {"pairComparisonRequired", true},
        {"comparisonStatus", "not_evaluated_in_single_package"},
        {"modelSemanticComparable", true},
        {"singleMaterialConsistency", "requires_pair_comparison"},
        {"geometryComparableFields",
         Json::array({
             "grid.widthPx",
             "grid.heightPx",
             "grid.layerCount",
             "layers[].zMm",
             "layers[].modelPixels",
             "layers[].supportPixels",
             "layers[].supportTypeStats",
             "layers[].outerVarnishPixels",
             "layers[].outerSurfaceVarnishPixels",
             "layers[].innerSurfaceVarnishPixels",
         })},
        {"allowedMaterialDifferences",
         Json::array({
             "textureSurfacePixels",
             "modelFillPixels",
             "rgbPrintPixels",
             "whitePrintPixels",
             "varnishPrintPixels",
             "texture_report.*",
         })},
        {"note", "Use a paired color/single-material fixture to assert geometry, support and layer-order consistency."},
    });
}

}  // namespace slicer_core::reports
