// 层报告 JSON 序列化的实现（F-09 第 2 步从 slicer.cpp 搬出）。
//
// 只移位、不改行为：函数体逐字节搬过来，判据是字节级基线逐产物全等。
//
// 私有助手（channel_names / channel_stats_to_json / support_connectivity_to_json）
// 放在匿名命名空间；公开函数的定义顺序不受约束——头文件在最前面已声明全部 13 个，
// 彼此调用不依赖定义先后。

#include "slicer_core/output/reports/SliceReportJson.h"

#include "slicer_core/system/Utf8Path.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>

namespace slicer_core::reports {

namespace {

constexpr std::array<const char*, rgbwsv_channel_count> channel_names{"R", "G", "B", "W", "S", "V"};

Json channel_stats_to_json(const ChannelStats& stats) {
    return Json::object({
        {"printPixels", stats.print_pixels},
        {"fullPrintPixels", stats.full_print_pixels},
        {"partialPrintPixels", stats.partial_print_pixels},
        {"emptyPixels", stats.empty_pixels},
        {"minValue", stats.min_value},
        {"maxValue", stats.max_value},
    });
}

Json support_connectivity_to_json(const SupportConnectivityDiagnostics& diagnostics) {
    constexpr int tiny_component_area_px{8};
    constexpr int small_component_area_px{512};
    Json::Array components;
    for (const SupportComponentSummary& component : diagnostics.components) {
        components.push_back(Json::object({
            {"areaPx", component.area_px},
            {"bbox",
             Json::object({
                 {"minX", component.min_x},
                 {"minY", component.min_y},
                 {"maxX", component.max_x},
                 {"maxY", component.max_y},
             })},
        }));
    }
    return Json::object({
        {"enabled", diagnostics.enabled},
        {"componentCount", diagnostics.component_count},
        {"largestComponentPixels", diagnostics.largest_component_pixels},
        {"smallComponentCount", diagnostics.small_component_count},
        {"tinyComponentCount", diagnostics.tiny_component_count},
        {"tinyComponentAreaPx", tiny_component_area_px},
        {"smallComponentAreaPx", small_component_area_px},
        {"components", Json{components}},
    });
}

}  // namespace

Json bbox_to_json(const BoundingBox& bbox) {
    return Json::object({
        {"min", Json::array({bbox.min.x, bbox.min.y, bbox.min.z})},
        {"max", Json::array({bbox.max.x, bbox.max.y, bbox.max.z})},
    });
}

void write_json_file(const std::filesystem::path& path, const Json& value) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output{path};
    if (!output) {
        throw std::runtime_error("failed to write JSON file: " + path.string());
    }
    output << value.dump(2) << '\n';
}

Json rgb_to_json(const std::array<std::uint8_t, 3>& rgb) {
    return Json::array({static_cast<int>(rgb.at(0)), static_cast<int>(rgb.at(1)), static_cast<int>(rgb.at(2))});
}

Json channel_stats_array_to_json(const std::array<ChannelStats, rgbwsv_channel_count>& stats) {
    Json::Object object;
    for (std::size_t i{0}; i < stats.size(); ++i) {
        object.emplace(channel_names.at(i), channel_stats_to_json(stats.at(i)));
    }
    return Json{object};
}

Json support_connectivity_summary_to_json(const std::vector<LayerDiagnostics>& diagnostics) {
    int layers_with_support_components{0};
    int layers_with_fragmentation{0};
    int max_component_count{0};
    int layer_with_max_component_count{-1};
    int max_small_component_count{0};
    int max_tiny_component_count{0};
    bool enabled{false};
    for (const LayerDiagnostics& layer : diagnostics) {
        const SupportConnectivityDiagnostics& support = layer.support_connectivity;
        enabled = enabled || support.enabled;
        if (!support.enabled || support.component_count <= 0) {
            continue;
        }
        ++layers_with_support_components;
        if (support.component_count > 1) {
            ++layers_with_fragmentation;
        }
        if (support.component_count > max_component_count) {
            max_component_count = support.component_count;
            layer_with_max_component_count = layer.layer_index;
        }
        max_small_component_count = std::max(max_small_component_count, support.small_component_count);
        max_tiny_component_count = std::max(max_tiny_component_count, support.tiny_component_count);
    }
    return Json::object({
        {"enabled", enabled},
        {"layersWithSupportComponents", layers_with_support_components},
        {"layersWithFragmentation", layers_with_fragmentation},
        {"maxComponentCount", max_component_count},
        {"layerWithMaxComponentCount", layer_with_max_component_count},
        {"maxSmallComponentCount", max_small_component_count},
        {"maxTinyComponentCount", max_tiny_component_count},
    });
}

Json semantic_stats_to_json(const LayerSemanticStats& stats) {
    return Json::object({
        {"textureSurfacePixels", stats.texture_surface_pixels},
        {"unprintableWhiteCarrierPixels", stats.unprintable_white_carrier_pixels},
        {"modelFillPixels", stats.model_fill_pixels},
        {"supportPixels", stats.support_pixels},
        {"internalVoidSupportPixels", stats.internal_void_support_pixels},
        {"outerVarnishPixels", stats.outer_varnish_pixels},
        {"outerSurfaceVarnishPixels", stats.outer_surface_varnish_pixels},
        {"innerSurfaceVarnishPixels", stats.inner_surface_varnish_pixels},
    });
}

Json layer_diagnostics_to_json(const LayerDiagnostics& diagnostics) {
    Json::Array fill_warnings;
    if (diagnostics.odd_intersection_rows > 0) {
        fill_warnings.push_back("odd_scanline_intersections");
    }
    return Json::object({
        {"layerIndex", diagnostics.layer_index},
        {"zMm", diagnostics.z_mm},
        {"segmentCount", diagnostics.segment_count},
        {"openSegmentWarnings", diagnostics.odd_intersection_rows},
        {"filledSpans", diagnostics.filled_spans},
        {"fillWarnings", Json{fill_warnings}},
        {"modelNonZeroPixels", diagnostics.model_pixels},
        {"supportNonZeroPixels", diagnostics.support_pixels},
        {"textureSurfacePixels", diagnostics.semantic.texture_surface_pixels},
        {"unprintableWhiteCarrierPixels", diagnostics.semantic.unprintable_white_carrier_pixels},
        {"modelFillPixels", diagnostics.semantic.model_fill_pixels},
        {"supportPixels", diagnostics.semantic.support_pixels},
        {"internalVoidSupportPixels", diagnostics.semantic.internal_void_support_pixels},
        {"upperSurfaceSupportPixels", diagnostics.upper_projection_support_pixels},
        {"projectionBaseSupportPixels", diagnostics.projection_base_support_pixels},
        {"outerVarnishPixels", diagnostics.semantic.outer_varnish_pixels},
        {"rgbNonZeroPixels", diagnostics.rgb_non_zero_pixels},
        {"whiteNonZeroPixels", diagnostics.white_non_zero_pixels},
        {"modelPrintPixels", diagnostics.model_pixels},
        {"supportPrintPixels", diagnostics.support_non_zero_pixels},
        {"rgbPrintPixels", diagnostics.rgb_non_zero_pixels},
        {"whitePrintPixels", diagnostics.white_non_zero_pixels},
        {"varnishPrintPixels", diagnostics.varnish_non_zero_pixels},
        {"varnishNonZeroPixels", diagnostics.varnish_non_zero_pixels},
        {"islandCount", diagnostics.island_count},
        {"islandPixels", diagnostics.island_pixels},
        {"unsupportedPixels", diagnostics.unsupported_pixels},
        {"filteredIslandCount", diagnostics.filtered_island_count},
        {"filteredIslandPixels", diagnostics.filtered_island_pixels},
        {"supportTypeStats",
         Json::object({
             {"bottom_projection", diagnostics.bottom_projection_support_pixels},
             {"unsupported_island", diagnostics.unsupported_island_support_pixels},
             {"full_vertical_projection", diagnostics.full_vertical_projection_support_pixels},
             {"internal_void", diagnostics.internal_void_support_pixels},
             {"upper_projection", diagnostics.upper_projection_support_pixels},
             {"projection_base", diagnostics.projection_base_support_pixels},
        })},
        {"supportConnectivity", support_connectivity_to_json(diagnostics.support_connectivity)},
        {"semantic", semantic_stats_to_json(diagnostics.semantic)},
        {"channelStats", channel_stats_array_to_json(diagnostics.channel_stats)},
    });
}

Json texture_report_to_json(const TextureReportData& report) {
    return Json::object({
        {"enabled", report.enabled},
        {"applyMode", report.apply_mode},
        {"nonSurfaceRgbPolicy", report.non_surface_rgb_policy},
        {"topSurfaceLayers", report.top_surface_layers},
        {"source", report.source},
        {"materials", Json{report.materials}},
        {"textureFiles", Json{report.texture_files}},
        {"loadedTextures", report.loaded_textures},
        {"missingTextures", report.missing_textures},
        {"stats",
         Json::object({
             {"facesWithUv", report.faces_with_uv},
             {"facesWithoutUv", report.faces_without_uv},
             {"sampledPixels", report.sampled_pixels},
             {"fallbackPixels", report.fallback_pixels},
             {"uvOutOfRangePixels", report.uv_out_of_range_pixels},
         })},
        {"sampledPixels", report.sampled_pixels},
        {"fallbackPixels", report.fallback_pixels},
        {"uvOutOfRangePixels", report.uv_out_of_range_pixels},
        {"warnings", Json{report.warnings}},
    });
}

Json material_policy_report_to_json(const SliceConfig& config, const MaterialPolicyReportData& report) {
    return Json::object({
        {"enabled", config.material_policy.enabled},
        {"conflictPolicy", config.material_policy.conflict_policy},
        {"rgb",
         Json::object({
             {"enabled", config.material_policy.rgb.enabled},
             {"source", config.material_policy.rgb.source},
             {"printPixels", report.rgb_print_pixels},
         })},
        {"white",
         Json::object({
             {"enabled", config.material_policy.white.enabled},
             {"mode", config.material_policy.white.mode},
             {"layers", config.material_policy.white.layers},
             {"value", static_cast<int>(config.material_policy.white.value)},
             {"printPixels", report.white_print_pixels},
         })},
        {"varnish",
         Json::object({
             {"enabled", config.material_policy.varnish.enabled},
             {"mode", config.material_policy.varnish.mode},
             {"topLayers", config.material_policy.varnish.top_layers},
             {"value", static_cast<int>(config.material_policy.varnish.value)},
             {"printPixels", report.varnish_print_pixels},
         })},
        {"warnings", Json{report.warnings}},
    });
}

Json material_role_mapping_report_to_json(const MaterialRoleMappingReportData& report) {
    return Json::object({
        {"enabled", report.enabled},
        {"inputFormat", report.input_format},
        {"rules", Json{report.rules}},
        {"defaultRole", report.default_role},
        {"allowInputSupportMaterial", report.allow_input_support_material},
        {"materialCount", report.material_count},
        {"mappedRgb", report.mapped_rgb},
        {"mappedWhite", report.mapped_white},
        {"mappedVarnish", report.mapped_varnish},
        {"mappedIgnore", report.mapped_ignore},
        {"mappedSupportCandidate", report.mapped_support_candidate},
        {"mappedSupport", report.mapped_support},
        {"facesWithMappedMaterial", report.faces_with_mapped_material},
        {"facesWithoutMappedMaterial", report.faces_without_mapped_material},
        {"materials", Json{report.materials}},
        {"stats",
         Json::object({
             {"materialCount", report.material_count},
             {"mappedRgb", report.mapped_rgb},
             {"mappedWhite", report.mapped_white},
             {"mappedVarnish", report.mapped_varnish},
             {"mappedIgnore", report.mapped_ignore},
             {"mappedSupportCandidate", report.mapped_support_candidate},
             {"mappedSupport", report.mapped_support},
             {"facesWithMappedMaterial", report.faces_with_mapped_material},
             {"facesWithoutMappedMaterial", report.faces_without_mapped_material},
         })},
        {"warnings", Json{report.warnings}},
    });
}

Json obj_mtl_material_report_to_json(const ModelReport& model_report) {
    Json::Array materials;
    Json::Array textures;
    for (const MaterialInfo& material : model_report.material_infos) {
        materials.push_back(Json::object({
            {"name", material.name},
            {"hasDiffuse", material.has_diffuse},
            {"diffuseRgb", rgb_to_json(material.diffuse_rgb)},
            {"hasTexture", material.has_texture},
            {"texturePath", slicer_core::PathToUtf8(material.diffuse_texture_path)},
            {"textureExists", material.texture_exists},
        }));
        if (material.has_texture) {
            textures.push_back(slicer_core::PathToUtf8(material.diffuse_texture_path));
        }
    }
    int faces_with_material{0};
    int faces_without_material{0};
    for (const TriangleTextureInfo& triangle : model_report.triangle_textures) {
        if (triangle.material_name.empty()) {
            ++faces_without_material;
        } else {
            ++faces_with_material;
        }
    }
    return Json::object({
        {"inputFormat", model_report.format},
        {"materialCount", static_cast<int>(model_report.material_infos.size())},
        {"materials", Json{materials}},
        {"facesWithMaterial", faces_with_material},
        {"facesWithoutMaterial", faces_without_material},
        {"textures", Json{textures}},
    });
}

Json three_mf_report_to_json(const ModelReport& model_report) {
    Json::Array unsupported_resources;
    for (const std::string& resource : model_report.three_mf.unsupported_resources) {
        unsupported_resources.push_back(resource);
    }
    Json::Array unsupported_extensions;
    for (const std::string& extension : model_report.three_mf.unsupported_extensions) {
        unsupported_extensions.push_back(extension);
    }
    Json::Array warnings;
    for (const std::string& warning : model_report.three_mf.warnings) {
        warnings.push_back(warning);
    }
    Json::Array errors;
    for (const std::string& error : model_report.three_mf.errors) {
        errors.push_back(error);
    }
    return Json::object({
        {"enabled", model_report.three_mf.enabled},
        {"packagePath", slicer_core::PathToUtf8(model_report.three_mf.package_path)},
        {"modelPartPath", model_report.three_mf.model_part_path},
        {"unit", model_report.three_mf.unit},
        {"unitScaleToMm", model_report.three_mf.unit_scale_to_mm},
        {"zipCompressionStats",
         Json::object({
             {"entryCount", model_report.three_mf.entry_count},
             {"storedEntryCount", model_report.three_mf.stored_entry_count},
             {"deflatedEntryCount", model_report.three_mf.deflated_entry_count},
             {"totalUncompressedBytes", static_cast<double>(model_report.three_mf.total_uncompressed_bytes)},
         })},
        {"entryCount", model_report.three_mf.entry_count},
        {"totalUncompressedBytes", static_cast<double>(model_report.three_mf.total_uncompressed_bytes)},
        {"zip",
         Json::object({
             {"entryCount", model_report.three_mf.entry_count},
             {"storedEntryCount", model_report.three_mf.stored_entry_count},
             {"deflatedEntryCount", model_report.three_mf.deflated_entry_count},
             {"totalUncompressedBytes", static_cast<double>(model_report.three_mf.total_uncompressed_bytes)},
         })},
        {"xml",
         Json::object({
             {"parser", model_report.three_mf.xml_parser},
             {"allowExternalEntities", false},
             {"parseWarnings", Json::array({})},
         })},
        {"xmlParser", model_report.three_mf.xml_parser},
        {"validation",
         Json::object({
             {"invalidReferenceCount", model_report.three_mf.invalid_reference_count},
             {"unknownMaterialCount", model_report.three_mf.unknown_material_count},
             {"ignoredResourceCount", model_report.three_mf.ignored_resource_count},
         })},
        {"colorGroups",
         Json::object({
             {"count", model_report.three_mf.color_group_count},
             {"colorCount", model_report.three_mf.color_count},
             {"resolvedTriangles", model_report.three_mf.color_group_resolved_triangles},
             {"interpolatedColorFallbackCount", model_report.three_mf.interpolated_color_fallback_count},
         })},
        {"textures",
         Json::object({
             {"texture2dCount", model_report.three_mf.texture2d_count},
             {"texture2dGroupCount", model_report.three_mf.texture2d_group_count},
             {"tex2CoordCount", model_report.three_mf.tex2coord_count},
             {"resourceCount", model_report.three_mf.texture_resource_count},
             {"loadedCount", model_report.three_mf.texture_loaded_count},
             {"missingCount", model_report.three_mf.texture_missing_count},
             {"sampledPixels", static_cast<double>(model_report.three_mf.texture_sampled_pixels)},
             {"resolvedTriangles", model_report.three_mf.texture_group_resolved_triangles},
         })},
        {"colorGroupCount", model_report.three_mf.color_group_count},
        {"colorCount", model_report.three_mf.color_count},
        {"texture2dCount", model_report.three_mf.texture2d_count},
        {"texture2dGroupCount", model_report.three_mf.texture2d_group_count},
        {"tex2CoordCount", model_report.three_mf.tex2coord_count},
        {"textureResourceCount", model_report.three_mf.texture_resource_count},
        {"textureLoadedCount", model_report.three_mf.texture_loaded_count},
        {"textureMissingCount", model_report.three_mf.texture_missing_count},
        {"textureSampledPixels", static_cast<double>(model_report.three_mf.texture_sampled_pixels)},
        {"colorGroupResolvedTriangles", model_report.three_mf.color_group_resolved_triangles},
        {"textureGroupResolvedTriangles", model_report.three_mf.texture_group_resolved_triangles},
        {"interpolatedColorFallbackCount", model_report.three_mf.interpolated_color_fallback_count},
        {"invalidReferenceCount", model_report.three_mf.invalid_reference_count},
        {"ignoredResourceCount", model_report.three_mf.ignored_resource_count},
        {"objectCount", model_report.three_mf.object_count},
        {"componentCount", model_report.three_mf.component_count},
        {"meshObjectCount", model_report.three_mf.mesh_object_count},
        {"triangleCount", model_report.three_mf.triangle_count},
        {"materialResourceCount", model_report.three_mf.material_resource_count},
        {"unsupportedResources", Json{unsupported_resources}},
        {"unsupportedExtensions", Json{unsupported_extensions}},
        {"warnings", Json{warnings}},
        {"errors", Json{errors}},
    });
}

Json relief_report_to_json(
    const SliceConfig& config,
    const ReliefReportData& relief_report,
    const int support_pixels,
    const int columns_with_support) {
    const double coverage_ratio = relief_report.total_columns > 0
        ? static_cast<double>(relief_report.hit_columns) / static_cast<double>(relief_report.total_columns)
        : 0.0;
    return Json::object({
        {"slicingMode", config.slicing_mode},
        {"fillMode", config.relief.fill_mode},
        {"baseZMm", config.relief.base_z_mm},
        {"support",
         Json::object({
             {"enabled", config.support.enabled},
             {"source", config.slicing_mode == "relief_heightfield" ? "relief_lower_surface" : "first_model_layer"},
             {"expectedSupport", config.slicing_mode == "relief_heightfield" && config.support.enabled},
             {"supportPixels", support_pixels},
             {"columnsWithSupport", columns_with_support},
         })},
        {"columns",
         Json::object({
             {"total", relief_report.total_columns},
             {"hit", relief_report.hit_columns},
             {"empty", relief_report.empty_columns},
             {"multiHit", relief_report.multi_hit_columns},
             {"coverageRatio", coverage_ratio},
         })},
        {"height",
         Json::object({
             {"zMinMm", relief_report.has_hits ? relief_report.z_min_mm : 0.0},
             {"zMaxMm", relief_report.has_hits ? relief_report.z_max_mm : 0.0},
             {"thicknessMinMm", relief_report.has_hits ? relief_report.thickness_min_mm : 0.0},
             {"thicknessMaxMm", relief_report.has_hits ? relief_report.thickness_max_mm : 0.0},
         })},
        {"zRangeMm",
         Json::object({
             {"min", relief_report.has_hits ? relief_report.z_min_mm : 0.0},
             {"max", relief_report.has_hits ? relief_report.z_max_mm : 0.0},
         })},
        {"warnings", Json{relief_report.warnings}},
    });
}

}  // namespace slicer_core::reports
