#pragma once

// 层报告的数据结构与 JSON 序列化（F-09 第 2 步从 slicer.cpp 搬出）。
//
// 为什么类型放在 slicer_core 而函数放在 slicer_core::reports：
// 这 7 个类型在 slicer.cpp 里还有 5–13 处使用（它们【不是】只被序列化器用，
// 这一点与第 1 步的 PreviewImage 不同，那个只被簇内用、可以随簇搬进匿名命名空间）。
// 把类型留在 slicer_core，slicer.cpp 侧的类型使用一处都不用改；
// 只有 13 个序列化函数需要加 reports:: 限定，改动面最小。
//
// 这些类型此前全部只在 slicer.cpp 内使用（实测 src/ apps/ tests/ 无其他文件提及），
// 所以外移只影响 slicer.cpp 与本单元。

#include "slicer_core/TiffReadApi.h"
#include "slicer_core/config.h"
#include "slicer_core/json_value.h"
#include "slicer_core/material/RetainedMaterialLayerComposer.h"
#include "slicer_core/model.h"
#include "slicer_core/support/SupportConnectivityAnalysis.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace slicer_core {

using ChannelStats = BoundedMaterialChannelStats;

using LayerSemanticStats = BoundedMaterialLayerSemanticStats;

struct LayerDiagnostics {
    int layer_index{0};
    double z_mm{0.0};
    int segment_count{0};
    int odd_intersection_rows{0};
    int filled_spans{0};
    int model_pixels{0};
    int support_pixels{0};
    int rgb_non_zero_pixels{0};
    int white_non_zero_pixels{0};
    int support_non_zero_pixels{0};
    int varnish_non_zero_pixels{0};
    int island_count{0};
    int island_pixels{0};
    int unsupported_pixels{0};
    int filtered_island_count{0};
    int filtered_island_pixels{0};
    int bottom_projection_support_pixels{0};
    int unsupported_island_support_pixels{0};
    int full_vertical_projection_support_pixels{0};
    int internal_void_support_pixels{0};
    int upper_projection_support_pixels{0};
    int projection_base_support_pixels{0};
    SupportConnectivityDiagnostics support_connectivity;
    LayerSemanticStats semantic;
    std::array<ChannelStats, rgbwsv_channel_count> channel_stats{};
};

struct ReliefReportData {
    int total_columns{0};
    int hit_columns{0};
    int empty_columns{0};
    int multi_hit_columns{0};
    double z_min_mm{0.0};
    double z_max_mm{0.0};
    double thickness_min_mm{0.0};
    double thickness_max_mm{0.0};
    bool has_hits{false};
    Json::Array warnings;
};

struct TextureReportData {
    bool enabled{false};
    std::string apply_mode;
    std::string non_surface_rgb_policy{"model_material"};
    int top_surface_layers{1};
    std::string source{"filesystem"};
    int faces_with_uv{0};
    int faces_without_uv{0};
    std::uint64_t sampled_pixels{0};
    std::uint64_t fallback_pixels{0};
    std::uint64_t uv_out_of_range_pixels{0};
    int loaded_textures{0};
    int missing_textures{0};
    Json::Array materials;
    Json::Array texture_files;
    Json::Array warnings;
};

struct MaterialPolicyReportData {
    bool enabled{false};
    std::uint64_t rgb_print_pixels{0};
    std::uint64_t white_print_pixels{0};
    std::uint64_t varnish_print_pixels{0};
    Json::Array warnings;
};

struct MaterialRoleMappingReportData {
    bool enabled{false};
    std::string input_format;
    std::string default_role{"rgb"};
    bool allow_input_support_material{false};
    int material_count{0};
    int mapped_rgb{0};
    int mapped_white{0};
    int mapped_varnish{0};
    int mapped_ignore{0};
    int mapped_support_candidate{0};
    int mapped_support{0};
    int faces_with_mapped_material{0};
    int faces_without_mapped_material{0};
    Json::Array rules;
    Json::Array materials;
    Json::Array warnings;
};

namespace reports {

Json bbox_to_json(const BoundingBox& bbox);

void write_json_file(const std::filesystem::path& path, const Json& value);

Json rgb_to_json(const std::array<std::uint8_t, 3>& rgb);

Json channel_stats_array_to_json(
    const std::array<ChannelStats, rgbwsv_channel_count>& stats);

Json support_connectivity_summary_to_json(const std::vector<LayerDiagnostics>& diagnostics);

Json semantic_stats_to_json(const LayerSemanticStats& stats);

Json layer_diagnostics_to_json(const LayerDiagnostics& diagnostics);

Json texture_report_to_json(const TextureReportData& report);

Json material_policy_report_to_json(
    const SliceConfig& config, const MaterialPolicyReportData& report);

Json material_role_mapping_report_to_json(const MaterialRoleMappingReportData& report);

Json obj_mtl_material_report_to_json(const ModelReport& model_report);

Json three_mf_report_to_json(const ModelReport& model_report);

Json relief_report_to_json(
    const SliceConfig& config,
    const ReliefReportData& relief_report,
    const int support_pixels,
    const int columns_with_support);

}  // namespace reports

}  // namespace slicer_core
