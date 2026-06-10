#include "slicer_core/support/SupportShapeReport.h"

namespace slicer_core
{
namespace
{

Json ComponentToJson(const SupportComponentInfo& component)
{
    return Json::object({
        {"areaPx", component.area_px},
        {"bbox",
         Json::object({
             {"minX", component.min_x},
             {"minY", component.min_y},
             {"maxX", component.max_x},
             {"maxY", component.max_y},
         })},
    });
}

Json AnalysisToJson(const SupportComponentAnalysis& analysis)
{
    Json::Array components;
    for (const SupportComponentInfo& component : analysis.components)
    {
        components.push_back(ComponentToJson(component));
    }
    return Json::object({
        {"enabled", analysis.enabled},
        {"componentCount", analysis.component_count},
        {"largestComponentArea", analysis.largest_component_area},
        {"smallComponentCount", analysis.small_component_count},
        {"tinyComponentCount", analysis.tiny_component_count},
        {"tinyComponentAreaPx", analysis.tiny_component_area_px},
        {"smallComponentAreaPx", analysis.small_component_area_px},
        {"components", Json{components}},
    });
}

Json FilteredComponentToJson(const FilteredSupportComponent& component)
{
    return Json::object({
        {"layerIndex", component.layer_index},
        {"areaPx", component.area_px},
        {"bbox",
         Json::object({
             {"minX", component.min_x},
             {"minY", component.min_y},
             {"maxX", component.max_x},
             {"maxY", component.max_y},
         })},
    });
}

Json BridgedGapToJson(const BridgedSupportGap& gap)
{
    return Json::object({
        {"layerIndex", gap.layer_index},
        {"x0", gap.x0},
        {"y0", gap.y0},
        {"x1", gap.x1},
        {"y1", gap.y1},
        {"gapPx", gap.gap_px},
        {"direction", gap.direction},
    });
}

}  // namespace

Json MakeSupportShapeReport(
    const SupportShapePolicy& policy,
    const SupportShapeOptimizationResult& result)
{
    Json::Array layers;
    Json::Array filteredComponents;
    Json::Array bridgedGaps;
    Json::Array warnings;
    for (const std::string& warning : result.warnings)
    {
        warnings.push_back(warning);
    }

    for (const SupportShapeLayerReport& layer : result.layers)
    {
        Json::Array layerFiltered;
        for (const FilteredSupportComponent& component : layer.filtered_components)
        {
            const Json componentJson = FilteredComponentToJson(component);
            layerFiltered.push_back(componentJson);
            filteredComponents.push_back(componentJson);
        }
        Json::Array layerBridged;
        for (const BridgedSupportGap& gap : layer.bridged_gaps)
        {
            const Json gapJson = BridgedGapToJson(gap);
            layerBridged.push_back(gapJson);
            bridgedGaps.push_back(gapJson);
        }
        Json::Array layerWarnings;
        for (const std::string& warning : layer.warnings)
        {
            layerWarnings.push_back(warning);
        }
        layers.push_back(Json::object({
            {"layerIndex", layer.layer_index},
            {"pre", AnalysisToJson(layer.pre)},
            {"post", AnalysisToJson(layer.post)},
            {"addedSupportPixels", layer.added_support_pixels},
            {"removedSupportPixels", layer.removed_support_pixels},
            {"filteredComponents", Json{layerFiltered}},
            {"bridgedGaps", Json{layerBridged}},
            {"warnings", Json{layerWarnings}},
        }));
    }

    return Json::object({
        {"schema", "p0.support_shape_report.1"},
        {"enabled", result.enabled},
        {"policy",
         Json::object({
             {"enabled", policy.enabled},
             {"minComponentAreaPx", policy.min_component_area_px},
             {"xyDilationPx", policy.xy_dilation_px},
             {"closingRadiusPx", policy.closing_radius_px},
             {"bridgeGapPx", policy.bridge_gap_px},
             {"preserveModelPriority", policy.preserve_model_priority},
             {"maxAddedSupportRatio", policy.max_added_support_ratio},
         })},
        {"addedSupportPixels", result.added_support_pixels},
        {"removedSupportPixels", result.removed_support_pixels},
        {"filteredComponents", Json{filteredComponents}},
        {"bridgedGaps", Json{bridgedGaps}},
        {"warnings", Json{warnings}},
        {"layers", Json{layers}},
    });
}

}  // namespace slicer_core
