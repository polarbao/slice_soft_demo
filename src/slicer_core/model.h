#pragma once

#include "slicer_core/model/ModelLoadConfig.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace slicer_core {

struct SliceConfig;

struct Vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

struct TexCoord {
    double u{0.0};
    double v{0.0};
};

struct BoundingBox {
    Vec3 min;
    Vec3 max;
};

struct Triangle {
    Vec3 a;
    Vec3 b;
    Vec3 c;
};

struct MaterialStat {
    std::string name;
    std::size_t face_count{0};
    std::size_t triangle_count{0};
};

struct MaterialInfo {
    std::string name;
    std::array<std::uint8_t, 3> diffuse_rgb{0, 0, 0};
    bool has_diffuse{false};
    std::filesystem::path diffuse_texture_path;
    bool has_texture{false};
    bool texture_exists{false};
    std::string texture_source{"filesystem"};
};

struct TriangleTextureInfo {
    bool has_uv{false};
    std::array<TexCoord, 3> uv{};
    std::string material_name;
};

struct AutoOrientReport {
    bool enabled{true};
    bool applied{false};
    double max_height_mm{9.0};
    std::string selected_orientation{"identity"};
    std::array<double, 3> rotation_deg{0.0, 0.0, 0.0};
    BoundingBox original_bbox_mm;
};

struct ThreeMfReportInfo {
    bool enabled{false};
    std::filesystem::path package_path;
    std::string model_part_path;
    std::string unit{"millimeter"};
    double unit_scale_to_mm{1.0};
    std::string xml_parser{"restricted_string_xml_reader"};
    int entry_count{0};
    int stored_entry_count{0};
    int deflated_entry_count{0};
    std::uint64_t total_uncompressed_bytes{0};
    int object_count{0};
    int component_count{0};
    int mesh_object_count{0};
    int triangle_count{0};
    int material_resource_count{0};
    int invalid_reference_count{0};
    int unknown_material_count{0};
    int ignored_resource_count{0};
    int color_group_count{0};
    int color_count{0};
    int color_group_resolved_triangles{0};
    int interpolated_color_fallback_count{0};
    int texture2d_count{0};
    int texture2d_group_count{0};
    int tex2coord_count{0};
    int texture_resource_count{0};
    int texture_loaded_count{0};
    int texture_missing_count{0};
    std::uint64_t texture_sampled_pixels{0};
    int texture_group_resolved_triangles{0};
    std::vector<std::string> unsupported_resources;
    std::vector<std::string> unsupported_extensions;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

struct ModelReport {
    std::filesystem::path model_path;
    std::string format;
    std::string stl_encoding;
    std::size_t vertex_count{0};
    std::size_t face_count{0};
    std::size_t triangle_count{0};
    std::size_t degenerate_triangle_count{0};
    std::size_t texcoord_count{0};
    std::size_t faces_with_uv{0};
    std::size_t faces_without_uv{0};
    std::vector<std::string> material_libraries;
    std::vector<MaterialStat> materials;
    std::vector<MaterialInfo> material_infos;
    AutoOrientReport auto_orient;
    ThreeMfReportInfo three_mf;
    BoundingBox bbox_mm;
    std::vector<Triangle> triangles;
    std::vector<TriangleTextureInfo> triangle_textures;
};

/**
 * @brief Load a model using the narrow base-layer configuration.
 * @param config Model load configuration.
 * @param configDir Base directory for relative input paths.
 * @return Imported model report.
 */
ModelReport load_model_report(
    const ModelLoadConfig& config,
    const std::filesystem::path& configDir);

/**
 * @brief Compatibility adapter for existing engine callers.
 * @param config Full engine configuration.
 * @param configDir Base directory for relative input paths.
 * @return Imported model report.
 */
ModelReport load_model_report(
    const SliceConfig& config,
    const std::filesystem::path& configDir);

/**
 * @brief Convert an engine configuration to the narrow model-load contract.
 * @param config Full engine configuration.
 * @return Narrow model-load configuration.
 */
ModelLoadConfig MakeModelLoadConfig(const SliceConfig& config);

}  // namespace slicer_core
